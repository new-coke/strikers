#!/usr/bin/env python3
"""Transactional installation of the current native dynamic GXM test.

Preserves the existing catalog, INI, saved games and other applications.
Writes a PC rollback SELF, stages both changed files, then verifies full FTP
readback. Never launches or changes runtime config; testing is a separate step.
"""
from datetime import datetime
from ftplib import FTP
from io import BytesIO
from pathlib import Path
from zoneinfo import ZoneInfo
import hashlib
import json
import sys
import time
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "smstrikers-port/tools"))
import vita_workflow_device as device

HOST = "192.168.1.79"
EVIDENCE = ROOT / "report_performance/evidence/NATIVE_GXM_RENDERER_2026-10-13/native-dynamic-20261013"
BUILD = ROOT / "smstrikers-port/build-vita-gxm-latest"
UV = ROOT / "ab-artifacts/native-gxm-direct-20261013/gxm_native_match_uv.bin"
PACK = ROOT / "ab-artifacts/native-gxm-direct-20261013/gxm_native_match_meshes_full_preview.bin"
OLD_SELF = "032ab9474cfd8dbc04955309854f75b19ba6ca9d21770444efa17df586ed7134"
PACK_HASH = "9018aaef5ba70b2c462eb4085b1fa03e0d8126eed9df216cc7107aa882c19888"
UV_HASH = "b9578b3b76208a5b8edef97a082abfbfc0b15b66a958c72885dfc2e8a6e5ef94"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def stamp():
    return datetime.now(ZoneInfo("Europe/Rome")).isoformat()


def preserve(name, data):
    target = EVIDENCE / name
    if target.exists() and target.read_bytes() != data:
        raise RuntimeError("Refusing to overwrite different rollback file: " + str(target))
    target.write_bytes(data)


def names(ftp, folder):
    ftp.cwd(folder)
    return set(ftp.nlst())


def main():
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    new_self = (BUILD / "strikers_vita.self").read_bytes()
    new_uv = UV.read_bytes()
    assert sha(new_uv) == UV_HASH, "Unexpected UV source"
    assert sha(PACK.read_bytes()) == PACK_HASH, "Unexpected full mesh pack"
    with zipfile.ZipFile(BUILD / "strikers_vita.vpk") as archive:
        assert archive.testzip() is None, "Invalid VPK ZIP"
        assert archive.read("eboot.bin") == new_self, "VPK/SELF mismatch"
    journal = {
        "started_at": stamp(), "title_id": "SMSVITA01", "host": HOST,
        "phase": "preflight", "new_self_sha256": sha(new_self),
        "new_uv_sha256": sha(new_uv), "new_vpk_sha256": sha((BUILD / "strikers_vita.vpk").read_bytes()),
        "catalog_sha256": PACK_HASH, "ini_changed": False,
        "changes": {},
    }

    def save():
        (EVIDENCE / "install-identity.json").write_text(json.dumps(journal, indent=2) + "\n")

    ftp = device.connect(HOST)
    targets = []
    committed = False
    try:
        installed = device.read(ftp, device.APP, "eboot.bin")
        ini = device.read(ftp, device.DATA, "strikers.ini")
        catalog = device.read(ftp, device.DATA, "gxm_native_match_meshes.bin")
        assert sha(installed) == OLD_SELF, "Unexpected current installed SELF"
        assert sha(catalog) == PACK_HASH, "Current mesh catalog does not match UV source"
        values = {}
        for line in ini.decode(errors="replace").splitlines():
            if not line.lstrip().startswith(("#", ";")) and "=" in line:
                key, value = line.split("=", 1)
                values.setdefault(key.strip(), value.strip())
        assert values.get("gxm_native_match_direct") == "1"
        assert values.get("gxm_direct_probe") == "0"
        assert values.get("vita_test_match") == "1"
        preserve("eboot.before", installed)
        preserve("strikers.ini.before", ini)
        journal["previous_self_sha256"] = sha(installed)
        journal["ini_sha256"] = sha(ini)
        journal["settings"] = {key: values.get(key) for key in
                               ("gxm_native_match_direct", "gxm_direct_probe", "vita_test_match",
                                "diagnostics", "vita_snapshot_play_frame")}
        data_names = names(ftp, device.DATA)
        old_uv = device.read(ftp, device.DATA, "gxm_native_match_uvs.bin") if "gxm_native_match_uvs.bin" in data_names else None
        if old_uv is not None:
            preserve("gxm_native_match_uvs.before", old_uv)
        targets = [
            {"folder": device.DATA, "name": "gxm_native_match_uvs.bin", "new": new_uv,
             "old": old_uv, "modified": False},
            {"folder": device.APP, "name": "eboot.bin", "new": new_self,
             "old": installed, "modified": False},
        ]
        for item in targets:
            item["stage"] = item["name"] + ".native-dynamic-stage"
            item["backup"] = item["name"] + ".native-dynamic-previous"
            remote = names(ftp, item["folder"])
            assert item["stage"] not in remote and item["backup"] not in remote, (
                "Unresolved previous installation stage: " + item["name"])
        journal["kill_response"] = device.command(HOST, "kill SMSVITA01")
        # VitaCompanion reports an error rather than "already stopped" for
        # a dormant title. Starting the exact requested app and subsequently
        # killing that title gives a positive stop confirmation.
        if journal["kill_response"] != "Killed.":
            journal["launch_for_stop"] = device.command(HOST, "launch SMSVITA01")
            assert journal["launch_for_stop"] == "Launched.", (
                "Cannot activate requested app for safe targeted stop")
            time.sleep(3)
            journal["kill_retry"] = device.command(HOST, "kill SMSVITA01")
            assert journal["kill_retry"] == "Killed.", (
                "Targeted stop not confirmed: " + journal["kill_retry"])
        journal["phase"] = "staging"
        save()
        for item in targets:
            if item["new"] == item["old"]:
                continue
            ftp.cwd(item["folder"])
            ftp.storbinary("STOR " + item["stage"], BytesIO(item["new"]))
            assert device.read(ftp, item["folder"], item["stage"]) == item["new"], (
                "Staged readback mismatch: " + item["name"])
        journal["phase"] = "promoting"
        save()
        for item in targets:
            if item["new"] == item["old"]:
                continue
            if item["old"] is not None:
                assert device.read(ftp, item["folder"], item["name"]) == item["old"]
                ftp.cwd(item["folder"])
                ftp.rename(item["name"], item["backup"])
                item["modified"] = True
            else:
                assert item["name"] not in names(ftp, item["folder"])
                item["modified"] = True
            ftp.cwd(item["folder"])
            ftp.rename(item["stage"], item["name"])
        for item in targets:
            actual = device.read(ftp, item["folder"], item["name"])
            assert actual == item["new"], "Final readback mismatch: " + item["name"]
            journal["changes"][item["name"]] = {
                "path": item["folder"] + "/" + item["name"],
                "bytes": len(actual), "sha256": sha(actual),
                "previous_sha256": sha(item["old"]) if item["old"] is not None else None,
            }
        assert device.read(ftp, device.DATA, "strikers.ini") == ini, "INI changed"
        assert device.read(ftp, device.DATA, "gxm_native_match_meshes.bin") == catalog, "Mesh pack changed"
        committed = True
        journal["phase"] = "installed_verified"
        journal["completed_at"] = stamp()
        save()
    except Exception as error:
        journal["error"] = repr(error)
        rollback_errors = []
        for item in reversed(targets):
            if not item["modified"]:
                continue
            try:
                ftp.cwd(item["folder"])
                remote = names(ftp, item["folder"])
                if item["name"] in remote:
                    ftp.delete(item["name"])
                if item["old"] is not None:
                    if item["backup"] in remote:
                        ftp.rename(item["backup"], item["name"])
                    else:
                        ftp.storbinary("STOR " + item["name"], BytesIO(item["old"]))
                    assert device.read(ftp, item["folder"], item["name"]) == item["old"]
            except Exception as rollback_error:
                rollback_errors.append(item["name"] + ": " + repr(rollback_error))
        journal["phase"] = "rollback_failed" if rollback_errors else "failed_restored"
        journal["rollback_errors"] = rollback_errors
        save()
        raise
    finally:
        for item in targets:
            try:
                ftp.cwd(item["folder"])
                remote = names(ftp, item["folder"])
                if item["stage"] in remote:
                    ftp.delete(item["stage"])
                if committed and item["backup"] in remote:
                    ftp.delete(item["backup"])
            except Exception as cleanup_error:
                journal.setdefault("cleanup_errors", []).append(str(cleanup_error))
        save()
        ftp.close()
    print(json.dumps(journal, indent=2))


if __name__ == "__main__":
    main()

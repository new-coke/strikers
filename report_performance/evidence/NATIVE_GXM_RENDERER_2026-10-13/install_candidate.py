#!/usr/bin/env python3
"""Install the artifact-bound native candidate into the existing Strikers app.

Stage/read back both files before replacement. Reuse the existing canonical
rollback SELF/pack, retain only small evidence here, and roll back both targets
if promotion fails. Do not change the INI or start a benchmark.
"""
from datetime import datetime
from ftplib import error_perm
from io import BytesIO
from pathlib import Path
from zoneinfo import ZoneInfo
import hashlib
import json
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "smstrikers-port/tools"))
import vita_workflow_device as device

OUT = Path(__file__).resolve().parent
BUILD = ROOT / "smstrikers-port/build-vita-gxm-latest"
HOST = "192.168.1.79"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def stamp():
    return datetime.now(ZoneInfo("Europe/Rome")).isoformat()


def main():
    evidence = json.loads((OUT / "validation.json").read_text())
    for name, digest in evidence["sources"].items():
        assert sha((ROOT / name).read_bytes()) == digest, "Source changed: " + name
    for name, record in evidence["artifacts"].items():
        assert sha((BUILD / name).read_bytes()) == record["sha256"], "Artifact changed: " + name
    new_self = (BUILD / "strikers_vita.self").read_bytes()
    with zipfile.ZipFile(BUILD / "strikers_vita.vpk") as archive:
        assert archive.testzip() is None
        assert archive.read("eboot.bin") == new_self
    old_self_path = BUILD / "strikers_vita.previous.self"
    old_pack_path = ROOT / evidence.get("previous_catalog_path", "ab-artifacts/native-gxm-direct-20261013/gxm_native_match_meshes.bin")
    new_pack_path = ROOT / evidence.get("catalog_path", "ab-artifacts/native-gxm-direct-20261013/gxm_native_match_meshes_expanded_preview.bin")
    new_pack = new_pack_path.read_bytes()
    assert sha(new_pack) == evidence["expanded_catalog_sha256"]
    # A corrective reinstall already has the expanded pack. Reuse its
    # canonical local copy for rollback instead of duplicating an asset.
    prior_install = OUT / "install-identity.mvp-error.json"
    if prior_install.exists() and "previous_catalog_path" not in evidence:
        prior = json.loads(prior_install.read_text())
        if prior["installed"]["gxm_native_match_meshes.bin"]["sha256"] == sha(new_pack):
            old_pack_path = new_pack_path

    journal = {
        "date": stamp(), "host": HOST, "title_id": "SMSVITA01", "phase": "preflight",
        "candidate_self_sha256": sha(new_self), "candidate_pack_sha256": sha(new_pack),
        "candidate_vpk_sha256": evidence["artifacts"]["strikers_vita.vpk"]["sha256"],
        "rollback_self": str(old_self_path), "rollback_pack": str(old_pack_path),
        "ini_changed": False, "launch_requested": False,
    }
    def save():
        (OUT / "install-identity.json").write_text(json.dumps(journal, indent=2) + "\n")

    ftp = device.connect(HOST)
    targets = []
    committed = False
    try:
        ini = device.read(ftp, device.DATA, "strikers.ini")
        settings = {}
        for line in ini.decode(errors="replace").splitlines():
            line = line.strip()
            if not line or line.startswith(("#", ";")) or "=" not in line:
                continue
            key, value = line.split("=", 1)
            settings.setdefault(key.strip(), value.strip())
        assert settings.get("gxm_native_match_direct") == "1", "Native mode is not selected"
        assert settings.get("gxm_direct_probe") == "0", "Standalone probe is selected"
        journal["ini_sha256"] = sha(ini)
        journal["effective_native_settings"] = {
            key: settings.get(key) for key in
            ("gxm_native_match_direct", "gxm_direct_probe", "vita_test_match", "diagnostics", "gxm_shader_profile")
        }
        before_ini = OUT / "strikers.ini.before"
        assert not before_ini.exists() or before_ini.read_bytes() == ini, "Different INI backup already exists"
        before_ini.write_bytes(ini)
        targets = [
            {"folder": device.DATA, "name": "gxm_native_match_meshes.bin",
             "old": old_pack_path.read_bytes(), "new": new_pack, "touched": False},
            {"folder": device.APP, "name": "eboot.bin",
             "old": old_self_path.read_bytes(), "new": new_self, "touched": False},
        ]
        for target in targets:
            folder, name = target["folder"], target["name"]
            target["stage"] = name + ".native-gxm-stage"
            target["backup"] = name + ".native-gxm-previous"
            assert device.read(ftp, folder, name) == target["old"], "Remote rollback identity changed: " + name
            ftp.cwd(folder)
            names = ftp.nlst()
            assert target["stage"] not in names and target["backup"] not in names, "Unresolved prior transaction: " + name
        try:
            old_status = device.read(ftp, device.DATA, "gxm_native_match_status.csv")
            (OUT / ("native-status.before-" + sha(old_status)[:12] + ".csv")).write_bytes(old_status)
            journal["previous_status_sha256"] = sha(old_status)
        except error_perm as exc:
            if not str(exc).startswith("550"):
                raise
        save()
        journal["kill_response"] = device.command(HOST, "kill SMSVITA01")
        journal["phase"] = "staging"
        save()
        for target in targets:
            if target["old"] == target["new"]:
                continue
            ftp.cwd(target["folder"])
            ftp.storbinary("STOR " + target["stage"], BytesIO(target["new"]))
            assert device.read(ftp, target["folder"], target["stage"]) == target["new"], "Stage readback mismatch"
        journal["phase"] = "promoting"
        save()
        for target in targets:
            if target["old"] == target["new"]:
                continue
            assert device.read(ftp, target["folder"], target["name"]) == target["old"], "Remote changed during staging"
            ftp.cwd(target["folder"])
            target["touched"] = True
            ftp.rename(target["name"], target["backup"])
            assert device.read(ftp, target["folder"], target["backup"]) == target["old"], "Rollback readback mismatch"
            ftp.cwd(target["folder"])
            ftp.rename(target["stage"], target["name"])
        journal["installed"] = {}
        for target in targets:
            actual = device.read(ftp, target["folder"], target["name"])
            assert actual == target["new"], "Installed readback mismatch"
            journal["installed"][target["name"]] = {
                "path": target["folder"] + "/" + target["name"],
                "sha256": sha(actual), "bytes": len(actual), "previous_sha256": sha(target["old"]),
            }
        assert device.read(ftp, device.DATA, "strikers.ini") == ini, "INI changed during install"
        committed = True
        journal["phase"] = "installed_verified"
        journal["completed_at"] = stamp()
        save()
    except Exception as error:
        journal["error"] = repr(error)
        rollback_errors = []
        for target in reversed(targets):
            if not target["touched"]:
                continue
            try:
                ftp.cwd(target["folder"])
                names = ftp.nlst()
                if target["backup"] in names:
                    if target["name"] in names:
                        ftp.delete(target["name"])
                    ftp.rename(target["backup"], target["name"])
                else:
                    ftp.storbinary("STOR " + target["name"], BytesIO(target["old"]))
                assert device.read(ftp, target["folder"], target["name"]) == target["old"]
            except Exception as rollback_error:
                rollback_errors.append(target["name"] + ": " + repr(rollback_error))
        journal["phase"] = "rollback_failed" if rollback_errors else "failed_restored"
        journal["rollback_errors"] = rollback_errors
        save()
        raise
    finally:
        cleanup_errors = []
        for target in targets:
            try:
                ftp.cwd(target["folder"])
                names = ftp.nlst()
                if target["stage"] in names:
                    ftp.delete(target["stage"])
                if committed and target["backup"] in names:
                    ftp.delete(target["backup"])
            except Exception as cleanup_error:
                cleanup_errors.append(target["name"] + ": " + repr(cleanup_error))
        journal["cleanup_errors"] = cleanup_errors
        save()
        try:
            ftp.quit()
        except Exception:
            ftp.close()
    print(json.dumps(journal, indent=2))


if __name__ == "__main__":
    main()

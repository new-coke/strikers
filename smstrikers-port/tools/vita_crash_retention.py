"""Validate retained evidence for an already reviewed Vita crash.

This module never writes files, connects to a device, or acknowledges a new
crash. The caller must retain the exact dump bytes before calling it.
"""
import hashlib
import json
import os
from pathlib import Path
import re
import stat


class CrashRetentionError(RuntimeError):
    pass


CAPSULE_SCHEMA = "strikers-crash-analysis-capsule-v1"
CAPSULE_SCHEMA_V2 = "strikers-crash-analysis-capsule-v2"
EXTRACT_SCHEMA = "strikers-crash-elf-extract-v1"
BUILD_NAMES = frozenset(("strikers", "strikers.velf", "strikers.map",
                         "strikers_vita.self", "strikers_vita.vpk"))
REQUIRED_ROLES = frozenset(("inventory", "diagnosis", "call_chain", "elf_extract",
                            "disassembly", "original_disassembly", "parser",
                            "allocator_state", "corrupt_chunk", "renderer_state",
                            "upload_descriptor", "ehabi_decode"))
OPTIONAL_ROLES = frozenset(("memory_state", "memory_chunk", "parse_log"))
AUDIO_PROFILE = "audio_null_track"
AUDIO_REQUIRED_ROLES = frozenset(("inventory", "diagnosis", "call_chain", "elf_extract",
                                  "disassembly", "parser", "track_state", "ehabi_decode"))
AUDIO_OPTIONAL_ROLES = frozenset(("run_identity", "ui_review", "parse_log",
                                  "diagnosis_notes", "original_disassembly"))
AUDIO_SCOPE = {"immediate_fault_identified": True, "verified_stack_frames": 7,
               "lifecycle_basis": "source_and_elf_reconstruction",
               "live_manager_global_captured": False,
               "hardware_regression_passed": False,
               "new_addresses_require_matching_elf": True}
MAX_EVIDENCE_FILE = 1024 * 1024
MAX_EVIDENCE_TOTAL = 2 * 1024 * 1024


def _fail(message):
    raise CrashRetentionError(message)


def _digest(value):
    if not isinstance(value, str) or not re.fullmatch(r"[0-9a-f]{64}", value):
        _fail("Invalid SHA-256 in crash retention evidence")
    return value


def _size(value, maximum):
    if type(value) is not int or not 0 < value <= maximum:
        _fail("Invalid bounded size in crash retention evidence")
    return value


def _parts(relative):
    if (not isinstance(relative, str) or not relative or len(relative) > 512
            or "\\" in relative or ":" in relative or "\0" in relative):
        _fail("Unsafe crash evidence path")
    parts = relative.split("/")
    if any(part in ("", ".", "..") for part in parts):
        _fail("Unsafe crash evidence path")
    return parts


def read_regular(root, relative, maximum):
    """Read one bounded regular file, rejecting symlinks in every component."""
    parts = _parts(relative)
    directory = file_fd = None
    try:
        directory = os.open(Path(root), os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
        for part in parts[:-1]:
            child = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW,
                            dir_fd=directory)
            os.close(directory)
            directory = child
        file_fd = os.open(parts[-1], os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK,
                          dir_fd=directory)
        before = os.fstat(file_fd)
        if not stat.S_ISREG(before.st_mode) or not 0 < before.st_size <= maximum:
            _fail("Crash evidence is not a bounded regular file: " + relative)
        with os.fdopen(file_fd, "rb") as stream:
            file_fd = None
            data = stream.read(maximum + 1)
            after = os.fstat(stream.fileno())
        if len(data) != before.st_size or after.st_size != before.st_size:
            _fail("Crash evidence changed during read: " + relative)
        return data
    except (OSError, ValueError) as error:
        raise CrashRetentionError("Cannot read retained crash evidence: " + relative) from error
    finally:
        if file_fd is not None:
            os.close(file_fd)
        if directory is not None:
            os.close(directory)


def _object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            _fail("Duplicate JSON key in crash retention evidence")
        result[key] = value
    return result


def parse_json(data):
    try:
        value = json.loads(data, object_pairs_hook=_object,
                           parse_constant=lambda _: _fail("Non-finite JSON value"))
    except (ValueError, UnicodeError, RecursionError) as error:
        raise CrashRetentionError("Invalid crash retention JSON") from error
    if not isinstance(value, dict):
        _fail("Crash retention JSON must be an object")
    return value


def _identity(item, expected_sha, expected_size=None):
    if not isinstance(item, dict) or _digest(item.get("sha256")) != expected_sha:
        _fail("Crash retention identity mismatch")
    if expected_size is not None and (type(item.get("size")) is not int
                                      or item["size"] != expected_size):
        _fail("Crash retention size mismatch")


def _inventory(data, record, name):
    value = parse_json(data)
    if value.get("schema") != "strikers-core-offline-inventory-v1":
        _fail("Unsupported reviewed crash inventory schema")
    _identity(value.get("core"), record["sha256"])
    _identity(value.get("elf"), record["elf_sha256"])
    if value["core"].get("path") != ".debug/" + name:
        _fail("Reviewed crash inventory names a different dump")
    return value


def _verify_capsule(name, dump_bytes, record, evidence_root, inventory_data):
    capsule_path = record.get("capsule")
    capsule_sha = _digest(record.get("capsule_sha256"))
    raw = read_regular(evidence_root, capsule_path, 256 * 1024)
    if hashlib.sha256(raw).hexdigest() != capsule_sha:
        _fail("Retained crash capsule hash mismatch")
    capsule = parse_json(raw)
    audio_profile = capsule.get("schema") == CAPSULE_SCHEMA_V2
    if audio_profile:
        if capsule.get("profile") != AUDIO_PROFILE:
            _fail("Unsupported crash capsule v2 profile")
        required_roles, optional_roles = AUDIO_REQUIRED_ROLES, AUDIO_OPTIONAL_ROLES
    elif capsule.get("schema") == CAPSULE_SCHEMA:
        required_roles, optional_roles = REQUIRED_ROLES, OPTIONAL_ROLES
    else:
        _fail("Unsupported crash capsule schema")
    if capsule.get("status") != "analyzed_hardware_unresolved":
        _fail("Crash capsule must preserve unresolved hardware status")
    _identity(capsule.get("core"), record["sha256"], len(dump_bytes))
    _identity(capsule.get("elf"), record["elf_sha256"])
    _identity(capsule.get("self"), record["self_sha256"])
    if (capsule["core"].get("name") != name
            or capsule["core"].get("path") != ".debug/" + name):
        _fail("Crash capsule names a different dump")
    scope = capsule.get("analysis_scope")
    if audio_profile:
        if (not isinstance(scope, dict) or scope.keys() != AUDIO_SCOPE.keys()
                or any(type(scope[key]) is not type(value) or scope[key] != value
                       for key, value in AUDIO_SCOPE.items())):
            _fail("Audio crash capsule lacks explicit analysis limits")
    elif (not isinstance(scope, dict)
          or scope.get("fault_cause_resolved") is not False
          or scope.get("hardware_regression_passed") is not False
          or scope.get("new_addresses_require_matching_elf") is not True):
        _fail("Crash capsule lacks explicit analysis limits")

    products = capsule.get("build_artifacts")
    if not isinstance(products, list) or len(products) != len(BUILD_NAMES):
        _fail("Crash capsule needs the exact former-build identity set")
    by_name = {}
    for product in products:
        if not isinstance(product, dict):
            _fail("Invalid former-build artifact")
        product_name = product.get("name")
        if (not isinstance(product_name, str) or product_name not in BUILD_NAMES
                or product_name in by_name):
            _fail("Invalid or duplicate former-build artifact")
        _digest(product.get("sha256"))
        _size(product.get("size"), 64 * 1024 * 1024)
        by_name[product_name] = product
    for field, product_name in (("elf", "strikers"), ("self", "strikers_vita.self")):
        product = by_name[product_name]
        _identity(capsule[field], product["sha256"], product["size"])

    files = capsule.get("files")
    if not isinstance(files, list) or not len(required_roles) <= len(files) <= 32:
        _fail("Crash capsule evidence list is incomplete or oversized")
    roles, paths, content, total = {}, set(), {}, 0
    for item in files:
        if not isinstance(item, dict):
            _fail("Invalid capsule evidence record")
        role = item.get("role")
        relative = item.get("path")
        _parts(relative)
        if (not isinstance(role, str) or role not in required_roles | optional_roles or role in roles
                or relative in paths or relative == capsule_path):
            _fail("Unknown or duplicate crash evidence role/path")
        size = _size(item.get("size"), MAX_EVIDENCE_FILE)
        total += size
        if total > MAX_EVIDENCE_TOTAL:
            _fail("Crash evidence exceeds capsule retention bound")
        data = read_regular(evidence_root, relative, MAX_EVIDENCE_FILE)
        if len(data) != size or hashlib.sha256(data).hexdigest() != _digest(item.get("sha256")):
            _fail("Retained crash evidence changed: " + relative)
        roles[role], content[role] = item, data
        paths.add(relative)
    if not required_roles <= roles.keys():
        _fail("Required crash evidence role is absent")
    if (roles["inventory"]["path"] != record["analysis"]
            or roles["diagnosis"]["path"] != record.get("diagnosis")
            or content["inventory"] != inventory_data):
        _fail("Capsule does not bind the reviewed inventory/diagnosis")
    extract = parse_json(content["elf_extract"])
    if extract.get("schema") != EXTRACT_SCHEMA:
        _fail("Unsupported retained ELF extract schema")
    for field, sha in (("core", record["sha256"]), ("elf", record["elf_sha256"]),
                       ("self", record["self_sha256"])):
        _identity(extract.get(field), sha)
    if audio_profile:
        # Import only for v2; the old allocator capsule and full-build path
        # retain their existing validation and return values.
        from vita_crash_audio import AudioEvidenceError, validate_extract
        try:
            validate_extract(extract, dump_bytes)
            track = parse_json(content["track_state"])
            if (track.get("schema") != "strikers-audio-track-state-v1"
                    or track.get("profile") != AUDIO_PROFILE
                    or track.get("manager") != extract["proof"]["manager"]
                    or track.get("windows") != extract["core_windows"]):
                _fail("Audio track-state evidence disagrees with the retained proof")
            _identity(track.get("core"), record["sha256"], len(dump_bytes))
            diagnosis = parse_json(content["diagnosis"])
            if (diagnosis.get("schema") != "strikers-core-diagnosis-v1"
                    or len(diagnosis["verified_call_chain"]) != 7
                    or diagnosis["fault"]["pc_elf"] != "0x%08x" % extract["proof"]["pc_elf"]
                    or diagnosis["fault"]["this_value"] != "0x00000000"):
                _fail("Audio diagnosis disagrees with the retained proof")
            for field, digest in (("core", record["sha256"]), ("elf", record["elf_sha256"])):
                _identity(diagnosis["inputs"].get(field), digest)
        except (AudioEvidenceError, KeyError, TypeError, ValueError, OverflowError) as error:
            raise CrashRetentionError("Invalid retained audio proof: " + str(error)) from error
    return capsule_sha


def verify_reviewed_crash(name, dump_bytes, record, *, evidence_root, build_root):
    """Accept only an exact reviewed crash with verified full/capsule evidence.

    The return value documents retention, never crash resolution. The caller
    keeps dump_bytes at .debug/name; this function does not rotate any artifact.
    """
    if (len(_parts(name)) != 1 or not name.startswith("psp2core-")
            or not name.endswith(".psp2dmp") or not isinstance(dump_bytes, bytes)
            or not 0 < len(dump_bytes) <= 64 * 1024 * 1024):
        _fail("Invalid retained dump input")
    digest = hashlib.sha256(dump_bytes).hexdigest()
    if not isinstance(record, dict) or record.get("sha256") != digest:
        _fail("New Strikers crash; inspect .debug/" + name)
    for field in ("sha256", "elf_sha256", "self_sha256"):
        _digest(record.get(field))
    mode = record.get("retention_mode", "full")
    if mode not in ("full", "analysis_capsule"):
        _fail("Unsupported crash retention mode")
    inventory_data = read_regular(evidence_root, record.get("analysis"), MAX_EVIDENCE_FILE)
    _inventory(inventory_data, record, name)
    result = {"mode": mode, "core_sha256": digest,
              "self_sha256": record["self_sha256"], "elf_sha256": record["elf_sha256"],
              "hardware_status": "unresolved"}
    if mode == "analysis_capsule":
        result["capsule_sha256"] = _verify_capsule(
            name, dump_bytes, record, evidence_root, inventory_data)
        return result

    for suffix in ("", ".previous"):
        try:
            elf = read_regular(build_root, "strikers" + suffix, 64 * 1024 * 1024)
            executable = read_regular(build_root, "strikers_vita.self" + suffix, 32 * 1024 * 1024)
        except CrashRetentionError:
            continue
        if (hashlib.sha256(elf).hexdigest() == record["elf_sha256"]
                and hashlib.sha256(executable).hexdigest() == record["self_sha256"]):
            return result
    _fail("The reviewed crash's complete executable and symbols must remain intact")

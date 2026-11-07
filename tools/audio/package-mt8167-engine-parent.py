#!/usr/bin/env python3
"""Prepare a distinct EnginePcm INF around the fixed signed parent; no build/sign/install."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess

RUNTIME = "de214c8b7495913d0716f149b24fd210ef42de8a"
SIGNED_PARENT = "99338c1bbe30205b09af6c594a7c79cc4ef8a7c080a692330d36cdbb973f86ca"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--signed-bundle", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    out = args.output.resolve()
    if root not in out.parents:
        parser.error("output must be a new directory inside this checkout")
    bundle = args.signed_bundle.resolve()
    manifest = read_json(bundle / "signed-manifest.json")
    if manifest.get("binary_source_commit") != RUNTIME or manifest.get("audio_mode") != "engine_pcm_retained":
        parser.error("requires the fixed de214c8 EnginePcm signed bundle")
    entries = {item["path"].replace("\\", "/"): item["sha256"].lower() for item in manifest["files"]}
    for name in ("parent/mt8167-shared.sys", "parent/parent-manifest.json", "parent/shared-parent.inf"):
        if digest(bundle / name) != entries.get(name):
            parser.error("signed manifest hash mismatch: " + name)
    if entries["parent/mt8167-shared.sys"] != SIGNED_PARENT:
        parser.error("unexpected signed parent SYS; do not substitute the run26 parent")
    caps = read_json(bundle / "parent/parent-manifest.json")
    if caps.get("source_commit") != RUNTIME or caps.get("flags", {}).get("A04_SHARED_ENGINE_PCM") != "1":
        parser.error("missing EnginePcm parent provenance/capability")
    inf = root / "drivers/bus/mt8167-shared/prototype/shared-parent-engine-pcm.inf"
    version = re.search(r"(?m)^DriverVer=([^\r\n]+)", inf.read_text()).group(1)
    if version != "11/06/2026,2026.11.7.1005":
        parser.error("unexpected EnginePcm INF DriverVer")
    commit = subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip()
    dirty = bool(subprocess.check_output(["git", "-C", str(root), "status", "--porcelain"], text=True).strip())
    # A fresh output never inherits the obsolete CAT. Keep raw build metadata
    # under an explicitly historical name: its SYS hash predates signing.
    out.mkdir(parents=True, exist_ok=False)
    shutil.copyfile(bundle / "parent/mt8167-shared.sys", out / "mt8167-shared.sys")
    shutil.copyfile(inf, out / "shared-parent.inf")
    shutil.copyfile(bundle / "parent/parent-manifest.json", out / "runtime-parent-manifest.json")
    if digest(out / "mt8167-shared.sys") != SIGNED_PARENT:
        raise RuntimeError("parent SYS changed during copy")
    result = {
        "schema_version": 1,
        "state": "catalog-resign-required",
        "binary_source_commit": RUNTIME,
        "inf_source_commit": commit,
        "inf_source_dirty": dirty,
        "driver_date": "11/06/2026",
        "driver_version": "2026.11.7.1005",
        "driver_ver": version,
        "signed_parent_sys_sha256": SIGNED_PARENT,
        "source_signed_manifest_sha256": digest(bundle / "signed-manifest.json"),
        "runtime_recompiled": False,
        "runtime_manifest_is_pre_sign_history": True,
        "hardware_verified": False,
        "loaded_image_verified": False,
        "files": {name: digest(out / name) for name in
                  ("mt8167-shared.sys", "shared-parent.inf", "runtime-parent-manifest.json")},
    }
    (out / "package-manifest.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()

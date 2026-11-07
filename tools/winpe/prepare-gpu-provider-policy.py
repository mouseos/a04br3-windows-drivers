#!/usr/bin/env python3
"""Validate one real GPU package and generate the policy for its signed installer."""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess

FILES = ("mt8167-ge8300.inf", "mt8167-ge8300.sys", "mt8167-ge8300.cat", "ge8300-mips.fw")
SOURCE = "04d0e573bd1bee7b619d25441f8ac05d28d5ccd5"
OLD_SYS = "9c3c7d846565c0e5e73c2afe1d4d9900089f74bbe92d97ff8d1627729321265e"
FIXED_HASHES = {
    "mt8167-ge8300.inf": "a49c79d2b765470cbf82001195d0aec43a175eacae77a48a3f5daac5e7c21444",
    "mt8167-ge8300.sys": "13c8a9ca6b6d7fef921afd38d60cf84ea8e37b599b51af82d6e90342d8b9da07",
    "mt8167-ge8300.cat": "5d4cfad2d2b0e1758777c0d531aae92e172a1b4d53b206649b5f7f2b08c4d828",
    "ge8300-mips.fw": "5c1fdff55e8e57f8be0c6c36b94f2167ea53ece84308068bc79c6b9c37cd7fe1",
}
FIRMWARE = "5c1fdff55e8e57f8be0c6c36b94f2167ea53ece84308068bc79c6b9c37cd7fe1"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--candidate", type=Path, required=True)
    ap.add_argument("--output", type=Path, required=True)
    a = ap.parse_args()
    root = Path(__file__).resolve().parents[2]
    if not a.candidate.is_dir():
        ap.error("real GPU candidate package has not been received")
    candidate = a.candidate.resolve(strict=True)
    out = a.output.resolve()
    if root not in out.parents or out.exists():
        ap.error("output must be a new directory inside this checkout")
    signed = json.loads((candidate / "signed-manifest.json").read_text(encoding="utf-8-sig"))
    if signed.get("source_commit") != SOURCE or signed.get("native_clear_activate") is not True:
        ap.error("signed manifest does not describe the fixed activation candidate")
    for item in signed["files"]:
        if item["path"] not in (*FILES, "build-config.json", "provider-package-manifest.json", "mt8167-development.cer"):
            ap.error("unexpected signed manifest path")
        if sha(candidate / item["path"]) != item["sha256"].lower():
            ap.error("signed manifest file mismatch: " + item["path"])
    if {item["path"] for item in signed["files"]} != {*FILES, "build-config.json", "provider-package-manifest.json", "mt8167-development.cer"}:
        ap.error("incomplete signed manifest")
    raw = candidate / "provider-package-manifest.json"
    manifest = json.loads(raw.read_text(encoding="utf-8-sig"))
    config = json.loads((candidate / "build-config.json").read_text(encoding="utf-8-sig"))
    if manifest.get("source_commit") != SOURCE or config.get("source_commit") != SOURCE:
        ap.error("requires the fixed 04d0e57 source package")
    for part in (manifest, config):
        if str(part.get("native_clear_activate")) != "1" or str(part.get("native_clear_job")) != "1":
            ap.error("explicit private-clear activation/job is not enabled")
        if part.get("native_ta3d_publish_gate") != "armed-one-shot-private-480000":
            ap.error("unexpected native publish gate")
    if str(config.get("native_backing")) != "1":
        ap.error("native backing was not selected")
    if manifest.get("fingerprint_state") != "final-signed-package" or manifest.get("signature_status") != "Valid":
        ap.error("final signed package manifest is required")
    if signed.get("signing_thumbprint") != manifest.get("signer_thumbprint") or signed.get("signing_thumbprint") != "BCBF6C456C6F0DD0818434763B189488FBF13570":
        ap.error("unexpected candidate signer identity")
    if manifest.get("selected_installed_sys_sha256") != OLD_SYS:
        ap.error("manifest baseline SYS differs from the confirmed 9c3 baseline")
    if manifest.get("selected_installed_driver_ver") != "11/06/2026,0.0.1.3":
        ap.error("manifest baseline INF differs from the supplied 0.0.1.3 baseline")
    commit = SOURCE
    subprocess.run(["git", "-C", str(root), "cat-file", "-e", commit + "^{commit}"], check=True)
    if set(manifest.get("files", {})) != set(FILES):
        ap.error("manifest must name exactly the four fixed package files")
    hashes = [sha(raw)]
    for name in FILES:
        path = candidate / name
        if path.is_symlink() or not path.is_file() or not 0 < path.stat().st_size <= 16 * 1024 * 1024:
            ap.error("invalid package file: " + name)
        digest = sha(path)
        if digest != FIXED_HASHES[name]:
            ap.error("file differs from the recovered fixed signed GPU candidate: " + name)
        if digest != manifest["files"][name]["sha256"] or path.stat().st_size != manifest["files"][name]["bytes"]:
            ap.error("candidate hash mismatch: " + name)
        hashes.append(digest)
    if hashes[2] == OLD_SYS or hashes[4] != FIRMWARE:
        ap.error("old provider or unexpected GPU firmware")
    pe = (candidate / FILES[1]).read_bytes()
    if len(pe) < 64 or pe[:2] != b"MZ":
        ap.error("candidate is not an ARM32 PE image")
    offset = int.from_bytes(pe[60:64], "little")
    if pe[offset:offset+6] != b"PE\0\0\xc4\x01":
        ap.error("candidate must be ARMNT, not a host image")
    inf_bytes = (candidate / FILES[0]).read_bytes()
    inf = inf_bytes.decode("utf-16" if inf_bytes[:2] in (b"\xff\xfe", b"\xfe\xff") else "utf-8-sig")
    source_inf = subprocess.check_output(["git", "-C", str(root), "show", SOURCE + ":drivers/gpu/mt8167/mt8167-ge8300.inf"]).decode("utf-8-sig")
    if inf.replace("\r\n", "\n") != source_inf.replace("\r\n", "\n"):
        ap.error("package INF differs from the fixed candidate source")
    version = re.search(r"(?im)^DriverVer\s*=\s*([^\r\n;]+)", inf)
    if not version or version.group(1).strip() != manifest.get("driver_ver"):
        ap.error("manifest/INF DriverVer mismatch")
    if manifest["driver_ver"] != "11/07/2026,0.0.2.1":
        ap.error("requires the source-committed 04d0e57 INF 11/07/2026,0.0.2.1")
    if config.get("driver_ver") != manifest["driver_ver"] or config.get("inf_sha256", "").lower() != hashes[1]:
        ap.error("build config and signed INF differ")
    date, text = manifest["driver_ver"].split(",")
    parts = tuple(int(x) for x in text.split("."))
    if len(parts) != 4 or any(x > 65535 or x < 0 for x in parts) or parts <= (0, 0, 1, 3):
        ap.error("GPU DriverVersion must exceed 0.0.1.3")
    if datetime.datetime.strptime(date, "%m/%d/%Y").date() < datetime.date(2026, 11, 6):
        ap.error("GPU DriverDate must not precede 11/06/2026")
    if not re.search(r"(?im)^%Device%\s*=\s*Install\s*,\s*ACPI\\MOU0006\s*$", inf):
        ap.error("INF must target the single fixed GPU hardware ID")
    if not re.search(r"(?im)^ServiceBinary\s*=\s*%12%\\mt8167-ge8300\.sys\s*$", inf):
        ap.error("INF service binary differs from the fixed provider")
    if not re.search(r"(?im)^AddService\s*=\s*MT8167GE8300\s*,", inf):
        ap.error("INF service name mismatch")
    if re.search(r"(?i)\b(CoInstallers32|RegisterDlls|UnregisterDlls|RunPreSetupCommands|RunPostSetupCommands)\b", inf):
        ap.error("no executable coinstaller or command directives are accepted")
    out.mkdir(parents=True)
    package = out / "provider"
    package.mkdir()
    for index, name in enumerate(("provider-package-manifest.json",) + FILES):
        shutil.copyfile(candidate / name, package / name)
        if sha(package / name) != hashes[index]:
            ap.error("package changed during staging: " + name)
    # Pin the manifest's exact bytes too; no runtime JSON parser or input paths.
    header = "/* Generated only from the supplied real candidate. */\n"
    header += '#define GPU_SOURCE_COMMIT "' + commit + '"\n'
    encoded = sum(value << shift for value, shift in zip(parts, (48, 32, 16, 0)))
    header += f"#define GPU_DRIVER_VERSION 0x{encoded:016x}ULL\n"
    header += "static const unsigned char gpu_baseline_hash[32]={" + ",".join("0x" + OLD_SYS[i:i+2] for i in range(0,64,2)) + "};\n"
    header += "static const unsigned char gpu_expected_hashes[5][32]={\n"
    header += ",\n".join("{" + ",".join("0x" + h[i:i+2] for i in range(0, 64, 2)) + "}" for h in hashes)
    header += "\n};\n"
    (out / "gpu-provider-candidate.h").write_text(header)
    result = {"state": "policy-prepared-not-trust-verified", "candidate_source_commit": commit,
              "driver_ver": manifest["driver_ver"], "files": dict(zip(("provider-package-manifest.json",) + FILES, hashes)),
              "policy_sha256": sha(out / "gpu-provider-candidate.h"), "source_signed_manifest_sha256": sha(candidate / "signed-manifest.json"), "source_build_config_sha256": sha(candidate / "build-config.json"), "authenticode_verified": False,
              "runtime_verified": False, "deployed": False}
    (out / "policy-manifest.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()

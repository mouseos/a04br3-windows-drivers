#!/usr/bin/env python3
"""固定GPU package検証。Herschel f0d0c64のschema2 load_package/validate_transitionを再利用。
GPU所有ファイルを編集せず、Engine optional GPU入力だけで呼ぶ。
"""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess

FILES = ("mt8167-ge8300.inf", "mt8167-ge8300.sys", "mt8167-ge8300.cat", "ge8300-mips.fw")
INPUTS = (*FILES, "build-config.json", "provider-package-manifest.json", "mt8167-development.cer")
SIGNER = "BCBF6C456C6F0DD0818434763B189488FBF13570"
FIRMWARE = "5c1fdff55e8e57f8be0c6c36b94f2167ea53ece84308068bc79c6b9c37cd7fe1"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def checked_file(directory, name):
    path = directory / name
    require(not path.is_symlink() and path.is_file() and 0 < path.stat().st_size <= 16 * 1024 * 1024,
            "missing, reparse/symlink or invalid package file: " + name)
    return path


def read_json(directory, name):
    return json.loads(checked_file(directory, name).read_text(encoding="utf-8-sig"))


def driver_version(value):
    require(isinstance(value, str) and re.fullmatch(r"\d{2}/\d{2}/\d{4},\d+\.\d+\.\d+\.\d+", value),
            "invalid complete DriverVer")
    date, text = value.split(",")
    parts = tuple(int(x) for x in text.split("."))
    require(all(0 <= x <= 65535 for x in parts), "DriverVersion component exceeds 16 bits")
    return datetime.datetime.strptime(date, "%m/%d/%Y").date(), parts, text


def load_package(root, directory, candidate=False):
    """Read fixed names only; verify final bytes and their source-committed INF."""
    signed_bytes = checked_file(directory, "signed-manifest.json").read_bytes()
    signed = json.loads(signed_bytes.decode("utf-8-sig"))
    source = signed.get("source_commit", "")
    require(isinstance(source, str) and re.fullmatch("[0-9a-f]{40}", source), "full source commit required")
    entries = signed["files"]
    require(isinstance(entries, list) and len(entries) == len(INPUTS) and
            {item["path"] for item in entries} == set(INPUTS), "unexpected/duplicate/incomplete signed file list")
    for item in entries:
        require(sha(checked_file(directory, item["path"])) == item["sha256"].lower(),
                "signed manifest file mismatch: " + item["path"])
    verified_hashes = {item["path"]: item["sha256"].lower() for item in entries}
    manifest = read_json(directory, "provider-package-manifest.json")
    config = read_json(directory, "build-config.json")
    require(manifest.get("source_commit") == source == config.get("source_commit"), "package source mismatch")
    require(manifest.get("fingerprint_state") == "final-signed-package" and manifest.get("signature_status") == "Valid",
            "final signed package manifest required")
    require(signed.get("signing_thumbprint") == manifest.get("signer_thumbprint") == SIGNER and
            hashlib.sha1(checked_file(directory, "mt8167-development.cer").read_bytes()).hexdigest().upper() == SIGNER,
            "unexpected signer/certificate identity")
    if candidate:
        require(signed.get("native_clear_activate") is True, "explicit activation opt-in required")
        for part in (manifest, config):
            require(str(part.get("native_clear_activate")) == "1" and str(part.get("native_clear_job")) == "1" and
                    part.get("native_ta3d_publish_gate") == "armed-one-shot-private-480000", "activation/job/publish gate mismatch")
        require(str(config.get("native_backing")) == "1", "native backing not enabled")
    require(set(manifest.get("files", {})) == set(FILES), "only the four fixed provider files are accepted")
    hashes = {}
    for name in FILES:
        path = checked_file(directory, name)
        digest = sha(path)
        require(digest == manifest["files"][name]["sha256"] and path.stat().st_size == manifest["files"][name]["bytes"],
                "final package hash/size mismatch: " + name)
        hashes[name] = digest
    require(hashes["ge8300-mips.fw"] == FIRMWARE, "unexpected GPU firmware")
    pe = checked_file(directory, FILES[1]).read_bytes()
    require(len(pe) >= 64 and pe[:2] == b"MZ", "provider is not a PE image")
    offset = int.from_bytes(pe[60:64], "little")
    require(pe[offset:offset + 6] == b"PE\0\0\xc4\x01", "provider is not ARMNT")
    inf_bytes = checked_file(directory, FILES[0]).read_bytes()
    inf = inf_bytes.decode("utf-16" if inf_bytes[:2] in (b"\xff\xfe", b"\xfe\xff") else "utf-8-sig")
    source_inf = subprocess.check_output(["git", "-C", str(root), "show", source + ":drivers/gpu/mt8167/mt8167-ge8300.inf"]).decode("utf-8-sig")
    require(inf.replace("\r\n", "\n") == source_inf.replace("\r\n", "\n"), "INF differs from committed source")
    version = re.search(r"(?im)^DriverVer\s*=\s*([^\r\n;]+)", inf)
    require(version and version.group(1).strip() == manifest.get("driver_ver") == config.get("driver_ver"), "INF/manifest DriverVer mismatch")
    date, parts, text = driver_version(manifest["driver_ver"])
    require(config.get("inf_sha256", "").lower() == hashes[FILES[0]], "build-config INF hash mismatch")
    require(re.search(r"(?im)^%Device%\s*=\s*Install\s*,\s*ACPI\\MOU0006\s*$", inf) and
            re.search(r"(?im)^ServiceBinary\s*=\s*%12%\\mt8167-ge8300\.sys\s*$", inf) and
            re.search(r"(?im)^AddService\s*=\s*MT8167GE8300\s*,", inf), "fixed GPU hardware/service mismatch")
    require(not re.search(r"(?i)\b(CoInstallers32|RegisterDlls|UnregisterDlls|RunPreSetupCommands|RunPostSetupCommands)\b", inf),
            "executable coinstaller/command directives are not accepted")
    require(all(sha(checked_file(directory, name)) == digest for name, digest in verified_hashes.items()) and
            sha(directory / "signed-manifest.json") == hashlib.sha256(signed_bytes).hexdigest(),
            "package changed during validation")
    return {"source": source, "manifest": manifest, "config": config, "hashes": hashes,
            "verified_hashes": verified_hashes,
            "date": date, "parts": parts, "version": text, "driver_ver": manifest["driver_ver"],
            "signed_sha256": hashlib.sha256(signed_bytes).hexdigest()}


def validate_transition(baseline, candidate):
    require(candidate["date"] >= baseline["date"] and candidate["parts"] > baseline["parts"],
            "candidate must have a nondecreasing DriverDate and strictly newer DriverVersion")
    require(candidate["hashes"][FILES[1]] != baseline["hashes"][FILES[1]], "candidate SYS equals the baseline")
    require(candidate["manifest"].get("selected_installed_sys_sha256") == baseline["hashes"][FILES[1]] and
            candidate["manifest"].get("selected_installed_driver_ver") == baseline["driver_ver"] and
            candidate["config"].get("installed_sys_sha256", "").lower() == baseline["hashes"][FILES[1]] and
            candidate["config"].get("installed_driver_ver") == baseline["driver_ver"], "candidate was not built/verified for this baseline")


def render_header(baseline, candidate, hashes, suffix):
    encoded = sum(value << shift for value, shift in zip(candidate["parts"], (48, 32, 16, 0)))
    header = "/* Generated for one validated signed roll-forward transition only. */\n#ifndef GPU_PROVIDER_CANDIDATE_H\n#define GPU_PROVIDER_CANDIDATE_H\n"
    header += '#define GPU_SOURCE_COMMIT "' + candidate["source"] + '"\n'
    header += '#define GPU_BASELINE_SOURCE_COMMIT "' + baseline["source"] + '"\n'
    header += "#define GPU_BASELINE_DRIVER_VER " + json.dumps(baseline["driver_ver"]) + "\n"
    header += "#define GPU_BASELINE_DRIVER_VERSION L" + json.dumps(baseline["version"]) + "\n"
    header += "#define GPU_UPDATE_DIRECTORY L" + json.dumps(suffix) + "\n"
    header += f"#define GPU_DRIVER_VERSION 0x{encoded:016x}ULL\n"
    header += "static const unsigned char gpu_baseline_hash[32]={" + ",".join("0x" + x for x in re.findall("..", baseline["hashes"][FILES[1]])) + "};\n"
    header += "static const unsigned char gpu_expected_hashes[5][32]={\n"
    header += ",\n".join("{" + ",".join("0x" + x for x in re.findall("..", h)) + "}" for h in hashes)
    return header + "\n};\n#endif\n"


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--candidate", type=Path, required=True)
    ap.add_argument("--baseline", type=Path, required=True)
    ap.add_argument("--output", type=Path, required=True)
    a = ap.parse_args()
    root = Path(__file__).resolve().parents[2]
    out = a.output.resolve()
    try:
        require(root in out.parents and not out.exists(), "output must be new and inside this checkout")
        baseline = load_package(root, a.baseline.resolve(strict=True))
        candidate_dir = a.candidate.resolve(strict=True)
        candidate = load_package(root, candidate_dir, candidate=True)
        validate_transition(baseline, candidate)
        # Whitespace-only manifest changes cannot create a fresh attempt directory.
        transition = hashlib.sha256((candidate["source"] + baseline["hashes"][FILES[1]] +
                                     candidate["hashes"][FILES[1]] + candidate["hashes"][FILES[0]]).encode("ascii")).hexdigest()
        suffix = "\\MT8167GpuUpdate-" + candidate["source"] + "-" + transition
        require(re.fullmatch(r"\\MT8167GpuUpdate-[0-9a-f]{40}-[0-9a-f]{64}", suffix), "invalid fixed directory suffix")
        names = ("provider-package-manifest.json",) + FILES
        hashes = [candidate["verified_hashes"][name] for name in names]
        out.mkdir(parents=True)
        package = out / "provider"
        package.mkdir()
        for name, digest in zip(names, hashes):
            shutil.copyfile(candidate_dir / name, package / name)
            require(sha(package / name) == digest, "package changed during staging: " + name)
        (out / "gpu-provider-candidate.h").write_text(render_header(baseline, candidate, hashes, suffix))
        result = {"schema_version": 2, "state": "policy-prepared-not-trust-verified", "transition_id": transition,
                  "candidate_source_commit": candidate["source"], "driver_ver": candidate["driver_ver"],
                  "baseline_source_commit": baseline["source"], "baseline_driver_ver": baseline["driver_ver"],
                  "baseline_sys_sha256": baseline["hashes"][FILES[1]], "baseline_signed_manifest_sha256": baseline["signed_sha256"],
                  "data_directory_suffix": suffix, "data_directory": "C:\\Windows\\System32" + suffix,
                  "files": dict(zip(names, hashes)), "policy_sha256": sha(out / "gpu-provider-candidate.h"),
                  "source_signed_manifest_sha256": candidate["signed_sha256"],
                  "source_build_config_sha256": candidate["verified_hashes"]["build-config.json"],
                  "authenticode_verified": False, "runtime_verified": False, "deployed": False}
        (out / "policy-manifest.json").write_text(json.dumps(result, indent=2) + "\n")
        print(json.dumps(result, indent=2))
    except (ValueError, KeyError, TypeError, OSError, subprocess.CalledProcessError) as error:
        ap.error(str(error))


if __name__ == "__main__":
    main()

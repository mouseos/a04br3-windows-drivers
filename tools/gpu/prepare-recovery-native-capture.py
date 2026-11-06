#!/usr/bin/env python3
"""REA capture用JSONと後日のstaging commandを生成する。ADBを実行しない。"""
import argparse
import hashlib
import json
import shlex
from pathlib import Path

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--plan-directory",type=Path,help="新規host directoryへmanifest/scenarioも保存。ADBは実行しない")
    ap.add_argument("--serial",required=True,help="主担当が確認した対象serial")
    ap.add_argument("--bundle",type=Path,required=True)
    ap.add_argument("--log-directory",type=Path,required=True,help="root内ios-feasibilityログ保存先")
    ap.add_argument("--device-directory",required=True,help="新規のdevice側絶対directory")
    args=ap.parse_args();bundle=args.bundle.resolve();device=args.device_directory.rstrip("/")
    if not device.startswith("/tmp/") or ".." in Path(device).parts or any(c in device for c in "\n\r\0"):
        ap.error("device-directory must be a fresh path below /tmp")
    if not args.serial or any(c in args.serial for c in "\n\r\0"):
        ap.error("invalid serial")
    serial=args.serial
    logs=args.log_directory.resolve()
    if not logs.is_dir():ap.error("log-directory must already exist")
    def adb(*items):return ["adb","-s",serial,*items]
    def shell(items):return adb("shell",shlex.join(items))
    def capture(items,timeout):
        command=shell(items)
        return {"executable":command[0],"arguments":command[1:],
                "working_directory":str(bundle),"timeout_ms":timeout,
                "idle_timeout_ms":timeout,"finalization_ms":3000,
                "terminal":{"columns":160,"rows":40,"scrollback":1500},
                "limits":{"output_bytes":262144,"processes":16}}
    staged=[("bin/recovery-native-clear","bin/recovery-native-clear"),
            ("bin/recovery-native-stage","bin/recovery-native-stage"),
            ("bin/recovery-native-bridge-capture.so","bin/recovery-native-bridge-capture.so"),
            ("source/tools/gpu/recovery-native-preflight.sh","recovery-native-preflight.sh")]
    commands=[shell(["mkdir",device]),shell(["mkdir",device+"/bin"])]
    for source,name in staged:
        if not (bundle/source).is_file():ap.error("missing bundle file: "+source)
        commands.append(adb("push",str(bundle/source),device+"/"+name))
    for directory in ["original-vendor","platform-candidates"]:
        if (bundle/directory).is_dir():commands.append(adb("push",str(bundle/directory),device+"/"+directory))
    commands.append(shell(["chmod","700",device+"/bin/recovery-native-clear",device+"/bin/recovery-native-stage"]))
    preflight=capture(["sh",device+"/recovery-native-preflight.sh","--inspect"],15000)
    direct=capture([device+"/bin/recovery-native-stage","--run",device],45000)
    result={"executed":False,"requires_primary_recovery_authorization":True,
        "serial_must_be_confirmed_by_primary":serial,"staging_commands_NOT_EXECUTED":commands,
        "rea_host_environment":{"TMPDIR":str(logs/"rea-runtime-tmp")},
        "rea_cli_capture_NOT_EXECUTED":["env","TMPDIR="+str(logs/"rea-runtime-tmp"),"rea","capture-process",str(bundle/"scenario-vendor-direct.json"),"--format","json","--full-output"],
        "rea_preflight":preflight,"rea_capture_vendor_direct":direct,
        "kernel_compatibility_must_be_confirmed":True,
        "metadata_before_activation_NOT_EXECUTED":shell(["sh","-c",'for p in /proc/pvr/version /sys/kernel/debug/pvr/version /sys/module/pvrsrvkm/version /sys/module/firmware_class/parameters/path; do if [ -r "$p" ]; then echo "SOURCE $p"; cat "$p"; fi; done']),
        "staging_layout":{
            "vendor_candidate":device+"/original-vendor",
            "platform_candidate":device+"/platform-candidates",
            "required_absolute_alias":"/system/vendor/lib64/egl/libGLESv2_mtk.so",
            "required_firmware":"kernel firmware search path/rgx.fw.22.40.54.30",
            "activation":"recovery-native-stageがprivate mount namespaceと/tmp内chrootを作成。Recoveryの/system・Bionic・APEXを保持し/vendorと/system/vendor、/lib/firmwareを固定。/data/Windows/実partitionにはmountも書込みもしない。"},
        "selection":"主担当が実KM互換を確認後、REAでvendor-direct scenarioを1回だけ実行。launcherは実機未試験。namespace不可/未知FW pathはGPU open前に終了。",
        "retrieval_command_NOT_EXECUTED":adb("pull",device+"/runtime/capture-01.bin",str(logs/"runtime-capture-01.bin")),
        "evidence_export_path":str(logs/"runtime-rea-evidence.json"),
        "parse_after_retrieval":["python3","-B",str(bundle/"source/tools/gpu/read-recovery-native-capture.py"),str(logs/"runtime-capture-01.bin"),"--resources-directory",str(logs/"runtime-resources-01")],
        "success":"PowerVR renderer, full readback match, complete pre/post pairs, ioctl/bridge success, broken=0. Native resource completeness separately requires every VDM/PPP/USC/PDS/region reference to be covered; unknown is not success. GPU/FW addresses cannot be replayed unchanged"}
    if args.plan_directory is not None:
        output=args.plan_directory.resolve();output.mkdir(exist_ok=False)
        checks=[(bundle/source,name) for source,name in staged]
        for directory in ["original-vendor","platform-candidates"]:
            checks.extend((p,str(p.relative_to(bundle))) for p in sorted((bundle/directory).rglob("*")) if p.is_file())
        sums=output/"DEVICE-SHA256SUMS"
        sums.write_text("".join(hashlib.sha256(p.read_bytes()).hexdigest()+"  "+name+"\n" for p,name in checks))
        commands.append(adb("push",str(sums),device+"/DEVICE-SHA256SUMS"))
        commands.append(shell(["sh","-c","cd "+shlex.quote(device)+" && sha256sum -c DEVICE-SHA256SUMS"]))
        result["rea_cli_capture_NOT_EXECUTED"][4]=str(output/"scenario-vendor-direct.json")
        for name,key in [("scenario-vendor-direct.json","rea_capture_vendor_direct"),("scenario-preflight.json","rea_preflight")]:
            (output/name).write_text(json.dumps(result[key],indent=2)+"\n")
        (output/"command-manifest.json").write_text(json.dumps(result,indent=2)+"\n")
    print(json.dumps(result,indent=2))
if __name__=="__main__":main()

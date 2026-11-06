#!/usr/bin/env python3
"""REA capture用JSONと後日のstaging commandを生成する。ADBを実行しない。"""
import argparse
import json
import shlex
from pathlib import Path

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--serial",required=True,help="主担当が確認した対象serial")
    ap.add_argument("--bundle",type=Path,required=True)
    ap.add_argument("--log-directory",type=Path,required=True,help="root内ios-feasibilityログ保存先")
    ap.add_argument("--device-directory",required=True,help="新規のdevice側絶対directory")
    args=ap.parse_args();bundle=args.bundle.resolve();device=args.device_directory.rstrip("/")
    if not device.startswith("/") or device=="/" or any(c in device for c in "\n\r\0"):
        ap.error("device-directory must be an absolute non-root path")
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
    staged=[("bin/recovery-native-clear","recovery-native-clear"),
            ("bin/recovery-native-bridge-capture.so","recovery-native-bridge-capture.so"),
            ("source/tools/gpu/recovery-native-preflight.sh","recovery-native-preflight.sh")]
    commands=[shell(["mkdir",device])]
    for source,name in staged:
        if not (bundle/source).is_file():ap.error("missing bundle file: "+source)
        commands.append(adb("push",str(bundle/source),device+"/"+name))
    for directory in ["original-vendor","platform-candidates"]:
        if (bundle/directory).is_dir():commands.append(adb("push",str(bundle/directory),device+"/"+directory))
    commands.append(shell(["chmod","700",device+"/recovery-native-clear"]))
    preflight=capture(["sh",device+"/recovery-native-preflight.sh","--inspect"],15000)
    common=["env","A04_M111_CAPTURE="+device+"/capture-01.bin",
            "LD_PRELOAD="+device+"/recovery-native-bridge-capture.so"]
    direct=capture(common+["LD_LIBRARY_PATH=/vendor/lib64:/vendor/lib64/egl:/system/lib64",
            device+"/recovery-native-clear","--run","/vendor/lib64/egl/libEGL_mtk.so",
            "/vendor/lib64/egl/libGLESv2_mtk.so"],45000)
    platform=capture(common+[device+"/recovery-native-clear","--run",
            "/system/lib64/libEGL.so","/system/lib64/libGLESv2.so"],45000)
    print(json.dumps({"executed":False,"requires_primary_recovery_authorization":True,
        "serial_must_be_confirmed_by_primary":serial,"staging_commands_NOT_EXECUTED":commands,
        "rea_preflight":preflight,"rea_capture_vendor_direct":direct,
        "rea_capture_platform_loader_alternative":platform,
        "staging_layout":{
            "vendor_candidate":device+"/original-vendor",
            "platform_candidate":device+"/platform-candidates",
            "required_absolute_alias":"/system/vendor/lib64/egl/libGLESv2_mtk.so",
            "required_firmware":"kernel firmware search path/rgx.fw.22.40.54.30",
            "activation":"未実施。/tmp pushのみでは固定pathは解決しない。依存closureと主担当のnamespace配置承認が必要"},
        "selection":"主担当が依存libraryとnamespaceを確認して片方だけを選ぶ。同じcapture名で両方を実行しない。",
        "retrieval_command_NOT_EXECUTED":adb("pull",device+"/capture-01.bin",str(logs/"runtime-capture-01.bin")),
        "evidence_export_path":str(logs/"runtime-rea-evidence.json"),
        "parse_after_retrieval":["python3","-B",str(bundle/"source/tools/gpu/read-recovery-native-capture.py"),str(logs/"runtime-capture-01.bin")],
        "success":"PowerVR renderer, full readback match, complete pre/post pairs, ioctl/bridge success, broken=0; packet GPU/FW addresses cannot be replayed unchanged"},indent=2))
if __name__=="__main__":main()

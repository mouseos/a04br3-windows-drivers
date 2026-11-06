#!/usr/bin/env python3
"""固定FWのRecovery ramdisk tree/newc overlayをhostで作る。端末操作なし。"""
import argparse
import hashlib
import json
import stat
from pathlib import Path

FW_NAME="rgx.fw.22.40.54.30"
FW_SHA256="5c1fdff55e8e57f8be0c6c36b94f2167ea53ece84308068bc79c6b9c37cd7fe1"

def newc(entries):
    output=bytearray()
    for ino,(name,mode,data) in enumerate([*entries,("TRAILER!!!",0,b"")],1):
        encoded=name.encode()+b"\0"
        fields=(ino,mode,0,0,2 if stat.S_ISDIR(mode) else 1,0,len(data),0,0,0,0,len(encoded),0)
        output.extend(b"070701"+b"".join(f"{v:08x}".encode() for v in fields))
        output.extend(encoded);output.extend(bytes((-len(output))%4))
        output.extend(data);output.extend(bytes((-len(output))%4))
    output.extend(bytes((-len(output))%512))
    return bytes(output)

def prepare(root,firmware,output):
    root=root.resolve(strict=True);firmware=firmware.resolve(strict=True);output=output.resolve()
    firmware.relative_to(root);output.relative_to(root)
    payload=firmware.read_bytes()
    if hashlib.sha256(payload).hexdigest()!=FW_SHA256:
        raise ValueError("指定m1.11/BVNC22.40.54.30の固定FW hashと不一致")
    entries=[("lib",stat.S_IFDIR|0o755,b""),("lib/firmware",stat.S_IFDIR|0o755,b""),
             ("lib/firmware/"+FW_NAME,stat.S_IFREG|0o644,payload)]
    archive=newc(entries)
    output.mkdir(parents=True,exist_ok=False)
    target=output/"root/lib/firmware"/FW_NAME;target.parent.mkdir(parents=True);target.write_bytes(payload)
    (output/"firmware-overlay.cpio").write_bytes(archive)
    manifest={"type":"Recovery ramdisk input/standalone newc overlay; not a boot image",
        "firmware_source":str(firmware.relative_to(root)),"firmware_sha256":FW_SHA256,
        "firmware_bytes":len(payload),"target":"/lib/firmware/"+FW_NAME,
        "cpio_sha256":hashlib.sha256(archive).hexdigest(),"cpio_bytes":len(archive),
        "files":[{"path":name,"mode":oct(mode),"uid":0,"gid":0,"mtime":0,"bytes":len(data)}
                 for name,mode,data in entries],
        "required_timing":"Recovery/minuiを含む最初のDRM openより前からramdiskで利用可能にする",
        "ddk":"1.11@5425693 release","observed_km_build_options":"0x00002010","bvnc":"22.40.54.30",
        "host_only":True,"device_accessed":False,"boot_image_repacked":False,"runtime_verified":False,
        "integration":"固定したRecovery buildのramdiskへroot/の内容を組み込み、kernel/dtb/boot headerと出自manifestを保持して別成果物として生成する。overlay単体をflashしない。",
        "reason":"同boot内の後置private FWではPVRSRV_DEVICE_STATE_BADから再初期化されない"}
    (output/"manifest.json").write_text(json.dumps(manifest,indent=2,ensure_ascii=False)+"\n")
    return manifest

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--work-root",type=Path,required=True)
    ap.add_argument("--firmware",type=Path,required=True)
    ap.add_argument("--output",type=Path,required=True)
    args=ap.parse_args()
    try:result=prepare(args.work_root,args.firmware,args.output)
    except (OSError,ValueError) as exc:ap.error(str(exc))
    print(json.dumps(result,indent=2,ensure_ascii=False))

if __name__=="__main__":main()

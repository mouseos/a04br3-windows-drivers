#!/usr/bin/env python3
"""完全なAndroid NDKでprobe/observerだけをローカルビルドする。実機操作なし。"""
import argparse
import hashlib
import json
import os
import subprocess
from pathlib import Path

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--ndk",type=Path,required=True)
    ap.add_argument("--output",type=Path,required=True)
    args=ap.parse_args();source=Path(__file__).resolve().parent
    compiler=args.ndk.resolve()/"toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android26-clang"
    if not compiler.is_file():ap.error("complete NDK LLVM prebuilt/sysroot required")
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    temporary=out/"tmp";temporary.mkdir(exist_ok=True)
    env=os.environ.copy();env["TMPDIR"]=str(temporary)
    products={"recovery-native-clear":["-fPIE","-pie","-ldl"],
              "recovery-native-bridge-capture.so":["-fPIC","-shared","-pthread"]}
    manifest={"hardware_executed":False,"compiler":str(compiler),"commands":[],"products":{}}
    manifest["sources"]={p.name:hashlib.sha256(p.read_bytes()).hexdigest()
        for p in sorted(source.glob("recovery-native-*.h"))}
    for name,flags in products.items():
        c=source/(name.removesuffix(".so")+".c")
        manifest["sources"][c.name]=hashlib.sha256(c.read_bytes()).hexdigest()
        cmd=[str(compiler),"-std=gnu11","-O2","-Wall","-Wextra","-Werror",
             str(c),*flags,"-Wl,-z,defs","-o",str(out/name)]
        subprocess.run(cmd,env=env,check=True)
        manifest["commands"].append(cmd)
        manifest["products"][name]={"sha256":hashlib.sha256((out/name).read_bytes()).hexdigest(),
                                     "source_sha256":hashlib.sha256(c.read_bytes()).hexdigest()}
    (out/"manifest.json").write_text(json.dumps(manifest,indent=2)+"\n")
if __name__=="__main__":main()

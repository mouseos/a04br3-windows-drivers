#!/usr/bin/env bash
# Host-only ARM32 build using imports checked against an explicit WinPE DLL.
set -euo pipefail
if [[ $# != 2 ]]; then echo 'usage: build-audit-hive-arm.sh KERNEL32_DLL OUTPUT_DIRECTORY' >&2; exit 2; fi
root=$(cd "$(dirname "$0")/../.." && pwd -P); workspace=$(cd "$root/../.." && pwd -P)
dll=$(realpath "$1"); out=$(realpath -m "$2")
[[ $dll == "$workspace"/* && $out == "$workspace"/* && -f $dll && ! -e $out ]] || exit 2
mkdir -p "$out"
exports=(GetCommandLineW CreateFileW GetFileSizeEx ReadFile CloseHandle VirtualAlloc VirtualFree GetLastError GetStdHandle WriteFile ExitProcess)
llvm-readobj-18 --coff-exports "$dll" > "$out/kernel32-exports.txt"
{ echo 'LIBRARY KERNEL32.dll'; echo EXPORTS; for name in "${exports[@]}"; do rg -q "^  Name: ${name}$" "$out/kernel32-exports.txt" || { echo "missing export: $name" >&2; exit 2; }; echo " $name"; done; } > "$out/kernel32.def"
llvm-dlltool-18 -m arm -d "$out/kernel32.def" -l "$out/kernel32.lib"
clang --target=thumbv7-pc-windows-msvc -std=c11 -ffreestanding -fno-stack-protector -I"$root/tools/winpe" -c "$root/tools/winpe/audit-hive-uncached.c" -o "$out/hive.obj"
lld-link /subsystem:console /machine:arm /entry:mainCRTStartup /nodefaultlib /timestamp:0 "$out/hive.obj" "$out/kernel32.lib" "/out:$out/audit-hive-uncached.exe"
llvm-readobj-18 --file-headers --coff-imports "$out/audit-hive-uncached.exe" > "$out/pe-inspection.txt"
rg -q 'IMAGE_FILE_MACHINE_ARMNT' "$out/pe-inspection.txt"
sha256sum "$dll" "$out/audit-hive-uncached.exe" > "$out/hashes.txt"

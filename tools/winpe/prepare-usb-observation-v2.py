#!/usr/bin/env python3
"""Prepare an offline integration package against the recorded touch/GPU v2 baseline."""
import argparse, hashlib, json, os, shutil, subprocess, tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
OUT = ROOT / 'port/xnu/out'
BASE = OUT / 'windows-rt-integrated-touch-gpu-6d0bfbdc-0bc3f2d-v2'
TOUCH = OUT / 'windows-rt-bu21025-b6b411c'
WIM_SHA = '66b1db86efc8f4451ca487bd4837d249423edd4e87f5e93255599311d6674752'
BOOT_SHA = 'eb67264abf0e72a9ca7eaa59903f787acbfea7c97078ea0be75e9d58e42c8734'
START_SHA = '1dac88ab5a219b1af03179a0fc4da40dec6acd0bae94d5a15dd7a9f153bbd852'
TOUCH_HASHES = {'bu21025.sys':'049f08ba2b7ea82943f97c22da118dfa466d98732fe75d309c1ac6f429bd6db3',
 'bu21025.cat':'072915730b1c83acd4a82b2cbc50ae447908b12dd02c53dd0936d5fb7684c640',
 'shared-touch.inf':'ca780add5a40d4fa0e04914e83b510b7c5cadbc0bac378be43a87d3f71bebbcf',
 'shared-touch-test.cer':'7381f7b2fb1c724af88d5ed5ddaf78a26c07ae6eb7c50a770c475830f4507422'}
EXPECTED_FLAGS={'query':1,'children':1,'button_d0':1,'touch_d0':1,'apmixed':1,'audio_child':1,
 'audio_top':1,'audio_afe':1,'audio_irq':1,'mt6392':1,'codec_domain':1,'clock_alias':0,
 'unmute':0,'observation':1,'ioctl':1,'audio_trace':1,'gpio22_source':0,'other_optional':0}

def sha(path):
 with path.open('rb') as f: return hashlib.file_digest(f,'sha256').hexdigest()
def inside(path):
 p=path.resolve()
 if not p.is_relative_to(ROOT): raise ValueError('path resolves outside project root')
 return p
def expect(path, digest):
 path=inside(path)
 if sha(path)!=digest.lower(): raise ValueError('hash mismatch: '+str(path))
 return path

def main():
 ap=argparse.ArgumentParser(description=__doc__)
 ap.add_argument('--candidate',type=Path,required=True)
 ap.add_argument('--output',type=Path,required=True)
 args=ap.parse_args();candidate=inside(args.candidate);dest=inside(args.output)
 if dest.exists(): raise ValueError('output already exists')
 m=json.loads((candidate/'manifest.json').read_text(encoding='utf-8-sig'))
 if m['source_commit'] != '9b055fa' and not m['source_commit'].startswith('9b055fa'): raise ValueError('unexpected signed source')
 if m['parent_flags'] != EXPECTED_FLAGS: raise ValueError('parent build flags differ from required profile')
 for name,digest in m['files'].items():
  p=Path(name)
  if p.is_absolute() or '..' in p.parts: raise ValueError('invalid candidate path')
  expect(candidate/p,digest)
 for name,digest in TOUCH_HASHES.items(): expect(TOUCH/name,digest)
 base=json.loads((BASE/'manifest.json').read_text())
 if base['uefi_commit']!='6d0bfbdce824bd8435424a791673d28160cb5265' or base['source_touch_commit']!='b6b411c4f66f0838f6a012ed60d702d63105cc95' or base['parent_commit']!='5ee5528a905896ace1f9b95dee1e1c195b4f0423': raise ValueError('unexpected baseline provenance')
 expect(BASE/'volume.wim',WIM_SHA);expect(BASE/'boot.img',BOOT_SHA)
 firmware=json.loads((BASE/'boot-manifest.json').read_text())
 defines=firmware['firmware_build_defines']
 for key in ('A04BR3_SHARED_PARENT','A04BR3_SHARED_TOUCH_POWER','A04BR3_SHARED_APMIXED','A04BR3_FOUR_CPU_TEST'):
  if defines.get(key)!='1': raise ValueError('firmware flag mismatch: '+key)
 asl=subprocess.run(['git','-C',str(inside(ROOT/'unixnu/uefi-a04br3')),'show','6d0bfbdc:Platforms/Benesse/a04br3Pkg/WindowsArm32/Acpi/SharedParentApmixed.asl'],check=True,capture_output=True,text=True).stdout
 if '0x10018000, 0x0714' not in asl: raise ValueError('APMIXED resource mismatch')
 tools=inside(OUT/'windows-rt-tools/wimtools');dest.mkdir(parents=True)
 env=os.environ.copy();env['LD_LIBRARY_PATH']=str(tools);env['TMPDIR']=str(dest)
 with tempfile.TemporaryDirectory(prefix='extract-',dir=dest) as tmp:
  subprocess.run([str(tools/'wimlib-imagex'),'extract',str(BASE/'volume.wim'),'1','/Windows/System32/startnet.cmd','/Windows/System32/Drivers/mt8167-toprgu.sys','/Windows/System32/config/SYSTEM','--dest-dir='+tmp,'--no-acls'],env=env,check=True,stdout=subprocess.DEVNULL)
  raw=expect(Path(tmp)/'startnet.cmd',START_SHA).read_bytes()
  driver=expect(Path(tmp)/'mt8167-toprgu.sys','e2e2ca36ddb51ddbb80d34fbb5c522dd357d25e1f778839d0e347463f049751d').read_bytes()
  if 'A04BR3ScreenUart'.encode('utf-16le') not in driver or b'A04RGU CPU1 ownership acquired' not in driver: raise ValueError('TOPRGU UART/CPU ownership markers missing')
  hive=expect(Path(tmp)/'SYSTEM','cc151bf339dd2683b313e95d42179cfe571f6392c7a294b9a98f19a8a83e5214')
  start=subprocess.run(['hivexget',str(hive),r'\ControlSet001\Services\Mt8167Toprgu','Start'],check=True,capture_output=True,text=True).stdout.strip()
  if start!='0': raise ValueError('TOPRGU is not boot-start')
  marker=b'echo MT8167_BU21025_DRVLOAD_RETURNED\r\n'
  if raw.count(marker)!=1: raise ValueError('baseline startnet insertion point mismatch')
  extra=(b'echo A04USB_OBSERVATION_START\r\ncall X:\\a04br3\\usb-power-observe.cmd\r\n'
         b'echo A04USB_OBSERVATION_EXIT=%errorlevel%\r\n')
  (dest/'startnet.cmd').write_bytes(raw.replace(marker,marker+extra))
 for d in ('parent','touch','tools','certificates'): (dest/d).mkdir()
 for n in ('mt8167-shared.sys','shared-parent.cat','shared-parent.inf'): shutil.copyfile(candidate/'parent'/n,dest/'parent'/n)
 for n in ('bu21025.sys','bu21025.cat','shared-touch.inf'): shutil.copyfile(TOUCH/n,dest/'touch'/n)
 for n in ('usb-power-uart.exe','usb-power-observe.cmd'): shutil.copyfile(candidate/'tools'/n,dest/'tools'/n)
 shutil.copyfile(candidate/'integration-test.cer',dest/'certificates'/'parent-test.cer')
 shutil.copyfile(TOUCH/'shared-touch-test.cer',dest/'certificates'/'touch-tested-test.cer')
 shutil.copyfile(candidate/'manifest.json',dest/'candidate-manifest.json')
 (dest/'README-統合.txt').write_text(r"""USB GPIO観測：タッチ成功WinPE v2向けオフライン統合候補

対象はUEFI 6d0bfbdc、boot SHA eb67264abf0e72a9ca7eaa59903f787acbfea7c97078ea0be75e9d58e42c8734、WinPE v2 WIM SHA 66b1db86efc8f4451ca487bd4837d249423edd4e87f5e93255599311d6674752。基準が一致しなければ適用しない。
UEFIのSHR0はAPMIXED 0x10018000長0x714、候補親は0x710/0x714を受理。実機成功のBU21025 b6b411c SYS/CAT/INFをバイト単位で維持。新しい親SYSは成功時5ee5528と別ソース・別バイナリで、ビルドフラグが一致しても動作同等の証拠ではない。

媒体作成担当向け配置：
parent/* → X:\a04br3\mt8167-shared\
touch/* → X:\a04br3\bu21025-shared\
tools/* → X:\a04br3\
startnet.cmd → X:\Windows\System32\startnet.cmd
certificates/parent-test.cer と touch-tested-test.cer は別証明書。既存証明書を維持し、作成する媒体の信頼設定とRT署名方針を個別に照合する。ここでは証明書ストアを変更しない。信頼・署名受理は未確認。

startnetは成功v2を元に、listener → 共有親drvload → タッチdrvload成功 → USB観測を追加 → 既存GPU段階、という順。USB観測の成否を表示し、失敗しても既存GPU診断へ進む。親・タッチのdrvload成功はPnP実動作を保証しない。共有親の読み取り専用IOCTLは既定OFFのオプションで、GPIO出力・VBUSを操作しない。

既存SYSTEM hiveはTOPRGU Start=0、既存SYS SHA e2e2ca36ddb51ddbb80d34fbb5c522dd357d25e1f778839d0e347463f049751d。これを置換せず既存TOPRGU/UART単一所有者を維持する。UEFI manifestは4 CPU試験・worker heartbeat・有限420秒の値を記録し、成功時UARTにはCPU1 ownership、timer kick、listener ready、タッチreportがある。実際の起動で4 CPU・TOPRGUのCPU1引継ぎが成立しないと送信IOCTLは拒否される。watchdogの継続は起動ごとに確認し、420秒をOS側の給餌保証と扱わない。

取得された3本のA04USB SNAP行は同一IDを照合。1=DIR1/DOUT1/DIN1/MODE21_22、2=DIR2/DOUT2/DIN2/MODE34、3=MODE41。送信成功はキュー登録であり受信完了の保証ではない。UART混線や欠落は不明。GPIO22出力ラッチは5V電圧・給電能力・電源配線の証拠ではない。実機電圧未確認。

GPU 0bc3f2dとTOPRGUは媒体側の成功v2を維持し、このパッケージは含まない。GPU DMA停止・正常dismountは別問題。実機書込み・起動・署名受理・電圧確認は行っていない。
""",encoding='utf-8')
 files={str(p.relative_to(dest)):sha(p) for p in sorted(dest.rglob('*')) if p.is_file()}
 manifest={'status':'offline candidate, not an image or target validation','source_commit':m['source_commit'],
 'baseline_uefi':'6d0bfbdce824bd8435424a791673d28160cb5265','baseline_boot_sha256':BOOT_SHA,
 'baseline_wim_sha256':WIM_SHA,'baseline_startnet_sha256':START_SHA,'baseline_parent_commit':'5ee5528a905896ace1f9b95dee1e1c195b4f0423',
 'baseline_toprgu_sha256':'e2e2ca36ddb51ddbb80d34fbb5c522dd357d25e1f778839d0e347463f049751d',
 'touch_commit':'b6b411c4f66f0838f6a012ed60d702d63105cc95','touch_exact_tested_binary':True,
 'baseline_gpu_commit':'0bc3f2d','parent_flags':m['parent_flags'],'files_sha256':files,
 'limits':['The new parent differs in source and binary from the tested parent despite matching baseline feature flags.',
 'Test certificates and driver acceptance under RT are unverified; no certificate store was modified.',
 'Four CPU and existing TOPRGU screen UART ownership are required; delivery is not guaranteed.',
 'Actual VBUS voltage, source wiring and power delivery are unverified; GPIO22 is not 5V evidence.',
 'No target write or hardware test was performed.']}
 (dest/'manifest.json').write_text(json.dumps(manifest,indent=2,ensure_ascii=False)+'\n')
 print(dest)
if __name__=='__main__': main()

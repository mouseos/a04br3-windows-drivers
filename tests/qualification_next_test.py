"""新controllerのhost-only契約。device subprocessを全て置換する。"""
import copy
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import types
import unittest
import zlib
from unittest.mock import patch

REPO = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('qualification_next', REPO/'tools/recovery/qualify-unattended-winpe-next.py')
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
TOKEN = '0123456789ABCDEF'
REPORT = b'LEASE_QUERY_EXIT=0\nLEASE_EXPECTED_SECONDS=1800\nNORMAL_WINDOWS_UPDATE_REQUESTED=0\n'
RAW = (b'A04RGU_WINPE_LEASE_V1:1800\nA04AU2E '+TOKEN.encode()+b' 12345678\n'
       +m.EXPIRED+b'\nBOOT_REASON: 4\n')
TRACE = rb'A04WIN example\n'


def crc_stream(report):
    crc = zlib.crc32(report)
    lines = [b'A04RGU_WINPE_LEASE_V1:1800',
             ('A04AU20 %s %08X %08X' % (TOKEN, len(report), crc)).encode()]
    for seq, start in enumerate(range(0, len(report), 16)):
        chunk = report[start:start+16]
        lines.append(('A04AU21 %s %08X %02X %s %08X' %
                      (TOKEN, seq, len(chunk), chunk.hex().upper(), zlib.crc32(chunk))).encode())
    lines += [('A04AU2E %s %08X' % (TOKEN, crc)).encode(), m.EXPIRED, b'BOOT_REASON: 4']
    return b'\n'.join(lines) + b'\n'


class Contracts(unittest.TestCase):
    def test_exact_report_keys_with_real_crc_decoder(self):
        path = REPO.parents[1]/'windows-rt/a04br3-windows-public-transport/tools/recovery/decode-audit-stream.py'
        spec = importlib.util.spec_from_file_location('actual_crc_decoder', path)
        decoder = importlib.util.module_from_spec(spec); spec.loader.exec_module(decoder)
        self.assertTrue(m.evaluate_uart(crc_stream(REPORT),TOKEN,decoder,TRACE)[0]['runtime_query_valid'])
        for key, value, other in ((b'LEASE_QUERY_EXIT',b'0',b'9020'),
                                  (b'LEASE_EXPECTED_SECONDS',b'1800',b'1'),
                                  (b'NORMAL_WINDOWS_UPDATE_REQUESTED',b'0',b'1')):
            for bad in (REPORT+key+b'='+other+b'\n', REPORT+key+b'='+value+b'\n',
                        REPORT.replace(key+b'='+value,key+b'=unknown'),
                        REPORT.replace(key+b'='+value,key+b' ='+value),
                        REPORT.replace(key+b'='+value,key)):
                with self.subTest(key=key,report=bad):
                    raw=crc_stream(bad)
                    self.assertEqual(decoder.decode(raw,TOKEN),bad)
                    proof,_=m.evaluate_uart(raw,TOKEN,decoder,TRACE)
                    self.assertFalse(proof['runtime_query_valid'])
                    self.assertFalse(proof['qualification_transport_evidence'])

    def test_event_order_and_query(self):
        decoder = types.SimpleNamespace(decode=lambda raw, token: REPORT)
        proof, _ = m.evaluate_uart(RAW, TOKEN, decoder, TRACE)
        self.assertTrue(proof['qualification_transport_evidence'])
        self.assertFalse(proof['kernel_duration_measured'])
        self.assertIsNone(proof['kernel_elapsed_low'])
        bad = [RAW.replace(m.EXPIRED, b'other'), RAW.replace(b'BOOT_REASON: 4', b'BOOT_REASON: 2'),
               RAW.replace(m.EXPIRED+b'\nBOOT_REASON: 4', b'BOOT_REASON: 4\n'+m.EXPIRED),
               RAW.replace(b'BOOT_REASON: 4', b'BOOT_REASON: 2\nBOOT_REASON: 4'),
               RAW.replace(b'A04RGU_WINPE_LEASE_V1:1800', b'old-marker'), RAW+RAW]
        for raw in bad:
            self.assertFalse(m.evaluate_uart(raw, TOKEN, decoder, TRACE)[0]['qualification_transport_evidence'])
        decoder.decode = lambda raw, token: REPORT.replace(b'EXIT=0', b'EXIT=9020')
        self.assertFalse(m.evaluate_uart(RAW, TOKEN, decoder, TRACE)[0]['qualification_transport_evidence'])
        decoder.decode = lambda raw, token: (_ for _ in ()).throw(ValueError('CRC不正'))
        self.assertFalse(m.evaluate_uart(RAW, TOKEN, decoder, TRACE)[0]['qualification_transport_evidence'])

    def test_validate_only_never_spawns(self):
        argv = []
        for flag in ('workspace','qualification-manifest','lease-manifest','atomic-manifest',
                     'prior-manifest','prior-supplement','prior-usb','out'):
            argv += ['--'+flag, 'fixture']
        for flag in ('qualification-sha256','lease-sha256','atomic-sha256','prior-sha256',
                     'prior-supplement-sha256','prior-usb-sha256'):
            argv += ['--'+flag, 'a'*64]
        with patch.object(m, 'preflight', return_value={}), patch.object(m.subprocess, 'run', side_effect=AssertionError('device禁止')):
            self.assertEqual(m.main(argv+['--validate-only']), 0)

    def test_recovery_timeout_once_and_query_failure_still_collects(self):
        for query_ok in (True, False):
            with self.subTest(query_ok=query_ok), tempfile.TemporaryDirectory(
                    dir=REPO.parents[1]/'port/gsi/logs/ios-feasibility', prefix='qualification-next-test-') as td:
                self.exercise(Path(td), query_ok)

    def test_early_reset_and_lower_boundary(self):
        for elapsed, passed in ((1,False),(1789.9,False),(1790,True)):
            with self.subTest(elapsed=elapsed),tempfile.TemporaryDirectory(
                    dir=REPO.parents[1]/'port/gsi/logs/ios-feasibility',prefix='qualification-early-reset-test-') as td:
                self.exercise(Path(td),True,elapsed=elapsed,expected_pass=passed)

    def exercise(self, root, query_ok, elapsed=2200, expected_pass=None):
        if expected_pass is None:expected_pass=query_ok
        logs = root/'port/gsi/logs/ios-feasibility'; logs.mkdir(parents=True)
        serial = 'fixture'
        boot1, boot2 = '11111111-1111-1111-1111-111111111111', '22222222-2222-2222-2222-222222222222'
        primary = root/'port/xnu/out/private-windows-rt-prebackup-run239/manifest.json'
        supplement = primary.parent.with_name(primary.parent.name+'-gpu-assets')/'manifest.json'
        usb = primary.parent.with_name(primary.parent.name+'-usb1121-exe')/'manifest.json'
        sources = ['Windows/System32/config/'+x for x in ('SYSTEM','SYSTEM.LOG1','SYSTEM.LOG2')]
        sources += ['Windows/System32/Drivers/fixture%d.sys'%i for i in range(39)]

        def bundle_file(path, names, boot, origin=None):
            path.parent.mkdir(parents=True)
            rows=[]
            for name in names:
                p=path.parent/'files'/name; p.parent.mkdir(parents=True, exist_ok=True); p.write_bytes(name.encode())
                rows.append(dict(source_path=name,backup_path='files/'+name,bytes=p.stat().st_size,sha256=m.sha(p)))
            obj=dict(schema_version=1,status='captured-unmounted',read_only=True,unmounted=True,errors=[],files=rows,
                     serial=serial,recovery_boot_id=boot)
            if origin: obj['source_manifest_sha256']=m.sha(origin)
            path.write_text(json.dumps(obj)); return obj

        before=[bundle_file(primary,sources,boot1),bundle_file(supplement,['Windows/System32/umd.dll'],boot1,primary),
                bundle_file(usb,['Windows/System32/usb.exe'],boot1,primary)]
        pins={}
        self.assertEqual(m.validate_bundle(root,primary,supplement,usb,pins), before)
        # 実byte driftと誤primary結合を拒否する。
        damaged=read_json(supplement); damaged['source_manifest_sha256']='0'*64; supplement.write_text(json.dumps(damaged))
        with self.assertRaises(ValueError):m.validate_bundle(root,primary,supplement,usb,{})
        supplement.write_text(json.dumps(before[1]))
        data=primary.parent/'files'/sources[0]; saved=data.read_bytes(); data.write_bytes(b'drift')
        with self.assertRaises(ValueError):m.validate_bundle(root,primary,supplement,usb,{})
        data.write_bytes(saved)
        fresh=primary.parent.with_name('private-windows-rt-prebackup-run241')
        outputs=(fresh,fresh.with_name(fresh.name+'-gpu-assets'),fresh.with_name(fresh.name+'-usb1121-exe'))
        tools={n:root/(n+'.py') for n in ('capture','extra','usb','engine','decoder')}
        for p in tools.values():p.write_text('fixture')
        ntfs=root/'ntfs';ntfs.write_bytes(b'fixture');esp=root/'qualification-esp.img';esp.write_bytes(b'fixture')
        uart=logs/'windows-rt-uart-workspace.log';uart.write_bytes(b'')
        out=logs/'runtime'; clock=[100.]; transition=[False]; rooted=[False]; calls=[]
        c=dict(root=root,out=out,serial=serial,pins=pins,before=before,primary=primary,outputs=outputs,
               q=dict(esp_sha256=m.sha(esp),UART_token=TOKEN),esp=esp,ntfs=ntfs,**tools)
        a=types.SimpleNamespace(qualification_sha256='a'*64,prior_sha256=m.sha(primary))
        oldbytes={p:p.read_bytes() for p in (primary,supplement,usb)}
        report=REPORT if query_ok else REPORT.replace(b'EXIT=0',b'EXIT=9020')
        class Recovery:
            def __init__(self,s):assert s==serial
            def identity(self):return dict(serial=serial,recovery_boot_id=boot2 if transition[0] else boot1)
            def require_unmounted(self,i):pass
        def fake_load(name,path):
            if name=='next_capture_backend':return types.SimpleNamespace(Recovery=Recovery)
            if name=='next_qualification_crc':return types.SimpleNamespace(decode=lambda raw,token:report)
            if name=='next_qualification_engine':return types.SimpleNamespace(STORAGE_TRACE=TRACE,TARGET_SYS=('fixture',))
            raise AssertionError(name)
        def run(args,**kw):
            calls.append(args)
            if args[0]=='adb':
                op=args[3:]
                if op==['reboot']:
                    uart.write_bytes(RAW);clock[0]+=elapsed
                    raise subprocess.TimeoutExpired(args,30,output=b'reboot ack unknown')
                if op==['get-state']:answer=b'recovery'
                elif op==['get-serialno']:answer=serial.encode()
                elif op==['shell','id -u']:answer=b'0' if rooted[0] else b'2000'
                elif op==['root']:
                    rooted[0]=True;raise subprocess.TimeoutExpired(args,10)
                elif op[0]=='shell' and 'sha256sum' in op[1]:answer=m.sha(esp).encode()+b'  fixture'
                else:
                    self.assertTrue(op[0] in ('push','shell'));answer=b''
                return subprocess.CompletedProcess(args,0,answer)
            if args[0]=='fastboot':
                if args[-2:]==['getvar','product']:return subprocess.CompletedProcess(args,0,b'product: A04BR3\n')
                self.assertEqual(args[-2:],['oem','reboot-recovery']);transition[0]=True
                raise subprocess.TimeoutExpired(args,20)
            if str(tools['capture']) in args:
                if 'capture' in args:bundle_file(fresh/'manifest.json',sources,boot2)
                else:self.assertIn('verify',args)
            elif str(tools['extra']) in args:bundle_file(outputs[1]/'manifest.json',['Windows/System32/umd.dll'],boot2,fresh/'manifest.json')
            elif str(tools['usb']) in args:bundle_file(outputs[2]/'manifest.json',['Windows/System32/usb.exe'],boot2,fresh/'manifest.json')
            else:raise AssertionError(args)
            return subprocess.CompletedProcess(args,0,b'')
        with patch.object(m,'load',side_effect=fake_load),patch.object(m.subprocess,'run',side_effect=run),\
             patch.object(m.time,'monotonic',side_effect=lambda:clock[0]),\
             patch.object(m.time,'sleep',side_effect=lambda n:clock.__setitem__(0,clock[0]+n)):
            self.assertEqual(m.execute(a,c),0 if expected_pass else 2)
        st=read_json(out/'result.json')
        self.assertTrue(st['evidence_recovered']);self.assertEqual(st['qualification_pass'],expected_pass)
        self.assertEqual(st['hardware_verified'],expected_pass)
        self.assertEqual(st['host_reset_window_valid'],1790<=elapsed<=2400)
        self.assertFalse(st['kernel_duration_measured'])
        self.assertEqual(st['host_transport_elapsed_seconds'],elapsed)
        self.assertEqual(sum(x[0]=='adb' and x[3:]==['reboot'] for x in calls),1)
        self.assertEqual(sum(x[0]=='adb' and x[3:]==['root'] for x in calls),1)
        self.assertEqual(sum(x[0]=='fastboot' and x[-2:]==['oem','reboot-recovery'] for x in calls),1)
        self.assertEqual(oldbytes,{p:p.read_bytes() for p in oldbytes})
        self.assertTrue((root/st['once_latch']).is_file())
        # 別outで同候補を再stageしない。既241出力によりdevice呼出し前に拒否。
        c['out']=logs/'runtime2'
        with patch.object(m.subprocess,'run',side_effect=AssertionError('再stage禁止')):
            with self.assertRaises(ValueError):m.execute(a,c)
        # 241未作成の失敗と同じ条件でも、別outで既once latchを突破しない。
        c['outputs']=(root/'unused-primary',root/'unused-supplement',root/'unused-usb')
        c['out']=logs/'runtime3';transition[0]=False
        count=len(calls)
        with patch.object(m,'load',side_effect=fake_load),patch.object(m.subprocess,'run',side_effect=run):
            with self.assertRaises(FileExistsError):m.execute(a,c)
        self.assertTrue(all(x[0]!='adb' for x in calls[count:]))
        # 既controllerと共通のserial flockで別outの並行実行も拒否。
        c['out']=logs/'runtime4'
        with (logs/('unattended-qualification-'+serial+'.lock')).open('a+') as lock:
            m.fcntl.flock(lock,m.fcntl.LOCK_EX|m.fcntl.LOCK_NB)
            with patch.object(m.subprocess,'run',side_effect=AssertionError('競合device操作禁止')):
                with self.assertRaises(BlockingIOError):m.execute(a,c)
        self.assertFalse(c['out'].exists())


def read_json(p):return json.loads(p.read_text())


if __name__=='__main__':unittest.main()

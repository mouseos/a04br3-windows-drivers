#!/usr/bin/env python3
"""Prepare pins for already-installed, test-signed packages; never edit a device."""
import argparse,hashlib,json,ssl,subprocess
from pathlib import Path
import importlib.util
ROOT=Path(__file__).resolve().parents[2];WORKSPACE=ROOT.parents[1]
spec=importlib.util.spec_from_file_location('maintenance_builder',ROOT/'tools/winpe/build-maintenance-esp.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

def main():
 p=argparse.ArgumentParser();p.add_argument('--toprgu',type=Path,required=True);p.add_argument('--shared',type=Path,required=True);p.add_argument('--catalog-proof',type=Path,required=True);p.add_argument('--osslsigncode',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 top=a.toprgu.resolve(strict=True);shared=a.shared.resolve(strict=True);proof=m.regular(a.catalog_proof);verify=m.regular(a.osslsigncode);out=a.output.resolve()
 if any(WORKSPACE not in x.parents for x in (top,shared,out)) or not top.is_dir() or not shared.is_dir() or out.exists():raise ValueError('paths rejected')
 report=json.loads(proof.read_text(encoding='utf-8-sig'))
 if report.get('version')!=1 or report.get('catalog_membership_verified') is not True:raise ValueError('catalog proof rejected')
 observations=report.get('observations',[]);required={'mt8167-toprgu.sys','mt8167-toprgu.inf','mt8167-shared.sys','shared-parent.inf','bu21025.sys','bu21025.inf'}
 if len(observations)!=6 or {x.get('name') for x in observations}!=required:raise ValueError('catalog observations incomplete')
 files={'mt8167-toprgu.sys':top/'mt8167-toprgu.sys','mt8167-toprgu.cat':top/'mt8167-toprgu.cat','mt8167-toprgu.inf':top/'mt8167-toprgu.inf','mt8167-toprgu.cer':top/'mt8167-toprgu.cer',
        'mt8167-shared.sys':shared/'parent/mt8167-shared.sys','shared-parent.cat':shared/'parent/shared-parent.cat','shared-parent.inf':shared/'parent/shared-parent.inf',
        'bu21025.sys':shared/'touch/bu21025.sys','bu21025.cat':shared/'touch/bu21025.cat','bu21025.inf':shared/'touch/bu21025.inf','shared-touch-test.cer':shared/'shared-touch-test.cer'}
 files={n:m.regular(v) for n,v in files.items()}
 if any(m.sha(f)!=report.get('hashes',{}).get(n,'').lower() for n,f in files.items()):raise ValueError('proof file hash mismatch')
 topcert=files['mt8167-toprgu.cer'].read_bytes();sharedcert=files['shared-touch-test.cer'].read_bytes()
 if [hashlib.sha1(x).hexdigest().upper() for x in (topcert,sharedcert)]!=[x.upper() for x in report.get('thumbprints',[])]:raise ValueError('certificate identity mismatch')
 out.mkdir(parents=True)
 for label,cert,names in (('toprgu',topcert,('mt8167-toprgu.sys','mt8167-toprgu.cat')),('shared',sharedcert,('mt8167-shared.sys','shared-parent.cat','bu21025.sys','bu21025.cat'))):
  pem=out/(label+'.pem');pem.write_text(ssl.DER_cert_to_PEM_cert(cert))
  for name in names:subprocess.run([str(verify),'verify','-CAfile',str(pem),'-in',str(files[name])],check=True,stdout=subprocess.DEVNULL)
  (out/(label+'.cer')).write_bytes(cert);(out/(label+'-cert.sha256')).write_bytes(hashlib.sha256(cert).digest())
 for short,name in (('toprgu','mt8167-toprgu.sys'),('shared','mt8167-shared.sys'),('touch','bu21025.sys')):
  (out/(short+'-sys.sha256')).write_bytes(bytes.fromhex(m.sha(files[name])))
 result={'version':1,'catalog_proof_sha256':m.sha(proof),'package_hashes':{n:m.sha(f) for n,f in files.items()},'thumbprints':report['thumbprints'],
         'files':{f.name:m.sha(f) for f in out.iterdir() if f.is_file()},'catalog_observations':observations,
         'status':'test-signed exact installed SYS precondition; RT policy and PnP unverified'}
 (out/'manifest.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
if __name__=='__main__':main()

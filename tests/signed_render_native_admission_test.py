"""共通の反例fixtureをhostとWindowsの実PowerShell入口で使う。"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools/recovery'))
from signed_render_native_admission import admit
from collection_reboot import CONFIG, SNAPSHOT_CONFIG


def cases():
    p=dict(profile='combined-features-fresh241-signed-render-v1',service='MT8167CombinedFollowup241',data_directory='MT8167CombinedFollowup241',capture_selection={'capture_number':241},first_install_mask=0,deferred_unbound_mask=0,
           diagnostic_clients=[{'name':'gpu_present','phase':1},{'name':'gpu_globaldiag','phase':1}],
           signed_render_contract=dict(schema='SIGNED_RENDER_CAPTURE_TRANSITION_V1',capture=241,run=242,post_capture=243,render_update_mask=1,install_mask=1024,planned_reboots=1,boot_write_required=False,gpt_written=False,terminal_windows_hold=False),
           unattended_collection_reboot=dict(schema='SIGNED_RENDER_UNATTENDED_COLLECTION_V1',capture=241,bounded_winpe_required=True,require_atomic_publication=True),collection_reboot=copy.deepcopy(CONFIG))
    result=[('collection',copy.deepcopy(p),True)]
    old=copy.deepcopy(p)
    for k in ('unattended_collection_reboot','collection_reboot'):del old[k]
    old['signed_render_contract']['terminal_windows_hold']=True
    result.append(('legacy-hold',old,True))
    disabled=copy.deepcopy(old)
    disabled.update(collection_reboot=False,vidpn_uart_snapshot=False)
    result.append(('legacy-disabled-flags',disabled,True))
    snap=copy.deepcopy(p);snap['vidpn_uart_snapshot']=copy.deepcopy(SNAPSHOT_CONFIG);result.append(('collection-snapshot',snap,True))
    mutations=[(('unattended_collection_reboot',),None),(('collection_reboot',),None),(('signed_render_contract','terminal_windows_hold'),True),(('signed_render_contract','terminal_windows_hold'),0),(('unattended_collection_reboot','capture'),239),(('unattended_collection_reboot','bounded_winpe_required'),False),(('unattended_collection_reboot','require_atomic_publication'),False),(('unattended_collection_reboot','bounded_winpe_required'),1),(('collection_reboot','maximum_shutdown_requests'),2),(('collection_reboot','uart_reset_allowed'),True),(('collection_reboot','enabled'),1),(('collection_reboot','schema'),'wrong'),(('collection_reboot','extra'),True),(('first_install_mask',),1),(('deferred_unbound_mask',),1),(('diagnostic_clients',),[{'name':'gpu_present','phase':0},{'name':'gpu_globaldiag','phase':1}]),(('diagnostic_clients',),[{'name':'gpu_present','phase':1}]),(('diagnostic_clients',),[{'name':'gpu_globaldiag','phase':1},{'name':'gpu_present','phase':1}]),(('diagnostic_clients',),[{'name':'gpu_present','phase':True},{'name':'gpu_globaldiag','phase':1}]),(('service',),'other'),(('data_directory',),'other'),(('profile',),'combined-features-fresh241-signed-render-v1-collection-reboot-v1'),(('signed_render_contract','boot_write_required'),True),(('signed_render_contract','gpt_written'),True),(('signed_render_contract','run'),240),(('signed_render_contract','install_mask'),1152),(('filter_root_transitions',),[{}]),(('vidpn_uart_snapshot',),{'enabled':True})]
    for i,(path,value) in enumerate(mutations):
        q=copy.deepcopy(p);at=q
        for key in path[:-1]:at=at[key]
        at[path[-1]]=value;result.append((f'reject-{i}',q,False))
    oldoff=copy.deepcopy(old);oldoff['signed_render_contract']['terminal_windows_hold']=False;result.append(('reject-no-opt-in',oldoff,False))
    mixed=copy.deepcopy(old);mixed['collection_reboot']=copy.deepcopy(CONFIG);result.append(('reject-hold-with-config',mixed,False))
    stale=copy.deepcopy(p)
    # 全世代参照を整えても、237未満のcollection retrofitは拒否。
    for key in ('service','data_directory'):stale[key]='MT8167CombinedFollowup235'
    stale['profile']='combined-features-fresh235-signed-render-v1';stale['capture_selection']['capture_number']=235
    for key,value in [('capture',235),('run',236),('post_capture',237)]:stale['signed_render_contract'][key]=value
    stale['unattended_collection_reboot']['capture']=235
    result.append(('reject-retrofit235',stale,False))
    hold235=copy.deepcopy(stale)
    for key in ('collection_reboot','unattended_collection_reboot'):del hold235[key]
    hold235['signed_render_contract']['terminal_windows_hold']=True
    result.append(('legacy235-hold',hold235,True))
    return result


def main():
    ap=argparse.ArgumentParser();ap.add_argument('--emit-fixtures',type=Path);ap.add_argument('--actual-policy',type=Path);a=ap.parse_args()
    rows=[]
    for name,p,expected in cases():
        try:mode=admit(p,p['capture_selection']['capture_number']);accepted=True
        except (ValueError,KeyError,TypeError):accepted=False
        assert accepted==expected,name
        if a.emit_fixtures:
            a.emit_fixtures.mkdir(parents=True,exist_ok=True)
            f=a.emit_fixtures/(name+'.json');raw=(json.dumps(p)+'\n').encode()
            with f.open('xb') as out:out.write(raw)
            rows.append(dict(name=name,path=f.name,sha256=hashlib.sha256(raw).hexdigest(),capture=p['capture_selection']['capture_number'],accepted=expected))
    if a.emit_fixtures:
        with (a.emit_fixtures/'cases.json').open('x') as f:json.dump(rows,f,indent=2)
    if a.actual_policy:assert admit(json.loads(a.actual_policy.read_text()),241)=='unattended-collection'
    print(json.dumps(dict(cases=len(cases()),positive=sum(x[2] for x in cases()),negative=sum(not x[2] for x in cases()),actual_policy_checked=bool(a.actual_policy),host_gate='PASS')))

if __name__=='__main__':main()

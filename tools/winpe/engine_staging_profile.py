"""Explicit normal-installer staging for an already bootstrapped fixed Engine OS."""
import re
NONBOOT={'PARENT','AUDIO','USB','WIFI','DISPLAY','SD'}

def apply_profile(cmd,policy,backup,previous,components):
    if policy and policy.get('transaction')=='codec1026-msdc-quiet-fresh84':
        from codec_msdc_transaction import staging
        return staging(cmd,policy,backup,previous)
    profile=policy.get('staging_profile','offline-all') if policy else 'offline-all'
    if profile=='offline-all':return cmd,{'profile':profile,'offline_driver_policy':'legacy-all'}
    if profile!='normal-deferred' or not previous or previous.get('audio_mode')!='engine_pcm_retained':
        raise ValueError('normal-deferredは既存Engine媒体/freshbackup再arm専用です')
    actual={x['source_path'].replace('\\','/').lower():x['sha256'].lower() for x in backup['files']}
    expected=dict(previous.get('expected_installed_sys',{}))
    # The GPU updater may already have advanced the shared parent since the
    # last Engine run. Accept only this signed fixed tuple when the fresh
    # backup actually contains its SYS; never infer it from a version alone.
    parent=components.get('parent',{})
    if (parent.get('wrapper_source_commit')=='287241c37fa0eae538e5e2d136dea5df42de7e43'
        and parent.get('driver_ver')=='11/07/2026,2026.11.7.1014'):
        digest=parent.get('files',{}).get('parent/mt8167-shared.sys')
        if digest!='0498bd9f5cadb0e064e42fb80f4717d3cd04729d61a0e2763a77d45487bf8343':
            raise ValueError('GPU1014 parent signature tuple mismatch')
        expected['mt8167-shared.sys']=digest
    if (parent.get('wrapper_source_commit')=='13ae9f85c37331a3dd7324278e932956b9a840ef'
        and parent.get('driver_ver')=='11/07/2026,2026.11.7.1015'):
        candidate='e7df942265a8216ba393152f1fe781e47d48ebe09b6da02f235dd8c2ba7d9752'
        if parent.get('files',{}).get('parent/mt8167-shared.sys')!=candidate:raise ValueError('親1015固定署名SYS不一致')
        installed=actual.get('windows/system32/drivers/mt8167-shared.sys')
        if installed not in (candidate,'0498bd9f5cadb0e064e42fb80f4717d3cd04729d61a0e2763a77d45487bf8343'):raise ValueError('親1015の移行基点は既知1014/1015だけです')
        expected['mt8167-shared.sys']=installed
    expected['mt8167-ge8300.sys']=policy.get('gpu_transition',{}).get('baseline_sys_sha256','a8f226b33fa11915e88b3424c5ad3ed3aaa62ff35deb7f91902b04e7df4bf090')
    for name in ('mt8167-shared.sys','mt8167-toprgu.sys','mt8167-ge8300.sys'):
        if not expected.get(name) or actual.get('windows/system32/drivers/'+name)!=expected[name].lower():
            raise ValueError('normal stagingの既存parent/TOPRGU/GPU bootstrap実SYS不一致: '+name)
    # The old service itself and consumed marker/log are checked by Engine
    # rearm separately. Files/trust/firmware/native SYSTEM remain untouched.
    skip=set(NONBOOT);top=components.get('toprgu');oldtop=previous.get('components',{}).get('toprgu')
    if top and oldtop and top.get('files')==oldtop.get('files') and top.get('driver_ver')==oldtop.get('driver_ver'):
        skip.add('TOPRGU')
    lines=cmd.splitlines();out=[];removed=[];i=0
    while i<len(lines):
        match=re.fullmatch(r'call :adddriver ([A-Z]+) .+',lines[i])
        if match and match[1] in skip:
            if i+1>=len(lines) or lines[i+1]!='if errorlevel 1 goto dismfail':raise ValueError('固定DISM template形状不一致')
            removed.append(match[1]);i+=2;continue
        out.append(lines[i]);i+=1
    if not {'PARENT','AUDIO','USB','WIFI','DISPLAY'}.issubset(removed):raise ValueError('normal staging固定5DISM対象欠落')
    return '\n'.join(out)+'\n',{'profile':profile,'removed_offline_driver_calls':removed,
        'bootstrap_verified_sys':{n:expected[n] for n in ('mt8167-shared.sys','mt8167-toprgu.sys','mt8167-ge8300.sys')},
        'normal_install':'fixed signed packages, DIF_INSTALLDEVICE + DI_DONOTCALLCONFIGMG, one planned reboot',
        'bootcritical_policy':'unchanged TOPRGU may skip; changed TOPRGU stays offline; eMMC/bootstrap never modified'}

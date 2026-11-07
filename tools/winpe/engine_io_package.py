"""Fixed EnginePcm composition from separately signed, immutable components."""
import hashlib
import json
import re
import struct
from pathlib import Path, PurePosixPath
from private_hardware_probe_input import sha

ENGINE_RUNTIME = 'de214c8b7495913d0716f149b24fd210ef42de8a'
ENGINE_INF = 'e4dff2b25cc164892dce1d534cb564218845c29c'
LEGACY_RUNTIME = 'f17b743dbed1053451a89d03de805c27021b1cc5'
LEGACY_CLIENT = 'a016d53763b7872cd1235d5e96204d36ab48cca4'
ENGINE_CLIENT = 'd0eb43691b4358fb3dc165946da33d03a4ce1b29'
BOUNDED_ENGINE_CLIENT = '9bce14680011b4f208144905823e777057da2a22'
ENGINE_CERT = 'BCBF6C456C6F0DD0818434763B189488FBF13570'
LEGACY_CERT = '2655FC89A7082D3BAB3F26EC556EE8A3C52F4358'
BASE_VERSION = '11/06/2026,2026.11.6.1001'
AUDIO_VERSION = '11/06/2026,2026.11.7.1004'
WIFI_REGISTRATION_SOURCE = '37a5bb1cc0d34b95d065bbe7bcb3e3a467e93f81'
WIFI_INF1002_SOURCE = 'd751c6792af4e8c01b0d9442c2e462a54c4d8624'
WIFI_DIRECT_OID_SOURCE = '9d3a88907b1ac9286d730b1efc8a58b21072462c'
BRIGHTNESS_SOURCE = '0c73f273dc470dd3cb07c254808d534709dd711f'
BRIGHTNESS_BOOT = '341894cdcbc90c696e9d8b5b8733314f107e03c1'
TOPRGU_SOURCE = '0821853f798407858a1e2995b8f4e7d9d13c3e36'
NORMAL_TOPRGU = 'd6451d58ca82a4cbc888feaf1b716ce8f3398bc9'
NORMAL_RUNTIME_BLOBS = {
    'wdf_toprgu.c': '1ec6a26cb1031f41aee673ac4a0f42184962a37f',
    'reset_command.c': 'a0ef67918a299d11fbd7c8f048c620b7c611d6b0',
    'reset_command.h': 'd15fe52fea9f8d3e73eff3fc6ad6d6633099ab92',
    'watchdog_policy.h': '2cc72ce02a6ed994793e787908a925101dbc0cde',
    'handoff_policy.h': 'ad777c5ccf58355f8c71aab7fa5968c6ac1de495',
    'thermal_stop.h': '61b774ee40510d3a1b1dd333713d9c3cd194e1db',
    'uart_screen.h': '2b21d4252c4563834475edc5a787861246ec1fd9',
}


def check_inf(path, version, install='Install'):
    if install not in ('Install','Host'):raise ValueError('固定INF section不一致')
    text = path.read_text()
    actual = re.search(r'(?m)^DriverVer\s*=\s*([^\r\n]+)', text)
    section = re.search(r'(?im)^\['+install+r'\.(NT(?:ARM)?)\]\s*$', text)
    services = section and re.search(r'(?im)^\['+install+r'\.' + section[1] + r'\.Services\]\s*$', text)
    if not actual or actual[1].strip() != version or not section or not services:
        raise ValueError('component INF date/version/NT decoration不一致: ' + path.name)


def require_armnt(path):
    with path.open('rb') as stream:
        dos = stream.read(64)
        if len(dos) != 64 or dos[:2] != b'MZ':
            raise ValueError('ARM32 PEが必要です: ' + path.name)
        offset = struct.unpack_from('<I', dos, 60)[0]
        if offset < 64 or offset > path.stat().st_size - 24:
            raise ValueError('PE header範囲不正')
        stream.seek(offset)
        header = stream.read(6)
    if header != b'PE\0\0\xc4\x01':
        raise ValueError('IMAGE_FILE_MACHINE_ARMNT以外のartifactです')


class SignedPackage:
    def __init__(self, directory, root, manifest_name='signed-manifest.json'):
        if manifest_name not in ('signed-manifest.json','tuple-manifest.json'):raise ValueError('固定署名manifest名だけ許可します')
        self.directory = directory.resolve(strict=True)
        if not self.directory.is_relative_to(root.resolve()):
            raise ValueError('署名packageは作業ルート内が必要です')
        self.manifest = self.directory / manifest_name
        self.record = json.loads(self.manifest.read_text(encoding='utf-8-sig'))
        self.files = {}
        self.native_wifi_layout = False
        entries=self.record.get('files')
        if entries is None:
            r=self.record
            if ((r.get('source_commit'),r.get('status'),r.get('driver_ver')) not in (
                ('bafafa9ea6d94fe6357616a3edfff66e568bfc7c','signed-native1006-not-device-validated','11/06/2026,2026.11.6.1006'),
                ('2d726d42702831aedbcee136d901595f65b39eee','signed-native1007-not-device-validated','11/06/2026,2026.11.6.1007'),
                ('8e5d01c748de275cd3560737c8897add05f0c2ab','signed-native1008-not-device-validated','11/06/2026,2026.11.6.1008'),
                ('08e3f5a832159d9dd93c30a88bb537b979c1c910','signed-native1009-not-device-validated','11/06/2026,2026.11.6.1009'))
                or r.get('catalog_regenerated') is not True
                or r.get('build_manifest',{}).get('source_dirty') is not False):raise ValueError('未知の署名manifest layout')
            if r.get('source_commit')=='8e5d01c748de275cd3560737c8897add05f0c2ab' and (r.get('expected_installed_sys_sha256')!='ff8bd2f035458974c15f035d1c62b5452d4789d5b615e995580bf3a7d8f88ed8' or r.get('build_manifest',{}).get('checkpoint_driver_build')!=1008):raise ValueError('固定WiFi1008 SYS/checkpoint出自不一致')
            if r.get('source_commit')=='08e3f5a832159d9dd93c30a88bb537b979c1c910' and (r.get('expected_installed_sys_sha256')!='c862a37a6593278753902aadb0b462b332f574d569c3adca5b44553b99b56117' or r.get('build_manifest',{}).get('checkpoint_driver_build')!=1009):raise ValueError('固定WiFi1009 SYS/checkpoint出自不一致')
            entries=r.get('package_files',[])
            if {x.get('path') for x in entries}!={'package/mt6630-runtime.'+e for e in ('sys','inf','cat')} or len(entries)!=3:raise ValueError('WiFi1006/1007/1008 fixed package_files不一致')
            entries=[{'path':x['path'].removeprefix('package/'),'sha256':x['sha256']} for x in entries]
            entries.append({'path':'build-manifest.json','sha256':r['build_manifest_sha256']})
            self.native_wifi_layout=True
        for entry in entries:
            name = entry['path'].replace('\\', '/')
            rel = PurePosixPath(name)
            if rel.is_absolute() or '..' in rel.parts or name in self.files:
                raise ValueError('署名manifest内path重複・逸脱')
            if not re.fullmatch('[0-9a-fA-F]{64}', entry['sha256']):
                raise ValueError('不正なartifact SHA256')
            self.files[name] = entry['sha256'].lower()

    def file(self, name):
        path = (self.directory / name).resolve(strict=True)
        if not path.is_relative_to(self.directory) or not path.is_file() or self.files.get(name) != sha(path):
            raise ValueError('署名packageのartifact hash/path不一致: ' + name)
        if path.suffix.lower() in ('.exe', '.sys'):
            require_armnt(path)
        return path

    def json(self, name):
        return json.loads(self.file(name).read_text(encoding='utf-8-sig'))

    def certificate(self, expected):
        if self.native_wifi_layout:
            if self.record.get('certificate_thumbprint')!=expected:raise ValueError('WiFi1006/1007/1008証明書metadata不一致')
            self.file('build-manifest.json')
            path=(self.directory/'mt8167-development.cer').resolve(strict=True)
            if not path.is_relative_to(self.directory):raise ValueError('証明書path逸脱')
        else:path = self.file('mt8167-development.cer')
        data = path.read_bytes()
        if not data.startswith(b'\x30') or hashlib.sha1(data).hexdigest().upper() != expected:
            raise ValueError('componentの署名cert thumbprint不一致')
        return path, data


def wifi_override_check(directory, root):
    """Accept only the fixed, separately signed Wi-Fi-only registration update.

    Inputs are flat WDK output plus signed-manifest.json; sidecar build files
    and any parent/client siblings are never selected or imported.
    """
    package = SignedPackage(directory, root)
    if package.record.get('signed') is False:raise ValueError('明示未署名Wi-Fi packageは使用できません')
    wifi_source = package.record.get('source_commit')
    wifi_version = BASE_VERSION if wifi_source == WIFI_REGISTRATION_SOURCE else '11/06/2026,2026.11.6.1002'
    if wifi_source not in (WIFI_REGISTRATION_SOURCE, WIFI_INF1002_SOURCE):
        raise ValueError('固定Wi-Fi source以外です')
    if wifi_source == WIFI_INF1002_SOURCE:
        build = package.json('manifest.json')
        if (package.record.get('binary_source_commit') != WIFI_REGISTRATION_SOURCE
                or package.record.get('binary_rebuilt') is not False
                or build.get('source_commit') != wifi_source or build.get('driver_ver') != wifi_version
                or build.get('binary_rebuilt') is not False or build.get('catalog_regenerated') is not True
                or build.get('catalog_certificate_thumbprint') != ENGINE_CERT
                or build.get('expected_installed_sys_sha256') != '77b53ac0b902d5d6c864247707e9f319585ebb64d3a17b8a14696c8153e1a821'):
            raise ValueError('Wi-Fi1002再CAT署名recipeの固定出自不一致')
    else:
        if package.record.get('signing_thumbprint') != ENGINE_CERT:
            raise ValueError('37a5bb1/BCBF署名Wi-Fi packageのmanifestが必要です')
        if package.record.get('signed') is not True:
            if ('signed' in package.record
                    or package.record.get('driver_source_commit') != WIFI_DIRECT_OID_SOURCE
                    or package.record.get('parent_rebuilt') is not False):
                raise ValueError('Wi-Fi署名manifestの固定回収schema不一致')
            build = package.json('manifest.json')
            if (build.get('source_commit') != wifi_source or build.get('driver_ver') != wifi_version
                    or build.get('wdk_link_succeeded') != 1 or build.get('native_runtime') != 1
                    or build.get('shared_parent_reused') is not True
                    or build.get('shared_parent_source_commit') != ENGINE_RUNTIME
                    or build.get('shared_parent_interface_blob') != '8d1ad9e6221e673a94afdf96af2f099baacb906c'):
                raise ValueError('回収Wi-Fi packageのhash済みbuild/親interface metadata不一致')
    cert_path, certificate = package.certificate(ENGINE_CERT)
    names = ['mt6630-runtime.' + extension for extension in ('inf', 'sys', 'cat')]
    selected = {'wifi/' + name: package.file(name) for name in names}
    check_inf(selected['wifi/mt6630-runtime.inf'], wifi_version)
    if wifi_source == WIFI_INF1002_SOURCE and sha(selected['wifi/mt6630-runtime.sys']) != build['expected_installed_sys_sha256']:
        raise ValueError('Wi-Fi1002の保持すべき署名SYS hash不一致')
    component = {'binary_source_commit': WIFI_REGISTRATION_SOURCE,
                 'inf_source_commit': wifi_source,
                 'direct_oid_fix_source_commit': WIFI_DIRECT_OID_SOURCE,
                 'driver_ver': wifi_version, 'certificate_thumbprint': ENGINE_CERT,
                 'signed_manifest_sha256': sha(package.manifest), 'explicit_override': True,
                 'replaces_binary_source_commit': LEGACY_RUNTIME,
                 'parent_replaced': False, 'wlan_client_replaced': False,
                 'files': {name: sha(path) for name, path in selected.items()}}
    selected['wifi-development.cer'] = cert_path
    return selected, component, certificate


def engine_package_check(legacy_path, engine_path, client_path, root, wifi_path=None):
    legacy, engine, client = [SignedPackage(path, root) for path in (legacy_path, engine_path, client_path)]
    if (legacy.record.get('driver_source_commit') != LEGACY_RUNTIME
            or legacy.record.get('client_source_commit') not in (LEGACY_CLIENT, LEGACY_CLIENT[:7])):
        raise ValueError('旧USB/Wi-Fi/WLAN packageの出自不一致')
    if (engine.record.get('binary_source_commit') != ENGINE_RUNTIME
            or engine.record.get('inf_source_commit') != ENGINE_INF
            or engine.record.get('audio_mode') != 'engine_pcm_retained'):
        raise ValueError('Engine SYS/INFの出自・mode不一致')
    client_source=client.record.get('source_commit')
    if (client_source not in (ENGINE_CLIENT,BOUNDED_ENGINE_CLIENT) or client.record.get('signed') is not True
            or client.record.get('signing_thumbprint') != ENGINE_CERT):
        raise ValueError('固定d0eb/9bceの署名済みEngine clientが必要です')
    old_cert_path, old_cert = legacy.certificate(LEGACY_CERT)
    engine_cert_path, engine_cert = engine.certificate(ENGINE_CERT)
    client_cert_path, client_cert = client.certificate(ENGINE_CERT)
    build = engine.json('build-manifest.json')
    caps = engine.json('parent/parent-manifest.json')
    if (build.get('source_commit') != ENGINE_RUNTIME or build.get('engine_pcm') is not True
            or build.get('source_dirty') is not False or caps.get('source_commit') != ENGINE_RUNTIME
            or caps.get('source_dirty') is not False):
        raise ValueError('Engine親の実build profile出自不一致')
    for name in ('A04_SHARED_ENGINE_PCM', 'A04_SHARED_WIFI_RUNTIME', 'A04_MT6392_RTC32K_DIAGNOSTIC',
                 'A04_SHARED_WIFI_IRQ_DIAGNOSTIC', 'A04_USB_GPIO22_SOURCE_EXPERIMENT',
                 'A04_SHARED_TOUCH_D0_EXPERIMENT', 'A04_SHARED_BUTTON_D0_EXPERIMENT'):
        if caps['flags'].get(name) != '1':
            raise ValueError('Engine親の必須機能不足: ' + name)
    if caps['flags'].get('A04_SHARED_USB_ONLY') != '0':
        raise ValueError('USB-only親はEngine統合に使用できません')
    old_build = legacy.json('build-manifest.json')
    if old_build.get('driver_source_commit') != LEGACY_RUNTIME:
        raise ValueError('旧bundleのbuild出自不一致')
    probe_build = client.json('probe-manifest.json')
    if probe_build.get('source_commit')!=client_source or probe_build.get('source_dirty') is not False:
        raise ValueError('通常音声専用probe source不一致')
    if client_source==ENGINE_CLIENT:
        if (probe_build.get('package_version')!='2026.11.7.2'
            or probe_build.get('driver_runtime_source_expected')!=ENGINE_RUNTIME
            or probe_build.get('audio_inf_source_expected')!=ENGINE_INF):raise ValueError('旧通常音声専用probe profile不一致')
    elif (probe_build.get('package_version')!='2026.11.7.4'
        or probe_build.get('driver_runtime_source_expected')!='3d83f6fee554455434e2c03112c56bbf351e4c4b'
        or probe_build.get('audio_inf_source_expected')!='3d83f6fee554455434e2c03112c56bbf351e4c4b'
        or probe_build.get('audio_inf_driverver_expected')!='11/06/2026,2026.11.7.1007'
        or probe_build.get('output_limit_bytes')!=16384 or probe_build.get('snapshot_period_ms')!=100
        or probe_build.get('signed_audio_sys_sha256_expected')!='b917801c0f673aa5ab36be25c4133ad5d1370f1130936bbc4fda7086cf374edc'
        or probe_build.get('signed_parent_sys_sha256_expected')!='4638ab23c614aa6537b54b321c2b3bea5e6c54ac02f3c61956a7b1102fac42a5'):
        raise ValueError('bounded probeの固定Audio1007/親1010/出力契約不一致')
    selected, components = {}, {}
    definitions = (
        ('parent', engine, 'shared-parent', 'mt8167-shared', BASE_VERSION, ENGINE_RUNTIME, ENGINE_INF, ENGINE_CERT),
        ('audio', engine, 'mt8167-afe-engine-pcm', 'mt8167-audio', AUDIO_VERSION, ENGINE_RUNTIME, ENGINE_INF, ENGINE_CERT),
        ('usb', legacy, 'mt8167-usb20', 'mt8167-usb20', BASE_VERSION, LEGACY_RUNTIME, LEGACY_RUNTIME, LEGACY_CERT),
        ('wifi', legacy, 'mt6630-runtime', 'mt6630-runtime', BASE_VERSION, LEGACY_RUNTIME, LEGACY_RUNTIME, LEGACY_CERT),
    )
    for kind, package, inf, binary, version, runtime_source, inf_source, certificate in definitions:
        if kind == 'wifi' and wifi_path is not None:
            continue
        names = [kind + '/' + name for name in (inf + '.inf', binary + '.sys', inf + '.cat')]
        for name in names:
            selected[name] = package.file(name)
        check_inf(selected[names[0]], version)
        components[kind] = {'binary_source_commit': runtime_source, 'inf_source_commit': inf_source,
                            'driver_ver': version, 'certificate_thumbprint': certificate,
                            'signed_manifest_sha256': sha(package.manifest),
                            'files': {name: sha(selected[name]) for name in names}}
    components['parent']['capability_manifest_sha256'] = sha(engine.file('parent/parent-manifest.json'))
    components['parent']['build_manifest_sha256'] = sha(engine.file('build-manifest.json'))
    components['parent']['interfaces'] = {'audio': 11, 'usb': 1, 'wifi': 2}
    selected['clients/mt6630-wlan-probe.exe'] = legacy.file('clients/mt6630-wlan-probe.exe')
    selected['clients/mt8167-audio-engine-probe.exe'] = client.file('mt8167-audio-engine-probe.exe')
    for kind, name, package, source, certificate in (
            ('wlan_client', 'mt6630-wlan-probe.exe', legacy, LEGACY_RUNTIME, LEGACY_CERT),
            ('audio_client', 'mt8167-audio-engine-probe.exe', client, client_source, ENGINE_CERT)):
        components[kind] = {'binary_source_commit': source, 'certificate_thumbprint': certificate,
                            'sha256': sha(selected['clients/' + name]),
                            'signed_manifest_sha256': sha(package.manifest), 'arguments': []}
    components['wlan_client']['legacy_bundle_client_fix_source'] = LEGACY_CLIENT
    for name, expected in (('parent/mt8167-shared.sys', probe_build['signed_parent_sys_sha256_expected']),
                           ('audio/mt8167-audio.sys', probe_build['signed_audio_sys_sha256_expected'])):
        if client_source==ENGINE_CLIENT and sha(selected[name]) != expected.lower():
            raise ValueError('専用probeのexpected Engine SYS hashとpackageが不一致')
    if client_source==BOUNDED_ENGINE_CLIENT:
        components['audio_client']['expected_driver_sys']={
            'parent/mt8167-shared.sys':probe_build['signed_parent_sys_sha256_expected'],
            'audio/mt8167-audio.sys':probe_build['signed_audio_sys_sha256_expected']}
    selected.update({'mt8167-development.cer': old_cert_path, 'engine-development.cer': engine_cert_path,
                     'engine-client-development.cer': client_cert_path})
    certificates = {'legacy_usb_wifi_wlan': old_cert, 'engine_parent_audio': engine_cert,
                    'engine_audio_client': client_cert}
    if wifi_path is not None:
        wifi_files, wifi_component, wifi_cert = wifi_override_check(wifi_path, root)
        selected.update(wifi_files)
        components['wifi'] = wifi_component
        # The old certificate is still required for USB and the WLAN client.
        certificates['legacy_usb_wlan'] = certificates.pop('legacy_usb_wifi_wlan')
        certificates['wifi_override'] = wifi_cert
    assert len([name for name in selected if name.endswith('/mt8167-shared.sys')]) == 1
    assert not any('first-playback' in name for name in selected)
    return selected, components, certificates


def engine_optional_check(brightness_path, toprgu_path, root, brightness_manifest_sha256=None):
    selected, components, certificates, commands = {}, {}, {}, []
    if brightness_path:
        package = SignedPackage(brightness_path, root)
        if brightness_manifest_sha256 is not None and (not re.fullmatch('[0-9a-fA-F]{64}',brightness_manifest_sha256) or sha(package.manifest)!=brightness_manifest_sha256.lower()):
            raise ValueError('brightness manifest SHA256 pin不一致')
        if package.record.get('signed') is False:raise ValueError('明示未署名brightness packageは使用できません')
        brightness_source=package.record.get('source_commit')
        repack=brightness_source != BRIGHTNESS_SOURCE
        known_repack=brightness_source=='6c665d43bf4a123605f83f6436562dffce25722f'
        if ((not known_repack and package.record.get('brightness_enabled') != 1) or (repack and
                (not re.fullmatch('[0-9a-f]{40}', brightness_source or '')
                 or not re.fullmatch('[0-9a-fA-F]{64}', brightness_manifest_sha256 or '')
                 or sha(package.manifest) != brightness_manifest_sha256.lower()))):
            raise ValueError('brightness source/profileと更新manifest hash pinが必要です')
        if known_repack:
            metadata=package.json('manifest.json')
            if (package.record.get('binary_source_commit')!=BRIGHTNESS_SOURCE or package.record.get('sys_resigned') is not False
                    or package.record.get('signing_thumbprint')!=ENGINE_CERT or metadata.get('binary_recompiled') is not False
                    or metadata.get('cat_signer_thumbprint')!=ENGINE_CERT or metadata.get('driver_version')!='0.0.1.4'
                    or metadata.get('expected_sys_sha256')!='71f4ddaec639045a8b7dc1aaa7bc4c341e3f5955e2bec3b00f7a1a91528c7dce'):
                raise ValueError('brightness14の既知SYS保持/INF出自不一致')
        cert_path, der = package.certificate(ENGINE_CERT)
        names = ['mt8167-display.' + extension for extension in ('sys', 'inf', 'cat')]
        for name in names:
            selected['display/' + name] = package.file(name)
        brightness_version='11/06/2026,0.0.1.3'
        if repack:
            text=selected['display/mt8167-display.inf'].read_text()
            found=re.search(r'(?m)^DriverVer\s*=\s*([^\r\n]+)', text)
            brightness_version=found[1].strip() if found else ''
            from engine_rearm import version
            if version(brightness_version) <= version('11/06/2026,0.0.1.3'):
                raise ValueError('brightness更新には旧版より高いINF版が必要です')
        check_inf(selected['display/mt8167-display.inf'], brightness_version)
        if known_repack and sha(selected['display/mt8167-display.sys']) != metadata['expected_sys_sha256']:
            raise ValueError('brightness14の署名SYS保持hash不一致')
        selected['display-development.cer'] = cert_path
        certificates['optional_brightness'] = der
        components['display'] = {'binary_source_commit': brightness_source, 'inf_source_commit': brightness_source,
            'driver_ver': brightness_version, 'certificate_thumbprint': ENGINE_CERT,
            'signed_manifest_sha256': sha(package.manifest), 'prerequisite_boot_source': BRIGHTNESS_BOOT,
            'boot_installed_verified_here': False, 'normal_live_full_preserved_reported_by_owner': True,
            'files': {'display/' + name: sha(selected['display/' + name]) for name in names}}
        commands += ['call :adddriver DISPLAY X:\\a04br3\\shared-io\\display\\mt8167-display.inf', 'if errorlevel 1 goto dismfail']
    if toprgu_path:
        directory = toprgu_path.resolve(strict=True)
        if not directory.is_relative_to(root.resolve()):
            raise ValueError('TOPRGU packageは作業ルート内が必要です')
        manifest = directory / 'manifest.json'
        record = json.loads(manifest.read_text(encoding='utf-8-sig'))
        flags = {'A04_TOPRGU_CPU1_HANDOFF': '1', 'A04_TOPRGU_UART_SCREEN': '1',
                 'A04_TOPRGU_THERMAL_STOP': '0', 'A04_TOPRGU_DEADLINE_SECONDS': '0'}
        blobs = {Path(entry['path']).name: entry['git_blob'] for entry in record['source_files']}
        if (record.get('source_commit') != TOPRGU_SOURCE or record.get('signed') is not True
                or record.get('normal_runtime_reference') != NORMAL_TOPRGU
                or record.get('required_active_processors') != 4 or record.get('ownership_cpu') != 1
                or record.get('critical_flags') != flags
                or record.get('preservation', {}).get('runtime_blobs_equal_normal_reference') is not True
                or any(blobs.get(name) != value for name, value in NORMAL_RUNTIME_BLOBS.items())):
            raise ValueError('normal TOPRGUのsource/7blob/4CPU/HANDOFF/DEADLINE/THERMAL/UART契約不一致')
        cert_path = (directory / 'development-public.cer').resolve(strict=True)
        der = cert_path.read_bytes()
        certificate = record['certificate']
        if (not cert_path.is_relative_to(directory) or not der.startswith(b'\x30')
                or hashlib.sha1(der).hexdigest().upper() != ENGINE_CERT
                or certificate.get('thumbprint') != ENGINE_CERT
                or certificate.get('public_cer_sha256', '').lower() != sha(cert_path)):
            raise ValueError('TOPRGU certificate不一致')
        declared = {entry['path']: entry for entry in record['package_files']}
        for extension in ('sys', 'inf', 'cat'):
            name = 'mt8167-toprgu.' + extension
            entry = declared['package/' + name]
            path = (directory / 'package' / name).resolve(strict=True)
            if (not path.is_relative_to(directory) or path.stat().st_size != entry['bytes']
                    or sha(path) != entry['sha256'].lower()):
                raise ValueError('TOPRGU artifact hash/path不一致')
            if extension == 'sys':
                require_armnt(path)
            selected['toprgu/' + name] = path
        version = '11/06/2026,0.0.11.0'
        if record.get('driver_version') != version:
            raise ValueError('TOPRGU version不一致')
        check_inf(selected['toprgu/mt8167-toprgu.inf'], version)
        selected['toprgu-development.cer'] = cert_path
        certificates['optional_toprgu'] = der
        components['toprgu'] = {'binary_source_commit': TOPRGU_SOURCE, 'inf_source_commit': TOPRGU_SOURCE,
            'driver_ver': version, 'certificate_thumbprint': ENGINE_CERT, 'manifest_sha256': sha(manifest),
            'normal_runtime_reference': NORMAL_TOPRGU, 'critical_flags': flags,
            'runtime_git_blobs_equal_normal_reference': True, 'device_verified_here': False,
            'files': {name: sha(path) for name, path in selected.items() if name.startswith('toprgu/')}}
        commands += ['call :adddriver TOPRGU X:\\a04br3\\shared-io\\toprgu\\mt8167-toprgu.inf', 'if errorlevel 1 goto dismfail']
    return selected, components, certificates, commands


def parent_override_check(directory, manifest_sha256, root, selected, components):
    package = SignedPackage(directory, root)
    if (not re.fullmatch('[0-9a-fA-F]{64}', manifest_sha256 or '')
            or sha(package.manifest) != manifest_sha256.lower()
            or not re.fullmatch('[0-9a-f]{40}', package.record.get('source_commit', ''))
            or package.record.get('signed') is False):
        raise ValueError('親repackageには固定manifest hash/source/BCBF署名が必要です')
    if package.record.get('signing_thumbprint') != ENGINE_CERT:
        if (package.record.get('source_commit') != 'a9f86314f4c63c8c41091682e593e04d3fd0ac2a'
                or package.record.get('template_commit') != 'a25f089061f2adee487e32c20a397ee5f1060cc6'
                or package.record.get('binary_source_commit') != ENGINE_RUNTIME):
            raise ValueError('親1005の固定署名wrapper出自不一致')
        repack=package.json('package-manifest.json')
        if (repack.get('binary_source_commit') != ENGINE_RUNTIME
                or repack.get('inf_source_commit') != package.record['source_commit']
                or repack.get('inf_source_dirty') is not False or repack.get('runtime_recompiled') is not False
                or repack.get('driver_ver') != '11/06/2026,2026.11.7.1005'
                or repack.get('signed_parent_sys_sha256') != sha(selected['parent/mt8167-shared.sys'])
                or sha(package.file('runtime-parent-manifest.json')) != components['parent']['capability_manifest_sha256']):
            raise ValueError('親1005のruntime保持/capability/INF出自不一致')
    cert_path, certificate = package.certificate(ENGINE_CERT)
    files = {'parent/' + name: package.file(name) for name in
             ('shared-parent.inf', 'mt8167-shared.sys', 'shared-parent.cat')}
    if sha(files['parent/mt8167-shared.sys']) != sha(selected['parent/mt8167-shared.sys']):
        raise ValueError('親repackageは既知Engine署名SYSを保持してください')
    text = files['parent/shared-parent.inf'].read_text()
    match = re.search(r'(?m)^DriverVer\s*=\s*([^\r\n]+)', text)
    driver_ver = match[1].strip() if match else ''
    from engine_rearm import version
    if version(driver_ver) <= version(components['parent']['driver_ver']):
        raise ValueError('親repackageにはEngine旧親より高いDriverVerが必要です')
    check_inf(files['parent/shared-parent.inf'], driver_ver)
    component = dict(components['parent'])
    component.update(inf_source_commit=package.record['source_commit'], driver_ver=driver_ver,
                     signed_manifest_sha256=sha(package.manifest), explicit_override=True,
                     files={name: sha(path) for name, path in files.items()})
    files['parent-development.cer'] = cert_path
    return files, component, certificate

"""署名済み試験用WinPE watchdogを照合し、通常Windowsには触れず組み込む。"""
import hashlib
import json
import re
import struct
import subprocess
from pathlib import Path

THUMB = 'BCBF6C456C6F0DD0818434763B189488FBF13570'
MARKER = b'A04RGU_WINPE_LEASE_V1:1800'
NAMES = {'mt8167-toprgu.sys', 'mt8167-toprgu.inf', 'mt8167-toprgu.cat', 'mt8167-toprgu.cer'}
ATOMIC_REFERENCE_CASES = {
    'replace-existing': 0, 'publish-new-target': 0, 'empty-source': 0,
    'fault-before-publish': 90, 'existing-temp-rejected': 5,
    'missing-source-no-temp': 4, 'start-limit': 3, 'publish-limit': 8,
    'below-both-limits': 0, 'extra-argument': 2,
    'production-check-mode-rejected-in-reference': 2, 'ads-rejected': 2,
    'dos-device-rejected': 2, 'trailing-dot-rejected': 2,
    'outside-prefix-rejected': 2, 'directory-rejected': 4,
    'parent-reparse-rejected': 4, 'source-too-large': 4,
    'maximum-source-size': 0, 'locked-target': 10,
}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def need(ok, message):
    if not ok:
        raise ValueError(message)


def atomic_signature_gate(root, manifest, m, data, pe):
    """Pinned Windows署名検証reportを実EXE/CER/ログへ結合する（CI実機受理とは別）。"""
    need(m.get('signed') is True, 'Signed production atomic helper required')
    signing = m.get('signing', {})
    need(signing.get('schema') == 'ATOMIC_STAGE_AUTHENTICODE_V1'
         and signing.get('policy') == 'Authenticode' and signing.get('digest') == 'SHA256'
         and signing.get('thumbprint') == THUMB
         and signing.get('private_key_exported') is False
         and signing.get('new_identity_created') is False, 'Existing BCBF production signature required')
    optional = pe + 24
    need(optional + 136 <= len(data) and struct.unpack_from('<H', data, optional)[0] == 0x10b,
         'Signed production PE32 header required')
    offset, size = struct.unpack_from('<II', data, optional + 128)
    need(offset >= optional + 136 and offset % 8 == 0 and size >= 8
         and offset + size == len(data), 'Actual production certificate directory missing or invalid')
    length, revision, kind = struct.unpack_from('<IHH', data, offset)
    need(8 < length <= size and ((length + 7) & ~7) == size
         and revision == 0x200 and kind == 2 and data[offset + 8] == 0x30,
         'Actual production WIN_CERTIFICATE/PKCS7 required')
    certrow = signing['public_certificate']
    need(certrow['name'] == 'atomic-stage-replace.cer', 'Fixed public certificate leaf required')
    cert = (manifest.parent / certrow['name']).resolve(strict=True)
    need(cert.is_relative_to(root) and cert.is_file() and cert.stat().st_size == certrow['bytes']
         and sha(cert) == certrow['sha256'], 'Production public certificate drift')
    der = cert.read_bytes()
    need(hashlib.sha1(der).hexdigest().upper() == THUMB
         and der in data[offset + 8:offset + length], 'Actual embedded BCBF certificate mismatch')
    signatures = m.get('signatures', [])
    need(len(signatures) == 1 and signatures[0]['name'] == 'atomic-stage-replace.exe'
         and signatures[0]['status'] == 'Valid' and signatures[0]['thumbprint'] == THUMB
         and signatures[0]['sha256'] == hashlib.sha256(data).hexdigest()
         and signatures[0]['certificate_sha256'] == sha(cert),
         'Windows production signature report must bind actual bytes')
    steps = {s['name']: s for s in m['steps']}
    need(len(steps) == len(m['steps']) and all(s['exit_code'] == 0 for s in m['steps']),
         'Failed or duplicate atomic build/signature stage')
    for stage in ('sign-production', 'verify-production'):
        need(stage in steps and re.fullmatch('[0-9a-f]{64}', steps[stage].get('log_sha256') or ''),
             'Native production signature stage missing: ' + stage)
        log = (manifest.parent / 'logs' / (stage + '.log')).resolve(strict=True)
        need(log.is_relative_to(root) and log.is_file() and log.stat().st_size > 0
             and sha(log) == steps[stage]['log_sha256'], 'Actual signature log drift: ' + stage)
    payload = signing['non_signature_payload']
    unsigned_size = payload['unsigned_bytes']
    need(payload['unchanged'] is True and type(unsigned_size) is int
         and optional + 136 <= unsigned_size <= offset
         and payload['certificate_offset'] == offset and payload['certificate_bytes'] == size
         and not any(data[unsigned_size:offset]), 'Production signing payload layout mismatch')
    normalized = bytearray(data[:unsigned_size])
    normalized[optional + 64:optional + 68] = bytes(4)
    normalized[optional + 128:optional + 136] = bytes(8)
    digest = hashlib.sha256(normalized).hexdigest()
    need(payload['normalized_unsigned_sha256'] == digest
         and payload['normalized_signed_sha256'] == digest, 'Production non-signature payload mismatch')


def atomic_reference_cases_gate(cases):
    """件数だけのreceiptや重複による水増しを、実suiteの固定契約で拒否する。"""
    need(isinstance(cases, list) and len(cases) == len(ATOMIC_REFERENCE_CASES),
         'Fixed 20-case atomic reference suite required')
    names = [case.get('name') for case in cases if isinstance(case, dict)]
    need(len(names) == len(cases) and all(isinstance(name, str) for name in names)
         and len(set(names)) == len(names) and set(names) == set(ATOMIC_REFERENCE_CASES),
         'Fixed 20-case atomic reference names must be unique and complete')
    need(all(case.get('passed') is True and type(case.get('expected_exit')) is int
             and type(case.get('actual_exit')) is int
             and case['expected_exit'] == ATOMIC_REFERENCE_CASES[case['name']]
             and case['actual_exit'] == case['expected_exit'] for case in cases),
         'Fixed atomic reference case outcome mismatch')


def load_atomic_helper(root, manifest, digest):
    root = root.resolve(strict=True)
    manifest = (root / manifest).resolve(strict=True)
    need(manifest.is_relative_to(root) and re.fullmatch('[0-9a-f]{64}', digest or '')
         and sha(manifest) == digest, 'Atomic helper manifest pin mismatch')
    m = json.loads(manifest.read_text(encoding='utf-8-sig'))
    need(m['schema'] == 'ATOMIC_STAGE_REPLACE_BUILD_V1' and m['success'] is True
         and m['production_reference_mode'] is False and m['deadline_query_required'] is True,
         'Completed production atomic helper required')
    need(re.fullmatch('[0-9a-f]{40}', m['source_commit']) and m['source_dirty'] is False, 'Clean atomic helper source required')
    row = m['production']
    need(row['name'] == 'atomic-stage-replace.exe', 'Fixed production helper leaf required')
    exe = (manifest.parent / row['name']).resolve(strict=True)
    need(exe.is_relative_to(root) and exe.stat().st_size == row['bytes']
         and sha(exe) == row['sha256'], 'Actual production helper drift')
    data = exe.read_bytes()
    need(len(data) >= 256 and data[:2] == b'MZ'
         and b'A04_ATOMIC_STAGE_V1:LEASE_QUERY:PRODUCTION\0' in data,
         'Actual production lease-query marker missing')
    pe = struct.unpack_from('<I', data, 60)[0]
    need(pe + 24 < len(data) and data[pe:pe+4] == b'PE\0\0'
         and struct.unpack_from('<H', data, pe+4)[0] == 0x1c4, 'Production helper must be ARMNT')
    atomic_signature_gate(root, manifest, m, data, pe)
    tests = m['reference_tests']
    need(tests['status'] == 'passed' and tests['reference_sha256'] == m['reference']['sha256']
         and type(tests['cases']) is int and tests['cases'] == len(ATOMIC_REFERENCE_CASES)
         and re.fullmatch('[0-9a-f]{64}', tests['result_sha256']),
         'Bound Windows reference tests required')
    reference = (manifest.parent / m['reference']['name']).resolve(strict=True)
    need(reference.is_relative_to(root) and reference.stat().st_size == m['reference']['bytes']
         and sha(reference) == tests['reference_sha256'], 'Actual native reference helper drift')
    report = (manifest.parent / 'reference-result.json').resolve(strict=True)
    need(report.is_relative_to(root) and sha(report) == tests['result_sha256'],
         'Actual Windows reference receipt drift')
    result = json.loads(report.read_text(encoding='utf-8-sig'))
    atomic_reference_cases_gate(result.get('cases'))
    need(result['schema'] == 'ATOMIC_STAGE_REPLACE_REFERENCE_TEST_V1'
         and result['success'] is True and result['production_executed'] is False
         and result['reference_sha256'] == sha(reference)
         and result['production_sha256'] == sha(exe)
         and len(result['cases']) == tests['cases'],
         'Native failure-injection cases must bind actual production/reference bytes')
    return m, exe


def atomic_copy_stage(prepare):
    """既存copyの失敗分岐・照合順を保ち、全payloadを同volumeの原子的置換へ変える。"""
    converted = []
    count = system = 0
    pattern = re.compile(r'copy /b (?:/y )?(X:\\a04br3\\payload\\[A-Za-z0-9_.\\-]+) '
                         r'(C:\\Windows\\System32\\[A-Za-z0-9_.\\-]+) >> "%A04_REPORT%" 2>&1')
    for line in prepare:
        if not line.lower().startswith('copy /b '):
            converted.append(line)
            continue
        match = pattern.fullmatch(line)
        need(match is not None, 'Unknown payload copy cannot bypass atomic publication')
        src, dst = match.groups()
        need('..' not in src.split('\\') and '..' not in dst.split('\\'), 'Parent traversal forbidden')
        count += 1
        system += dst.lower() == r'c:\windows\system32\config\system'
        converted.append('X:\\a04br3\\atomic-stage-replace.exe "' + src + '" "' + dst + '" >> "%A04_REPORT%" 2>&1')
    need(count > 0 and system == 1, 'Exactly one atomic SYSTEM publication required')
    return converted, count


def load_package(root, manifest, digest):
    root = root.resolve(strict=True)
    manifest = (root / manifest).resolve(strict=True)
    need(manifest.is_relative_to(root) and re.fullmatch('[0-9a-f]{64}', digest or '')
         and sha(manifest) == digest, 'WinPE lease manifest pin mismatch')
    m = json.loads(manifest.read_text(encoding='utf-8-sig'))
    need(m['schema'] == 'TOPRGU_WINPE_LEASE_PACKAGE_V1' and m['success'] is True
         and m['signed'] is True and m['lease_seconds'] == 1800
         and m['force_swrst'] is True and m['hardware_verified'] is False,
         'Completed fixed WinPE-only lease package required')
    need(re.fullmatch('[0-9a-f]{40}', m['source_commit']) and m['source_dirty'] is False,
         'Committed lease runtime source required')
    flags = m['critical_flags']
    for key, value in {'A04_TOPRGU_WINPE_LEASE_SECONDS': '1800',
                       'A04_TOPRGU_CPU1_HANDOFF': '1', 'A04_TOPRGU_UART_SCREEN': '1',
                       'A04_TOPRGU_UART_VOLUME': '1', 'A04_TOPRGU_DEADLINE_SECONDS': '0',
                       'A04_TOPRGU_THERMAL_STOP': '0'}.items():
        need(str(flags.get(key)) == value, 'Lease compile flag mismatch: ' + key)
    need(m['certificate']['thumbprint'] == THUMB
         and m['certificate']['private_key_exported'] is False, 'Existing signer only')
    rows = {r['name']: r for r in m['outputs']}
    need(set(rows) == NAMES and len(rows) == len(m['outputs']), 'Four exact lease package leaves')
    files = {}
    for name, row in rows.items():
        p = (manifest.parent / 'package' / name).resolve(strict=True)
        need(p.is_relative_to(root) and p.is_file() and p.stat().st_size == row['bytes']
             and sha(p) == row['sha256'], 'Lease package leaf drift: ' + name)
        files[name] = p
    binary = files['mt8167-toprgu.sys'].read_bytes()
    need(len(binary) >= 256 and binary[:2] == b'MZ' and MARKER + b'\0' in binary,
         'Actual bounded-runtime marker missing')
    pe = struct.unpack_from('<I', binary, 60)[0]
    need(pe + 24 < len(binary) and binary[pe:pe+4] == b'PE\0\0'
         and struct.unpack_from('<H', binary, pe+4)[0] == 0x1c4, 'Signed ARMNT SYS required')
    need(hashlib.sha1(files['mt8167-toprgu.cer'].read_bytes()).hexdigest().upper() == THUMB,
         'Certificate DER differs from existing signer')
    signatures = m['signatures']
    for name in ('mt8167-toprgu.sys', 'mt8167-toprgu.cat'):
        sigs = [s for s in signatures if s.get('name', s.get('path', '').replace('\\', '/').rsplit('/', 1)[-1]) == name]
        need(len(sigs) == 1 and sigs[0]['status'] == 'Valid'
             and sigs[0]['thumbprint'] == THUMB and sigs[0]['sha256'] == rows[name]['sha256'],
             'Windows signature report must bind actual bytes: ' + name)
    steps = {s['name']: s for s in m['steps']}
    need(len(steps) == len(m['steps']) and all(s['exit_code'] == 0 for s in m['steps']),
         'No failed or duplicate native stage')
    for stage in ('verify-sys', 'verify-cat', 'member-mt8167-toprgu.sys', 'member-mt8167-toprgu.inf'):
        need(stage in steps and re.fullmatch('[0-9a-f]{64}', steps[stage]['log_sha256']),
             'Native signature/catalog evidence missing: ' + stage)
    return m, files



def inspect_boot_binding(system, env):
    """固定baseの選択ControlSet、サービス、KMDF、ACPI結合を現物から確認する。"""
    def value(key, name):
        return subprocess.run(['hivexget', str(system), key, name], env=env,
                              check=True, capture_output=True).stdout
    selected = {n: int(value('\\Select', n).strip()) for n in ('Current', 'Default', 'LastKnownGood')}
    need(all(n == 1 for n in selected.values()), 'Unexpected WinPE selected ControlSet')
    service = r'\ControlSet001\services\Mt8167Toprgu'
    actual = {n: value(service, n).decode().strip() for n in ('Start', 'Type', 'ImagePath', 'Group')}
    need(actual['Start'] == '0' and actual['Type'] == '1'
         and actual['ImagePath'].replace('/', '\\').lower() == r'system32\drivers\mt8167-toprgu.sys'
         and actual['Group'] == 'System Bus Extender', 'WinPE TOPRGU service binding mismatch')
    kmdf = value(service + r'\Parameters\Wdf', 'KmdfLibraryVersion').decode().strip()
    need(kmdf == '1.13', 'WinPE KMDF version mismatch')
    framework = r'\ControlSet001\services\Wdf01000'
    need(value(framework, 'Start').strip() == b'0' and value(framework, 'Type').strip() == b'1',
         'Boot-start KMDF runtime missing')
    need(value(r'\DriverDatabase\DeviceIds\ACPI\MOU0005', 'oem21.inf') == b'\x01\xff\x00\x00',
         'WinPE ACPI MOU0005 driver database binding mismatch')
    active = value(r'\DriverDatabase\DriverInfFiles\oem21.inf', 'Active').decode().strip()
    need(active == 'mt8167-toprgu.inf_arm_8d603e8df23457f7', 'Fixed base TOPRGU package differs')
    configured = value('\\DriverDatabase\\DriverPackages\\' + active + r'\Configurations\Install.NT', 'Service').decode().strip()
    need(configured == 'Mt8167Toprgu', 'WinPE ACPI package selects different service')
    return dict(selected_control_sets=selected, service=actual, kmdf=kmdf,
                hardware_id='ACPI/MOU0005', selected_INF='oem21.inf',
                active_base_package=active, configured_service=configured,
                load_success_proven=False, runtime_query_required_before_candidate_writes=True)


def inject(image, root, tools, work, env, manifest, digest):
    """既存boot-startサービスを保ち、WinPE側だけに署名済みSYSと証明書を配置する。"""
    root = root.resolve(strict=True)
    image = image.resolve(strict=True)
    tools = tools.resolve(strict=True)
    work = work.resolve()
    need(all(p.is_relative_to(root) for p in (image, tools, work)), 'WinPE lease work must stay in workspace')
    manifest = (root / manifest).resolve(strict=True)
    m, files = load_package(root, manifest, digest)
    work.mkdir(exist_ok=False)
    wimtool = str(tools / 'wimtools/wimlib-imagex')

    def run(args, **kw):
        return subprocess.run(args, check=True, env=env, **kw)

    run([wimtool, 'extract', str(image), '1', '/Windows/System32/config/SOFTWARE',
         '/Windows/System32/config/SYSTEM', '--dest-dir=' + str(work)])
    software = work / 'SOFTWARE'
    system = work / 'SYSTEM'
    system_sha = sha(system)
    binding = inspect_boot_binding(system, env)
    cert = files['mt8167-toprgu.cer'].read_bytes()
    blob = struct.pack('<III', 32, 1, len(cert)) + cert
    for store in ('Root', 'TrustedPublisher'):
        parent = '\\Microsoft\\SystemCertificates\\' + store + '\\Certificates'
        listing = run(['hivexsh', str(software)], input='cd ' + parent + '\nls\n',
                      text=True, capture_output=True)
        need(not listing.stderr, 'Existing certificate store unavailable')
        if THUMB not in listing.stdout.upper():
            cmds = 'cd ' + parent + '\nadd ' + THUMB + '\ncd ' + THUMB + '\nsetval 1\nBlob\nhex:3:' + blob.hex() + '\ncommit\n'
            result = run(['hivexsh', '-w', str(software)], input=cmds, text=True, capture_output=True)
            need(not result.stderr, 'Certificate addition failed')
        actual = run(['hivexget', str(software), parent + '\\' + THUMB, 'Blob'], capture_output=True).stdout
        need(actual == blob, 'WinPE signer blob mismatch')
    destinations = {'/Windows/System32/Drivers/mt8167-toprgu.sys': files['mt8167-toprgu.sys'],
                    '/Windows/System32/config/SOFTWARE': software}
    destinations.update({'/a04br3/winpe-lease/' + n: p for n, p in files.items()})
    commands = ''.join('add "' + str(p) + '" ' + dest + '\n' for dest, p in destinations.items())
    run([wimtool, 'update', str(image), '1'], input=commands, text=True)
    run([wimtool, 'verify', str(image)])
    # Extract real final bytes, including unchanged SYSTEM, instead of trusting update exit status.
    destinations['/Windows/System32/config/SYSTEM'] = system
    for i, (dest, original) in enumerate(destinations.items()):
        check = work / ('verify-' + str(i))
        check.mkdir()
        run([wimtool, 'extract', str(image), '1', dest, '--dest-dir=' + str(check)])
        leaves = [p for p in check.rglob('*') if p.is_file()]
        need(len(leaves) == 1 and sha(leaves[0]) == sha(original), 'Actual WinPE lease bytes differ: ' + dest)
    need(sha(system) == system_sha, 'WinPE SYSTEM changed')
    return dict(schema='WINPE_LEASE_EMBEDDED_V1', manifest_path=str(manifest.relative_to(root)),
                manifest_sha256=digest, source_commit=m['source_commit'], lease_seconds=1800,
                force_swrst=True, normal_windows_modified=False,
                SYS_sha256=sha(files['mt8167-toprgu.sys']), system_hive_sha256=system_sha,
                certificate_thumbprint=THUMB, boot_binding=binding, final_wim_files_extracted_and_verified=len(destinations),
                hardware_verified=False)

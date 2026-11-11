"""Explicit owned-filter root transition; no whole-service exemption."""
import hashlib
import re
from pathlib import Path


def require(ok, message):
    if not ok:
        raise ValueError(message)


def raw(value):
    data = bytes.fromhex(value['raw_hex'])
    require(value['bytes'] == len(data), 'root value length')
    return data


def strings(value, multi=False):
    data = raw(value)
    require(value['type'] == (7 if multi else 1) and len(data) % 2 == 0, 'root string type')
    text = data.decode('utf-16le')
    require(text.endswith('\0\0' if multi else '\0'), 'root string terminator')
    values = text[:-2].split('\0') if multi else [text[:-1]]
    require(all(v and '\0' not in v for v in values), 'empty/embedded string')
    return values


def owners(value):
    values = strings(value, True)
    require(0 < len(values) <= 32 and all(re.fullmatch(r'[A-Za-z0-9_.-]+\.inf', v, re.I) for v in values), 'owner basenames')
    require(len({v.lower() for v in values}) == len(values), 'duplicate owners')
    return values


def root_hash(values):
    require(len({k.upper() for k in values}) == len(values), 'duplicate root names')
    text = ''.join(k.upper() + '\0' + str(v['type']) + ':' + str(v['bytes']) + ':' +
                   hashlib.sha256(raw(v)).hexdigest() + '\n'
                   for k, v in sorted(values.items(), key=lambda row: row[0].upper()))
    return hashlib.sha256(text.encode('ascii')).hexdigest()


def candidate_display(inf, service):
    """Bounded supported INF syntax; fail closed on ambiguous/localized input."""
    data = inf.read_bytes()
    text = data.decode('utf-16' if data[:2] in (b'\xff\xfe', b'\xfe\xff') else 'utf-8-sig')
    sections = {}; section = None
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith(';'):
            continue
        if line.startswith('[') and line.endswith(']'):
            section = line[1:-1].lower()
            require(section not in sections, 'duplicate INF section')
            sections[section] = []
        else:
            require(section is not None, 'unsupported INF syntax')
            sections[section].append(tuple(x.strip() for x in line.split('=', 1)) if '=' in line else (line, None))
    require(not any(s.startswith('strings.') for s in sections), 'localized display needs explicit handling')
    matches = []
    for key, value in sections.get('install.nt.services', []):
        require(value is not None, 'unsupported service directive')
        fields = [x.strip() for x in value.split(',')]
        if key.lower() == 'addservice' and fields[0].lower() == service.lower():
            require(len(fields) == 3 and int(fields[1] or '0', 0) == 0, 'owned filter AddService only')
            matches.append(fields[2].lower())
    require(len(matches) == 1, 'one explicit candidate filter service')
    displays = [v for k, v in sections[matches[0]] if k.lower() == 'displayname']
    require(len(displays) == 1 and displays[0] is not None, 'one candidate display')
    display = displays[0]
    if re.fullmatch(r'%[A-Za-z0-9_]+%', display):
        token = display[1:-1].lower()
        found = [v for k, v in sections.get('strings', []) if k.lower() == token]
        require(len(found) == 1, 'one candidate display token')
        display = found[0]
    require(display.startswith('"') and display.endswith('"'), 'quoted literal display')
    display = display[1:-1]
    require(display and all(32 <= ord(c) < 127 and c not in '"%\\' for c in display), 'bounded display literal')
    return display


def bind(policy, root, refs, pin):
    rows = []; seen = set()
    for ref in refs:
        require(set(ref) == {'role', 'service', 'baseline_root'}, 'transition input fields')
        role, service = ref['role'], ref['service']
        require(role in policy['install_targets'] and re.fullmatch(r'[A-Za-z0-9_-]+', service), 'updated filter only')
        require(service.lower() not in seen, 'duplicate filter transition')
        seen.add(service.lower())
        baseline = pin(ref['baseline_root'])
        require(root_hash(baseline) == policy['service_preservation'][service]['root_sha256'], 'baseline full root binding')
        package = policy['packages'][role]
        inf = Path(root) / package['directory'] / package['names']['inf']
        require(hashlib.sha256(inf.read_bytes()).hexdigest() == package['hashes']['inf'], 'signed candidate INF bytes')
        display = candidate_display(inf, service)
        old_owners = owners(baseline['Owners'])
        old_display = strings(baseline['DisplayName'])[0]
        require(old_display == display or any(old_display.lower().startswith('@' + o.lower() + ',') and
                old_display.split(';', 1)[-1] == display for o in old_owners), 'baseline display refers to old owner and candidate text')
        expected = sorted({v.lower() for v in old_owners} | {inf.name.lower()})
        require(len(expected) <= 32, 'owner union capacity')
        rows.append(dict(service=service, target=policy['stage_order'].index(role), baseline_root=ref['baseline_root'],
                         display=display, owners=expected, before_display=baseline['DisplayName'], before_owners=baseline['Owners']))
    return rows


def validate_observed(baseline, observed, row, after_dif):
    """Offline fixture check; never loads a hive or changes runtime state."""
    if not after_dif:
        require(root_hash(observed) == root_hash(baseline), 'before-DIF strict root')
        return
    require(strings(observed['DisplayName']) == [row['display']], 'candidate display bytes')
    require({v.lower() for v in owners(observed['Owners'])} == set(row['owners']), 'exact owner union')
    normalized = dict(observed)
    for name in ('DisplayName', 'Owners'):
        normalized[name] = baseline[name]
    require(root_hash(normalized) == root_hash(baseline), 'other root values unchanged')


def generate(rows):
    if not rows:
        return ''
    def cstr(v):
        return 'L"' + v.replace('\\', '\\\\').replace('"', '\\"') + '"'
    def digest(v):
        return '{' + ','.join('0x' + hashlib.sha256(raw(v)).hexdigest()[i:i+2] for i in range(0, 64, 2)) + '}'
    out = ['#define A04_FILTER_ROOT_TRANSITION_COUNT ' + str(len(rows)) + 'u',
           'static const struct filter_root_transition filter_root_transitions[]={']
    for row in rows:
        out.append('{' + ','.join([cstr(row['service']), str(row['target']) + 'u', cstr(row['display']),
                   str(len(row['owners'])) + 'u', '{' + ','.join(map(cstr, row['owners'])) + '}',
                   str(row['before_display']['bytes']) + 'u', digest(row['before_display']),
                   str(row['before_owners']['bytes']) + 'u', digest(row['before_owners'])]) + '},')
    return '\n'.join(out + ['};']) + '\n'

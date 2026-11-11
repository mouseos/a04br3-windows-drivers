"""One bounded regression using saved actual227/229 proof; no hive/VM access."""
import copy
import hashlib
import json
from pathlib import Path
import sys

sys.dont_write_bytecode = True
repo = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(repo / 'tools/winpe'))
from filter_root_transition import bind, generate, root_hash, validate_observed

root = Path(sys.argv[1]).resolve()
logs = root / 'port/gsi/logs/ios-feasibility/windows-rt-wire378-controller-independent-review'
before_path = logs / 'clean-prepared227-Mt6392Keys-root.json'
before = json.loads(before_path.read_text())
proof = json.loads((logs / 'main-postDIF228-filter-root-proof.json').read_text())
after = copy.deepcopy(before)
for change in proof['changes']:
    assert before[change['name']] == change['before']
    after[change['name']] = change['after']
assert root_hash(before) == proof['before_root_sha256']
assert root_hash(after) == proof['after_root_sha256']
policy = json.loads((root / 'port/xnu/out/windows-rt-gpu378-finalize227/profile227-r2/peripheral-policy.json').read_text())
ref = dict(path=before_path.relative_to(root).as_posix(), sha256=hashlib.sha256(before_path.read_bytes()).hexdigest())
def pin(r):
    p = root / r['path']
    assert hashlib.sha256(p.read_bytes()).hexdigest() == r['sha256']
    return json.loads(p.read_text())
row, = bind(policy, root, [dict(role='keypad', service='Mt6392Keys', baseline_root=ref)], pin)
validate_observed(before, before, row, False)
validate_observed(before, after, row, True)
assert row['owners'] == ['mt8167-keypad.inf', 'oem138.inf']  # fixture result only
assert generate([row]).count('filter_root_transitions[]=') == 1

def rejects(value, after_dif=True):
    try:
        validate_observed(before, value, row, after_dif)
    except (ValueError, KeyError):
        return
    raise AssertionError('invalid transition admitted')

rejects(after, False)  # cannot use the allowance before DIF
rejects(before)  # post-DIF requires candidate state, not stale baseline
changed = copy.deepcopy(after); changed['Start']['raw_hex'] = '04000000'; rejects(changed)
changed = copy.deepcopy(after); del changed['ImagePath']; rejects(changed)
changed = copy.deepcopy(after); changed['Unexpected'] = before['Start']; rejects(changed)
changed = copy.deepcopy(after); changed['DisplayName'] = before['DisplayName']; rejects(changed)
def multi(values):
    b = ('\0'.join(values) + '\0\0').encode('utf-16le')
    return dict(type=7, bytes=len(b), raw_hex=b.hex())
for values in (['mt8167-keypad.inf'], ['mt8167-keypad.inf', 'oem138.inf', 'unknown.inf'],
               ['mt8167-keypad.inf', 'oem138.inf', 'oem138.inf']):
    changed = copy.deepcopy(after); changed['Owners'] = multi(values); rejects(changed)
changed = copy.deepcopy(after); changed['Owners'] = multi(['OEM138.INF', 'mt8167-keypad.inf'])
validate_observed(before, changed, row, True)  # set, not enumeration order

# A different published OEM number is learned from its own baseline, never fixed.
other_before = copy.deepcopy(before); other_before['Owners'] = multi(['oem987.inf'])
text = '@oem987.inf,%Device%;' + row['display']
b = (text + '\0').encode('utf-16le')
other_before['DisplayName'] = dict(type=1, bytes=len(b), raw_hex=b.hex())
other_policy = copy.deepcopy(policy)
other_policy['service_preservation']['Mt6392Keys']['root_sha256'] = root_hash(other_before)
other_row, = bind(other_policy, root, [dict(role='keypad', service='Mt6392Keys', baseline_root=ref)], lambda _: other_before)
assert other_row['owners'] == ['mt8167-keypad.inf', 'oem987.inf']
print(json.dumps(dict(actual_before_sha256=root_hash(before), actual_after_sha256=root_hash(after),
                     actual_fixture=True, negative_guards=True, OEM_number_not_fixed=True,
                     C_runtime_compiled=False, VM_device_operations=0)))

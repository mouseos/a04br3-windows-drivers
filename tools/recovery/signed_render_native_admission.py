"""署名renderのnative planに許す終了契約。hive/端末には触れない。"""
import argparse
import hashlib
import json
from pathlib import Path
from collection_reboot import CONFIG, policy_config, require


def admit(policy, capture):
    require(type(capture) is int and 235 <= capture <= 999, 'actual signed-render capture required')
    service = 'MT8167CombinedFollowup' + str(capture)
    require(policy.get('profile') == f'combined-features-fresh{capture}-signed-render-v1'
            and policy.get('service') == policy.get('data_directory') == service
            and type(policy.get('capture_selection', {}).get('capture_number')) is int
            and policy['capture_selection']['capture_number'] == capture, 'capture/profile/service mismatch')
    cc = policy.get('signed_render_contract', {})
    require(cc.get('schema') == 'SIGNED_RENDER_CAPTURE_TRANSITION_V1'
            and all(type(cc.get(k)) is int and cc[k] == v for k, v in (
                ('capture', capture), ('run', capture+1), ('post_capture', capture+2),
                ('render_update_mask', 1), ('install_mask', 1024), ('planned_reboots', 1)))
            and cc.get('boot_write_required') is False and cc.get('gpt_written') is False,
            'signed-render transition mismatch')
    require(policy.get('filter_root_transitions') in (None, []), 'no filter exemptions')
    hold = cc.get('terminal_windows_hold')
    if hold is True:
        require(all(policy.get(k) is None or policy.get(k) is False for k in (
            'unattended_collection_reboot', 'collection_reboot', 'vidpn_uart_snapshot')),
            'terminal hold cannot enable collection')
        return 'terminal-windows-held'
    require(hold is False and capture >= 237, 'explicit hold or new collection required')
    # policy_config is the build/engine authority: CONFIG, opt-in marker, snapshot,
    # exact phase1 client pair and zero first/deferred install masks are shared.
    require(policy.get('unattended_collection_reboot') is not None
            and policy_config(policy) == CONFIG, 'exact unattended collection required')
    require(all(type(policy.get(k)) is int and policy[k] == 0 for k in
                ('first_install_mask', 'deferred_unbound_mask'))
            and all(type(x.get('phase')) is int and x['phase'] == 1
                    for x in policy['diagnostic_clients']), 'typed phase1/zero masks required')
    marker = dict(schema='SIGNED_RENDER_UNATTENDED_COLLECTION_V1', capture=capture,
                  bounded_winpe_required=True, require_atomic_publication=True)
    # canonical JSON also rejects bool/int aliases that Python equality permits.
    require(json.dumps(policy['unattended_collection_reboot'], sort_keys=True) ==
            json.dumps(marker, sort_keys=True), 'typed unattended marker required')
    require(json.dumps(policy['collection_reboot'], sort_keys=True) ==
            json.dumps(CONFIG, sort_keys=True), 'typed exact collection config required')
    return 'unattended-collection'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--policy', type=Path, required=True)
    ap.add_argument('--sha256', required=True)
    ap.add_argument('--capture', type=int, required=True)
    a = ap.parse_args()
    raw = a.policy.read_bytes()
    require(hashlib.sha256(raw).hexdigest() == a.sha256, 'native plan bytes changed')
    print(admit(json.loads(raw.decode('utf-8-sig')), a.capture))


if __name__ == '__main__':
    main()

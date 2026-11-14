"""Check the spec, the generated Python module and the CLI against protocol/vectors.json."""
import json
import pathlib
import struct
import subprocess
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import airpods
import dongle_protocol as protocol

VECTORS = json.loads((ROOT / 'protocol/vectors.json').read_text(encoding='utf-8'))
SPEC = json.loads((ROOT / 'protocol/dongle-protocol.json').read_text(encoding='utf-8'))


def field(fields, data, name):
    offset, size = fields[name]
    raw = data[offset:offset + size]
    return int.from_bytes(raw, 'little') if size in (1, 2, 4) else raw


def names(table, mask):
    return sorted(name for name, bit in table.items() if mask & bit)


def setting_value(setting, value):
    if isinstance(value, int):
        return value
    return {item['name']: item['value'] for item in setting['domain']['values']}[value]


class ProtocolVectors(unittest.TestCase):
    def test_generated_files_are_current(self):
        subprocess.run([sys.executable, str(ROOT / 'tools/gen_protocol.py'), '--check'], check=True)

    def test_vectors_match_spec_version(self):
        self.assertEqual(VECTORS['protocol_version'], SPEC['protocol_version'])
        self.assertEqual(protocol.PROTOCOL_VERSION, SPEC['protocol_version'])

    def test_control_commands(self):
        opcodes = {value: name for name, value in protocol.OPCODES.items()}
        for vector in VECTORS['control_commands']:
            with self.subTest(vector['name']):
                data = bytes.fromhex(vector['hex'])
                valid = (len(data) == protocol.REPORT_SIZE and data[0] == protocol.CONTROL_REPORT_ID and
                         data[1:5] == protocol.MAGIC and data[5] == protocol.PROTOCOL_VERSION and
                         data[6] == protocol.REPORT_SIZE and
                         field(protocol.COMMAND_FIELDS, data, 'command_sequence') != 0 and
                         not any(field(protocol.COMMAND_FIELDS, data, 'reserved')) and
                         field(protocol.COMMAND_FIELDS, data, 'argument') in
                         protocol.OPCODE_ARGUMENTS.get(field(protocol.COMMAND_FIELDS, data, 'opcode'), ()))
                self.assertEqual(valid, vector['valid'])
                if valid:
                    expected = vector['fields']
                    self.assertEqual(opcodes[field(protocol.COMMAND_FIELDS, data, 'opcode')], expected['opcode'])
                    for name in ('connection_generation', 'command_sequence', 'argument'):
                        self.assertEqual(field(protocol.COMMAND_FIELDS, data, name), expected[name])
                    self.assertEqual(field(protocol.COMMAND_FIELDS, data, 'selected_address').hex(),
                                     expected['selected_address'])
        covered = {v['fields']['opcode'] for v in VECTORS['control_commands'] if v['valid']}
        self.assertEqual(covered, set(protocol.OPCODES))

    def test_control_status(self):
        for vector in VECTORS['control_status']:
            with self.subTest(vector['name']):
                data = bytes.fromhex(vector['hex'])
                expected = vector['fields']
                status = protocol.STATUS_FIELDS
                self.assertEqual(names(protocol.STATUS_FLAGS, field(status, data, 'flags')), sorted(expected['flags']))
                self.assertEqual(names(protocol.CAPABILITIES, field(status, data, 'capabilities')),
                                 sorted(expected['capabilities']))
                self.assertEqual(field(status, data, 'listening_mode'),
                                 protocol.LISTENING_MODES[expected['listening_mode']])
                self.assertEqual(field(status, data, 'output_codec'), protocol.OUTPUT_CODECS[expected['output_codec']])
                self.assertEqual(field(status, data, 'last_command_result'),
                                 protocol.RESULTS[expected['last_command_result']])
                self.assertEqual(list(field(status, data, 'battery_percent')), expected['battery_percent'])
                self.assertEqual(list(struct.unpack('<III', field(status, data, 'battery_ages_milliseconds'))),
                                 expected['battery_ages_milliseconds'])
                for name in ('connection_generation', 'charging_components', 'last_command_sequence',
                             'last_command_opcode', 'present_components', 'case_observed_components'):
                    self.assertEqual(field(status, data, name), expected[name], name)
                self.assertFalse(any(field(status, data, 'reserved')))
        published = {name for name, value in protocol.RESULTS.items()
                     if not next(r for r in SPEC['control']['results'] if r['name'] == name).get('internal')}
        self.assertEqual({v['fields']['last_command_result'] for v in VECTORS['control_status']}, published)

    def test_rpc_frames(self):
        operations = {value: name for name, value in protocol.RPC_OPERATIONS.items()}
        phases = {value: name for name, value in protocol.RPC_PHASES.items()}
        failures = set()
        for vector in VECTORS['rpc_frames']:
            with self.subTest(vector['name']):
                data = bytes.fromhex(vector['hex'])
                expected = vector['fields']
                rpc = protocol.RPC_FIELDS
                length = data[rpc['fragment_length'][0]]
                valid = (data[0] == protocol.RPC_REPORT_ID and data[1:5] == protocol.MAGIC and
                         data[5] == protocol.PROTOCOL_VERSION and data[6] == protocol.REPORT_SIZE and
                         length <= protocol.RPC_MAXIMUM_FRAGMENT_LENGTH and
                         field(rpc, data, 'response_phase') == 0 and
                         not any(data[rpc['payload'][0] + length:]))
                self.assertEqual(valid, vector['request_valid'])
                operation = field(rpc, data, 'operation')
                self.assertEqual(operations.get(operation, operation), expected['operation'])
                self.assertEqual(phases[field(rpc, data, 'response_phase')], expected['response_phase'])
                for name in ('connection_generation', 'transaction_sequence', 'total_length',
                             'fragment_offset', 'fragment_length'):
                    self.assertEqual(field(rpc, data, name), expected[name], name)
                payload = data[rpc['payload'][0]:rpc['payload'][0] + min(length, protocol.RPC_MAXIMUM_FRAGMENT_LENGTH)]
                self.assertEqual(payload.hex(), expected['payload'])
                if expected['response_phase'] == 'failed':
                    failures.add(payload[0])
        requested = {v['fields']['operation'] for v in VECTORS['rpc_frames'] if v['request_valid']}
        self.assertEqual(requested, set(protocol.RPC_OPERATIONS))
        self.assertEqual(failures, set(protocol.RPC_ERRORS.values()))

    def test_feature_payloads_match_cli_encoding(self):
        settings = {s['name']: s for s in SPEC['settings']}
        for vector in VECTORS['features']:
            with self.subTest(vector['name']):
                payload = bytes.fromhex(vector['payload'])
                self.assertEqual(payload[0], protocol.FEATURES[vector['feature']])
                if 'setting' not in vector:
                    continue
                setting = settings[vector['setting']]
                value = setting_value(setting, vector['value'])
                self.assertEqual(payload, bytes((protocol.FEATURES['write_setting'], setting['id'])) +
                                 struct.pack('<I', value))
                self.assertTrue(protocol.setting_value_valid(setting['id'], value))
                # The CLI produces the same value from its user-facing name.
                control, kind = airpods.SETTINGS[setting['cli']]
                self.assertEqual(control, setting['id'])
                self.assertEqual(airpods.encode(setting['cli'], airpods.decode(setting['cli'], value)), value)

    def test_setting_values(self):
        settings = {s['name']: s for s in SPEC['settings']}
        for vector in VECTORS['setting_values']:
            setting = settings[vector['setting']]
            for valid in (True, False):
                for value in vector['valid' if valid else 'invalid']:
                    with self.subTest(setting=setting['name'], value=value):
                        self.assertEqual(protocol.setting_value_valid(setting['id'], setting_value(setting, value)),
                                         valid)

    def test_settings_snapshots_parse_with_cli(self):
        for vector in VECTORS['settings_sessions']:
            with self.subTest(vector['name']):
                records = airpods.snapshot(bytes.fromhex(vector['snapshot']))
                extended = set(records) & protocol.EXTENDED_SETTINGS
                self.assertTrue(vector['extended'] or not extended)
                self.assertTrue(all(control in protocol.SETTING_DOMAINS for control in records))

    def test_phone_frames(self):
        fields = {f['name']: f['offset'] for f in SPEC['phone_transport']['fields']}
        lengths = {o['value']: o['length'] for o in SPEC['phone_transport']['operations']}

        def parse(data):
            requests = 0
            while data:
                header = data[:protocol.PHONE_HEADER_SIZE]
                if (len(header) < protocol.PHONE_HEADER_SIZE or header[:4] != protocol.MAGIC or
                        header[fields['version']] != protocol.PROTOCOL_VERSION or
                        header[fields['report_id']] not in (protocol.CONTROL_REPORT_ID, protocol.RPC_REPORT_ID) or
                        lengths.get(header[fields['operation']]) != header[fields['length']]):
                    return False, requests
                size = protocol.PHONE_HEADER_SIZE + header[fields['length']]
                report = data[protocol.PHONE_HEADER_SIZE:size]
                if report and (report[0] != header[fields['report_id']] or report[1:5] != protocol.MAGIC or
                               report[5] != protocol.PROTOCOL_VERSION or report[6] != protocol.REPORT_SIZE):
                    return False, requests
                requests += 1
                data = data[size:]
            return True, requests

        for vector in VECTORS['phone_frames']:
            with self.subTest(vector['name']):
                self.assertEqual(parse(bytes.fromhex(vector['hex'])), (vector['valid'], vector['requests']))

if __name__ == '__main__':
    unittest.main()

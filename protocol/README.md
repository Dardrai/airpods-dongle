# Dongle control protocol: spec and golden vectors

`dongle-protocol.json` is the single source of truth for the USB HID / phone
control protocol: report framing and offsets, command opcodes, result and RPC
error codes, typed feature IDs, cached report slots, the settings snapshot
layout, the AirPods setting catalog with value domains, the per-model
setting profiles, the request and response layouts of the typed features
(audio profile, performance, nearby finder, physical volume, volume
authority) and the USB identity and HID collection. It describes what the firmware does; where the prose in
`docs/USB-CONTROL.md` or `docs/CONTROL-EXTENSIONS.md` disagrees, the spec and
the firmware win.

## Conventions

- Plain JSON, no dependencies. Integers are decimal; wire integers are
  little-endian. Named lists are ordered arrays of `{"name", "value", ...}`
  objects so every language keeps their order.
- Every `name` is the canonical lower_snake identifier: whole words named for
  what the value means (`change_listening_mode`, `send_att_request`), no
  clipped abbreviations. Established acronyms (ANC, AACP, USB, RPC, ATT, PME,
  EN 352, AAC-ELD, BLE) stay. Generators derive each language's casing from it.
  The `cli` names and the value names inside `settings[].domain` are the
  user-facing strings and keep their kebab-case.
- `settings[].index` is the bit position in each profile's capability mask.
  Only append settings; never reorder or reuse an index.
- Profiles list their writable controls by setting name; the generator turns
  that into the 32-bit mask.
- `layouts` describe typed feature payloads. A response has `size` and usually
  `version` (byte 0); a request has `request_size` and starts with its feature
  byte. `groups` hold named values for a field of the same name (`flags: true`
  marks a bit set, normally read from the `flags` field); `constants` hold
  sentinels and limits. Request layouts reuse their response's groups.

## Regenerating

```
python3 tools/gen_protocol.py          # rewrite the committed outputs
python3 tools/gen_protocol.py --check  # fail if any output is stale (tests and CI run this)
python3 tools/gen_protocol.py --kotlin PATH [--kotlin-package NAME] [--kotlin-object NAME]
                                       # write the Kotlin object for another repository
```

Outputs (committed, so the firmware build needs no extra step):

| File | Contents |
| --- | --- |
| `src/tinyusb/dongle_protocol_spec.h` | named constants, setting value validator, write predicates |
| `src/btstack/dongle_settings_spec.h` | setting catalog and per-model profile table |
| `tools/dongle_protocol.py` | the same data for the Python tools and the CLI |
| `protocol/generated/DongleControlProtocol.kt` | Kotlin constants consumed by LibrePods Android |
| `protocol/generated/dongle_control_protocol.rs` | Rust constants consumed by NTPods |

The Kotlin binding uses package `me.kavishdevar.librepods.dongle` and object
`DongleControlProtocol`. Each spec section becomes a nested object, each `name` an UPPER_SNAKE
constant, field offsets end in `_OFFSET` and multi-byte sizes in `_SIZE`.

## Updating app clients

Export both bindings and the shared test vectors from this checkout:

```
python3 tools/gen_protocol.py
python3 tools/sync_clients.py --android /path/to/librepods/android --windows /path/to/ntpods
python3 tools/sync_clients.py --android /path/to/librepods/android --windows /path/to/ntpods --check
```

The export writes only the generated module, golden vectors, offline verifier and
`dongle-protocol.lock.json`. The lock records the specification/generator hashes
and exact exported text hashes. Both app CI workflows run
`tools/check_dongle_protocol.py` from their client root (Android's root is its
`android` directory). This detects edited or missing exports without requiring
network access or another repository. The source-side `--check` also detects a
client pinned to an older specification. Parsing and transport ownership stay in
the existing app modules; their tests decode these same vectors through production
code. Update all clients when changing the wire format.

## Golden vectors (`vectors.json`)

Concrete frames with their expected meaning, for every client implementation
to test against. Hex strings are raw bytes; enumerated values are spec names.

| Section | Each entry | Check |
| --- | --- | --- |
| `control_commands` | report-2 command `hex`, `valid`, decoded `fields` when valid | validate and decode |
| `control_status` | report-2 status `hex` and its decoded `fields` (one per published result code) | decode, or encode `fields` and compare bytes |
| `rpc_frames` | report-3 `hex`, `request_valid` (passes the request header check), decoded `fields` | validate and decode; covers every operation, phase and error code |
| `features` | typed feature `payload`; setting writes also give `setting` and `value` (name or integer) | encode and compare |
| `aacp_readback` | AirPods AACP packet `hex` applied to empty readback state, `expect`ed status fields | decode |
| `settings_sessions` | AACP `packets` with arrival times (ms), then the settings snapshot at `now` (`extended` selects `[2,1]`) | replay and compare bytes |
| `setting_values` | per setting, `valid` and `invalid` values | value-domain check |
| `phone_frames` | an RFCOMM byte stream, whether it is `valid`, and how many `requests` it carries | parse |
| `layouts` | a typed feature request or response `hex` for one `layout`, its `fields` (integer, value name, or list of flag names; omitted fields are zero) and, for some requests, `valid` | decode, or encode `fields` and compare bytes; validate requests |

Consumers in this repository: `tests/protocol_vectors_test.c` (production C
code; `tools/test.py` converts the JSON with `gen_protocol.py --vectors-c`) and
`tests/test_protocol_vectors.py` (generated Python module and CLI). The exported
vectors also exercise LibrePods Android and NTPods production parsers. Add new
cases to the JSON rather than to one language's test so all clients share them.

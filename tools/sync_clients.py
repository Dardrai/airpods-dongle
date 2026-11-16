#!/usr/bin/env python3
"""Export the canonical dongle bindings and test vectors to Android and Windows."""
import argparse
import json
from pathlib import Path
import sys

from check_client_protocol import digest

ROOT = Path(__file__).resolve().parents[1]
CLIENTS = {
    "android": {
        "protocol/generated/DongleControlProtocol.kt": "app/src/main/kotlin/me/kavishdevar/librepods/dongle/DongleControlProtocol.kt",
        "protocol/vectors.json": "app/src/test/resources/dongle-protocol-vectors.json",
    },
    "windows": {
        "protocol/generated/dongle_control_protocol.rs": "windows/daemon/src/dongle_control_protocol.rs",
        "protocol/vectors.json": "windows/daemon/tests/fixtures/dongle-protocol-vectors.json",
    },
}


def sync(kind, destination, check=False):
    import gen_protocol
    spec = gen_protocol.load()
    gen_protocol.check_spec(spec)
    # Refuse to export stale source files even if the client copy happens to match.
    for source, expected in gen_protocol.outputs(spec).items():
        if source.read_text(encoding="utf-8") != expected:
            raise ValueError(f"Stale generated firmware binding: {source.relative_to(ROOT)}")
    sources = dict(CLIENTS[kind])
    sources["tools/check_client_protocol.py"] = "tools/check_dongle_protocol.py"
    lock = {
        "version": 1,
        "repository": "https://github.com/Ripthulhu/airpods-dongle",
        "spec_sha256": digest(ROOT / "protocol/dongle-protocol.json"),
        "generator_sha256": digest(ROOT / "tools/gen_protocol.py"),
        "files": {target: digest(ROOT / source) for source, target in sources.items()},
    }
    exports = {target: (ROOT / source).read_text(encoding="utf-8") for source, target in sources.items()}
    exports["dongle-protocol.lock.json"] = json.dumps(lock, indent=2) + "\n"
    destination = destination.resolve()
    paths = {name: (destination / name).resolve() for name in exports}
    if any(not path.is_relative_to(destination) for path in paths.values()):
        raise ValueError("Protocol export path leaves the client directory")
    for name, text in exports.items():
        path = paths[name]
        if path.exists() and path.read_text(encoding="utf-8") == text:
            continue
        if check:
            raise ValueError(f"Stale {kind} protocol export: {name}")
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8", newline="\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--android", type=Path, help="LibrePods android directory")
    parser.add_argument("--windows", type=Path, help="NTPods repository directory")
    parser.add_argument("--check", action="store_true", help="verify without rewriting client files")
    args = parser.parse_args()
    if not args.android and not args.windows:
        parser.error("Specify --android, --windows, or both")
    for kind in CLIENTS:
        destination = getattr(args, kind)
        if destination:
            sync(kind, destination.resolve(), args.check)
    print("Dongle client exports verified" if args.check else "Dongle client exports updated")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)

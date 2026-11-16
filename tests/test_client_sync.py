import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import check_client_protocol
import sync_clients


class ClientSyncTest(unittest.TestCase):
    def test_both_clients_share_vectors_and_detect_modified_exports(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            for kind in sync_clients.CLIENTS:
                target = root / kind
                sync_clients.sync(kind, target)
                sync_clients.sync(kind, target, check=True)
                check_client_protocol.check(target)
                vectors = target / sync_clients.CLIENTS[kind]["protocol/vectors.json"]
                self.assertEqual(vectors.read_text(), (sync_clients.ROOT / "protocol/vectors.json").read_text())
                vectors.write_text("{}")
                with self.assertRaises(ValueError):
                    check_client_protocol.check(target)
                with self.assertRaises(ValueError):
                    sync_clients.sync(kind, target, check=True)

    def test_missing_export_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            target = pathlib.Path(directory)
            sync_clients.sync("android", target)
            (target / "tools/check_dongle_protocol.py").unlink()
            with self.assertRaises(OSError):
                check_client_protocol.check(target)

    def test_nested_link_is_rejected_before_any_export_is_written(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            target, outside = root / "client", root / "outside"
            target.mkdir()
            outside.mkdir()
            sentinel = outside / "check_dongle_protocol.py"
            sentinel.write_text("preserve")
            try:
                (target / "tools").symlink_to(outside, target_is_directory=True)
            except OSError as error:
                self.skipTest(f"Directory symlinks unavailable: {error}")
            with self.assertRaises(ValueError):
                sync_clients.sync("android", target)
            self.assertEqual(sentinel.read_text(), "preserve")
            self.assertFalse((target / "app").exists())


if __name__ == "__main__":
    unittest.main()

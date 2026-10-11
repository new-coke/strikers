"""Device updates retain recoverable PC copies without growing remote folders."""
import tempfile
import unittest
from pathlib import Path

import vita_workflow_device as device


class FakeFTP:
    def __init__(self, fail_partial=False, corrupt_readback=False):
        self.files = {"eboot.bin": b"original"}
        self.fail_partial = fail_partial
        self.corrupt_readback = corrupt_readback
        self.stores = 0

    def cwd(self, directory):
        pass

    def retrbinary(self, command, callback):
        callback(self.files[command.removeprefix("RETR ")])

    def storbinary(self, command, stream):
        self.stores += 1
        name = command.removeprefix("STOR ")
        data = stream.read()
        if self.fail_partial and self.stores == 1:
            self.files[name] = data[:3]
            raise OSError("interrupted transfer")
        self.files[name] = b"corrupt" if self.corrupt_readback and self.stores == 1 else data


class DeviceUpdateTest(unittest.TestCase):
    def test_success_keeps_only_live_remote_file_and_local_original(self):
        ftp = FakeFTP()
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory)
            result = device.promote(ftp, device.APP, "eboot.bin", b"candidate", out)
            self.assertEqual(ftp.files, {"eboot.bin": b"candidate"})
            self.assertEqual((out / "eboot.bin.before").read_bytes(), b"original")
            self.assertIsNone(result["remote_backup"])

    def test_partial_transfer_restores_original_without_remote_artifacts(self):
        ftp = FakeFTP(fail_partial=True)
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(OSError, "interrupted"):
                device.promote(ftp, device.APP, "eboot.bin", b"candidate", Path(directory))
            self.assertEqual(ftp.files, {"eboot.bin": b"original"})

    def test_bad_readback_restores_original(self):
        ftp = FakeFTP(corrupt_readback=True)
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(RuntimeError, "readback differs"):
                device.promote(ftp, device.APP, "eboot.bin", b"candidate", Path(directory))
            self.assertEqual(ftp.files, {"eboot.bin": b"original"})

    def test_existing_distinct_local_backup_is_preserved(self):
        ftp = FakeFTP()
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory)
            (out / "eboot.bin.before").write_bytes(b"other original")
            with self.assertRaisesRegex(RuntimeError, "different local backup"):
                device.promote(ftp, device.APP, "eboot.bin", b"candidate", out)
            self.assertEqual(ftp.stores, 0)
            self.assertEqual((out / "eboot.bin.before").read_bytes(), b"other original")


if __name__ == "__main__":
    unittest.main()

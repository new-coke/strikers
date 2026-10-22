"""Device updates retain recoverable PC copies without growing remote folders."""
import tempfile
import unittest
from pathlib import Path
from unittest.mock import Mock, call, patch

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


class DeviceCleanupTest(unittest.TestCase):
    def setUp(self):
        self.ftp = Mock(spec=["cwd", "voidcmd", "size", "delete", "quit"])
        self.name = "startup.log"
        self.checked = [call.cwd(device.DATA), call.voidcmd("TYPE I"), call.size(self.name)]

    def test_only_exact_size_absence_skips_delete(self):
        for response in ("550 File not found.", "550 The file doesn't exist."):
            with self.subTest(response=response):
                self.ftp.reset_mock()
                self.ftp.size.side_effect = device.error_perm(response)
                self.assertFalse(device.delete_if_present(self.ftp, device.DATA, self.name))
                self.assertEqual(self.ftp.mock_calls, self.checked)

    def test_present_file_including_empty_requires_successful_delete(self):
        for size in (0, 1024):
            with self.subTest(size=size):
                self.ftp.reset_mock()
                self.ftp.size.return_value = size
                self.assertTrue(device.delete_if_present(self.ftp, device.DATA, self.name))
                self.assertEqual(self.ftp.mock_calls, self.checked + [call.delete(self.name)])

    def test_delete_error_is_never_reclassified_as_absence(self):
        for response in ("550 Could not delete the file.", "550 The file doesn't exist."):
            with self.subTest(response=response):
                self.ftp.reset_mock()
                self.ftp.size.return_value = 12
                error = device.error_perm(response)
                self.ftp.delete.side_effect = error
                with self.assertRaises(device.error_perm) as raised:
                    device.delete_if_present(self.ftp, device.DATA, self.name)
                self.assertIs(raised.exception, error)
                self.assertEqual(self.ftp.mock_calls, self.checked + [call.delete(self.name)])

    def test_size_errors_do_not_authorize_delete(self):
        errors = [device.error_perm(response) for response in (
            "550 Could not delete the file.", "550 Could not allocate memory.",
            "550 File not found. Permission denied.")]
        errors.append(TimeoutError("SIZE timed out"))
        for error in errors:
            with self.subTest(error=str(error)):
                self.ftp.reset_mock()
                self.ftp.size.side_effect = error
                with self.assertRaises(type(error)) as raised:
                    device.delete_if_present(self.ftp, device.DATA, self.name)
                self.assertIs(raised.exception, error)
                self.assertEqual(self.ftp.mock_calls, self.checked)

    def test_unconfirmed_size_does_not_authorize_delete(self):
        self.ftp.size.return_value = None
        with self.assertRaisesRegex(RuntimeError, "SIZE did not confirm"):
            device.delete_if_present(self.ftp, device.DATA, self.name)
        self.assertEqual(self.ftp.mock_calls, self.checked)

    def test_type_error_does_not_authorize_absence(self):
        error = device.error_perm("550 The file doesn't exist.")
        self.ftp.voidcmd.side_effect = error
        with self.assertRaises(device.error_perm) as raised:
            device.delete_if_present(self.ftp, device.DATA, self.name)
        self.assertIs(raised.exception, error)
        self.assertEqual(self.ftp.mock_calls, self.checked[:2])


class DeviceUpdateTest(unittest.TestCase):
    def test_startup_service_error_does_not_send_game_input(self):
        class StartupFTP:
            size_calls = 0

            def cwd(self, directory):
                pass

            def voidcmd(self, command):
                return "200 Type set"

            def delete(self, name):
                raise AssertionError("The missing startup file must not be deleted")

            def size(self, name):
                self.size_calls += 1
                if self.size_calls == 1:
                    raise device.error_perm("550 The file doesn't exist.")
                raise device.error_perm("550 Could not allocate memory.")

        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory)
            (out / "strikers.ini").write_bytes(b"diagnostics=0\n")
            argv = ["vita_workflow_device.py", "run", "--host", "test.invalid",
                    "--out", str(out), "--baseline", str(out), "--label", "startup-error",
                    "--frames", "1", "--skip", "0", "--diagnostics", "--launch-confirm", "startup-log"]
            with patch("sys.argv", argv), patch.object(device, "connect", return_value=StartupFTP()), \
                 patch.object(device, "promote", return_value={}), patch.object(device, "read", return_value=b"self"), \
                 patch.object(device, "command", return_value="Launched.") as command, \
                 patch.object(device.time, "sleep"):
                with self.assertRaisesRegex(device.error_perm, "Could not allocate memory"):
                    device.main()
                self.assertEqual([call.args[1] for call in command.call_args_list],
                                 ["kill SMSVITA01", "launch SMSVITA01"])

    def test_startup_and_snapshot_cleanup_precede_verified_launch(self):
        for mode in ("startup-log", "capture-ready"):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as directory:
                out = Path(directory)
                (out / "strikers.ini").write_bytes(b"diagnostics=0\n")
                ftp = Mock(spec=["cwd", "voidcmd", "size", "delete", "quit"])
                # The unique startup marker is absent; an empty old snapshot
                # still exists and must be removed before the new run.
                ftp.size.side_effect = [device.error_perm("550 The file doesn't exist."), 0, 24, 24]
                ready = (b"# strikers-frame-capture-ready-v1\n"
                         b"requested,warmup_remaining,match_only,play_only,diagnostics,fps_overlay\n"
                         b"1,0,1,1,1,0\n")
                def download(connection, path, name):
                    return ready if name.endswith(".csv.ready") else b"self"
                argv = ["vita_workflow_device.py", "run", "--host", "test.invalid",
                        "--out", str(out), "--baseline", str(out), "--label", "cleanup",
                        "--frames", "1", "--skip", "0", "--diagnostics", "--visual",
                        "--launch-confirm", mode]
                with patch("sys.argv", argv), patch.object(device, "connect", return_value=ftp), \
                     patch.object(device, "promote", return_value={}), patch.object(device, "read", side_effect=download), \
                     patch.object(device, "command", return_value="Launched.") as command, \
                     patch.object(device.time, "sleep"), patch("builtins.print"):
                    device.main()
                startup = "cleanup.csv.ready" if mode == "capture-ready" else "cleanup.log"
                self.assertEqual(ftp.mock_calls[:7], [
                    call.cwd(device.DATA), call.voidcmd("TYPE I"), call.size(startup),
                    call.cwd(device.DATA), call.voidcmd("TYPE I"), call.size("debug_frame_3d_120.ppm"),
                    call.delete("debug_frame_3d_120.ppm")])
                self.assertEqual([request.args[1] for request in command.call_args_list],
                                 ["kill SMSVITA01", "launch SMSVITA01"])

    def test_cleanup_error_prevents_launch_and_game_input(self):
        for failing_name in ("cleanup-error.log", "debug_frame_3d_120.ppm"):
            with self.subTest(failing_name=failing_name), tempfile.TemporaryDirectory() as directory:
                out = Path(directory)
                (out / "strikers.ini").write_bytes(b"diagnostics=0\n")
                ftp = Mock(spec=["cwd", "voidcmd", "size", "delete", "quit"])
                def size(name):
                    if name == failing_name:
                        return 10
                    raise device.error_perm("550 The file doesn't exist.")
                ftp.size.side_effect = size
                ftp.delete.side_effect = device.error_perm("550 Could not delete the file.")
                argv = ["vita_workflow_device.py", "run", "--host", "test.invalid",
                        "--out", str(out), "--baseline", str(out), "--label", "cleanup-error",
                        "--frames", "1", "--skip", "0", "--diagnostics", "--visual",
                        "--launch-confirm", "startup-log"]
                with patch("sys.argv", argv), patch.object(device, "connect", return_value=ftp), \
                     patch.object(device, "promote", return_value={}), patch.object(device, "read", return_value=b"self"), \
                     patch.object(device, "command", return_value="Killed.") as command:
                    with self.assertRaisesRegex(device.error_perm, "Could not delete the file"):
                        device.main()
                ftp.delete.assert_called_once_with(failing_name)
                self.assertEqual([request.args[1] for request in command.call_args_list], ["kill SMSVITA01"])

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

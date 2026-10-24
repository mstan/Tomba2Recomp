"""Debug protocol framing: multiline replies finish before connection closure."""
import json
from pathlib import Path
import socket
import sys
import threading
import time
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from mario_probe import request


class ProbeProtocolTest(unittest.TestCase):
    def exchange(self, response):
        errors = []
        with socket.socket() as server:
            server.bind(("127.0.0.1", 0))
            server.listen(1)
            port = server.getsockname()[1]

            def serve():
                try:
                    with server.accept()[0] as client:
                        client.settimeout(5)
                        data = b""
                        while not data.endswith(b"\n"):
                            data += client.recv(4096)
                        if json.loads(data)["cmd"] != "diagnostic":
                            raise AssertionError("Unexpected request")
                        payload = json.dumps(response, indent=1).encode()
                        for i in range(0, len(payload), 3):
                            client.sendall(payload[i:i+3])
                        # The client must finish from JSON framing. Waiting for
                        # EOF would deadlock until this independent deadline.
                        try:
                            if client.recv(1):
                                raise AssertionError("Unexpected second request")
                        except (ConnectionResetError, ConnectionAbortedError):
                            pass  # Windows may reset instead of sending FIN.
                except Exception as error:
                    errors.append(error)

            thread = threading.Thread(target=serve, daemon=True)
            thread.start()
            started = time.monotonic()
            try:
                return request(port, "diagnostic")
            finally:
                elapsed = time.monotonic() - started
                thread.join(6)
                self.assertFalse(thread.is_alive(), "Reply parsing waited for EOF")
                self.assertEqual(errors, [])
                self.assertLess(elapsed, 2, "Complete JSON was not returned promptly")

    def test_fragmented_multiline_nested_reply(self):
        expected = {"ok": True, "entries": [{"text": 'quoted "} [ \\" and newline\n',
                                              "bytes": [1, 2, 3]}, {}]}
        self.assertEqual(self.exchange(expected), expected)

    def test_failed_reply_is_reported(self):
        with self.assertRaisesRegex(RuntimeError, "fixture refused"):
            self.exchange({"ok": False, "error": "fixture refused"})


if __name__ == "__main__":
    unittest.main()

"""Verify virtual telemetry against the real Lambda source and loopback mTLS."""
from contextlib import suppress
import importlib.util
import json
import os
from pathlib import Path
import queue
import shutil
import socket
import ssl
import struct
import subprocess
import tempfile
import threading
import unittest

from test_aws_integration import NOW, ROOT

SPEC = importlib.util.spec_from_file_location("telemetry", ROOT / "aws/virtual_device/telemetry.py")
telemetry = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(telemetry)
THING = "chapa-inteligente-9001"


def read_packet(channel):
    def read(size):
        data = bytearray()
        while len(data) < size:
            part = channel.recv(size - len(data))
            if not part:
                raise EOFError("MQTT peer closed")
            data.extend(part)
        return bytes(data)

    header = read(1)[0]
    size = 0
    for factor in (1, 128, 16384, 2097152):
        digit = read(1)[0]
        size += (digit & 127) * factor
        if digit < 128:
            return header, read(size)
    raise ValueError("Invalid MQTT packet length")


def send_packet(channel, header, body):
    remaining = len(body)
    length = bytearray()
    while remaining >= 128:
        length.append((remaining % 128) | 128)
        remaining //= 128
    length.append(remaining)
    channel.sendall(bytes([header]) + length + body)


def send_message(channel, topic, document):
    name = topic.encode()
    body = struct.pack("!H", len(name)) + name + json.dumps(document).encode()
    send_packet(channel, 0x30, body)


def broker(listener, context, received):
    """Script the five MQTT response topics used by the probe."""
    try:
        transport, _ = listener.accept()
        transport.settimeout(8)
        with transport, context.wrap_socket(transport, server_side=True) as channel:
            header, body = read_packet(channel)
            assert header == 0x10 and body.endswith(THING.encode())
            send_packet(channel, 0x20, b"\x00\x00")
            header, body = read_packet(channel)
            assert header == 0x82
            subscriptions = body[2:]
            prefix = f"$aws/things/{THING}/shadow"
            topics = []
            while subscriptions:
                size, = struct.unpack("!H", subscriptions[:2])
                topics.append(subscriptions[2:2 + size].decode())
                subscriptions = subscriptions[3 + size:]
            assert set(topics) == {f"{prefix}/{topic}" for topic in telemetry.RESPONSES}
            send_packet(channel, 0x90, body[:2] + bytes([1] * len(topics)))
            for operation in ("get", "update", "events"):
                header, body = read_packet(channel)
                assert header == 0x32
                size, = struct.unpack("!H", body[:2])
                topic = body[2:2 + size].decode()
                packet_id = body[2 + size:4 + size]
                payload = body[4 + size:]
                send_packet(channel, 0x40, packet_id)
                if operation == "get":
                    assert topic == f"{prefix}/get" and payload == b""
                    send_message(channel, f"{prefix}/update/delta", {
                        "version": 4, "state": {"modo": "manual", "autocierre_s": 21}
                    })
                    send_message(channel, f"{prefix}/get/accepted", {
                        "version": 3, "state": {"desired": {"modo": "automatico", "autocierre_s": 10}},
                    })
                elif operation == "update":
                    assert topic == f"{prefix}/update"
                    data = json.loads(payload)
                    received.put(("shadow", data))
                    send_message(channel, f"{prefix}/update/accepted", {"clientToken": data["clientToken"]})
                else:
                    assert topic == f"chapa/{THING}/events"
                    received.put(("event", json.loads(payload)))
            with suppress(EOFError, OSError):
                read_packet(channel)
    except Exception as error:
        received.put(("error", error))


def make_test_certificates(openssl, folder):
    def invoke(*args):
        subprocess.run([openssl, *args], cwd=folder, check=True, capture_output=True, timeout=15)

    invoke("req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "2",
           "-subj", "/CN=Local Probe CA", "-keyout", "ca.key", "-out", "ca.crt",
           "-addext", "basicConstraints=critical,CA:TRUE",
           "-addext", "keyUsage=critical,keyCertSign,cRLSign",
           "-addext", "subjectKeyIdentifier=hash")
    for name, extensions in (("server", "subjectAltName=DNS:localhost,IP:127.0.0.1\nextendedKeyUsage=serverAuth"),
                             ("client", "extendedKeyUsage=clientAuth")):
        Path(folder, "cert-extensions.txt").write_text(
            "basicConstraints=critical,CA:FALSE\n"
            "keyUsage=critical,digitalSignature,keyEncipherment\n"
            "authorityKeyIdentifier=keyid\n" + extensions, encoding="ascii"
        )
        invoke("req", "-newkey", "rsa:2048", "-nodes", "-subj", f"/CN={name}",
               "-keyout", f"{name}.key", "-out", f"{name}.csr")
        invoke("x509", "-req", "-in", f"{name}.csr", "-CA", "ca.crt", "-CAkey", "ca.key",
               "-CAcreateserial", "-days", "2", "-extfile", "cert-extensions.txt", "-out", f"{name}.crt")


class VirtualProbeTests(unittest.TestCase):
    def test_target_requires_isolation_and_valid_aws_port(self):
        target = {"thingName": THING, "endpoint": "test-ats.iot.us-east-2.amazonaws.com", "port": 8883}
        with self.assertRaises(ValueError):
            telemetry.validate_target(target, False)
        telemetry.validate_target(target, True)
        for changes in ({"thingName": "chapa-inteligente-01"}, {"endpoint": "example.com"}, {"port": 1883}):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                telemetry.validate_target({**target, **changes}, True)

    def test_delta_applies_only_valid_configuration_and_ignores_motion(self):
        device = telemetry.VirtualDevice(THING, clock=lambda: NOW)
        device.receive(f"{device.shadow}/get/accepted", {
            "version": 5,
            "state": {"desired": {"modo": "manual", "autocierre_s": 12}},
        })
        for version, state in (
            (4, {"modo": "bloqueado", "autocierre_s": 20}),
            (6, {"modo": [], "autocierre_s": True, "orden": {"accion": "desbloquear"}}),
            (7, {"autocierre_s": 121, "cerradura": "desbloqueada"}),
        ):
            device.receive(f"{device.shadow}/update/delta", {"version": version, "state": state})
        report = device.report()["state"]
        self.assertEqual(report, {"reported": {
            "puerta": "cerrada", "cerradura": "desconocida",
            "modo": "manual", "autocierre_s": 12, "reportado_ms": NOW,
        }})

    def test_report_is_accepted_only_with_matching_token(self):
        device = telemetry.VirtualDevice(THING)
        topic = f"{device.shadow}/update/accepted"
        device.receive(topic, {"clientToken": "other"})
        self.assertFalse(device.report_accepted)
        device.receive(topic, {"clientToken": device.report_token})
        self.assertTrue(device.report_accepted)

    def test_loopback_mutual_tls_and_mqtt(self):
        if importlib.util.find_spec("paho") is None:
            self.skipTest("Install paho-mqtt from aws/virtual_device/requirements.txt")
        openssl = os.environ.get("OPENSSL") or shutil.which("openssl")
        if not openssl:
            self.skipTest("Set OPENSSL to an OpenSSL executable")
        with tempfile.TemporaryDirectory() as folder, socket.socket() as listener:
            make_test_certificates(openssl, folder)
            path = Path(folder)
            context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            context.minimum_version = ssl.TLSVersion.TLSv1_2
            context.load_cert_chain(path / "server.crt", path / "server.key")
            context.load_verify_locations(path / "ca.crt")
            context.verify_mode = ssl.CERT_REQUIRED
            received = queue.Queue()
            listener.bind(("127.0.0.1", 0))
            listener.listen()
            listener.settimeout(8)
            thread = threading.Thread(target=broker, args=(listener, context, received), daemon=True)
            thread.start()
            try:
                result = telemetry.run_probe({
                    "thingName": THING, "endpoint": "127.0.0.1", "port": listener.getsockname()[1],
                    "caFile": str(path / "ca.crt"), "certificateFile": str(path / "client.crt"),
                    "privateKeyFile": str(path / "client.key"),
                })
            finally:
                thread.join(timeout=10)
            self.assertFalse(thread.is_alive(), "Local broker did not finish")
            messages = {}
            while not received.empty():
                name, value = received.get_nowait()
                if name == "error":
                    raise value
                messages[name] = value
            self.assertEqual(result["mode"], "manual")
            self.assertEqual(result["autoLockSeconds"], 21)
            self.assertTrue(result["shadowAccepted"] and result["eventBrokerAcknowledged"])
            self.assertEqual(messages["event"]["eventId"], result["eventId"])
            self.assertEqual(messages["shadow"]["state"]["reported"]["autocierre_s"], 21)
            self.assertNotIn("desired", messages["shadow"]["state"])
            self.assertEqual(messages["shadow"]["state"]["reported"]["cerradura"], "desconocida")

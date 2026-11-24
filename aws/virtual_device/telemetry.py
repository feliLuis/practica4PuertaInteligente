"""Run one isolated MQTT/TLS telemetry probe with no actuator interface."""
import argparse
import json
import re
import ssl
import time
from pathlib import Path
from uuid import uuid4

LAB_THING = re.compile(r"chapa-inteligente-9[0-9]{3}")
AWS_ENDPOINT = re.compile(r"[a-z0-9]+-ats\.iot\.[a-z0-9-]+\.amazonaws\.com")
MODES = {"manual", "automatico", "bloqueado"}
RESPONSES = ("get/accepted", "get/rejected", "update/delta", "update/accepted", "update/rejected")


def now_ms():
    return time.time_ns() // 1_000_000


def validate_target(config, allow_aws):
    if not LAB_THING.fullmatch(config["thingName"]):
        raise ValueError("Use a dedicated virtual Thing named chapa-inteligente-9xxx")
    host, port = config["endpoint"], config["port"]
    if type(port) is not int or not 1 <= port <= 65535:
        raise ValueError("Invalid MQTT port")
    if host not in {"localhost", "127.0.0.1", "::1"}:
        if not allow_aws or not AWS_ENDPOINT.fullmatch(host) or port != 8883:
            raise ValueError("Use loopback or explicitly select an isolated AWS ATS endpoint on port 8883")


def tls_context(config):
    context = ssl.create_default_context(cafile=config["caFile"])
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    context.load_cert_chain(config["certificateFile"], config["privateKeyFile"])
    return context


class VirtualDevice:
    def __init__(self, thing, clock=now_ms):
        self.clock = clock
        self.shadow = f"$aws/things/{thing}/shadow"
        self.events = f"chapa/{thing}/events"
        self.mode = "automatico"
        self.autolock = 10
        self.version = -1
        self.snapshot_received = False
        self.report_accepted = False
        self.pending_delta = None
        self.report_token = uuid4().hex

    def apply_config(self, state, version):
        if not isinstance(state, dict) or type(version) is not int or version < self.version:
            return
        mode = state.get("modo")
        seconds = state.get("autocierre_s")
        if isinstance(mode, str) and mode in MODES:
            self.mode = mode
        if type(seconds) is int and 5 <= seconds <= 120:
            self.autolock = seconds
        self.version = version

    def receive(self, topic, message):
        if not topic.startswith(self.shadow + "/") or not isinstance(message, dict):
            return
        suffix = topic[len(self.shadow) + 1:]
        token = message.get("clientToken")
        version = message.get("version")
        if suffix == "update/delta" and type(version) is int:
            if self.snapshot_received:
                self.apply_config(message.get("state"), version)
            elif self.pending_delta is None or version >= self.pending_delta["version"]:
                self.pending_delta = message
        elif suffix in {"get/accepted", "get/rejected"} and not self.snapshot_received:
            if suffix == "get/accepted":
                state = message.get("state", {})
                if not isinstance(state, dict) or type(version) is not int:
                    raise RuntimeError("Malformed Shadow snapshot")
                self.apply_config(state.get("desired", {}), version)
            elif message.get("code") != 404:
                raise RuntimeError(f"Shadow GET rejected (code {message.get('code')})")
            self.snapshot_received = True
            if self.pending_delta:
                self.apply_config(self.pending_delta.get("state"), self.pending_delta["version"])
                self.pending_delta = None
        elif suffix in {"update/accepted", "update/rejected"} and token == self.report_token:
            if suffix == "update/rejected":
                raise RuntimeError(f"Shadow UPDATE rejected (code {message.get('code')})")
            self.report_accepted = True

    def report(self):
        return {
            "state": {"reported": {
                "puerta": "cerrada", "cerradura": "desconocida",
                "modo": self.mode, "autocierre_s": self.autolock, "reportado_ms": self.clock(),
            }},
            "clientToken": self.report_token,
        }

    def reading(self):
        return {
            "eventId": f"{self.clock()}#{uuid4().hex}",
            "tipo": "lectura", "origen": "edge", "resultado": "ejecutado", "puerta": "cerrada",
        }


def run_probe(config, allow_aws=False):
    validate_target(config, allow_aws)
    import paho.mqtt.client as mqtt

    device = VirtualDevice(config["thingName"])
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=config["thingName"],
                         protocol=mqtt.MQTTv311, reconnect_on_failure=False)
    client.tls_set_context(tls_context(config))
    failure = None

    def on_connect(connection, userdata, flags, reason, properties):
        if reason.is_failure:
            raise RuntimeError(f"MQTT connection refused: {reason}")
        result, _ = connection.subscribe([(f"{device.shadow}/{suffix}", 1) for suffix in RESPONSES])
        if result != mqtt.MQTT_ERR_SUCCESS:
            raise RuntimeError(f"MQTT subscribe failed: {result}")

    def send(topic, payload):
        result = client.publish(topic, None if payload is None else json.dumps(payload), qos=1)
        if result.rc != mqtt.MQTT_ERR_SUCCESS:
            raise RuntimeError(f"MQTT publish failed: {result.rc}")
        return result

    def on_subscribe(connection, userdata, mid, reasons, properties):
        if len(reasons) != len(RESPONSES) or any(reason.is_failure for reason in reasons):
            raise RuntimeError("MQTT subscriptions refused")
        send(f"{device.shadow}/get", None)

    def on_message(connection, userdata, message):
        try:
            payload = json.loads(message.payload)
        except (UnicodeDecodeError, json.JSONDecodeError):
            return
        device.receive(message.topic, payload)

    def on_disconnect(connection, userdata, disconnect_flags, reason, properties):
        nonlocal failure
        if reason.is_failure:
            failure = str(reason)

    client.on_connect = on_connect
    client.on_subscribe = on_subscribe
    client.on_message = on_message
    client.on_disconnect = on_disconnect

    def wait_for(condition, description):
        deadline = time.monotonic() + 12
        while not condition():
            if time.monotonic() >= deadline:
                raise TimeoutError(f"Timed out waiting for {description}")
            status = client.loop(timeout=0.25)
            if failure or status != mqtt.MQTT_ERR_SUCCESS:
                raise RuntimeError(f"MQTT connection failed: {failure or status}")

    try:
        client.connect(config["endpoint"], config["port"], keepalive=20)
        wait_for(lambda: device.snapshot_received, "Shadow snapshot")
        send(f"{device.shadow}/update", device.report())
        wait_for(lambda: device.report_accepted, "Shadow report acceptance")
        event = device.reading()
        publication = send(device.events, event)
        wait_for(publication.is_published, "telemetry PUBACK")
        return {
            "thingName": config["thingName"], "shadowAccepted": True,
            "eventBrokerAcknowledged": True, "eventId": event["eventId"],
            "mode": device.mode, "autoLockSeconds": device.autolock,
        }
    finally:
        client.disconnect()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("config", type=Path)
    parser.add_argument("--allow-isolated-aws", action="store_true",
                        help="Allow writes to a separately provisioned AWS test project")
    args = parser.parse_args()
    try:
        config = json.loads(args.config.read_text(encoding="utf-8-sig"))
        result = run_probe(config, allow_aws=args.allow_isolated_aws)
    except (ImportError, KeyError, ValueError, TypeError, OSError, RuntimeError) as error:
        parser.exit(1, f"{error}\n")
    print(json.dumps(result))


if __name__ == "__main__":
    main()

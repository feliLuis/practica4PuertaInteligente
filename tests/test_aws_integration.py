"""Local contract tests. All AWS clients and the physical lock are replaced."""
import copy
import importlib.util
import io
import json
from pathlib import Path
import sys
import types
import unittest
from uuid import uuid4
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / "aws" / "config"
MODEL = ROOT / "aws" / "alexa" / "interaction-model" / "es-MX.json"
THING = "chapa-inteligente-01"
OTHER_THING = "chapa-inteligente-02"
NOW = 1_795_550_000_000


class ClientError(Exception):
    def __init__(self, code):
        super().__init__(code)
        self.response = {"Error": {"Code": code}}


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def import_handler(name):
    path = ROOT / "aws" / "lambda" / name / "lambda_function.py"
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class HistoryTable:
    def __init__(self):
        self.items = {}
        self.error = None

    def put_item(self, *, Item, ConditionExpression):
        if self.error:
            raise self.error
        assert ConditionExpression == "attribute_not_exists(eventKey)"
        key = Item["thingName"], Item["eventKey"]
        if key in self.items:
            raise ClientError("ConditionalCheckFailedException")
        self.items[key] = copy.deepcopy(Item)


class BindingsTable:
    def __init__(self):
        self.items = {"user-1": {"userId": "user-1", "thingName": THING}}

    def get_item(self, *, Key, ConsistentRead):
        item = self.items.get(Key["userId"])
        return {"Item": copy.deepcopy(item)} if item else {}

    def put_item(self, *, Item):
        self.items[Item["userId"]] = copy.deepcopy(Item)

    def delete_item(self, *, Key):
        self.items.pop(Key["userId"], None)


class SimulatedCloud:
    def __init__(self):
        self.history = HistoryTable()
        self.bindings = BindingsTable()
        self.documents = {
            name: {"version": 1, "state": {"desired": {}, "reported": {
                "puerta": "cerrada", "cerradura": "bloqueada",
                "modo": "automatico", "autocierre_s": 10, "reportado_ms": NOW,
            }}}
            for name in (THING, OTHER_THING)
        }
        self.updates = []
        self.force_conflict = False

    def Table(self, name):
        return {"AlexaThingBindings": self.bindings, "ChapaHistory": self.history}[name]

    def describe_endpoint(self, *, endpointType):
        assert endpointType == "iot:Data-ATS"
        return {"endpointAddress": "unused.invalid"}

    def describe_thing(self, *, thingName):
        if thingName not in self.documents:
            raise ClientError("ResourceNotFoundException")
        return {"thingName": thingName}

    def get_thing_shadow(self, *, thingName):
        if thingName not in self.documents:
            raise ClientError("ResourceNotFoundException")
        return {"payload": io.BytesIO(json.dumps(self.documents[thingName]).encode())}

    def update_thing_shadow(self, *, thingName, payload):
        patch_data = json.loads(payload)
        doc = self.documents[thingName]
        if self.force_conflict or patch_data.get("version", doc["version"]) != doc["version"]:
            raise ClientError("ConflictException")
        for section, changes in patch_data["state"].items():
            for key, value in changes.items():
                if value is None:
                    doc["state"][section].pop(key, None)
                else:
                    doc["state"][section][key] = value
        doc["version"] += 1
        self.updates.append((thingName, patch_data))

    def ingest(self, handler, topic, message, ts=NOW):
        sql = read_json(CONFIG / "iot-rule.proposed.json")["sql"]
        select, topic_filter = sql.removeprefix("SELECT ").split(" FROM ")
        head, name, tail = topic.split("/")
        assert topic_filter == "'chapa/+/events'" and (head, tail) == ("chapa", "events")
        assert select.startswith("topic(2) AS thingName, timestamp() AS ts, ")
        fields = select.split(", ")[2:]
        data = {key: message[key] for key in fields if key in message}
        return handler({"thingName": name, "ts": ts, **data}, None)


class VirtualLock:
    """Model the proposed Shadow consumer, including local interlocks."""
    def __init__(self, cloud, history_handler):
        self.cloud = cloud
        self.history_handler = history_handler
        self.moves = []
        self.acknowledged = set()
        self.online = True

    def reconcile(self, now=NOW):
        if not self.online:
            return
        doc = self.cloud.documents[THING]
        state = doc["state"]
        desired = state["desired"]
        reported = state["reported"]
        report = {"reportado_ms": now}
        for key in ("modo", "autocierre_s"):
            if key in desired:
                report[key] = desired[key]
        pending = desired.get("orden")
        if pending and pending["id"] not in self.acknowledged:
            action = pending["accion"]
            mode = report.get("modo", reported["modo"])
            fresh = now - reported["reportado_ms"] <= 30_000
            target = {"desbloquear": "desbloqueada", "bloquear": "bloqueada"}[action]
            allowed = now <= pending["vence_ms"] and (
                (action == "desbloquear" and mode != "bloqueado")
                or (action == "bloquear" and fresh and reported["puerta"] == "cerrada")
            )
            if allowed and target != reported["cerradura"]:
                self.moves.append(target)
                report["cerradura"] = target
                self.cloud.ingest(self.history_handler, f"chapa/{THING}/events", {
                    "eventId": f"{now}#{uuid4().hex}", "origen": "voz", "resultado": "ejecutado",
                    "tipo": "bloqueo" if action == "bloquear" else "desbloqueo", "cerradura": target,
                }, now)
            report["ordenResultado"] = {
                "id": pending["id"], "resultado": "ejecutado" if allowed else "rechazado",
            }
            self.acknowledged.add(pending["id"])
        self.cloud.update_thing_shadow(thingName=THING, payload=json.dumps({
            "version": doc["version"], "state": {"reported": report, "desired": {"orden": None}},
        }).encode())


class LocalIntegration(unittest.TestCase):
    def setUp(self):
        self.cloud = SimulatedCloud()
        boto = types.ModuleType("boto3")
        boto.client = lambda *args, **kwargs: self.cloud
        boto.resource = lambda *args, **kwargs: self.cloud
        core = types.ModuleType("botocore")
        exceptions = types.ModuleType("botocore.exceptions")
        exceptions.ClientError = ClientError
        with patch.dict(sys.modules, {"boto3": boto, "botocore": core, "botocore.exceptions": exceptions}):
            self.history = import_handler("chapa-almacenar-evento")
            self.alexa = import_handler("chapa-skill-alexa")
        self.alexa.now_ms = lambda: NOW
        self.reported = self.cloud.documents[THING]["state"]["reported"]
        self.desired = self.cloud.documents[THING]["state"]["desired"]
        self.lock = VirtualLock(self.cloud, self.history.lambda_handler)

    def invoke(self, intent, slots=None, user="user-1", request_id="request-1"):
        result = self.alexa.lambda_handler({
            "request": {"type": "IntentRequest", "requestId": request_id,
                        "intent": {"name": intent, "slots": slots or {}}},
            "context": {"System": {"user": {"userId": user}}},
        }, None)
        return result["response"]["outputSpeech"]["text"]

    def publish(self, message, ts=NOW):
        return self.cloud.ingest(self.history.lambda_handler, f"chapa/{THING}/events", message, ts)

    def sample(self, **overrides):
        return {"eventId": f"{NOW - 1}#{'a' * 32}", "tipo": "lectura",
                "origen": "edge", "resultado": "ejecutado", "puerta": "cerrada", **overrides}

    def test_original_model_matches_handlers_and_slots(self):
        model = read_json(MODEL)["interactionModel"]["languageModel"]
        self.assertEqual(model["invocationName"], "chapa inteligente")
        intents = {item["name"]: item for item in model["intents"]}
        expected_slots = {"VincularIntent": ("numeroSerie", "AMAZON.NUMBER"),
                          "CambiarAutocierreIntent": ("segundos", "AMAZON.NUMBER"),
                          "CambiarModoIntent": ("modo", "ModoOperacion")}
        for name, spec in expected_slots.items():
            self.assertEqual([(s["name"], s["type"]) for s in intents[name]["slots"]], [spec])
        for value in model["types"][0]["values"]:
            with self.subTest(value=value):
                mode = value["id"]
                slot = {"modo": {"value": value["name"]["value"], "resolutions": {
                    "resolutionsPerAuthority": [{"status": {"code": "ER_SUCCESS_MATCH"},
                                                  "values": [{"value": {"id": mode}}]}],
                }}}
                self.assertIn("Solicité", self.invoke("CambiarModoIntent", slot))
                self.assertEqual(self.desired["modo"], mode)

    def test_numeric_autolock_bounds_and_invalid_values(self):
        for value in ("4", "121", "120.9", "nan", "1e1", "", True, None):
            with self.subTest(value=value):
                self.assertIn("entero", self.invoke("CambiarAutocierreIntent", {"segundos": {"value": value}}))
        self.assertFalse(self.cloud.updates)
        for value in ("5", "120"):
            self.assertIn("Solicité", self.invoke("CambiarAutocierreIntent", {"segundos": {"value": value}}))
            self.assertEqual(self.desired["autocierre_s"], int(value))

    def test_bind_replace_validate_delete_and_isolate(self):
        self.assertIn("Aún no tienes", self.invoke("EstadoPuertaIntent", user="user-2"))
        self.assertIn("entero", self.invoke("VincularIntent", {"numeroSerie": {"value": "1.9"}}))
        self.assertIn("No encontré", self.invoke("VincularIntent", {"numeroSerie": {"value": "99"}}))
        self.assertEqual(self.cloud.bindings.items["user-1"]["thingName"], THING)
        self.assertIn("vinculada", self.invoke("VincularIntent", {"numeroSerie": {"value": "2"}}, user="user-2"))
        self.cloud.documents[OTHER_THING]["state"]["reported"]["puerta"] = "abierta"
        self.assertIn("abierta", self.invoke("EstadoPuertaIntent", user="user-2"))
        self.assertIn("cerrada", self.invoke("EstadoPuertaIntent"))
        self.assertIn("Reemplacé", self.invoke("VincularIntent", {"numeroSerie": {"value": "2"}}))
        self.assertIn("desvinculé", self.invoke("DesvincularIntent"))
        self.assertNotIn("user-1", self.cloud.bindings.items)
        self.assertIn("user-2", self.cloud.bindings.items)

    def test_fresh_queries_and_unknown_lock(self):
        self.assertIn("cerrada", self.invoke("EstadoPuertaIntent"))
        self.assertIn("bloqueada", self.invoke("EstadoCerraduraIntent"))
        self.assertIn("automático", self.invoke("ConsultarModoIntent"))
        self.assertIn("10 segundos", self.invoke("ConsultarAutocierreIntent"))
        self.reported["cerradura"] = "desconocida"
        self.assertIn("no tengo", self.invoke("EstadoCerraduraIntent"))

    def test_absent_open_or_unknown_door_prevents_lock(self):
        for value in (None, "desconocida", "abierta"):
            with self.subTest(value=value):
                self.reported["puerta"] = value
                self.assertIn("lectura reciente", self.invoke("CerrarIntent"))
        self.assertFalse(self.cloud.updates)

    def test_stale_future_or_legacy_snapshot_prevents_motion(self):
        for stamp in (None, NOW - 60_001, NOW + 5_001):
            with self.subTest(stamp=stamp):
                self.reported["reportado_ms"] = stamp
                self.assertIn("estado reciente", self.invoke("AbrirIntent"))
                self.assertIn("lectura reciente", self.invoke("EstadoPuertaIntent"))
        self.assertFalse(self.cloud.updates)

    def test_contradictory_pending_mode_rejects_open(self):
        self.desired.update({"modo": "bloqueado", "cerradura": "desbloqueada"})
        self.assertIn("modo bloqueado", self.invoke("AbrirIntent"))
        self.assertFalse(self.cloud.updates)
        self.lock.reconcile()
        self.assertEqual(self.lock.moves, [])
        self.assertEqual(self.reported["modo"], "bloqueado")

    def test_read_version_conflict_is_not_success(self):
        self.cloud.force_conflict = True
        self.assertIn("estado cambió", self.invoke("AbrirIntent"))
        self.assertFalse(self.cloud.updates)

    def test_voice_shadow_virtual_actuator_history_round_trip(self):
        self.assertIn("Envié", self.invoke("AbrirIntent"))
        self.assertIn("ya fue recibida", self.invoke("AbrirIntent"))
        self.assertEqual(self.lock.moves, [])
        self.lock.reconcile(NOW + 1000)
        self.assertEqual(self.lock.moves, ["desbloqueada"])
        self.assertEqual(self.reported["ordenResultado"], {"id": "request-1", "resultado": "ejecutado"})
        self.assertNotIn("orden", self.desired)
        self.assertIn("desbloqueada", self.invoke("EstadoCerraduraIntent"))
        self.assertIn("ya fue recibida", self.invoke("AbrirIntent"))
        self.lock.reconcile(NOW + 2000)
        self.assertEqual(self.lock.moves, ["desbloqueada"])
        history = list(self.cloud.history.items.values())
        self.assertEqual([(row["tipo"], row["origen"]) for row in history], [("desbloqueo", "voz")])

    def test_virtual_actuator_rejects_command_after_expiry(self):
        self.assertIn("Envié", self.invoke("AbrirIntent"))
        self.lock.reconcile(NOW + self.alexa.COMMAND_TTL_MS + 1)
        self.assertFalse(self.lock.moves)
        self.assertEqual(self.reported["ordenResultado"]["resultado"], "rechazado")

    def test_virtual_actuator_checks_new_door_reading(self):
        self.reported["cerradura"] = "desbloqueada"
        self.assertIn("Envié", self.invoke("CerrarIntent"))
        self.reported["puerta"] = "abierta"
        self.lock.reconcile(NOW + 1000)
        self.assertFalse(self.lock.moves)
        self.assertEqual(self.reported["ordenResultado"]["resultado"], "rechazado")

    def test_settings_apply_together_on_virtual_reconnection(self):
        self.assertIn("automático", self.invoke("CambiarModoIntent", {"modo": {"value": "automático"}}))
        self.invoke("CambiarModoIntent", {"modo": {"value": "manual"}})
        self.invoke("CambiarAutocierreIntent", {"segundos": {"value": "15"}})
        self.lock.reconcile(NOW + 1500)
        self.assertEqual((self.reported["modo"], self.reported["autocierre_s"]), ("manual", 15))
        self.assertFalse(self.lock.moves)

    def test_telemetry_contract_persists_distinct_readings_and_sources(self):
        samples = [
            {"tipo": "lectura", "puerta": "cerrada", "origen": "edge"},
            {"tipo": "puerta", "puerta": "abierta", "origen": "edge"},
            {"tipo": "acceso", "origen": "rfid", "credentialId": "tarjeta-1"},
            {"tipo": "desbloqueo", "origen": "rfid", "cerradura": "desbloqueada"},
            {"tipo": "bloqueo", "origen": "edge", "cerradura": "bloqueada"},
        ]
        for index, sample in enumerate(samples):
            payload = self.sample(eventId=f"{NOW - 10 + index}#{'c' * 32}", **sample)
            payload["uid"] = "excluded-by-rule"
            self.assertEqual(self.publish(payload)["statusCode"], 200)
            self.assertTrue(json.loads(self.publish(payload, NOW + 10)["body"])["duplicado"])
        rows = sorted(self.cloud.history.items.values(), key=lambda row: row["eventKey"])
        self.assertEqual([(row["tipo"], row["origen"]) for row in rows],
                         [(sample["tipo"], sample["origen"]) for sample in samples])
        self.assertTrue(all("uid" not in row for row in rows))

    def test_valid_pending_order_applies_only_after_virtual_reconnection(self):
        self.lock.online = False
        self.assertIn("Envié", self.invoke("AbrirIntent"))
        self.lock.reconcile(NOW + 1000)
        self.assertIn("orden", self.desired)
        self.assertFalse(self.lock.moves)
        self.assertFalse(self.cloud.history.items)
        self.lock.online = True
        self.lock.reconcile(NOW + 2000)
        self.assertNotIn("orden", self.desired)
        self.assertEqual(self.lock.moves, ["desbloqueada"])
        self.assertEqual(self.reported["ordenResultado"]["resultado"], "ejecutado")
        self.assertEqual(len(self.cloud.history.items), 1)

    def test_blocked_configuration_precedes_pending_motion(self):
        self.assertIn("Envié", self.invoke("AbrirIntent"))
        self.invoke("CambiarModoIntent", {"modo": {"value": "bloqueado"}})
        self.lock.reconcile(NOW + 1000)
        self.assertEqual(self.reported["modo"], "bloqueado")
        self.assertEqual(self.reported["ordenResultado"]["resultado"], "rechazado")
        self.assertNotIn("orden", self.desired)
        self.assertFalse(self.lock.moves)
        self.assertFalse(self.cloud.history.items)

    def test_proposed_policy_matches_device_and_shadow_topics(self):
        policy = {entry["Action"]: entry["Resource"]
                  for entry in read_json(CONFIG / "iot-policy.proposed.json")["Statement"]}
        device = read_json(CONFIG / "device.example.json")
        prefix = f"arn:aws:iot:{device['region']}:<ACCOUNT_ID>:"
        shadow = f"$aws/things/{device['thingName']}/shadow"
        self.assertEqual(policy["iot:Connect"], prefix + f"client/{device['clientId']}")
        self.assertEqual(set(policy["iot:Publish"]), {
            prefix + f"topic/chapa/{THING}/events", prefix + f"topic/{shadow}/get",
            prefix + f"topic/{shadow}/update",
        })
        response_suffixes = ("get/accepted", "get/rejected", "update/accepted",
                             "update/rejected", "update/delta")
        for action, resource_type in (("iot:Subscribe", "topicfilter"), ("iot:Receive", "topic")):
            self.assertEqual(set(policy[action]),
                             {prefix + f"{resource_type}/{shadow}/{suffix}" for suffix in response_suffixes})

    def test_history_deduplicates_across_rule_timestamps(self):
        first = self.publish(self.sample(thingName="other", ts=1))
        dup = self.publish(self.sample(), NOW + 1000)
        self.assertEqual(first["statusCode"], 200)
        self.assertTrue(json.loads(dup["body"])["duplicado"])
        item, = self.cloud.history.items.values()
        self.assertEqual(item["thingName"], THING)
        self.assertNotEqual(item["ts"], item["ingresoTs"])
        self.assertEqual(item["ingresoTs"], self.history.iso_timestamp(NOW))
        self.assertEqual(item["ttl"], NOW // 1000 + 30 * 86400)

    def test_distinct_events_at_same_time_are_kept(self):
        self.publish(self.sample())
        self.publish(self.sample(eventId=f"{NOW - 1}#{'b' * 32}"))
        self.assertEqual(len(self.cloud.history.items), 2)

    def test_invalid_events_are_rejected_before_writing(self):
        for message in (self.sample(tipo=[]), self.sample(origen={}), self.sample(seq=True),
                        self.sample(diferido="false"), self.sample(autocierre_s=121),
                        self.sample(eventId="invalid"), self.sample(simulado=True),
                        self.sample(tipo="bloqueo", cerradura="desbloqueada"),
                        {key: val for key, val in self.sample().items() if key != "puerta"}):
            with self.subTest(message=message):
                self.assertEqual(self.publish(message)["statusCode"], 400)
        self.assertFalse(self.cloud.history.items)

    def test_retryable_storage_failure_propagates(self):
        self.cloud.history.error = ClientError("ThrottlingException")
        with self.assertRaisesRegex(ClientError, "ThrottlingException"):
            self.publish(self.sample())
        self.cloud.history.error = None
        self.assertEqual(self.publish(self.sample())["statusCode"], 200)


if __name__ == "__main__":
    unittest.main()

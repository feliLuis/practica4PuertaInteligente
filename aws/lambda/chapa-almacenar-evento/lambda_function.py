"""Validate IoT Rule messages and store the device event at most once."""
import json
import logging
import os
import re
from datetime import datetime, timezone
from functools import lru_cache

import boto3
from botocore.exceptions import ClientError

LOGGER = logging.getLogger(__name__)
RETENTION_SECONDS = 30 * 86400
THING_NAME = re.compile(r"[A-Za-z0-9:_-]{1,128}")
EVENT_ID = re.compile(r"([1-9][0-9]{12})#([a-f0-9]{32})")
MAX_TIMESTAMP_MS = 4_102_444_800_000  # 2100-01-01 UTC
CLOCK_SKEW_MS = 5_000
ENUM_FIELDS = {
    "tipo": {"acceso", "desbloqueo", "bloqueo", "puerta", "sensor",
             "modo", "autocierre", "lectura", "solicitud"},
    "origen": {"rfid", "voz", "edge", "nube"},
    "resultado": {"ejecutado", "rechazado"},
    "puerta": {"abierta", "cerrada", "desconocida"},
    "cerradura": {"bloqueada", "desbloqueada", "desconocida"},
    "modo": {"manual", "automatico", "bloqueado"},
}
TEXT_FIELDS = {"motivo": 64, "credentialId": 32}
REQUIRED_FIELDS = {
    "lectura": ("puerta",),
    "puerta": ("puerta",),
    "bloqueo": ("cerradura",),
    "desbloqueo": ("cerradura",),
    "modo": ("modo",),
    "autocierre": ("autocierre_s",),
}
LOCK_STATES = {"bloqueo": "bloqueada", "desbloqueo": "desbloqueada"}


@lru_cache(maxsize=1)
def history_table():
    return boto3.resource("dynamodb").Table(os.environ.get("HISTORY_TABLE", "ChapaHistory"))


def validate_event(event):
    if not isinstance(event, dict):
        return "object_required"
    thing = event.get("thingName")
    if not isinstance(thing, str) or not THING_NAME.fullmatch(thing):
        return "invalid_thing"
    if not all(field in event for field in ("tipo", "origen", "resultado")):
        return "missing_required_field"
    if "simulado" in event and event["simulado"] is not False:
        return "simulation_not_allowed"
    for field, allowed in ENUM_FIELDS.items():
        if field in event:
            value = event[field]
            if not isinstance(value, str) or value not in allowed:
                return f"invalid_{field}"
    for field, limit in TEXT_FIELDS.items():
        if field in event and (not isinstance(event[field], str) or len(event[field]) > limit):
            return f"invalid_{field}"
    for field, limits in {"seq": (0, 2**32 - 1), "autocierre_s": (5, 120)}.items():
        if field in event and (type(event[field]) is not int or not limits[0] <= event[field] <= limits[1]):
            return f"invalid_{field}"
    if "diferido" in event and type(event["diferido"]) is not bool:
        return "invalid_diferido"

    event_type = event["tipo"]
    if any(field not in event for field in REQUIRED_FIELDS.get(event_type, ())):
        return "missing_type_field"
    if event_type in LOCK_STATES and (
        event["cerradura"] != LOCK_STATES[event_type] or event["resultado"] != "ejecutado"
    ):
        return "contradictory_actuator_event"

    event_id = event.get("eventId")
    match = EVENT_ID.fullmatch(event_id) if isinstance(event_id, str) else None
    if not match:
        return "invalid_event_id"
    ingress = event.get("ts")
    if type(ingress) is not int or not 0 <= ingress < MAX_TIMESTAMP_MS:
        return "invalid_ingress_timestamp"
    occurred = int(match[1])
    if occurred >= MAX_TIMESTAMP_MS or occurred > ingress + CLOCK_SKEW_MS:
        return "invalid_occurrence_timestamp"
    return None


def iso_timestamp(timestamp_ms):
    return datetime.fromtimestamp(timestamp_ms / 1000, timezone.utc).isoformat(
        timespec="milliseconds"
    ).replace("+00:00", "Z")


def lambda_handler(event, context):
    error = validate_event(event)
    if error:
        LOGGER.warning("Discarded event: %s", error)
        return {"statusCode": 400, "body": json.dumps({"descartado": error})}

    event_id = event["eventId"]
    occurred = int(event_id.split("#", 1)[0])
    timestamp = iso_timestamp(occurred)
    event_key = f"{timestamp}#{event_id.split('#', 1)[1]}"
    item = {
        "thingName": event["thingName"],
        "eventKey": event_key,
        "eventId": event_id,
        "ts": timestamp,
        "ingresoTs": iso_timestamp(event["ts"]),
        "ttl": event["ts"] // 1000 + RETENTION_SECONDS,
    }
    for field in (*ENUM_FIELDS, *TEXT_FIELDS, "seq", "diferido", "autocierre_s"):
        if field in event:
            item[field] = event[field]

    try:
        history_table().put_item(Item=item, ConditionExpression="attribute_not_exists(eventKey)")
    except ClientError as error:
        if error.response["Error"]["Code"] != "ConditionalCheckFailedException":
            raise
        return {"statusCode": 200, "body": json.dumps({"eventKey": event_key, "duplicado": True})}
    return {"statusCode": 200, "body": json.dumps({"eventKey": event_key})}

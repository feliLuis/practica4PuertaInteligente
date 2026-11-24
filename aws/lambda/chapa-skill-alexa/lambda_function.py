"""Handle the existing Spanish Alexa interaction model through Device Shadow."""
import json
import logging
import os
import re
import time
from datetime import datetime, timezone
from functools import lru_cache
from uuid import uuid4

import boto3
from botocore.exceptions import ClientError

LOGGER = logging.getLogger(__name__)
MODES = {"manual", "automatico", "bloqueado"}
LOCK_STATES = {"bloqueada", "desbloqueada"}
SNAPSHOT_MAX_AGE_MS = 60_000
COMMAND_TTL_MS = 30_000
CLOCK_SKEW_MS = 5_000


@lru_cache(maxsize=1)
def registry():
    return boto3.client("iot")


@lru_cache(maxsize=1)
def bindings():
    return boto3.resource("dynamodb").Table(os.environ.get("BINDINGS_TABLE", "AlexaThingBindings"))


@lru_cache(maxsize=1)
def shadow_client():
    endpoint = os.environ.get("IOT_DATA_ENDPOINT")
    if not endpoint:
        endpoint = registry().describe_endpoint(endpointType="iot:Data-ATS")["endpointAddress"]
    return boto3.client("iot-data", endpoint_url=f"https://{endpoint}")


def now_ms():
    return time.time_ns() // 1_000_000


def speak(text, end=False):
    response = {"outputSpeech": {"type": "PlainText", "text": text}, "shouldEndSession": end}
    if not end:
        response["reprompt"] = {
            "outputSpeech": {"type": "PlainText", "text": "¿Qué más quieres hacer con tu chapa?"}
        }
    return {"version": "1.0", "response": response}


def slot_value(slots, name):
    slot = slots.get(name) or {}
    resolutions = slot.get("resolutions", {}).get("resolutionsPerAuthority", [])
    for authority in resolutions:
        if authority.get("status", {}).get("code") == "ER_SUCCESS_MATCH":
            for match in authority.get("values", []):
                entity_id = match.get("value", {}).get("id")
                if entity_id:
                    return entity_id
    return slot.get("value")


def integer_slot(slots, name, minimum, maximum):
    value = slot_value(slots, name)
    if not isinstance(value, str) or not re.fullmatch(r"[0-9]{1,10}", value):
        return None
    result = int(value)
    return result if minimum <= result <= maximum else None


def bound_thing(user_id):
    item = bindings().get_item(Key={"userId": user_id}, ConsistentRead=True).get("Item")
    return item["thingName"] if item else None


def load_shadow(thing):
    try:
        response = shadow_client().get_thing_shadow(thingName=thing)
    except ClientError as error:
        if error.response["Error"]["Code"] == "ResourceNotFoundException":
            return {}
        raise
    return json.loads(response["payload"].read())


def update_desired(thing, document, desired):
    update = {"state": {"desired": desired}}
    version = document.get("version")
    if version is not None:
        update["version"] = version
    try:
        shadow_client().update_thing_shadow(thingName=thing, payload=json.dumps(update).encode())
    except ClientError as error:
        if error.response["Error"]["Code"] == "ConflictException":
            return False
        raise
    return True


def bind_user(user_id, slots):
    number = integer_slot(slots, "numeroSerie", 1, 9_999_999_999)
    if number is None:
        return speak("Dime un número de chapa entero y positivo.")
    thing = f"chapa-inteligente-{number:02d}"
    try:
        registry().describe_thing(thingName=thing)
    except ClientError as error:
        if error.response["Error"]["Code"] == "ResourceNotFoundException":
            return speak(f"No encontré ninguna chapa con el número {number}.")
        raise
    previous = bound_thing(user_id)
    bindings().put_item(Item={
        "userId": user_id, "thingName": thing,
        "fechaVinculacion": datetime.now(timezone.utc).isoformat(),
    })
    if previous and previous != thing:
        return speak(f"Listo, ahora tu chapa es la número {number}. Reemplacé la anterior.")
    return speak(f"Listo, tu chapa número {number} quedó vinculada.")


def is_fresh(reported, current_time):
    timestamp = reported.get("reportado_ms")
    return type(timestamp) is int and -CLOCK_SKEW_MS <= current_time - timestamp <= SNAPSHOT_MAX_AGE_MS


def handle_intent(name, slots, thing, request_id):
    document = load_shadow(thing)
    state = document.get("state", {})
    reported = state.get("reported", {})
    desired = state.get("desired", {})
    current_time = now_ms()
    fresh = is_fresh(reported, current_time)
    door = reported.get("puerta")
    lock = reported.get("cerradura")
    mode = reported.get("modo")

    if name == "EstadoPuertaIntent":
        if fresh and isinstance(door, str) and door in {"abierta", "cerrada"}:
            return speak(f"La puerta está {door}.")
        return speak("Todavía no tengo una lectura reciente de la puerta.")
    if name == "EstadoCerraduraIntent":
        if fresh and isinstance(lock, str) and lock in LOCK_STATES:
            return speak(f"El último estado reportado indica que la chapa está {lock}.")
        return speak("Todavía no tengo un estado reciente de la chapa.")
    if name == "ConsultarModoIntent":
        if fresh and isinstance(mode, str) and mode in MODES:
            return speak(f"La chapa está en modo {mode.replace('automatico', 'automático')}.")
        return speak("Todavía no tengo un modo reciente de la chapa.")
    if name == "ConsultarAutocierreIntent":
        seconds = reported.get("autocierre_s")
        if fresh and type(seconds) is int and 5 <= seconds <= 120:
            return speak(f"El tiempo de autocierre configurado es de {seconds} segundos.")
        return speak("Todavía no tengo el tiempo de autocierre vigente.")

    if name in {"AbrirIntent", "CerrarIntent"}:
        opening = name == "AbrirIntent"
        if not fresh or not isinstance(mode, str) or mode not in MODES:
            return speak("No tengo un estado reciente de la chapa. Inténtalo cuando esté conectada.")
        if opening and (mode == "bloqueado" or desired.get("modo") == "bloqueado"):
            return speak("No puedo abrir: la chapa está en modo bloqueado o tiene ese cambio pendiente.")
        if not opening and door != "cerrada":
            return speak("No puedo bloquear sin una lectura reciente de la puerta cerrada.")
        action = "desbloquear" if opening else "bloquear"
        previous_request = desired.get("orden") or {}
        last_result = reported.get("ordenResultado") or {}
        if request_id and request_id in (previous_request.get("id"), last_result.get("id")):
            return speak("Esa solicitud ya fue recibida. Pregúntame el estado para confirmar.")
        changes = {"cerradura": None, "orden": {
            "id": request_id or uuid4().hex, "accion": action, "vence_ms": current_time + COMMAND_TTL_MS,
        }}
        message = f"Envié la orden de {action}. Pregúntame el estado para confirmar."
    elif name == "CambiarModoIntent":
        new_mode = slot_value(slots, "modo")
        if new_mode == "automático":
            new_mode = "automatico"
        if not isinstance(new_mode, str) or new_mode not in MODES:
            return speak("Los modos disponibles son manual, automático y bloqueado.")
        changes = {"modo": new_mode}
        message = f"Solicité el modo {new_mode.replace('automatico', 'automático')}."
    elif name == "CambiarAutocierreIntent":
        seconds = integer_slot(slots, "segundos", 5, 120)
        if seconds is None:
            return speak("Dime un número entero entre 5 y 120 segundos.")
        changes = {"autocierre_s": seconds}
        message = f"Solicité un autocierre de {seconds} segundos."
    else:
        return speak("No entendí lo que quieres hacer con la chapa.")

    if not update_desired(thing, document, changes):
        return speak("El estado cambió mientras procesaba tu solicitud. Vuelve a intentarlo.")
    return speak(message)


def lambda_handler(event, context):
    try:
        request = event["request"]
        request_type = request["type"]
        if request_type == "SessionEndedRequest":
            return {"version": "1.0", "response": {}}
        if request_type == "LaunchRequest":
            return speak("Bienvenido a chapa inteligente. Puedes vincular tu chapa, consultar la puerta o abrirla.")

        intent = request["intent"]
        name = intent["name"]
        slots = intent.get("slots", {})
        if name in {"AMAZON.StopIntent", "AMAZON.CancelIntent"}:
            return speak("Hasta luego.", end=True)
        if name in {"AMAZON.HelpIntent", "AMAZON.FallbackIntent"}:
            return speak("Puedes vincular tu chapa, preguntar su estado o cambiar el modo y el autocierre.")

        user_id = event["context"]["System"]["user"]["userId"]
        if name == "VincularIntent":
            return bind_user(user_id, slots)
        thing = bound_thing(user_id)
        if name == "DesvincularIntent":
            if not thing:
                return speak("No tienes ninguna chapa vinculada.")
            bindings().delete_item(Key={"userId": user_id})
            return speak("Listo, desvinculé tu chapa.")
        if not thing:
            return speak("Aún no tienes una chapa vinculada. Di: vincula mi chapa con el número uno.")
        return handle_intent(name, slots, thing, request.get("requestId"))
    except (ClientError, KeyError, TypeError, ValueError, AttributeError) as error:
        LOGGER.warning("Alexa request failed: %s", type(error).__name__)
        return speak("Ocurrió un problema al procesar tu solicitud. Inténtalo de nuevo.")

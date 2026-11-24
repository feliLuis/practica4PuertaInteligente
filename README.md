# Cerradura inteligente — firmware base

Firmware modular para dos ESP32 de 38 pines. La base actual implementa control
local de la cerradura, lectura RFID, observación del Reed, diagnósticos por USB y
pruebas de escritorio. El código, los comandos, los estados, los eventos y sus
identificadores están estandarizados en inglés; esta documentación académica se
mantiene en español.

Esta fase no implementa ESP-NOW, WiFi, MQTT, TLS, NTP, AWS IoT Core ni Alexa.
`SerialLink` solo muestra por USB los mensajes que deberá transportar la futura
capa ESP-NOW. No existe todavía comunicación real entre las placas.

## Arquitectura actual

```text
firmware/
├── esp32_control/
│   ├── include/
│   │   ├── CredentialList.h
│   │   ├── LockController.h
│   │   ├── LockServo.h
│   │   ├── SerialMonitor.h
│   │   ├── Types.h
│   │   └── config.h
│   ├── src/
│   │   ├── CredentialList.cpp
│   │   ├── LockController.cpp
│   │   ├── LockServo.cpp
│   │   └── main.cpp
│   └── platformio.ini
└── esp32_sensors/
    ├── include/
    │   ├── DoorSensor.h
    │   ├── RfidReader.h
    │   ├── SerialLink.h
    │   └── config.h
    ├── src/
    │   ├── DoorSensor.cpp
    │   ├── RfidReader.cpp
    │   ├── SerialLink.cpp
    │   └── main.cpp
    └── platformio.ini
shared/
└── Protocol/
    ├── library.json
    └── src/
        ├── LocalLink.h
        ├── Protocol.cpp
        └── Protocol.h
tests/
├── stubs/
├── TestSupport.h
├── control_tests.cpp
├── protocol_tests.cpp
├── sensors_tests.cpp
├── console_tests.cpp
└── run_tests.py
```

`LockController` mantiene separados `Protocol::DoorState`, que representa la
puerta observada por el Reed, y `LockState`, que representa la posición estimada
del pestillo después de emitir una orden PWM. No existe un sensor que confirme
la posición física del servo.

`CredentialList` contiene la lista local de autorizaciones. Solo admite
`Action::UNLOCK`, por lo que una credencial RFID no puede bloquear ni alternar
el estado. `LockServo` valida el GPIO, acepta únicamente 0° y 90°, y desacopla
el PWM 700 ms después de una orden. `SerialMonitor` procesa comandos de forma
incremental sin `String`, esperas de negocio ni asignación dinámica.

`DoorSensor` estabiliza el Reed y `RfidReader` gestiona sondeo, disponibilidad,
reintento y supresión temporal de lecturas repetidas. `Protocol` define el
mensaje compacto común y las conversiones UID hexadecimal.

## Hardware y conexiones

### ESP32 #1 — control

| SG90 | ESP32 #1 |
|---|---|
| Señal | GPIO13 |
| Alimentación | 5 V regulados; verificar la fuente antes de accionar |
| Tierra | GND común |

- `LockState::LOCKED`: 0°.
- `LockState::UNLOCKED`: 90°.
- El arranque conserva puerta y cerradura en `UNKNOWN` y no mueve el servo.
- No se utilizan LED, buzzer ni otros actuadores.

El servo no debe alimentarse desde 3V3 ni desde un GPIO. Para pruebas bajo carga
se recomienda una fuente externa regulada de 5 V con tierra común. La emisión de
PWM no demuestra que el mecanismo alcanzó la posición solicitada.

### ESP32 #2 — sensores

| RC522 | ESP32 #2 |
|---|---|
| SDA/SS | GPIO21 |
| SCK | GPIO18 |
| MOSI | GPIO23 |
| MISO | GPIO19 |
| RST | GPIO22 |
| Alimentación | 3.3 V |
| GND | GND |
| IRQ | Sin conectar |

El Reed conecta GPIO27 con GND y usa `INPUT_PULLUP`: LOW significa `closed` y
HIGH significa `open`. La lectura empieza en `unknown` y requiere 100 ms de
estabilidad inicial y después de cada transición.

El RC522 se sondea cada 50 ms, se comprueba cada segundo y se reinicializa como
máximo cada cinco segundos después de un fallo. La supresión de una presentación
repetida del mismo UID dura 1500 ms. Esto debe verificarse con hardware real,
porque no equivale a detectar físicamente que se retiró una tarjeta.

## Credenciales y reglas de acceso

| UID | Acción | `credentialId` |
|---|---|---|
| `39 61 D7 06` | `unlock` a 90° | `card-1` |
| `9C 31 8B 06` | `unlock` a 90° | `keyfob-1` |

- Un UID desconocido se rechaza con `unknown_uid` y no mueve el servo.
- Ambas credenciales se rechazan con `blocked_mode` cuando el modo es
  `Mode::BLOCKED`.
- Ninguna credencial puede bloquear ni alternar la cerradura.
- Repetir o alternar credenciales cuando el estado ya es `UNLOCKED` devuelve
  `no_change`, no mueve el servo y no reinicia el autocierre.
- Un acceso RFID aceptado emite el motivo `authorized_credential` y conserva el
  identificador lógico; el UID no se copia al evento.

La autenticación basada solamente en UID no impide clonación. No debe tratarse
como una solución de seguridad comercial.

## Modos y autocierre

Los modos son `Mode::MANUAL`, `Mode::AUTOMATIC` y `Mode::BLOCKED`.

- `manual`: permite órdenes autorizadas y no ejecuta autocierre.
- `automatic`: evalúa el autocierre cuando la cerradura está desbloqueada, la
  puerta está cerrada y el dato del sensor sigue vigente.
- `blocked`: rechaza desbloqueos y cancela el temporizador sin mover el servo al
  cambiar de modo. El estado físico bloqueado continúa siendo
  `LockState::LOCKED`.

El autocierre vale 10 segundos por defecto y acepta valores de 5 a 120 segundos.
Nunca bloquea con la puerta abierta, desconocida o con datos obsoletos. Abrir la
puerta cancela la cuenta; un cierre estable posterior inicia el período completo.
Perder el sensor cancela la cuenta y una lectura válida al recuperarlo vuelve a
evaluar las condiciones. Un fallo del servo no se reintenta en cada vuelta del
bucle.

El nodo de sensores genera heartbeat cada 10 segundos y el controlador considera
obsoletos los datos a los 30 segundos. Estos valores se conservan deliberadamente
hasta implementar y probar la comunicación ESP-NOW.

## Eventos y salida local

Los nombres de campos impresos usan camelCase y los valores de estado son
lowercase. Ejemplo local:

```text
[EVT-SIM] sequence=4 source=rfid type=access result=executed reason=authorized_credential doorState=closed lockState=unlocked mode=automatic credentialId=card-1
```

Los motivos usan snake_case. Entre los motivos principales están
`authorized_credential`, `unknown_uid`, `blocked_mode`, `door_not_closed`,
`sensor_data_unavailable`, `no_change`, `servo_command_failed`,
`pwm_command_issued`, `door_open`, `door_closed`, `sensor_online` y
`sensor_offline`.

Los eventos son estructuras locales en RAM. La secuencia vuelve a empezar al
reiniciar y no constituye una identidad persistente para AWS.

## Entornos y comandos USB

| Proyecto / entorno | Propósito |
|---|---|
| Control / `esp32dev` | Operación local; solo `help` y `status` mientras no exista enlace entre placas |
| Control / `simulation` | Lógica completa simulada, sin PWM |
| Control / `calibration` | Verificación física explícita del servo |
| Sensores / `esp32dev` | Lectura normal con UID oculto en USB |
| Sensores / `diagnostic` | Diagnóstico con UID completo por USB |

Comandos del entorno `simulation`:

```text
help
status
door open
door closed
heartbeat
uid HEX
unlock
lock
mode manual
mode automatic
mode blocked
auto-lock N
```

`heartbeat` sin una lectura no renueva el estado del Reed. `uid HEX` acepta UID
de 4, 7 o 10 bytes. Los comandos desconocidos, truncados o deshabilitados se
rechazan.

El entorno `calibration` acepta exclusivamente:

```text
calibrate 0
calibrate 90
```

La calibración omite deliberadamente los interbloqueos de negocio para verificar
el actuador. Debe usarse solo después de comprobar alimentación, recorrido libre
y montaje. No está habilitada en el binario normal.

## Compilación

Versiones fijadas:

| Dependencia | Versión |
|---|---|
| Plataforma `espressif32` | 7.1.3 |
| Arduino-ESP32 | 4.20017.260907 (core 2.0.17) |
| ESP32Servo | 0.13.0 |
| MFRC522 | 1.4.12 |

Desde la raíz del repositorio en PowerShell:

```powershell
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"
& $pio run -d firmware/esp32_control -e esp32dev -e simulation -e calibration
& $pio run -d firmware/esp32_sensors -e esp32dev -e diagnostic
```

Estos comandos solo compilan. No utilizar `upload`, `upload and monitor` ni
`debug` hasta completar la verificación física y autorizar la carga.

## Pruebas automatizadas

Las pruebas de escritorio compilan las clases de producción con dobles de reloj,
GPIO, PWM, SPI, RC522 y puerto serie. No acceden a placas ni puertos físicos.

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" tests/run_tests.py
```

El ejecutor usa C++11 con `-Wall -Wextra -Werror` y cubre:

- controlador, credenciales, servo, modos y autocierre;
- pérdida y recuperación del sensor, entradas inválidas y desbordamiento de
  `millis()`;
- estabilización inicial y rebotes del Reed;
- inicialización, recuperación, longitudes y antirrepetición RFID;
- privacidad del UID en modos normal y diagnóstico;
- codificación, parsing, nombres y estructura compacta del protocolo;
- separación de las consolas normal, simulación y calibración.

La suite de sensores procede de la prueba histórica añadida en `7ee899d` y
eliminada en `2f569d9`; fue recuperada y adaptada a `DoorSensor`, `RfidReader`,
`SerialLink`, los stubs actuales y la nomenclatura inglesa. Se añadió una suite
independiente para `Protocol`.

Validación ejecutada el 24/11/2026: siete binarios de prueba, **61 casos
aprobados y 0 fallidos**. También compilaron correctamente los cinco entornos
PlatformIO indicados arriba. Estas evidencias son simuladas y de compilación;
no sustituyen pruebas físicas.

## Contrato objetivo para AWS IoT Core

La siguiente fase deberá publicar el estado reportado del Device Shadow con
campos camelCase y valores lowercase:

```json
{
  "state": {
    "reported": {
      "doorState": "closed",
      "lockState": "unlocked",
      "mode": "automatic",
      "autoLockSeconds": 10,
      "sensorStatus": "online"
    }
  }
}
```

Valores permitidos:

- `doorState`: `open`, `closed`, `unknown`.
- `lockState`: `locked`, `unlocked`, `unknown`.
- `mode`: `manual`, `automatic`, `blocked`.
- `sensorStatus`: `online`, `offline`.
- `autoLockSeconds`: entero entre 5 y 120.

Estado deseado:

```json
{
  "state": {
    "desired": {
      "lockState": "unlocked"
    }
  }
}
```

Los campos modificables desde la nube serán `lockState`, `mode` y
`autoLockSeconds`. Toda orden remota deberá validarse y convertirse en una acción
del controlador sin eludir los interbloqueos.

Formato objetivo de eventos:

```json
{
  "type": "access",
  "source": "rfid",
  "result": "executed",
  "reason": "authorized_credential",
  "doorState": "closed",
  "lockState": "unlocked",
  "mode": "automatic",
  "credentialId": "card-1",
  "sequence": 15,
  "deferred": false
}
```

Las funciones Lambda, Alexa y cualquier consumidor existente deberán migrarse
coordinadamente a este contrato. Esta base no modifica servicios AWS ni publica
mensajes MQTT.

## Riesgos y próximas fases

El heartbeat de 10 s y timeout de 30 s permiten operar hasta 30 s con una lectura
potencialmente antigua. Para ESP-NOW se propone evaluar heartbeat de 1 s, timeout
de 3 s y una confirmación reciente del Reed antes de bloquear. Los intervalos no
se cambian todavía porque deben validarse junto con pérdida de paquetes, ACK,
reintentos y consumo.

ESP-NOW y WiFi comparten la radio del ESP32. La implementación deberá coordinar
canal, reconexión y coexistencia antes de añadir AWS. También debe definir versión
de trama, validación, sesiones, deduplicación y comportamiento ante reinicios.

El estado de puerta, cerradura, modo, temporizador y secuencia vive en RAM y se
pierde al reiniciar. Siguen pendientes la verificación de los UID y del Reed con
hardware real, la alimentación del servo bajo carga, la posición física 0°/90°,
el efecto del desacople, la detección de atascos y la prueba RFID→ESP-NOW→control.

La fase siguiente puede comenzar por el transporte ESP-NOW usando `Protocol` y
`LocalLink`, manteniendo separados transporte, autorización y lógica de seguridad.
Después corresponderán persistencia, WiFi/MQTT/TLS, Device Shadow, eventos AWS y
migración coordinada de Lambda/Alexa.

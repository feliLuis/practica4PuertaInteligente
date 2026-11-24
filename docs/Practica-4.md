# Práctica 4 — Puerta Inteligente

**Asignatura** SIS-234 · **Fecha** 24 de noviembre de 2026

**Bases integradas** `cd7ffe9` (ESP-NOW) y `0fe5377` (AWS y entrega) · **Rama de trabajo** `feat/esp32-sensores`

**Estado** ESP-NOW y RFID operativos en el montaje según el equipo. Telemetría MQTT/TLS de solo publicación implementada en el ESP32 de control y con tráfico reciente observado en AWS. El origen del tráfico es compatible con el firmware, pero no está probado por serial ni con un Reed vigente. La actuación remota permanece deshabilitada y las órdenes por voz solo están verificadas con el actuador virtual.

## 1. Caso de uso y requerimientos

La puerta permite comprobar si está cerrada, autorizar una presentación RFID, consultar el estado por voz y accionar una cerradura de baja tensión. El Reed informa la posición de la puerta. El RC522 reconoce tarjeta y llavero. El SG90 representa la posición del mecanismo de cierre. La Práctica 4 añade histórico de lecturas y movimientos, autocierre en el Edge y consulta y ajuste por voz del modo y del temporizador. El Reed no informa la posición del pestillo y el SG90 no tiene realimentación mecánica en el montaje actual. La estimación publicada se debe comparar con la observación del mecanismo en la demo.

El ESP32 de control es el único cliente AWS previsto para el montaje. El ESP32 de sensores entrega Reed, RFID y latidos por ESP-NOW unicast con protocolo v2. `AwsTelemetry` copia los eventos y los estados de `LockController` y publica en una tarea separada. El bloqueo local conserva la confirmación con `ControlLinkManager`. El firmware físico carece de suscripciones MQTT: no aplica `desired` ni mueve el servo por voz. La política activa v2 y la regla desplegada ya están limitadas a la publicación del Thing; el historial y las órdenes de voz se prueban con el dispositivo y actuador virtuales.

### Matriz RF1–RF15

| RF | Requisito | Implementación y evidencia verificable | Evidencia pendiente |
|---|---|---|---|
| RF1 | Capturar datos de sensores | `DoorSensor` y `RfidReader` probados en escritorio. El equipo informa funcionamiento local por RFID. El tráfico AWS reciente reporta `puerta=desconocida`, sin lectura Reed vigente | Medir polaridad, rebote, aciertos y omisiones con el montaje y correlacionarlos con el estado publicado |
| RF2 | Reportar sensor y actuador periódicamente | Implementado en `AwsTelemetry`. CLI observó 31 lecturas `edge` entre 21:44:18Z y 22:01:46Z a ~30 s y Shadow v28 con `reportado_ms=2026-11-24T21:56:17Z`; RFID queda como evento, no como último estado | Confirmar por monitor serie que el publicador es la placa, obtener un Reed vigente y decidir con el docente si se requiere el último RFID en `reported` |
| RF3 | Aplicar delta y confirmar en `reported` | Confirmación ESP-NOW del bloqueo local y recorrido de órdenes con actuador virtual comprobados por separado. El firmware publica y no consume delta; el Shadow actual conserva `desired.modo=bloqueado` y `desired.cerradura=desbloqueada` sin aplicar | Demostrar el recorrido remoto sobre el Thing virtual autorizado. No habilitar el consumidor remoto en la placa sin autorización |
| RF4 | Consultar sensor del Thing por voz en español | `EstadoPuertaIntent` consume `puerta`. La captura `intents.png` acredita el modelo compilado, no una invocación | Probar frases en Alexa Console con un `reported` reciente. La skill no ofrece consulta de RFID |
| RF5 | Ordenar movimientos por voz mediante `desired` | `AbrirIntent` y `CerrarIntent` escriben orden con ID y vencimiento; recorrido verificado con el actuador virtual | Verificar voz y confirmación sobre el Thing virtual. La actuación física sigue fuera de alcance |
| RF6 | Consultar el estado del actuador por voz | El backend lee la estimación y distingue `desconocida`; el Shadow actual reporta `cerradura=desconocida` | Probar frases y cotejar la estimación con la posición real observada |
| RF7 | Vincular y persistir entre sesiones | Tabla `AlexaThingBindings` activa; alta verificada con sustituto y captura `vinculacion.png` de la tabla | Persistencia real entre sesiones de Alexa |
| RF8 | Resolver por usuario y orientar si falta vínculo | Dos usuarios y dos Things simulados; consultas independientes en pruebas | Probar en Alexa Console con identidades y Things autorizados |
| RF9 | Comprobar existencia, cambiar y eliminar vínculo | Alta, sustitución, Thing inexistente y baja verificados con sustituto | Demostración integrada con registros autorizados |
| RF10 | Regla IoT invoca Lambda con datos del objeto | Regla activa con SQL explícita para `chapa/chapa-inteligente-01/events`; CloudWatch muestra invocaciones de `chapa-almacenar-evento` a las 21:44:20Z y 21:47:18Z, entre otras | Capturar la invocación correlacionada con una publicación identificada desde la placa |
| RF11 | Validar, enriquecer y almacenar lecturas | 31 filas `lectura` con `eventId`, `ingresoTs` y `ttl`; latencia observada `ts`→`ingresoTs` de 0,695–2,413 s. El descarte y la deduplicación se verificaron solo con sustitutos | Repetir un `eventId` real y publicar un mensaje inválido en AWS para medir deduplicación, integridad y descarte |
| RF12 | Historial de movimientos con su origen | Filas recientes solo `tipo=lectura, origen=edge`. El histórico antiguo tiene tres filas `rfid`; la voz tiene evidencia virtual | Capturar movimientos RFID, Edge y por voz supervisados y corroborar `origen` y `resultado` en cada fila |
| RF13 | Lógica automática y justificación Edge/Nube | Autocierre y confirmación ESP-NOW implementados y probados con reloj, radio y actuador simulados | Medir topes, cortes y conflictos con voz en el montaje |
| RF14 | Reportar, consultar y modificar modo y parámetros por voz | Shadow reporta `modo=automatico` y `autocierre_s=10`; el cambio y su confirmación se verificaron con el dispositivo virtual | Consultar estados físicos en Alexa y demostrar cambios en el Thing virtual; el `desired` actual sigue sin aplicarse en la placa |
| RF15 | Mockups basados en DynamoDB | [Mockups, diccionario de datos y muestra anonimizada real](mockups-reportes.md) | Muestras anonimizadas de la demo integrada con voz, RFID y Edge |

### Requerimientos no funcionales y aceptación

| Atributo | Meta inicial del grupo o referencia docente | Evidencia actual y prueba faltante |
|---|---|---|
| Estabilidad | ≥10 min continuos sin reinicio ni bloqueo | 31 publicaciones en ~17,5 min muestran continuidad del publicador, sin reinicios observados en esa ventana. Falta tráfico mixto con voz y autocierre |
| Exactitud | Reed: concordancia con posición observada en una serie de ciclos. RFID: medir aciertos, omisiones y repeticiones por presentación dentro del rango de prueba declarado | Pendiente declarar tamaño de muestra, tolerancia y medir ambas entradas. El estado reciente es `desconocida` |
| Respuesta a voz | ≤3 s de solicitud a movimiento | El flujo lógico está simulado. No hay invocaciones recientes de la Lambda de Alexa; medir de Alexa Console al servo con reloj común |
| Frecuencia | Cambio discreto ≤1 s y respaldo cada 30 s | Respaldo observado cada ~30 s en 31 lecturas. El reporte por cambio y el plazo de 1 s siguen sin medirse |
| Reconexión | Restablecer WiFi/MQTT automáticamente y aplicar `desired` pendiente válido | Simulación de reconciliación; el `desired` real sigue pendiente en Shadow. Falta corte real y verificación de vencimiento y versión |
| Integridad | ≥99 % de eventos válidos únicos publicados quedan almacenados, con descarte de inválidos | 31 IDs únicos en la ventana consultada; sin denominador completo ni repetición real. Deduplicación y descarte validados solo con sustitutos |
| Latencia de almacenamiento | ≤5 s máximo de publicación a registro | En 31 filas, `ingresoTs − ts` osciló entre 0,695 y 2,413 s. No mide el recorrido desde el sensor ni la espera offline |
| Reacción automática | Medir desde vencimiento del temporizador con Reed vigente hasta movimiento | Reloj simulado. Medir físicamente y reportar aparte los segundos configurados |
| Operación offline | RFID, interbloqueos y autocierre local operativos mientras el Reed esté vigente; cola de eventos acotada | Pruebas del controlador con lecturas simuladas. Falta medir la cola y comprobar el corte de Internet con enlace local activo |
| Costo | Estimar consumo de IoT, Lambda y DynamoDB, considerando el beneficio del proyecto | Calculado en §5; falta validar consumo real y beneficio aplicable |
| Seguridad y calidad | TLS, plantilla sin secretos, roles documentados y módulos separados | Política v2 limitada a `Connect` y publicación; regla limitada al Thing. La cabecera privada local contiene credenciales y está excluida de Git. Falta verificar TLS desde la placa y revisar UID de prototipo antes de uso real |

## 2. Análisis y diseño

### 2.1 Bloques del objeto inteligente

~~~mermaid
flowchart LR
    Reed[Reed] --> Sensors[ESP32 sensores]
    RFID[RC522] --> Sensors
    Sensors <-->|ESP-NOW v2| Link[EspNowLink y ReliableChannel]
    Link <--> Control[ESP32 control y ControlLinkManager]
    Control --> Servo[SG90]
    Control --> AWS[AwsTelemetry]
~~~

### 2.2 Arquitectura del sistema

~~~mermaid
flowchart LR
    U[Usuario] --> A[Alexa es-MX]
    A --> LA[Lambda de Alexa]
    LA --> B[(AlexaThingBindings)]
    LA <-->|Get y Update| SH[Shadow clásico]
    C[ESP32 control] -->|MQTT TLS reported| SH
    N[ESP32 sensores] <-->|ESP-NOW v2| C
    C -->|Eventos MQTT TLS| R[ChapaEventosRule]
    R --> L[Lambda de almacenamiento]
    L --> D[(ChapaHistory)]
    D -.-> M[Mockups de reportes]
~~~

### 2.3 Circuito previsto

~~~mermaid
flowchart LR
    Sensor[ESP32 sensores 3,3 V]
    RFID[RC522 SPI]
    Reed[Contacto Reed]
    Control[ESP32 control]
    Servo[SG90]
    V5[Fuente regulada 5 V]
    GND[Masa común con el ESP32 de control]

    Sensor ---|GPIO21 a SDA SS| RFID
    Sensor ---|GPIO18 a SCK| RFID
    Sensor ---|GPIO23 a MOSI| RFID
    Sensor ---|GPIO19 a MISO| RFID
    Sensor ---|GPIO22 a RST| RFID
    Sensor ---|3V3 y GND| RFID
    Sensor ---|GPIO27 con pull-up interno| Reed
    Reed ---|Otro terminal a GND| Sensor
    Control ---|GPIO13 a señal| Servo
    V5 ---|5 V a VCC| Servo
    Servo --- GND
    Control --- GND
~~~

El esquema refleja pines del código y el montaje propuesto. Para el servo, usar 5 V regulados y masa común con el controlador. Verificar corriente, recorrido y retención del mecanismo antes de acoplarlo. Para el Reed, confirmar la polaridad configurada y la estabilidad de 100 ms. La comunicación ESP-NOW es por radio entre las placas.

### 2.4 Clases del objeto y módulos Lambda

~~~mermaid
classDiagram
    class DoorSensor {
        +update()
        +state()
    }
    class RfidReader {
        +read()
    }
    class LockController {
        +updateDoor()
        +processUid()
        +processAction()
        +setMode()
        +setAutoLock()
        +onEvent()
    }
    class LockServo {
        +lock()
        +unlock()
    }
    class CredentialList {
        +add()
    }
    class AwsTelemetry {
        +record()
        +observe()
    }
    class ControlLinkManager {
        +requestLock()
        +confirmationPending()
    }
    class SensorLinkManager
    class ReliableChannel
    class AlexaHandler {
        +lambda_handler()
        +bind_user()
        +handle_intent()
        +update_desired()
    }
    class HistoryHandler {
        +lambda_handler()
        +validate_event()
        +history_table()
    }
    DoorSensor ..> SensorLinkManager : publica lectura
    RfidReader ..> SensorLinkManager : publica presentación
    SensorLinkManager --> ReliableChannel
    ControlLinkManager --> ReliableChannel
    ControlLinkManager --> LockController
    LockController --> CredentialList
    LockController ..> LockServo : callback de movimiento
    LockController ..> AwsTelemetry : eventos y estados
    AlexaHandler ..> AwsTelemetry : contrato reported
    AwsTelemetry ..> HistoryHandler : contrato de evento
~~~

`AwsTelemetry` publica sin recibir órdenes. Los dos handlers Python son módulos funcionales representados en el diagrama para mostrar responsabilidades.

### 2.5 Secuencias

~~~mermaid
sequenceDiagram
    actor U as Usuario
    participant A as Alexa
    participant L as Lambda Alexa
    participant D as AlexaThingBindings
    participant I as Registro IoT
    participant S as Shadow
    participant C as ESP32 control
    participant V as Actuador virtual con Thing propio

    U->>A: Vincula mi chapa con el número uno
    A->>L: VincularIntent y userId
    L->>I: DescribeThing
    L->>D: Guarda vínculo por userId
    L-->>U: Confirma vínculo
    C->>S: Publica reported por MQTT TLS
    U->>A: Cómo está la puerta
    A->>L: EstadoPuertaIntent
    L->>D: Resuelve el Thing
    L->>S: GetThingShadow
    S-->>L: Reported con antigüedad
    L-->>U: Estado de la puerta o falta de dato reciente
    Note over A,V: Prueba de órdenes separada con vínculo al Thing virtual
    U->>A: Bloquea la chapa virtual
    A->>L: CerrarIntent
    L->>D: Resuelve el Thing
    L->>S: Consulta y escritura de desired con versión
    S-->>V: Delta para el Thing virtual
    V->>V: Evalúa interbloqueos simulados
    V->>S: Reporta resultado virtual y consume la orden
~~~

~~~mermaid
sequenceDiagram
    participant C as ESP32 control
    participant R as ChapaEventosRule
    participant L as Lambda histórico
    participant D as ChapaHistory

    C->>C: Genera ID estable antes de publicar
    C->>R: MQTT chapa/THING/events
    R->>L: Mensaje y metadatos derivados de la regla
    L->>L: Valida tipo, rango e identidad
    alt Evento válido
        L->>D: PutItem con condición
        D-->>L: insertado o duplicado
    else Evento inválido
        L->>L: Registra motivo y descarta
    end
~~~

~~~mermaid
sequenceDiagram
    participant R as ESP32 sensores y Reed
    participant C as LockController
    participant M as Servo
    participant E as Eventos

    R->>C: Puerta cerrada y lectura vigente
    C->>C: Inicia autocierre si está desbloqueada y en automático
    alt Puerta abierta, sensor vencido o salida de automático
        C->>C: Cancela temporizador
    else Vence con condiciones válidas
        C->>R: STATE_REQUEST por ESP-NOW
        alt Respuesta oportuna, correlacionada y cerrada
            R-->>C: STATE_RESPONSE cerrado
            C->>M: Solicita bloqueo
            M-->>C: Resultado de la orden PWM
            C->>E: Movimiento Edge o fallo de PWM
        else Lectura abierta, cambio de sesión o timeout
            C->>C: Cancela o descarta la confirmación
        end
    end
~~~

### 2.6 Estados del autocierre

~~~mermaid
stateDiagram-v2
    [*] --> Espera
    Espera --> Cuenta: Automático y desbloqueada con Reed cerrado vigente
    Cuenta --> Espera: Se abre la puerta o vence el Reed
    Cuenta --> Espera: Cambia a manual o bloqueado
    Cuenta --> Cuenta: Cambia retardo y reinicia intervalo
    Cuenta --> Confirmar: Vence y se mantienen condiciones
    Confirmar --> Ejecutar: STATE_RESPONSE cerrado y correlacionado
    Confirmar --> Espera: Puerta abierta, sesión nueva o vence plazo
    Ejecutar --> Espera: Registrar movimiento o fallo
~~~

Manual desactiva el temporizador. Automático inicia la cuenta según Reed y estado de cerradura. Bloqueado inhibe desbloqueos y cancela el autocierre activo. El Edge aplica primero la configuración y luego evalúa movimientos. El cierre requiere Reed cerrado y vigente; los comandos remotos no eluden ese control. El autocierre reside en el Edge por latencia, disponibilidad offline y porque sus entradas son locales.

### 2.7 Contrato del Device Shadow

Shadow clásico bajo `$aws/things/<thingName>/shadow`. Publicar a `get` y `update`. Suscribirse a `get/accepted`, `get/rejected`, `update/delta`, `update/accepted` y `update/rejected`. En reconexión recuperar la versión actual y reconciliar antes de aplicar movimientos.

| Sección | Atributos y tipos |
|---|---|
| `desired` persistente | `modo` string: `manual`, `automatico`, `bloqueado`. `autocierre_s` entero 5–120 |
| `desired` puntual | `orden` con `id` string, `accion` igual a `bloquear` o `desbloquear`, `vence_ms` entero UTC. La Lambda usa un plazo de 30 s |
| `reported` | `puerta`: `abierta`, `cerrada` o `desconocida`. `cerradura`: `bloqueada`, `desbloqueada` o `desconocida`. Reportar `modo`, `autocierre_s`, `reportado_ms` UTC y `ordenResultado` con `id` y `resultado` |
| `delta` | Diferencias pendientes. Aplicar primero configuración. Validar ID, plazo, modo y Reed para movimientos. Publicar resultado y retirar `desired.orden` usando versión para no retirar una orden nueva |

La Lambda local considera vigente un snapshot de hasta 60 s con tolerancia de 5 s para un reloj adelantado. El controlador aplica una vigencia local de 3 s y una ventana de confirmación de 750 ms para el bloqueo por eventos locales. La tarea MQTT deja de publicar snapshots si el lazo de control no le entrega observaciones nuevas en un segundo. Un Shadow previo sin `reportado_ms` se trata como desconocido. Los casos de órdenes válidas, caducadas y contradictorias corresponden al dispositivo virtual.

### 2.8 IoT Rule, Lambdas y DynamoDB

La [regla propuesta](../aws/config/iot-rule.proposed.json) conserva el filtro `chapa/+/events` y la acción `chapa-almacenar-evento`. Proyecta explícitamente campos admitidos y deriva `thingName` mediante `topic(2)` y `ts` mediante `timestamp()`. Así el cuerpo no suplanta esos dos metadatos. El publicador debe asignar `eventId` como `<epoch UTC de 13 dígitos>#<32 hex minúsculas>` y conservarlo al reintentar. El reloj debe estar sincronizado para asignar una hora fiable. La política de cola debe preservar los IDs ya emitidos y declarar cómo maneja eventos adquiridos antes de la primera sincronización.

| Función | Disparador y responsabilidad |
|---|---|
| [`chapa-almacenar-evento`](../aws/lambda/chapa-almacenar-evento/lambda_function.py) | La regla IoT. Valida tipos y rangos, descarta mensajes inválidos y conserva una fila por ID. Propaga errores transitorios para admitir reintento |
| [`chapa-skill-alexa`](../aws/lambda/chapa-skill-alexa/lambda_function.py) | Alexa. Valida slots, administra vínculos, consulta estado vigente y escribe `desired` con control de versión |
| Lógica automática | `LockController` en el ESP32 de control. La regla y las Lambdas existentes cubren las responsabilidades de Nube del caso de uso |

| Tabla | Claves y atributos | Acceso |
|---|---|---|
| `AlexaThingBindings` | PK `userId` string. `thingName` y `fechaVinculacion` string. Bajo demanda, sin índices secundarios, TTL deshabilitado | `GetItem`, `PutItem` y `DeleteItem` por usuario |
| `ChapaHistory` | PK `thingName` string. SK `eventKey` string con prefijo ISO UTC y sufijo estable. `eventId`, `ts`, `ingresoTs`, estados, tipo, origen y resultado string. `seq`, `autocierre_s` y `ttl` numéricos. `diferido` booleano. Bajo demanda, sin índices secundarios. TTL 30 días desde el ingreso registrado | Escritura condicional por clave. `Query` por objeto y fecha para los reportes. Las filas antiguas se pueden seguir consultando |

### 2.9 Alexa y reportes

Skill existente **chapa inteligente**, locale `es-MX`. El [modelo original](../aws/alexa/interaction-model/es-MX.json) conserva sus frases, tipos y slots. Ejemplos: «vincula mi chapa con el número uno», «cómo está la puerta», «bloquea la chapa», «pon la chapa en modo automático» y «cambia el autocierre a diez segundos». Los slots `numeroSerie` y `segundos` usan `AMAZON.NUMBER`, y `modo` usa `ModoOperacion`. Su código de Lambda se versionó localmente. El endpoint y el despliegue siguen con la configuración auditada. Los [mockups RF15](mockups-reportes.md) especifican la cronología y el resumen por origen.

## 3. Desarrollo e implementación

- `firmware/esp32_sensors`: Reed, RFID, latidos y respuestas de estado. `firmware/esp32_control`: autorización, servo y confirmación asíncrona del cierre. `shared/Protocol`: tramas v2, sesiones, ACK, deduplicación y transporte ESP-NOW. El adaptador AWS deberá mapear `Mode::BLOCKED` a `bloqueado`, `Event.sequence` a `seq` y los demás enums al contrato externo. El estado de ejecución viene de `Event.result` y el ID persistente de telemetría se genera aparte de la secuencia local.
- [Configuración observada y sanitizada](../aws/config/deployed-sanitized.json): Thing, política v2 con cuenta omitida, regla activa con SQL explícita, runtime y handler de ambas Lambdas, hashes observados, permisos de invocación, rol de ejecución y definiciones de tablas. La acción IoT usa un permiso basado en el recurso Lambda. La auditoría no identificó un rol de ejecución de regla para esta acción.
- [Política IoT propuesta](../aws/config/iot-policy.proposed.json): referencia histórica con suscripciones y recepción. La política vigente v2 es de solo publicación y ya cubre los dos tópicos usados por el firmware. [Roles propuestos](../aws/config/lambda-roles.proposed.json): separan escrituras de histórico, operaciones de vinculación, Shadow y logs; siguen sin aplicarse. El despliegue usa `PowerUserRole`; su amplitud se documenta en la exportación.
- [Plantilla C++ del ESP32](../firmware/esp32_control/include/aws_telemetry_config.example.h): copiar a `aws_telemetry_config.h`, excluido de Git. El firmware usa esa cabecera. La plantilla JSON auditada documenta nombres y parámetros y la del laboratorio se usa con el cliente virtual.

## 4. Pruebas y validaciones

Desde la raíz del repositorio:

~~~powershell
python -B -m unittest discover -s tests -p 'test_aws_integration.py' -v
& "$env:USERPROFILE/.platformio/penv/Scripts/python.exe" tests/run_tests.py
$pio = "$env:USERPROFILE/.platformio/penv/Scripts/platformio.exe"
& $pio run -d firmware/esp32_control -e esp32dev -e simulation -e calibration
& $pio run -d firmware/esp32_sensors -e esp32dev -e diagnostic
~~~

La suite nueva [`test_aws_integration.py`](../tests/test_aws_integration.py) importa los handlers locales, sustituye boto3 y simula la proyección de la regla, el control de versión de Shadow, la escritura condicional y el actuador. Cubre órdenes con estado desconocido y obsoleto, conflicto de modo, cierre con puerta abierta, reintentos, límites numéricos, dos usuarios y deduplicación. Los cambios de posición observados corresponden al actuador virtual.

### Verificación AWS en solo lectura del 24/11/2026

AWS CLI no está en el `PATH`; se invocó desde `$env:ProgramFiles\Amazon\AWSCLIV2\aws.exe` con el perfil `puerta-inteligente` y `us-east-2`. No se escribió en ningún recurso. Los comandos y su resultado se detallan en [revision-aws.md](revision-aws.md).

| Servicio | Observación verificada |
|---|---|
| Shadow clásico | `chapa-inteligente-01` devolvió versión 28. `reported` con `puerta=desconocida`, `cerradura=desconocida`, `modo=automatico`, `autocierre_s=10` y `reportado_ms=1795557376854` (`2026-11-24T21:56:17.654Z`), 12 s antes de la consulta. `desired` conserva `modo=bloqueado` y `cerradura=desbloqueada`; hay `delta` pendiente porque el firmware no suscribe ni aplica órdenes |
| DynamoDB | Consulta consistente por `thingName`: 31 filas `tipo=lectura`, `origen=edge`, `resultado=ejecutado` y `puerta=desconocida` entre 21:44:18Z y 22:01:46Z, a intervalos de ~30 s. Cada fila tiene `eventId`, `ingresoTs` y `ttl`. Las tres filas antiguas son `tipo=acceso, origen=rfid` de los días 23 y 24 |
| CloudWatch Logs | `/aws/lambda/chapa-almacenar-evento` registra START/REPORT continuos desde 21:44:20Z hasta 21:54:47Z en la ventana consultada, con duraciones de 25–2778 ms. `/aws/lambda/chapa-skill-alexa` no registró invocaciones después de las 03:30Z |
| Configuración AWS | Política activa v2, restringida a `client/chapa-inteligente-01` y a dos tópicos de publicación. Regla activa con SQL explícita para `chapa/chapa-inteligente-01/events`. `chapa-almacenar-evento` tiene `CodeSha256=OwGJItvs…`, distinto del valor auditado antes; `chapa-skill-alexa` conserva el valor previo. TTL de `ChapaHistory` habilitado en `ttl` |

El formato de `eventId` y la ausencia de `diferido` son compatibles con el publicador del ESP32, y la temporización coincide con `REPORT_MS = 30000`. Esa compatibilidad no identifica al emisor ni demuestra que el Reed esté vigente. Para atribuirlo se necesita el monitor serie y una lectura `abierta`/`cerrada` correlacionada por hora.

**Pendientes de la demo.** Comprobar reconocimiento de frases en Alexa Developer Console, conexión TLS y aplicación real de Shadow, captura con el enlace, recorrido del SG90, exactitud Reed/RFID, estabilidad, respuesta por voz, latencia e integridad de almacenamiento, autocierre, desconexión y recuperación. Obtener capturas y muestras anonimizadas. Ninguna medición física se da por realizada.

## 5. Resultados y estimación mensual

| Verificación | Resultado observado |
|---|---|
| Suite de firmware del merge, Zig C++ | **85/85** con hardware simulado en ocho ejecutables. Incluye controlador, sensores, protocolo, enlace y tres variantes de consola |
| Integración local de las Lambdas corregidas | **16/16** pruebas, con casos parametrizados. La misma publicación con dos tiempos de ingreso produjo una fila. Una orden válida movió el actuador virtual y dejó confirmación e histórico. Órdenes vencidas o incompatibles no lo movieron |
| Ampliación de la demo simulada | **20/20** pruebas en total y el caso de voz ejecutado también por separado. Los cinco mensajes de la muestra, publicados dos veces cada uno, dejaron cinco filas únicas con su origen. La orden pendiente se aplicó una vez tras la reconexión virtual. El modo bloqueado prevaleció sobre una orden simultánea. Se comprobó la correspondencia de los tópicos con los ARN de la política propuesta |
| Pruebas previas del backend auditado | **37 casos simulados**. Sirven de línea base para slots, formato de solicitudes y defectos observados antes de las correcciones |
| Compilación PlatformIO del merge ESP-NOW | **5/5** como línea base, antes del entorno `aws_telemetry` |
| Histórico auditado en DynamoDB | Tres filas leídas para el Thing en una consulta limitada. Las tres tenían `tipo`, `origen`, `resultado` y `puerta`. Una incluía `cerradura` y una incluía `seq` y `diferido`. Se registraron atributos y tipos; esta evidencia no atribuye origen físico ni mide rendimiento |
| Verificación AWS del 24/11/2026 | 31 lecturas `edge` recientes a ~30 s, Shadow actualizado 12 s antes de la consulta con estado desconocido, invocaciones recientes de la Lambda de histórico y TTL activo. Latencia `ts`→`ingresoTs` de 0,695–2,413 s. No hay invocaciones recientes de Alexa ni evidencia de Reed vigente |
| Pruebas repetidas al cierre | **25/25 pruebas Python** con `paho-mqtt==2.1.0` y OpenSSL de Git en el `PATH`, incluida la prueba MQTT/TLS en loopback. El ejecutable de C++ no pudo relanzarse por `PermissionError` al leer `.pio/host-tools/ziglang/zig.exe`; se conserva el resultado histórico de 91/91, no se reclama una repetición |

**Escenario mensual.** Un ESP32 conectado 30 días, un respaldo cada 30 s, 100 eventos adicionales diarios, aproximadamente 270.000 mensajes MQTT, 89.400 operaciones de Shadow para las publicaciones periódicas y de cambio, 89.400 reglas y acciones, 90.000 invocaciones Lambda de 128 MB y 200 ms, unas 90.000 escrituras bajo demanda y 2.000 unidades de lectura. A tarifas públicas de referencia, el núcleo cuesta aproximadamente **USD 0,52/mes** antes de créditos, logs, impuestos y tráfico adicional de reconexión. Los conteos de IoT están por debajo de las referencias publicadas de 500.000 mensajes, 225.000 operaciones Shadow y 250.000 reglas/acciones cuando aplica ese beneficio. Lambda queda por debajo de 1 millón de solicitudes y 400.000 GB·s. Las tablas están bajo demanda: las cuotas de capacidad aprovisionada no se descuentan de estas solicitudes. El proyecto debe confirmar su plan y consumo antes de informar un costo efectivo. [IoT Core](https://aws.amazon.com/iot-core/pricing/) · [Lambda](https://aws.amazon.com/lambda/pricing/) · [DynamoDB](https://aws.amazon.com/dynamodb/pricing/)

### Verificación nueva de MQTT/TLS con dispositivo virtual

- **Transporte.** La prueba abrió un socket de loopback con TLS mutuo y certificados X.509 efímeros. El cliente MQTT 3.1.1 esperó la confirmación de suscripción, solicitó el Shadow clásico con `/get`, recibió la configuración, publicó `reported` y recibió el acuse QoS 1 de la lectura. El servidor de prueba simuló las respuestas de AWS IoT. No hubo conexión al proyecto AWS.
- **Contrato.** Un delta de versión 4 recibido antes de la instantánea de versión 3 se aplicó después de la instantánea. El cliente reportó modo `manual`, autocierre de 21 s, puerta cerrada simulada y cerradura desconocida. Descartó valores inválidos y comandos de movimiento. La prueba con las dos Lambda locales confirmó que Alexa puede leer el estado reportado y que reenviar el mismo ID produce una única fila en el sustituto de DynamoDB. Un PUBACK acredita recepción por el broker.
- **WiFi y ESP-NOW.** La revisión de `EspNowLink::begin()` confirmó que fija el canal de radio y el del peer. Ambas plantillas seleccionan el canal 1. Para coexistir con WiFi STA, ambos nodos y el punto de acceso de 2,4 GHz deben usar el mismo canal. Queda pendiente comprobarlo con las placas, incluida la reconexión WiFi y la continuidad de latidos y solicitudes de estado. [FAQ de Espressif, §3.7.3](https://docs.espressif.com/projects/esp-faq/en/latest/esp-faq-en-master.pdf)
- **Línea base.** **25/25 pruebas Python**, incluida la prueba MQTT con TLS real en loopback, **85/85 pruebas de firmware** y **5/5 compilaciones PlatformIO**, antes de añadir el entorno de telemetría al ESP32 de control.

Para repetir la verificación local, usar Python 3.13 y OpenSSL en el PATH o indicado en `OPENSSL`:

~~~powershell
python -m pip install -r aws/virtual_device/requirements.txt
python -B -m unittest discover -s tests -p 'test_*.py' -v
~~~

El [cliente de telemetría](../aws/virtual_device/telemetry.py) y la
[plantilla sin secretos](../aws/config/virtual-device.example.json) quedan listos para un proyecto aislado ya aprovisionado. Cuando se autorice esa prueba, utilizar un Thing de laboratorio `chapa-inteligente-9xxx`, sus propios certificados y permisos limitados a sus tópicos. Completar la configuración en `virtual-device.local.json`, excluido de Git. El endpoint ATS en puerto 8883 requiere el argumento explícito `--allow-isolated-aws`. La persistencia real se verifica por `eventId` en DynamoDB del entorno aislado. [Protocolos de AWS IoT Core](https://docs.aws.amazon.com/iot/latest/developerguide/protocols.html)

### Integración de telemetría en el ESP32 de control

- `aws_telemetry` publica QoS 1 con ESP-MQTT y TLS mutuo, reloj SNTP, el certificado del dispositivo y verificación del servidor. El lazo de control entrega copias a una cola de 24 eventos en RAM. La tarea de red limita su bandeja de salida y cede el lazo local mientras asocia o reconecta. ESP-MQTT conserva el payload y el ID al retransmitir. La cola puede perder eventos si se llena o se reinicia la placa; la integridad se mide en DynamoDB.
- Publica eventos individuales y respaldo Reed cada 30 s en `chapa/<thingName>/events`. Publica el estado al cambiar, al conectar y cada 30 s en `$aws/things/<thingName>/shadow/update`. Incluye `reportado_ms` y distingue estado desconocido. El acceso RFID se registra por tipo, origen y resultado, sin UID ni identificador de tarjeta.
- La rama local ya contiene las referencias remotas verificadas por fetch. Se completaron **91/91 pruebas C++**, incluidas seis del serializador de firmware, **25/25 pruebas Python** y **6/6 compilaciones PlatformIO**. ESP-MQTT se enlazó también con la ruta habilitada y valores ficticios, usando 929.401 bytes de flash sobre 1.310.720 disponibles. Se conserva la evidencia simulada de las Lambda; aún falta comprobarlas con los mensajes de la placa en AWS.

**Configuración y firmware.** Mantener el ESP32 de sensores con su binario ESP-NOW actual. Copiar la plantilla C++ para el ESP32 de control, configurar SSID, contraseña, BSSID del AP fijo de 2,4 GHz, URI ATS `mqtts://…:8883`, Thing, CA, certificado y clave en PEM y habilitar `ENABLED`. Mantener la misma configuración ESP-NOW de ambos peers. Compilar exclusivamente para el controlador con `pio run -d firmware/esp32_control -e aws_telemetry`. El binario por defecto sigue siendo la variante local; el nuevo entorno también arranca localmente si falta configuración privada. Mantener fuera del repositorio la cabecera privada y el binario que contiene las credenciales.

**Estado de los cambios AWS.** La lectura del 24/11/2026 confirma que ya están aplicados la política de solo publicación (v2), la proyección explícita de la regla limitada a `chapa/chapa-inteligente-01/events` y un paquete nuevo en `chapa-almacenar-evento` (`CodeSha256=OwGJItvs…`). No se han vuelto a aplicar desde esta revisión. Falta correlacionar ese despliegue con el código local para afirmar que la deduplicación corregida está activa, y verificar con el monitor serie que el certificado usado corresponde al ESP32 de control. El backend Alexa sigue con el hash anterior; no se ha autorizado su actualización. Realizar primero consultas de puerta y cerradura con la skill y el Thing autorizado. Reservar comandos y cambios de configuración por voz para el Thing virtual.

**Validación manual.** Con el SG90 aislado durante la prueba remota, comprobar que el AP y ambos peers usan el mismo canal, que el Reed sigue vigente mientras se conecta el cliente y que el enlace se recupera tras desconectar y restablecer el AP. Comprobar un cambio Reed y una lectura periódica en Shadow y DynamoDB, el origen de eventos RFID/Edge, latencia e integridad por ID, y las consultas de Alexa. Probar el servo bajo supervisión con disparos locales. Registrar si se pierden latidos o confirmaciones durante la asociación o si aumenta el contador de descartes. La publicación reciente ya está documentada; estas mediciones físicas siguen pendientes.

## 6. Conclusiones

El firmware de control incorpora la publicación MQTT/TLS de eventos y Shadow y hay tráfico reciente en AWS: Shadow actualizado, 31 lecturas almacenadas e invocaciones de la Lambda de histórico. Eso acredita el flujo de publicación y persistencia, no la lectura física del Reed ni la actuación remota: el estado publicado es `desconocido` y el firmware no aplica `desired`. Para cerrar la demo integrada faltan la atribución del emisor por monitor serie, una lectura vigente, las consultas por voz y las mediciones del montaje. La actuación por voz se mantiene en el Thing virtual.

## 7. Recomendaciones

Completar la integración con una lectura Reed vigente, consulta por voz de un estado reciente y corte WiFi supervisado. Correlacionar el evento publicado con el monitor serie y conservar la política y el paquete de almacenamiento actuales. Ejecutar las órdenes únicamente sobre el Thing virtual mientras no se autorice explícitamente el control remoto.

## 8. Anexos

- [Modelo original Alexa y frases en español](../aws/alexa/interaction-model/es-MX.json).
- [Exportación sanitizada](../aws/config/deployed-sanitized.json) y [mockups RF15](mockups-reportes.md).
- [Pruebas reproducibles con actuador virtual](../tests/test_aws_integration.py).
- [Guía para revisar el proyecto AWS en solo lectura](revision-aws.md).
- [Plan de cierre, validación aislada y lista de comprobación física](cierre-practica-4.md).
- Capturas recibidas y su valor probatorio: inventario al final de este documento.

### Inventario de capturas recibidas

Las 11 imágenes de `evidencias/` se revisaron una por una; ninguna muestra una invocación de Alexa, un Reed vigente, un servo en movimiento ni una medición de latencia física.

| Captura | Acredita | No acredita |
|---|---|---|
| `thing.png` | Thing `chapa-inteligente-01` y su vista general | Certificado activo, conectividad en vivo ni canal WiFi |
| `vinculacion.png` | Tabla `AlexaThingBindings`, clave `userId`, capacidad bajo demanda | Pares usuario-Thing, alta real ni persistencia entre sesiones |
| `shadow.png` | Estado de ejemplo con `puerta=cerrada` y delta pendiente | Hora de la captura, vigencia del dato ni origen físico |
| `reglasIOT.png` | Regla activa, tópico `chapa/+/events` y acción Lambda | SQL actual ni invocación real; AWS hoy muestra una regla más restrictiva |
| `politicaIOT.png` | Existencia de la política y versión activa 1 en ese momento | ARN correctos; AWS hoy muestra la versión 2 |
| `lambdaAlmacenamiento.png` | Función de histórico, rol y desencadenador IoT | Código desplegado ni hash; AWS hoy muestra otro hash |
| `lambdaCodigoAlmacenamiento.png` | Código local de validación del histórico | Que ese código esté desplegado |
| `registrosAlmacenados.png` | Tres filas `rfid` en DynamoDB con `resultado` y `puerta` | Eventos recientes, latencia ni integridad |
| `dynamoHistorico.png` | Vista de `ChapaHistory` y tres elementos | Origen de las filas ni medición de rendimiento |
| `intents.png` | 15 intents del modelo y build correcto del 23/11/2026 | Ejecución de frases ni respuestas de Alexa |
| `modoOperacion.png` | Slot `ModoOperacion` con sinónimos | Cambio de modo por voz ni aplicación en la placa |

Sigue pendiente la captura de la verificación AWS en solo lectura (Shadow, DynamoDB y CloudWatch) con hora UTC, porque los datos consultados por CLI no tienen imagen en `evidencias/`.

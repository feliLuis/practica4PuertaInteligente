# Práctica 4 — Puerta Inteligente

**Asignatura** SIS-234 · **Fecha** 24 de noviembre de 2026

**Bases integradas** `cd7ffe9` (ESP-NOW) y `0fe5377` (AWS y entrega) · **Rama de trabajo** `feat/esp32-sensores`

**Estado** ESP-NOW y RFID operativos en el montaje según el equipo. Telemetría MQTT/TLS de solo publicación implementada en el ESP32 de control. Conexión del nuevo binario a AWS y mediciones del montaje pendientes.

## 1. Caso de uso y requerimientos

La puerta permite comprobar si está cerrada, autorizar una presentación RFID, consultar el estado por voz y accionar una cerradura de baja tensión. El Reed informa la posición de la puerta. El RC522 reconoce tarjeta y llavero. El SG90 representa la posición del mecanismo de cierre. La Práctica 4 añade histórico de lecturas y movimientos, autocierre en el Edge y consulta y ajuste por voz del modo y del temporizador. El Reed no informa la posición del pestillo y el SG90 no tiene realimentación mecánica en el montaje actual. La estimación publicada se debe comparar con la observación del mecanismo en la demo.

El ESP32 de control es el único cliente AWS previsto para el montaje. El ESP32 de sensores entrega Reed, RFID y latidos por ESP-NOW unicast con protocolo v2. `AwsTelemetry` copia los eventos y los estados de `LockController` y publica en una tarea separada. El bloqueo local conserva la confirmación con `ControlLinkManager`. El firmware físico carece de suscripciones MQTT y las órdenes de voz se prueban con el dispositivo virtual existente.

### Matriz RF1–RF15

| RF | Requisito | Implementación y evidencia local | Evidencia pendiente |
|---|---|---|---|
| RF1 | Capturar datos de sensores | `DoorSensor` y `RfidReader` probados en escritorio. El equipo informa funcionamiento local por RFID | Incorporar mediciones supervisadas de polaridad, rebote y presentaciones |
| RF2 | Reportar sensor y actuador periódicamente | El ESP32 de control implementa publicación por cambio y respaldo cada 30 s. RFID queda como evento de acceso; Reed y cerradura estimada quedan en `reported` | Verificar publicación e intervalos desde la placa; decidir con el docente si requiere el último resultado RFID en Shadow |
| RF3 | Aplicar delta y confirmar en reported | Confirmación ESP-NOW de bloqueo local y recorrido de órdenes con actuador virtual comprobados por separado | Demostrar el recorrido remoto sobre el Thing virtual autorizado |
| RF4 | Consultar sensor del Thing por voz en español | `EstadoPuertaIntent` consume el campo `puerta` que ahora genera el ESP32 de control | Probar frases en Alexa Console con el Reed publicado desde la placa. La skill no ofrece una consulta de RFID |
| RF5 | Ordenar movimientos por voz mediante desired | `AbrirIntent` y `CerrarIntent` escriben orden con ID y vencimiento; recorrido comprobado localmente | Verificar voz y confirmación sobre el Thing virtual |
| RF6 | Consultar el estado del actuador por voz | Lee cerradura estimada de snapshot vigente | Probar frases y cotejar estado estimado con posición real |
| RF7 | Vincular y persistir entre sesiones | Slot entero y DynamoDB; alta verificada con sustituto | Persistencia en sesiones reales |
| RF8 | Resolver por usuario y orientar si falta vínculo | Dos usuarios y dos Things simulados; consultas independientes | Probar en Alexa Console con recursos autorizados |
| RF9 | Comprobar existencia, cambiar y eliminar vínculo | Se probaron alta, sustitución, Thing inexistente y baja | Demostración integrada |
| RF10 | Regla IoT invoca Lambda con datos del objeto | El firmware genera el tópico de eventos y el contrato de la regla existente, que fue auditada | Autorizar política y proyección SQL corregidas y registrar una invocación desde la placa |
| RF11 | Validar, enriquecer y almacenar lecturas | Lambda local descarta inválidos, enriquece y escribe condicionalmente | Medir resultados contra DynamoDB y publicaciones reales |
| RF12 | Historial de movimientos con su origen | El firmware distingue movimientos RFID/Edge de solicitudes rechazadas. Voz tiene evidencia virtual | Capturar cambios locales por RFID y Edge y corroborar los registros. Comprobar voz en el Thing virtual |
| RF13 | Lógica automática y justificación Edge/Nube | Autocierre y confirmación de cierre por ESP-NOW cubiertos con reloj y enlace simulados | Demostrar topes, cortes y conflictos con voz en el montaje |
| RF14 | Reportar, consultar y modificar modo y parámetros por voz | El ESP32 reporta modo y tiempo. El cambio y su confirmación se verificaron con el dispositivo virtual | Consultar estados físicos en Alexa y demostrar cambios en el Thing virtual |
| RF15 | Mockups basados en DynamoDB | [Dos mockups con consultas y decisiones de usuario](mockups-reportes.md) | Incorporar ejemplos anonimizados obtenidos en la demo |

### Requerimientos no funcionales y aceptación

| Atributo | Meta inicial del grupo o referencia docente | Evidencia actual y prueba faltante |
|---|---|---|
| Estabilidad | ≥10 min continuos sin reinicio ni bloqueo | Pendiente. Usar tráfico mixto de lectura, voz y autocierre |
| Exactitud | Reed: concordancia con posición observada en una serie de ciclos. RFID: medir aciertos, omisiones y repeticiones por presentación dentro del rango de prueba declarado | Pendiente declarar tamaño de muestra, tolerancia y medir ambas entradas |
| Respuesta a voz | ≤3 s de solicitud a movimiento | El flujo lógico está simulado; medir de Alexa Console al servo con reloj común |
| Frecuencia | Cambio discreto ≤1 s y respaldo cada 30 s | Diseño inicial. Medir intervalos en Shadow e histórico y ajustar con resultados |
| Reconexión | Restablecer WiFi/MQTT automáticamente y aplicar `desired` pendiente válido | Simulación de reconciliación; falta corte real y verificación de estado, vencimiento y versión |
| Integridad | ≥99 % de eventos válidos únicos publicados quedan almacenados, con descarte de inválidos | Pruebas de duplicación y validación locales; contar IDs en una ventana declarada |
| Latencia de almacenamiento | ≤5 s máximo de publicación a registro | Pendiente. Medir con tiempos sincronizados, diferenciando la espera offline |
| Reacción automática | Medir desde vencimiento del temporizador con Reed vigente hasta movimiento | Reloj simulado. Medir físicamente y reportar aparte los segundos configurados |
| Operación offline | RFID, interbloqueos y autocierre local operativos mientras el Reed esté vigente; cola de eventos acotada | Pruebas del controlador con lecturas simuladas. Falta implementar y medir la cola y comprobar el corte de Internet con enlace local activo |
| Costo | Estimar consumo de IoT, Lambda y DynamoDB, considerando el beneficio del proyecto | Calculado en §5; falta validar consumo y beneficio aplicable |
| Seguridad y calidad | TLS, plantilla sin secretos, roles documentados y módulos separados | Configuración propuesta y fuentes locales revisados. Falta aprovisionamiento seguro, verificación de TLS y revisar IDs RFID de prototipo antes de uso real |

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
- [Configuración auditada y sanitizada](../aws/config/deployed-sanitized.json): Thing, ARN de política observado con cuenta omitida, regla, runtime y handler de ambas Lambdas, permisos de invocación, rol de ejecución y definiciones de tablas. La acción IoT usa un permiso basado en el recurso Lambda. La auditoría no identificó un rol de ejecución de regla para esta acción.
- [Política IoT propuesta](../aws/config/iot-policy.proposed.json): delimita cliente, publicaciones, suscripciones y recepción. La política vigente v1 tiene los ARN incorrectos. [Roles propuestos](../aws/config/lambda-roles.proposed.json): separan escrituras de histórico, operaciones de vinculación, Shadow y logs. El despliegue auditado usa `PowerUserRole`; su amplitud se documenta en la exportación. Las propuestas usan marcadores y requieren revisión antes de aplicarse.
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

**Cambios AWS pendientes de autorización.** Aplicar la [política de solo publicación](../aws/config/iot-policy.telemetry-only.proposed.json) al certificado del controlador, después de verificar sus asociaciones. Mantener Thing, regla y tabla. Adoptar la proyección explícita de la [regla propuesta](../aws/config/iot-rule.proposed.json) y desplegar el paquete corregido de almacenamiento en la función existente para preservar `eventId` y deduplicar. El backend Alexa desplegado reconoce los campos del reporte; para aplicar la comprobación de vigencia del código local se requiere autorizar su actualización. Realizar primero consultas de puerta y cerradura con la skill y el Thing autorizado. Reservar comandos y cambios de configuración por voz para el Thing virtual.

**Validación manual.** Con el SG90 aislado durante la prueba remota, comprobar que el AP y ambos peers usan el mismo canal, que el Reed sigue vigente mientras se conecta el cliente y que el enlace se recupera tras desconectar y restablecer el AP. Comprobar un cambio Reed y una lectura periódica en Shadow y DynamoDB, el origen de eventos RFID/Edge, latencia e integridad por ID, y las consultas de Alexa. Probar el servo bajo supervisión con disparos locales. Registrar si se pierden latidos o confirmaciones durante la asociación o si aumenta el contador de descartes. Estas mediciones siguen pendientes.

## 6. Conclusiones

El firmware de control ya incorpora la publicación MQTT/TLS de eventos y Shadow. Para la demo integrada faltan la autorización de los cambios AWS, la verificación del nuevo binario en el montaje y la evidencia de persistencia y consultas por voz. La actuación por voz se mantiene en el Thing virtual.

## 7. Recomendaciones

Completar la integración con publicación, consulta de un estado reciente y corte WiFi supervisado. Verificar la política y el paquete de almacenamiento antes de conectar el dispositivo. Ejecutar las órdenes únicamente sobre el Thing virtual.

## 8. Anexos

- [Modelo original Alexa y frases en español](../aws/alexa/interaction-model/es-MX.json).
- [Exportación sanitizada](../aws/config/deployed-sanitized.json) y [mockups RF15](mockups-reportes.md).
- [Pruebas reproducibles con actuador virtual](../tests/test_aws_integration.py).
- [Guía para revisar el proyecto AWS en solo lectura](revision-aws.md).
- [Plan de cierre, validación aislada y lista de comprobación física](cierre-practica-4.md).
- Pendientes las fotografías, capturas de la demo y muestras de histórico vinculadas a IDs de la prueba integrada.

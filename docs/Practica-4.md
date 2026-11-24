# Práctica 4 — Puerta Inteligente

**Asignatura** SIS-234 · **Fecha** 24 de noviembre de 2026

**Bases integradas** `cd7ffe9` (ESP-NOW) y `0fe5377` (AWS y entrega) · **Rama de trabajo** `feat/esp32-sensores`

**Estado** Implementación local de Lambda, configuración preparada y pruebas con clientes simulados. MQTT/TLS en el ESP32, despliegue y demo física pendientes.

## 1. Caso de uso y requerimientos

La puerta permite comprobar si está cerrada, autorizar una presentación RFID, consultar el estado por voz y accionar una cerradura de baja tensión. El Reed informa la posición de la puerta. El RC522 reconoce tarjeta y llavero. El SG90 representa la posición del mecanismo de cierre. La Práctica 4 añade histórico de lecturas y movimientos, autocierre en el Edge y consulta y ajuste por voz del modo y del temporizador. El Reed no informa la posición del pestillo y el SG90 no tiene realimentación mecánica en el montaje actual. La estimación publicada se debe comparar con la observación del mecanismo en la demo.

El ESP32 de control será el único cliente de AWS. Concentra autorización, vigencia del Reed y decisiones sobre el servo. El ESP32 de sensores entrega Reed, RFID y latidos por ESP-NOW unicast con protocolo v2. El futuro adaptador AWS consumirá las copias de `Event` y los getters de `LockController`. Para bloquear usará `ControlLinkManager::requestLock()`, que pide confirmación al nodo de sensores antes de ordenar el movimiento.

### Matriz RF1–RF15

| RF | Requisito | Implementación y evidencia local | Evidencia pendiente |
|---|---|---|---|
| RF1 | Capturar datos de sensores | `DoorSensor` y `RfidReader` implementados | Verificar polaridad, rebote, tarjetas y montaje |
| RF2 | Reportar sensor y actuador periódicamente | Snapshot y frecuencia especificados | Implementar adaptador MQTT/TLS, publicar Reed, resultado lógico RFID y estado estimado |
| RF3 | Aplicar delta y confirmar en reported | Control local con confirmación ESP-NOW y flujo AWS con actuador virtual comprobados por separado | Implementar consumidor MQTT y correlacionar la orden Shadow con el resultado de la confirmación local |
| RF4 | Consultar sensor del Thing por voz en español | `EstadoPuertaIntent` lee Reed vigente y responde a estado ausente | Probar frases en Alexa Console y decidir cómo exponer el último resultado lógico RFID como sensor adicional |
| RF5 | Ordenar movimientos por voz mediante desired | `AbrirIntent` y `CerrarIntent` escriben orden con ID y vencimiento; simulación completa | Ejecutar un comando válido con revisión de interbloqueos y medir su respuesta |
| RF6 | Consultar el estado del actuador por voz | Lee cerradura estimada de snapshot vigente | Probar frases y cotejar estado estimado con posición real |
| RF7 | Vincular y persistir entre sesiones | Slot entero y DynamoDB; alta verificada con sustituto | Persistencia en sesiones reales |
| RF8 | Resolver por usuario y orientar si falta vínculo | Dos usuarios y dos Things simulados; consultas independientes | Probar en Alexa Console con recursos autorizados |
| RF9 | Comprobar existencia, cambiar y eliminar vínculo | Se probaron alta, sustitución, Thing inexistente y baja | Demostración integrada |
| RF10 | Regla IoT invoca Lambda con datos del objeto | Acción existente auditada; proyección SQL propuesta ensayada con cliente simulado | Política MQTT válida y ejecución con el objeto |
| RF11 | Validar, enriquecer y almacenar lecturas | Lambda local descarta inválidos, enriquece y escribe condicionalmente | Medir resultados contra DynamoDB y publicaciones reales |
| RF12 | Historial de movimientos con su origen | El actuador virtual emitió y persistió un desbloqueo con origen voz | Capturar cambios reales por voz, RFID y Edge, y corroborar su origen |
| RF13 | Lógica automática y justificación Edge/Nube | Autocierre y confirmación de cierre por ESP-NOW cubiertos con reloj y enlace simulados | Demostrar topes, cortes y conflictos con voz en el montaje |
| RF14 | Reportar, consultar y modificar modo y parámetros por voz | Intents y slots existentes. Cambio y confirmación con actuador virtual | Reconocimiento en Alexa Console y aplicación desde el ESP32 |
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
    Control --> AWS[Adaptador AWS previsto]
~~~

### 2.2 Arquitectura del sistema

~~~mermaid
flowchart LR
    U[Usuario] --> A[Alexa es-MX]
    A --> LA[Lambda de Alexa]
    LA --> B[(AlexaThingBindings)]
    LA <-->|Get y Update| SH[Shadow clásico]
    C[ESP32 control] <-->|MQTT TLS| SH
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
    class AwsAdapter {
        +reconcileShadow()
        +publishEvent()
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
    AwsAdapter ..> LockController : comandos y eventos
    AwsAdapter ..> ControlLinkManager : solicitud de bloqueo
    AlexaHandler ..> AwsAdapter : contrato Shadow
    AwsAdapter ..> HistoryHandler : contrato de evento
~~~

`AwsAdapter` identifica el componente previsto. Los dos handlers Python son módulos funcionales representados en el diagrama para mostrar responsabilidades.

### 2.5 Secuencias

~~~mermaid
sequenceDiagram
    actor U as Usuario
    participant A as Alexa
    participant L as Lambda Alexa
    participant D as AlexaThingBindings
    participant I as Registro IoT
    participant S as Shadow
    participant C as Controlador
    participant N as ESP32 sensores
    participant M as Servo

    U->>A: Vincula mi chapa con el número uno
    A->>L: VincularIntent y userId
    L->>I: DescribeThing
    L->>D: Guarda vínculo por userId
    L-->>U: Confirma vínculo
    U->>A: Cómo está la puerta
    A->>L: EstadoPuertaIntent
    L->>D: Resuelve el Thing
    L->>S: GetThingShadow
    S-->>L: Reported con antigüedad
    L-->>U: Estado de la puerta o falta de dato reciente
    U->>A: Bloquea la chapa
    A->>L: CerrarIntent
    L->>D: Resuelve el Thing
    L->>S: Consulta y escritura de desired con versión
    S-->>C: Delta ordenado por versión
    C->>C: Aplica modo y solicita confirmación de cierre
    C->>N: STATE_REQUEST por ESP-NOW
    N-->>C: STATE_RESPONSE correlacionado
    C->>M: Emite PWM si la respuesta confirma cerrado
    C->>S: Reporta resultado y consume la orden
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

La Lambda considera vigente un snapshot de hasta 60 s con tolerancia de 5 s para un reloj adelantado. El controlador aplica una vigencia local de 3 s y una ventana de confirmación de 750 ms para el bloqueo. El adaptador deberá correlacionar el ID de Shadow con el resultado local. Si `requestLock()` devuelve éxito mientras la confirmación sigue pendiente, se espera movimiento, rechazo o vencimiento antes de confirmar. Un Shadow previo sin `reportado_ms` se trata como desconocido y la simulación ignora `desired.cerradura` legado. En reconexión se comprobarán órdenes válidas, caducadas y contradictorias.

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
- [Plantilla del objeto](../aws/config/device.example.json): completar localmente en `device.local.json`, que está ignorado junto con los certificados, claves y `secrets.h`. La plantilla describe parámetros para el futuro adaptador.

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
| Compilación PlatformIO | **5/5**. Control: `esp32dev`, `simulation` y `calibration`. Sensores: `esp32dev` y `diagnostic`. Las compilaciones incluyen ESP-NOW. El adaptador AWS está pendiente |
| Histórico auditado en DynamoDB | Tres filas leídas para el Thing en una consulta limitada. Las tres tenían `tipo`, `origen`, `resultado` y `puerta`. Una incluía `cerradura` y una incluía `seq` y `diferido`. Se registraron atributos y tipos; esta evidencia no atribuye origen físico ni mide rendimiento |

**Escenario mensual.** Un ESP32 conectado 30 días, un respaldo cada 30 s, 100 eventos adicionales diarios, aproximadamente 270.000 mensajes MQTT, 89.400 operaciones de Shadow para las publicaciones periódicas y de cambio, 89.400 reglas y acciones, 90.000 invocaciones Lambda de 128 MB y 200 ms, unas 90.000 escrituras bajo demanda y 2.000 unidades de lectura. A tarifas públicas de referencia, el núcleo cuesta aproximadamente **USD 0,52/mes** antes de créditos, logs, impuestos y tráfico adicional de reconexión. Los conteos de IoT están por debajo de las referencias publicadas de 500.000 mensajes, 225.000 operaciones Shadow y 250.000 reglas/acciones cuando aplica ese beneficio. Lambda queda por debajo de 1 millón de solicitudes y 400.000 GB·s. Las tablas están bajo demanda: las cuotas de capacidad aprovisionada no se descuentan de estas solicitudes. El proyecto debe confirmar su plan y consumo antes de informar un costo efectivo. [IoT Core](https://aws.amazon.com/iot-core/pricing/) · [Lambda](https://aws.amazon.com/lambda/pricing/) · [DynamoDB](https://aws.amazon.com/dynamodb/pricing/)

### Verificación nueva de MQTT/TLS con dispositivo virtual

- **Transporte.** La prueba abrió un socket de loopback con TLS mutuo y certificados X.509 efímeros. El cliente MQTT 3.1.1 esperó la confirmación de suscripción, solicitó el Shadow clásico con `/get`, recibió la configuración, publicó `reported` y recibió el acuse QoS 1 de la lectura. El servidor de prueba simuló las respuestas de AWS IoT. No hubo conexión al proyecto AWS.
- **Contrato.** Un delta de versión 4 recibido antes de la instantánea de versión 3 se aplicó después de la instantánea. El cliente reportó modo `manual`, autocierre de 21 s, puerta cerrada simulada y cerradura desconocida. Descartó valores inválidos y comandos de movimiento. La prueba con las dos Lambda locales confirmó que Alexa puede leer el estado reportado y que reenviar el mismo ID produce una única fila en el sustituto de DynamoDB. Un PUBACK acredita recepción por el broker.
- **WiFi y ESP-NOW.** La revisión de `EspNowLink::begin()` confirmó que fija el canal de radio y el del peer. Ambas plantillas seleccionan el canal 1. Para coexistir con WiFi STA, ambos nodos y el punto de acceso de 2,4 GHz deben usar el mismo canal. Queda pendiente comprobarlo con las placas, incluida la reconexión WiFi y la continuidad de latidos y solicitudes de estado. [FAQ de Espressif, §3.7.3](https://docs.espressif.com/projects/esp-faq/en/latest/esp-faq-en-master.pdf)
- **Resultados nuevos.** **25/25 pruebas Python**, incluida la prueba MQTT con TLS real en loopback, **85/85 pruebas de firmware** y **5/5 compilaciones PlatformIO**. Las dos últimas verifican el firmware existente. El cliente virtual corre en Python y el adaptador MQTT/TLS de las placas sigue pendiente.

Para repetir la verificación local, usar Python 3.13 y OpenSSL en el PATH o indicado en `OPENSSL`:

~~~powershell
python -m pip install -r aws/virtual_device/requirements.txt
python -B -m unittest discover -s tests -p 'test_*.py' -v
~~~

El [cliente de telemetría](../aws/virtual_device/telemetry.py) y la
[plantilla sin secretos](../aws/config/virtual-device.example.json) quedan listos para un proyecto aislado ya aprovisionado. Cuando se autorice esa prueba, utilizar un Thing de laboratorio `chapa-inteligente-9xxx`, sus propios certificados y permisos limitados a sus tópicos. Completar la configuración en `virtual-device.local.json`, excluido de Git. El endpoint ATS en puerto 8883 requiere el argumento explícito `--allow-isolated-aws`. La persistencia real se verifica por `eventId` en DynamoDB del entorno aislado. [Protocolos de AWS IoT Core](https://docs.aws.amazon.com/iot/latest/developerguide/protocols.html)

## 6. Conclusiones

El trabajo local confirma los contratos de Alexa, Shadow e histórico con clientes simulados y conserva los recursos que ya existen. El firmware ya incorpora ESP-NOW y confirmación del Reed antes del bloqueo. Para la demo completa falta el adaptador MQTT/TLS, validar la coexistencia del canal WiFi con ESP-NOW y comprobar la posición del mecanismo y el flujo integrado en hardware.

## 7. Recomendaciones

Completar la integración en una prueba coordinada: publicar un estado reciente, ejecutar una orden válida, provocar una pérdida de WiFi y confirmar la recuperación. Antes de conectar el dispositivo, revisar la política IoT, la eliminación del movimiento legado del Shadow y la compatibilidad simultánea del backend y el adaptador. Para un uso real posterior, usar credenciales RFID adecuadas, autorización de titularidad y roles mínimos.

## 8. Anexos

- [Modelo original Alexa y frases en español](../aws/alexa/interaction-model/es-MX.json).
- [Exportación sanitizada](../aws/config/deployed-sanitized.json) y [mockups RF15](mockups-reportes.md).
- [Pruebas reproducibles con actuador virtual](../tests/test_aws_integration.py).
- [Guía para revisar el proyecto AWS en solo lectura](revision-aws.md).
- [Plan de cierre, validación aislada y lista de comprobación física](cierre-practica-4.md).
- Pendientes las fotografías, capturas de la demo y muestras de histórico vinculadas a IDs de la prueba integrada.

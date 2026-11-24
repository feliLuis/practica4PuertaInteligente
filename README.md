# Chapa inteligente — adaptación local de la Práctica 4

Firmware modular para dos ESP32 de 38 pines. Esta fase contiene lógica local,
sensores y diagnósticos individuales. **No contiene ESP-NOW, WiFi, MQTT, NTP,
AWS ni comunicación real entre placas.** `SerialLink` solo imprime mensajes
en el USB del ESP32 de sensores; no implementa UART entre dispositivos.

No se ha cargado este firmware ni se han ejecutado pruebas físicas durante
esta tarea. La compilación y las pruebas con sustitutos no validan el montaje.
No se declara cumplimiento completo de la Práctica 4.

## Hardware y conexiones definitivas

### ESP32 #1: control

| SG90 | Conexión |
|---|---|
| Señal naranja | GPIO13 |
| Alimentación roja | 5V provisional; verificar antes de accionar |
| Tierra marrón | GND |

- Bloqueado: **0°**. Desbloqueado: **90°**.
- Se eliminaron las clases, configuraciones y comandos de LEDs adicionales y buzzer.
- La puerta se observa con el Reed; la cerradura es una **estimación por orden PWM**.
- Al arrancar, puerta y cerradura son desconocidas. No se conecta PWM ni se ordena movimiento.
- El servo se desacopla 700 ms después de la última orden. Esto **no garantiza retención mecánica**.
- `LockServo` valida GPIO, posiciones, retorno de `attach()` y estado `attached()`.
  Solo después de aceptar la orden el controlador cambia su estimación.
  `write()` no devuelve confirmación eléctrica o mecánica: estos controles no demuestran
  que el eje alcanzó su posición ni detectan un atasco o una alimentación insuficiente.

### ESP32 #2: sensores

| RC522 | ESP32 #2 |
|---|---|
| SDA/SS | GPIO21 |
| SCK | GPIO18 |
| MOSI | GPIO23 |
| MISO | GPIO19 |
| RST | GPIO22 |
| 3.3V | 3V3 |
| GND | GND |
| IRQ | Sin conectar |

Reed pasivo: un conductor en **GPIO27** y el otro en **GND**, con `INPUT_PULLUP`.
Polaridad inicial configurable: LOW = cerrada, HIGH = abierta.
Se exigen **100 ms** de estabilidad inicial y después de cada cambio;
hasta la primera lectura estable se conserva `desconocida`.

El RC522 tiene sondeo cada 50 ms, comprobación de disponibilidad cada segundo y
reintento de inicialización cada cinco segundos tras fallo inicial o pérdida posterior.
La biblioteca MFRC522 contiene esperas internas durante inicialización y transacciones;
no se reinicializa en cada vuelta del bucle. No son temporizadores de negocio.

### Alimentación y comprobaciones físicas

La alimentación provisional del servo mediante USB y el pin de 5V **no está validada**.
Se recomienda una fuente externa regulada de 5V con GND común al ESP32 #1.
Verificar alimentación y recorrido antes de probar con carga. No alimentar el
SG90 desde 3V3 o un GPIO ni el RC522 desde 5V. No unir entre sí las salidas de
5V de fuentes distintas. No se añade hardware como dependencia del firmware.

Los GPIO acordados se conservan. Todavía debe comprobarse el pinout físico de
la segunda placa y la polaridad del Reed.

## Arquitectura

- `firmware/esp32_control`: `LockController`, `CredentialList`,
  `LockServo`, eventos y monitor serie incremental.
- `firmware/esp32_sensors`: `DoorSensor`, `RfidReader` y salida `SerialLink`.
- `shared/Protocol`: contrato compacto existente e interfaz `LocalLink`,
  conservados para la próxima fase.
- `tests`: ejecutables de escritorio que compilan las clases reales con reloj,
  GPIO, PWM y lector falsos. Nunca acceden al hardware.

La autorización se ejecuta exclusivamente en ESP32 #1. Las credenciales se
configuran en su `include/config.h`:

| UID inicial, pendiente de lectura física | Acción | Identificador lógico |
|---|---|---|
| 39 61 D7 06 | Desbloquear a 90° | tarjeta-1 |
| 9C 31 8B 06 | Desbloquear a 90° | llavero-1 |

Un UID desconocido genera un acceso rechazado sin mover el servo.
Ambas credenciales se rechazan en modo bloqueado. Ninguna bloquea ni alterna
la cerradura; repetir una credencial con la cerradura ya desbloqueada conserva
el estado sin mover el servo ni reiniciar el autocierre.
Las constantes son `CARD_UID`, `KEYFOB_UID`, sus respectivas longitudes,
`CARD_ID` e `KEYFOB_ID`. La lista rechaza registros con acción de bloqueo.
El bloqueo corresponde a Alexa (integración futura), al autocierre automático
o a comandos de diagnóstico habilitados para su entorno.
El UID no constituye autenticación segura para una cerradura comercial.

El lector acepta únicamente UID de 4, 7 o 10 bytes y verifica la capacidad del
búfer. Conserva el filtro de 1500 ms por último UID y `PICC_HaltA()`.
**No es una prueba de retirada física**: mantener, retirar y volver a presentar
tarjeta o llavero debe validarse con el lector real. La supresión temporal puede
descartar una presentación legítima muy rápida de la misma credencial.

## Modos, seguridad y autocierre

Configuración inicial: `automatico`, diez segundos. Rango: **5–120 s**.

- **Manual:** permite solicitudes autorizadas; no ejecuta autocierre.
- **Automático:** al desbloquear con puerta cerrada y lectura vigente, comienza la cuenta.
  Si está abierta o sin lectura vigente, espera un cierre válido.
  Abrir o perder el sensor cancela la cuenta; un cierre o recuperación válida la
  inicia por el período completo.
- **Bloqueado:** rechaza desbloqueos y cancela la cuenta; cambiar de modo no mueve el servo.

Entrar en automático evalúa inmediatamente las condiciones. Cambiar el tiempo
reinicia el período completo si corresponde; repetir el mismo valor no cambia nada.
Repetir un desbloqueo ya estimado no mueve el servo ni prolonga la cuenta.

Toda orden de bloqueo exige puerta **exactamente cerrada** y sensor vigente.
Valores de puerta o acción inválidos no autorizan bloqueo. Una orden ya satisfecha
no genera otro movimiento. El autocierre verifica las condiciones nuevamente al vencer.

Los tiempos usan diferencias de `millis()`, sin `delay()` de negocio.
Se conservan latidos cada diez segundos y timeout de treinta segundos.
Un estado inválido o un latido sin lectura **no renueva el Reed**.
Tras vencer, la puerta pasa a desconocida y se registra una transición offline.
Solo una nueva lectura abierta/cerrada válida produce la transición online.
Los latidos del nodo de sensores incluyen su estado de puerta; en la próxima fase
el receptor deberá validar la trama antes de entregar esa lectura al controlador.

Si falla una orden de autocierre, se registra el rechazo y no se reintenta en cada
vuelta del bucle. Una nueva lectura válida puede iniciar otro período completo.
Modo, parámetros, secuencias y estados son locales a RAM; no se persisten al reiniciar.

## Eventos locales

Cada `Event` contiene tipo, origen, resultado, motivo, puerta, cerradura estimada,
modo, `credentialId`, secuencia local y marca `simulated`.
Los textos y el estado se copian dentro del evento, sin depender de punteros al UID.

- `acceso`: un resultado por presentación RFID.
- `bloqueo`/`desbloqueo`: una nueva orden emitida al actuador.
- `solicitud`: rechazo, fallo o solicitud directa que no requiere movimiento.
- `puerta`, `sensor`, `modo`, `autocierre`: sus respectivos cambios.

Un acceso y una orden asociada representan hechos diferentes.
El callback de eventos no mueve el servo; el callback de movimiento devuelve
si pudo emitir la orden. Los comandos USB usan origen `diagnostico`, nunca `voz`.
En simulación todos los eventos llevan `simulado=true` y prefijo `[EVT-SIM]`.

Son estructuras **internas**, no un nuevo contrato Lambda. No hay publicación,
cola offline ni UID en la estructura de histórico. La secuencia reinicia al arrancar;
la identidad persistente y la adaptación al contrato AWS corresponden a fases futuras.

## Compilación en VS Code / PlatformIO

Para usar las tareas de PlatformIO IDE, abrir en VS Code la carpeta
`firmware/esp32_control` o `firmware/esp32_sensors`, según la placa.
Seleccionar el entorno y su tarea **Build** en Project Tasks.
También se puede abrir la raíz del repositorio y usar su terminal con los comandos siguientes.
No usar **Upload**, **Upload and Monitor** ni **Debug** durante esta fase.

Versiones fijadas:

| Elemento | Versión |
|---|---|
| Plataforma espressif32 | 7.1.3 |
| Paquete framework Arduino-ESP32 | 4.20017.260907 (core 2.0.17) |
| ESP32Servo | 0.13.0 |
| MFRC522 | 1.4.12 |

Desde la raíz del repositorio, en PowerShell:

```powershell
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"
Test-Path -LiteralPath $pio
& $pio run -d firmware/esp32_control
& $pio run -d firmware/esp32_sensors
& $pio run -d firmware/esp32_control -e simulation -e calibration
& $pio run -d firmware/esp32_sensors -e diagnostic
```

Si `pio` está en el PATH también puede usarse directamente.
No es necesario cambiar el PATH global ni reinstalar PlatformIO.

### Entornos y comandos USB (115200 baudios)

| Proyecto / entorno | Uso | Comandos habilitados |
|---|---|---|
| Control / `esp32dev` | Normal; sin datos de sensores hasta la próxima fase | `ayuda`, `estado` |
| Control / `simulation` | Lógica simulada, sin emitir PWM | Los anteriores y los de simulación listados abajo |
| Control / `calibration` | Prueba física explícita, sin simulación | `ayuda`, `estado`, `calibrar 0`, `calibrar 90` |
| Sensores / `esp32dev` | Lecturas individuales, UID oculto en USB | Salida automática |
| Sensores / `diagnostic` | Lecturas individuales, UID completo solo por USB | Salida automática |

Comandos del entorno `simulation`:

```text
puerta abierta
puerta cerrada
latido
uid HEX
desbloquear
bloquear
modo manual
modo automatico
modo bloqueado
autocierre N
```

`latido` no inventa una lectura: para refrescar datos en esta simulación usar
`puerta abierta` o `puerta cerrada`. Por ejemplo: puerta cerrada, desbloquear,
esperar diez segundos y consultar estado. Esta secuencia no genera PWM.

La calibración acepta exclusivamente `calibrar 0` y `calibrar 90`; evita los
interbloqueos deliberadamente para comprobar el actuador individual, **solo en
ese binario**, después de verificar alimentación y liberar el mecanismo.
No se permite `servo N` ni calibración con otros valores.

El parser usa un búfer de 80 bytes, procesa hasta 64 bytes por vuelta y descarta
líneas excesivas o inválidas hasta el terminador. Acepta LF/CRLF; valida por completo
los enteros y su desbordamiento. La compilación normal rechaza comandos de simulación.
El nodo de sensores imprime `[SIM-TX SIN ENLACE]`; su salida no llega al controlador.
Los comandos, los campos y valores de salida estructurada, los motivos de
eventos y los identificadores de credenciales conservan su grafía original
para que las secuencias y lecturas por USB existentes sigan siendo válidas.

## Pruebas automatizadas sin hardware

Requieren Python 3 y un compilador de escritorio C++11 (g++, clang++ o Zig).
No necesitan PlatformIO, puertos serie ni placas. Desde la raíz:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" tests/run_tests.py
```

El ejecutor busca `CXX`, g++/clang++ en PATH y finalmente el Zig portátil local.
Si no existe compilador, termina explicándolo; no declara pruebas aprobadas.
En Linux también puede ejecutarse con `python3 tests/run_tests.py`.

En este equipo no se encontró compilador de escritorio en Windows ni g++/clang++
en Ubuntu/WSL. Para las pruebas se utiliza una copia portátil de Zig, sin cambiar
PATH o Python global; sus binarios y cachés quedan en `.pio/`, ignorado por Git:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" -m pip install --target .pio/host-tools --no-cache-dir ziglang==0.13.0
```

Zig redistribuye un compilador C++ basado en Clang; no es una dependencia del
firmware ESP32. Fuente del paquete: https://pypi.org/project/ziglang/0.13.0/

Las suites actuales cubren interbloqueos, credenciales, autocierre, modos,
fallos de accionamiento, desbordamiento de reloj, copias de eventos,
límites del parser y separación de los tres modos de consola.
Compilan el código real con sustitutos; **no validan físicamente el PWM ni el RC522**.

Validación histórica ejecutada el 24/11/2026: seis suites completadas, **54 ejecuciones
aprobadas y cero fallidas** (29 de control, 11 de sensores por cada configuración
y una de consola por cada uno de los tres entornos).
El código de prueba se compila con C++11, `-Wall -Wextra -Werror`.
También finalizaron correctamente las cinco compilaciones PlatformIO:

| Proyecto | Entornos compilados | Resultado |
|---|---|---|
| Control | `esp32dev`, `simulation`, `calibration` | 3 SUCCESS |
| Sensores | `esp32dev`, `diagnostic` | 2 SUCCESS |

Se ejecutaron los comandos `run` indicados arriba, sin upload ni acceso a placas.
La primera compilación de las bibliotecas internas de Zig produjo advertencias
de atributos de exportación en `libunwind`; no pertenecen al firmware.

## Pruebas físicas pendientes y próxima fase

1. Verificar alimentación del servo y pinout real de las placas.
2. Confirmar polaridad, alineación y rebotes del Reed.
3. Leer tarjeta/llavero y comprobar los UID configurados.
4. Validar ausencia de lecturas repetidas por mantener una credencial.
5. Comprobar pérdida y recuperación del RC522 sin bloquear la lectura del Reed.
6. Con autorización posterior para cargar y accionar, comprobar posiciones 0°/90°
   sin forzar topes, alimentación bajo carga y efecto del desacople.
7. Verificar que el binario normal rechaza simulación y calibración.

Antes de comunicar placas: obtener MAC reales y acordar canal, validación,
sesiones, ACK y reintentos. ESP-NOW y la integración AWS se implementarán después,
con autorización. No hay todavía una prueba física de RFID→servo entre placas.

## Distribución colaborativa y auditoría Git (24/11/2026)

Rama de integración general: `feature/firmware-integracion`. La auditoría local
parte de `4f6795c` (`configuracion del firmware`); el único commit anterior es
`e1a202c` (`Initial commit`). `git log --oneline -10` solo muestra esos dos.
Las referencias remotas locales solo muestran `origin/main` en `4f6795c`;
no se consultó GitHub para confirmar ramas o permisos actuales.

Hay **37 archivos pendientes: 18 modificados, 6 eliminados y 13 nuevos**.
El índice está vacío: todos los cambios están fuera del área de preparación.
Los seis eliminados y el resto de la adaptación local ya existían antes de
esta corrección; se conservan. `git diff --stat` y `git diff --name-status`
describen 24 archivos versionados; los otros 13 aparecen con
`git ls-files --others --exclude-standard`. `git diff --check` no reporta errores.
Los artefactos `.pio/` se ignoran y no forman parte de los aportes.

La corrección de esta tarea toca `include/config.h`, `src/LockController.cpp`
y `src/CredentialList.cpp` dentro de control, `tests/control_tests.cpp`,
`tests/console_tests.cpp` y este README. No se modificaron los sensores ni el
protocolo funcionalmente durante esta corrección.
Durante la auditoría final se observó una desaparición transitoria de los nueve
archivos versionados de sensores. Los fuentes y cabeceras reaparecieron con
las diferencias iniciales conservadas; se restauró `platformio.ini` desde la
lectura completa capturada al inicio, sin volver a HEAD. Tras ello se repitieron
las seis suites (54/54) y las dos compilaciones de sensores (2 SUCCESS).
No se conoce la causa de la desaparición; revisar sincronización/editor antes
de trabajar simultáneamente en la misma carpeta. Se conserva una copia local
de los fuentes en `.pio/reparto/respaldo-final.zip`.

### Inventario completo y responsables

`M`: modificado; `D`: eliminación pendiente; `N`: nuevo sin seguimiento;
`=`: versionado sin cambios. Todos los paths son relativos a la raíz.
Cada archivo tiene un responsable primario; las revisiones compartidas no
cambian esa propiedad. Las eliminaciones pertenecen al responsable del módulo.

| Integrante | Archivo | Estado |
|---|---|---|
| 1 | `firmware/esp32_control/include/Buzzer.h` | D |
| 1 | `firmware/esp32_control/include/LockController.h` | M |
| 1 | `firmware/esp32_control/include/IndicadorAcceso.h` | D |
| 1 | `firmware/esp32_control/include/Led.h` | D |
| 1 | `firmware/esp32_control/include/CredentialList.h` | M |
| 1 | `firmware/esp32_control/include/SerialMonitor.h` | N |
| 1 | `firmware/esp32_control/include/LockServo.h` | M |
| 1 | `firmware/esp32_control/include/Types.h` | M |
| 1 | `firmware/esp32_control/include/config.h` | M |
| 1 | `firmware/esp32_control/platformio.ini` | M |
| 1 | `firmware/esp32_control/src/Buzzer.cpp` | D |
| 1 | `firmware/esp32_control/src/LockController.cpp` | M |
| 1 | `firmware/esp32_control/src/IndicadorAcceso.cpp` | D |
| 1 | `firmware/esp32_control/src/Led.cpp` | D |
| 1 | `firmware/esp32_control/src/CredentialList.cpp` | M |
| 1 | `firmware/esp32_control/src/LockServo.cpp` | M |
| 1 | `firmware/esp32_control/src/main.cpp` | M |
| 1 | `tests/control_tests.cpp` | N |
| 1 | `tests/console_tests.cpp` | N |
| 2 | `firmware/esp32_sensors/include/SerialLink.h` | = |
| 2 | `firmware/esp32_sensors/include/RfidReader.h` | M |
| 2 | `firmware/esp32_sensors/include/DoorSensor.h` | = |
| 2 | `firmware/esp32_sensors/include/config.h` | M |
| 2 | `firmware/esp32_sensors/platformio.ini` | M |
| 2 | `firmware/esp32_sensors/src/SerialLink.cpp` | M |
| 2 | `firmware/esp32_sensors/src/RfidReader.cpp` | M |
| 2 | `firmware/esp32_sensors/src/DoorSensor.cpp` | M |
| 2 | `firmware/esp32_sensors/src/main.cpp` | M |
| 2 | `tests/sensores_tests.cpp` (suite histórica retirada) | N |
| 3 | `.gitignore` | = |
| 3 | `firmware/esp32_control/.gitignore` | N |
| 3 | `firmware/esp32_sensors/.gitignore` | N |
| 3 | `shared/Protocol/library.json` | = |
| 3 | `shared/Protocol/src/LocalLink.h` | = |
| 3 | `shared/Protocol/src/Protocol.cpp` | = |
| 3 | `shared/Protocol/src/Protocol.h` | = |
| 3 | `tests/run_tests.py` | N |
| 3 | `tests/TestSupport.h` | N |
| 3 | `tests/stubs/Arduino.h` | N |
| 3 | `tests/stubs/ESP32Servo.h` | N |
| 3 | `tests/stubs/Fakes.cpp` | N |
| 3 | `tests/stubs/MFRC522.h` | N |
| 3 | `tests/stubs/driver/gpio.h` | N |
| 3 | `README.md` | M |

| Integrante | Rama propuesta | Responsabilidad |
|---|---|---|
| 1 | `feature/control-cerradura` | Cerradura, servo, autorización RFID, modos, autocierre y consola de control |
| 2 | `feature/sensores-rfid-reed` | Lector RFID, Reed, latidos y transmisión de estados del nodo sensor |
| 3 | `feature/integracion-iot` | Protocolo, soporte de pruebas, documentación e ignores; futura comunicación ESP-NOW y adaptación AWS/Alexa/Lambda/DynamoDB |

Los `.gitignore` de cada firmware quedan bajo coordinación del integrante 3.
La asignación de ESP-NOW y nube es trabajo futuro: no hay archivos de Lambda,
DynamoDB o Alexa en este repositorio ni se implementan ahora.

### Commits propuestos y selección explícita

Los comandos siguientes son una propuesta para revisión y ejecución posterior
por cada integrante. No se ejecutaron `add`, `commit`, `push`, `merge`, creación
o cambio de rama durante esta tarea. No cambiar la identidad Git ni atribuirse
trabajo ajeno. Cada integrante debe revisar y validar su aporte real desde su
propio clon y su cuenta; los cambios previos no permiten inferir su autoría.

Primer aporte del integrante 3: `test: agregar soporte común de pruebas e ignores`.
Publicar únicamente la base que necesitan las pruebas de los otros módulos:

```powershell
git add -- tests/TestSupport.h tests/stubs/Arduino.h tests/stubs/ESP32Servo.h tests/stubs/Fakes.cpp tests/stubs/MFRC522.h tests/stubs/driver/gpio.h firmware/esp32_control/.gitignore firmware/esp32_sensors/.gitignore
git diff --cached --check
git diff --cached --stat
git diff --cached
# Tras revisar, el integrante 3 podrá ejecutar:
git commit -m "test: agregar soporte comun de pruebas e ignores"
```

Aporte del integrante 1: `fix(control): ambas credenciales solo desbloquean`.
La adaptación pendiente de control es una unidad coherente: API, tipos,
eliminaciones, monitor, servo, main, configuración y pruebas deben viajar juntos.

```powershell
git add -A -- firmware/esp32_control/include firmware/esp32_control/src firmware/esp32_control/platformio.ini tests/control_tests.cpp tests/console_tests.cpp
git diff --cached --check
git diff --cached --stat
git diff --cached
git commit -m "fix(control): ambas credenciales solo desbloquean"
```

Aporte del integrante 2: `feat(sensores): estabilizar Reed y diagnóstico RFID`.

```powershell
git add -A -- firmware/esp32_sensors/include firmware/esp32_sensors/src firmware/esp32_sensors/platformio.ini
git diff --cached --check
git diff --cached --stat
git diff --cached
git commit -m "feat(sensores): estabilizar Reed y diagnostico RFID"
```

Último aporte del integrante 3, cuando estén ambos módulos:
`test(docs): integrar ejecutor y documentar validación y responsabilidades`.

```powershell
git add -- tests/run_tests.py README.md
git diff --cached --check
git diff --cached --stat
git diff --cached
git commit -m "test(docs): integrar ejecutor y documentar responsabilidades"
```

No se propone un commit vacío para `.gitignore` o `shared/Protocol`, que no
tienen cambios. Cuando haya cambios reales de protocolo, seleccionar sus cuatro
archivos explícitamente y revisarlos con ambos responsables de firmware.

### Entrega de los cambios locales y aportes desde GitHub

Un clon de GitHub no incluye estos 37 cambios pendientes. Se preparan copias
por responsabilidad en `.pio/reparto/integrante-1.zip`, `integrante-2.zip` e
`integrante-3.zip`. Cada ZIP contiene `base.txt`, `cambios.patch` para los archivos
ya versionados (incluye las eliminaciones) y `nuevos/` para los nuevos archivos.
Los ZIP no contienen `.git`, compiladores, binarios de firmware o secretos.
Son copias locales ignoradas: no se movieron los archivos fuente originales.
Entregar cada ZIP a su responsable antes de trabajar desde clones separados.

La rama de integración debe estar disponible en GitHub antes de abrir PR contra
ella. El mantenedor comprobará el remoto y, después de la revisión y autorización
correspondientes, podrá publicar la rama local existente con:

```powershell
git ls-remote --heads origin feature/firmware-integracion
git push -u origin feature/firmware-integracion
```

Ese push solo publica commits existentes; no publica los cambios pendientes.
Si ya existe una rama remota divergente, revisar las diferencias antes de seguir.
Cada integrante, en otro directorio, usa su propia sesión GitHub. Comprobar
`gh auth status` y `git var GIT_AUTHOR_IDENT`; si no corresponden a su cuenta,
resolver la sesión con el titular antes de realizar aportes. No reescribir autores.

Ejemplo para el integrante 1; los otros sustituyen rama, ZIP y nombre del clon:

```powershell
gh auth status
git clone --branch feature/firmware-integracion https://github.com/feliLuis/practica4PuertaInteligente.git practica4-control
Set-Location practica4-control
git var GIT_AUTHOR_IDENT
git switch -c feature/control-cerradura
git status --short
$entrega = Join-Path (Get-Location) '.pio/reparto/recibido'
Expand-Archive -LiteralPath 'C:/ruta/entregas/integrante-1.zip' -DestinationPath $entrega
Get-Content -LiteralPath (Join-Path $entrega 'base.txt')
git apply --check (Join-Path $entrega 'cambios.patch')
# Continuar solo si --check termina correctamente.
git apply (Join-Path $entrega 'cambios.patch')
$nuevos = Join-Path $entrega 'nuevos'
$pendientes = @(Get-ChildItem -LiteralPath $nuevos -Recurse -File -Force)
foreach ($archivo in $pendientes) {
  $relativo = $archivo.FullName.Substring($nuevos.Length + 1)
  $destino = Join-Path (Get-Location) $relativo
  if (Test-Path -LiteralPath $destino) { throw "Ya existe: $relativo; revisar antes de copiar" }
}
foreach ($archivo in $pendientes) {
  $relativo = $archivo.FullName.Substring($nuevos.Length + 1)
  $destino = Join-Path (Get-Location) $relativo
  New-Item -ItemType Directory -Path (Split-Path -Parent $destino) -Force | Out-Null
  Copy-Item -LiteralPath $archivo.FullName -Destination $destino
}
git status --short
git diff --check
# Validar, seleccionar y crear solo el commit de su responsabilidad, como arriba.
git push -u origin feature/control-cerradura
gh pr create --base feature/firmware-integracion --head feature/control-cerradura --web
```

| Integrante | Carpeta de clon sugerida | ZIP | Rama / argumento `--head` |
|---|---|---|---|
| 1 | `practica4-control` | `integrante-1.zip` | `feature/control-cerradura` |
| 2 | `practica4-sensores` | `integrante-2.zip` | `feature/sensores-rfid-reed` |
| 3 | `practica4-integracion` | `integrante-3.zip` | `feature/integracion-iot` |

Para el integrante 3: primer PR solo con soporte e ignores. Conservar sus cambios
de README y ejecutor en su copia/ZIP hasta integrar ambos firmwares; preparar
el segundo PR desde la rama de integración actualizada. No cambiar de rama
con archivos pendientes. Para actualizaciones, usar `git fetch origin` y
`git pull --ff-only` únicamente en clones limpios sobre la rama correspondiente.
Si los integrantes no tienen escritura, usar forks personales y PR al mismo
repositorio base; no compartir cuentas ni credenciales.

### Orden de integración, conflictos y pendientes

1. Revisar y conservar la copia local y los tres ZIP. Confirmar responsables y
   permisos; hacer disponible la rama general sin alterar los commits existentes.
2. Integrar soporte común e ignores del integrante 3. El protocolo actual ya
   existe en la base; no necesita un commit nuevo.
3. Integrar control completo con sus pruebas. Compilar sus tres entornos y
   comprobar sus pruebas con el soporte común.
4. Integrar sensores y sus pruebas. Compilar sus dos entornos. Control y
   sensores pueden desarrollarse por separado con el contrato actual congelado.
5. Integrar ejecutor y documentación del integrante 3 y ejecutar las seis suites
   y las cinco compilaciones sobre la rama general. No publicar el ejecutor antes
   de que existan las clases y suites que referencia.
6. Acordar después la siguiente fase ESP-NOW/nube y sus contratos. No desarrollar
   transporte ni integración AWS en esta corrección.

`firmware/esp32_control/src/main.cpp` pertenece al integrante 1 y
`firmware/esp32_sensors/src/main.cpp` al 2: no son el mismo archivo. El integrante
3 coordinará con cada dueño los futuros puntos de conexión de transporte;
evitar ediciones simultáneas. Lo mismo aplica a cada `include/config.h` y
`platformio.ini`: compartir versiones y flags, manteniendo responsable por placa.
No agregar secretos a esas configuraciones.

`Types.h` (1), `SerialLink.*` (2), `shared/Protocol/*` (3) y el soporte de
pruebas son dependencias entre equipos. Congelar y revisar firmas, enums,
longitudes UID, versión y formato de mensaje antes de cambios incompatibles;
protocolo y consumidores deben integrarse coordinadamente sin romper la base.
README y runner tienen un único editor principal (3); los demás entregan datos
de validación y propuestas. Esto evita conflictos de texto y referencias a
archivos todavía ausentes.

Riesgos pendientes: los UID y los 90° están verificados por software, no en
hardware; la autorización basada solo en UID no evita clonación. El sensor y
el controlador todavía no se comunican. Alexa, autenticación de comandos remotos,
sesiones, ACK/reintentos, persistencia e histórico DynamoDB siguen pendientes.
La presencia de `Source::VOICE` o `Source::CLOUD` no autentica un comando.
La consola de simulación es un diagnóstico local y la calibración tiene un
binario separado; no convertirlas en un acceso remoto sin autorización.
No cargar firmware ni accionar el montaje como parte de esta revisión.

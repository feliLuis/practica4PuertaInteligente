# practica4PuertaInteligente

Firmware Fase 1 de una maqueta academica de chapa inteligente con dos ESP32.
Esta fase implementa solo la lógica Edge y simulación por Monitor Serie. No contiene
WiFi, MQTT, AWS IoT, Device Shadow, Alexa, ESP-NOW ni secretos.

> No se declara probado con hardware. Los comandos físicos los validara el compañero
> ensamblador del circuito.

## Arquitectura

- `firmware/esp32_control`: ESP32 #1; controlador, servo, LEDs, buzzer y simulacion de voz/red.
- `firmware/esp32_sensores`: ESP32 #2; Reed, RC522 y mensajes locales simulados por serie.
- `shared/Protocolo`: librería compartida con el contrato de mensajes y la interfaz `EnlaceLocal`.
- El enlace actual es `EnlaceSerial`; puede sustituirse por ESP-NOW en Fase 2 sin cambiar la máquina
  de estados del controlador.

`cerradura` es una estimacion por comando del servo. Al arrancar queda `DESCONOCIDA`; el firmware
nunca afirma que el pestillo esta cerrado fisicamente.

## Tabla de conexiones

### ESP32 #1: control

| Elemento | Conexion |
|---|---|
| Servo SG90 señal | GPIO 13 |
| Servo SG90 VCC | Fuente externa regulada 5 V |
| Servo SG90 GND | GND de la fuente externa |
| GND ESP32 #1 | GND comun con fuente del servo |
| LED verde | GPIO 25 → resistencia 220–330 Ω → ánodo → cátodo → GND |
| LED rojo | GPIO 26 → resistencia 220–330 Ω → ánodo → cátodo → GND |
| Buzzer | GPIO 33 → buzzer segun tipo → GND |

### ESP32 #2: sensores

| Elemento | Conexion |
|---|---|
| RC522 SDA/SS | GPIO 21 |
| RC522 SCK | GPIO 18 |
| RC522 MOSI | GPIO 23 |
| RC522 MISO | GPIO 19 |
| RC522 RST | GPIO 22 |
| RC522 3.3V | 3V3 del ESP32 |
| RC522 GND | GND |
| Reed conductor 1 | GPIO 27 con INPUT_PULLUP |
| Reed conductor 2 | GND |

## Advertencias eléctricas

- El RC522 se alimenta únicamente con **3V3**. Nunca con 5 V.
- El SG90 no debe alimentarse desde 3V3 ni desde un GPIO. Usa una fuente externa regulada de 5 V,
  con al menos 1 A recomendado y GND comun con el ESP32 #1.
- Colocar un condensador electrolítico de 470–1000 µF y >=10 V entre +5 V y GND cerca del servo,
  observando polaridad.
- Probar el servo sin carga antes de unirlo al pestillo. Los angulos no son universales y no deben
  forzar mecanicamente la chapa.
- Los LEDs requieren resistencia de 220–330 Ω en serie.
- Un buzzer que consume más de aproximadamente 20 mA requiere transistor. No conectar directamente
  si su consumo supera ese limite.

## Pendiente de confirmar

- Modelo exacto de la placa ESP32-WROOM DevKit.
- Tipo de Reed: normalmente abierto o normalmente cerrado. La constante
  `REED_CERRADO_ES_LOW=true` y la polaridad actual son configurables en `include/config.h`.
- Fuente física de 5 V disponible para el servo.
- Angulos actuales del SG90: inicialmente 60° bloqueado y 120° desbloqueado.
- UIDs reales: los UID de prueba son 39 61 D7 06 y 9C 31 8B 06.
- Pines propuestos de indicadores: LEDs en GPIO 25 y 26 y buzzer en GPIO 33. La documentación
  del circuito no los incluye; se consideran propuestas pendientes de confirmar.
- Tipo de buzzer pendiente de confirmar: activo o pasivo. Configurable con `BUZZER_ACTIVO`.

## Compilar y subir

Desde la raiz del repositorio:

```powershell
pio run -d firmware/esp32_sensores
pio run -d firmware/esp32_control
```

Para subir:

```powershell
pio run -d firmware/esp32_sensores -t upload
pio run -d firmware/esp32_control -t upload
```

Abrir el Monitor Serie con 115200 baudios.

## Comandos de prueba del ESP32 #1

- `puerta abierta`
- `puerta cerrada`
- `uid 3961D706`
- `latido`
- `desbloquear`
- `bloquear`
- `modo manual`
- `modo automatico`
- `modo bloqueado`
- `autocierre N`
- `servo N`
- `test verde`
- `test rojo`
- `test buzzer`
- `estado`
- `ayuda`

Sin `latido` ni reporte del sensor durante 30 segundos, el controlador marca la puerta como
desconocida y cancela el autocierre.

## Lista de pruebas fisicas

1. Ejecutar `test verde`: debe encenderse el LED verde aproximadamente 2 s sin activar el buzzer.
2. Ejecutar `test rojo`: debe encenderse el LED rojo aproximadamente 2 s sin activar el buzzer.
3. Ejecutar `test buzzer`: deben escucharse 3 pitidos de 150 ms separados por 100 ms.
4. Con puerta cerrada, ejecutar `uid 3961D706`: LED verde, sin buzzer y movimiento del servo a
   desbloqueo.
5. Con puerta cerrada, ejecutar `uid 9C318B06`: LED verde y movimiento del servo a bloqueo.
6. Con puerta abierta, ejecutar `uid 9C318B06`: LED rojo, 3 pitidos y el servo permanece quieto.
7. Ejecutar `uid AABBCCDD`: LED rojo, 3 pitidos y el servo permanece quieto.
8. Ejecutar `desbloquear` y `bloquear` por serie: deben actuar sin LEDs ni buzzer.
9. Presentar una tarjeta mientras LED/buzzer siguen activos: no debe acumular patrones ni bloquear
   el comportamiento; el patrón activo debe reiniciarse.
10. Ejecutar desbloqueo, abrir la puerta y cerrarla en modo automatico; el servo debe bloquear al
    vencer el tiempo configurado si la puerta sigue cerrada y el sensor esta vigente.
11. Después de una apertura, reabrir la puerta durante la cuenta: debe cancelarse el autocierre.
12. Dejando la puerta sin cambiar desde la apertura, no debe activarse el autocierre.
13. Esperar 30 s sin `latido`: la puerta pasa a desconocida y se cancela el autocierre una sola vez.
14. Activar `modo bloqueado` y verificar que rechaza desbloqueos con el motivo `modo_bloqueado`.

## Fase 2

Pendientes:

- Sustituir `EnlaceSerial` por ESP-NOW con direccion MAC, canal, secuencia y ACK logico.
- Añadir WiFi, MQTT sobre TLS y certificados X.509 excluidos por `.gitignore`.
- Integrar AWS IoT Core, Device Shadow, reglas, Lambda, DynamoDB y Alexa.
- Publicar eventos y modo reportado.
- Conservar la cola limitada y reconciliar comandos remotos sin duplicados.

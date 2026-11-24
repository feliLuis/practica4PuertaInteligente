# Mockups de reportes — RF15

Los valores de las dos pantallas son **ficticios** y muestran el diseño. Se añade una muestra anonimizada obtenida por AWS CLI en solo lectura; acredita el formato de datos real, no una demo integrada ni mediciones físicas.

## 1. Cronología de puerta y accesos

**Objeto** Chapa 01 · **Desde** 24/11/2026 08:00 · **Hasta** 24/11/2026 12:00

**Filtros** Todos / Aperturas / Accesos RFID / Cambios de cerradura · **Orden** Más recientes primero

| Hora local | Movimiento o lectura | Puerta / cerradura | Origen | Resultado |
|---|---|---|---|---|
| 09:21:12 | Bloqueo | Cerrada / bloqueada | Edge | Ejecutado |
| 09:21:02 | Cierre de puerta | Cerrada / desbloqueada | Edge | Ejecutado |
| 09:20:43 | Presentación RFID | Abierta / desbloqueada | RFID | Rechazado |
| 09:20:25 | Desbloqueo | Cerrada / desbloqueada | Voz | Ejecutado |

**Decisión apoyada.** El usuario puede revisar si la puerta quedó abierta tras un acceso y distinguir un rechazo RFID de una orden de movimiento.

**Datos y consulta.** Consultar `ChapaHistory` con `thingName = :thing AND eventKey BETWEEN :desde AND :hasta`, `ScanIndexForward=false` y cursor `LastEvaluatedKey`. Convertir las fechas locales a UTC para crear los límites de la clave por prefijo ISO. La interfaz utiliza `ts`, `tipo`, `puerta`, `cerradura`, `origen` y `resultado`. Aplica filtros sobre las filas del rango y omite muestras periódicas del tipo `lectura` en la cronología de cambios. Un campo ausente se muestra como «Sin dato».

## 2. Cambios de cerradura por origen

**Objeto** Chapa 01 · **Período** Últimos 7 días

**Movimientos** 28 · **Bloqueos** 15 · **Desbloqueos** 13

| Origen | Bloqueos | Desbloqueos |
|---|---:|---:|
| Voz | 3 | 3 |
| RFID | 0 | 10 |
| Edge | 12 | 0 |
| Nube | 0 | 0 |

**Decisión apoyada.** El usuario puede comparar cuánto depende del autocierre y cuántos desbloqueos corresponden a voz o RFID.

**Datos y consulta.** Usar la misma consulta por objeto y rango. Recorrer todas las páginas para el total y agrupar filas con `tipo` igual a `bloqueo` o `desbloqueo` y `resultado=ejecutado` por `origen`. Se excluyen los rechazos y las repeticiones que no causaron movimiento. La agrupación usa los registros presentes y aprovecha las claves actuales. Con el TTL habilitado, limitar la selección al horizonte retenido y mostrar que la eliminación de registros expirados es eventual.

Para intervalos de calendario, usar un límite inferior ISO UTC con sufijo mínimo y un límite superior correspondiente al inicio del intervalo siguiente. Excluir en el cliente cualquier fila cuya fecha sea igual o posterior al límite superior.

## 3. Diccionario de datos de las pantallas

| Columna o indicador | Atributo de `ChapaHistory` | Regla de presentación |
|---|---|---|
| Hora local | `ts` | Convertir de UTC a la zona del usuario. Si falta, mostrar «Sin dato» |
| Movimiento o lectura | `tipo` | `lectura` se omite en la cronología de cambios; `acceso` se muestra solo en el filtro RFID |
| Puerta / cerradura | `puerta`, `cerradura` | Valores permitidos: `abierta`, `cerrada`, `desconocida` y `bloqueada`, `desbloqueada`, `desconocida` |
| Origen | `origen` | `rfid`, `voz`, `edge`, `nube`; el mockup etiqueta `edge` como Edge y `rfid` como RFID |
| Resultado | `resultado` | `ejecutado`, `rechazado` y los demás valores contractuales; solo `ejecutado` suma en el resumen de movimientos |
| Identificador de fila | `eventKey`, `eventId` | `eventKey` ordena la partición; `eventId` se trunca en la interfaz y nunca se muestra completo en evidencias |
| Ingreso y expiración | `ingresoTs`, `ttl` | No se muestran en la pantalla; se conservan para auditoría y para indicar que la expiración es eventual |
| Motivo y secuencia | `motivo`, `seq`, `diferido`, `autocierre_s` | Detalle expandible; `diferido=true` se marca como evento recuperado, no como movimiento en tiempo real |

La consulta usa `thingName = :thingName AND eventKey BETWEEN :desde AND :hasta`,
`ScanIndexForward=false` y `LastEvaluatedKey`. Los límites se construyen en ISO
UTC, no en hora local. El resumen recorre todas las páginas antes de agrupar;
si una página falla, la pantalla debe mostrar un estado de error y no un total
parcial. No se infiere `origen` a partir de `tipo`: ambos campos se leen por
separado.

## 4. Muestra anonimizada real consultada por CLI

Consulta consistente sobre `chapa-inteligente-01` el 24/11/2026 alrededor de las 22:02Z.
Los sufijos de `eventId` se truncan. Es telemetría `edge` con Reed
`desconocido`; no es una muestra de la demo integrada.

| `ts` (UTC) | `tipo` | `origen` | `resultado` | `puerta` | `eventId` | `ingresoTs` (UTC) |
|---|---|---|---|---|---|---|
| 22:01:16.959 | lectura | edge | ejecutado | desconocida | `1795557676959#c7ff…` | 22:01:18.703 |
| 22:00:46.942 | lectura | edge | ejecutado | desconocida | `1795557646942#5e58…` | 22:00:48.594 |
| 22:00:16.933 | lectura | edge | ejecutado | desconocida | `1795557616933#8317…` | 22:00:17.664 |
| 21:59:46.924 | lectura | edge | ejecutado | desconocida | `1795557586924#c7a1…` | 21:59:47.766 |
| 21:59:16.917 | lectura | edge | ejecutado | desconocida | `1795557556917#b58c…` | 21:59:18.064 |

En 31 filas de la ventana 21:44:18Z–22:01:46Z, `ingresoTs − ts` estuvo entre
0,695 y 2,413 s. Ese valor acota el registro de la fila, no la latencia desde
el sensor ni la respuesta por voz. El `eventId` completo quedó fuera del
documento para no reutilizarlo como evidencia de una prueba no realizada.

## 5. Elementos aún pendientes

- Muestras anonimizadas que incluyan movimientos `ejecutado` con origen `rfid`,
  `voz` y `edge`, con su resultado y hora local convertida desde `ts`.
- Captura de la consulta por voz y del estado devuelto, con la misma ventana
  temporal que la fila de DynamoDB.
- Captura del recorrido de una orden por voz hasta `desired`, su `delta` y su
  confirmación en `reported` usando el actuador virtual.
- Captura de una repetición real de `eventId` y de un mensaje inválido para
  acreditar deduplicación y descarte en AWS.
- Registro de paginación con `LastEvaluatedKey` si la ventana de demo supera una
  página, y comprobación del estado «Sin dato» cuando falte `cerradura`.

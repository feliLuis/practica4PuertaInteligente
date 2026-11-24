# Mockups de reportes — RF15

Los valores de estas pantallas son **ficticios** y muestran el diseño. Los datos del prototipo se incorporarán como evidencias después de la prueba integrada.

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

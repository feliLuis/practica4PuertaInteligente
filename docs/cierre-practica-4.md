# Cierre técnico de la Práctica 4

Este plan usa las evidencias del [informe](Practica-4.md) y queda pendiente de autorización para ejecución real. La prueba AWS empleará el cliente virtual y conservará el SG90 aislado del control remoto.

## 1. Configuraciones afectadas

Base observada con el perfil `puerta-inteligente` en `us-east-2`. Los archivos con sufijo `.proposed.json` son propuestas locales y los marcadores deben sustituirse solo en una copia privada del entorno autorizado.

| Archivo o recurso | Situación y decisión |
|---|---|
| [Exportación sanitizada](../aws/config/deployed-sanitized.json) | Conservar como referencia de lo observado. Identifica los paquetes, roles y configuraciones actuales sin valores sensibles. Actualizar la exportación solo si cambia el recurso real. |
| [Política IoT](../aws/config/iot-policy.proposed.json) | La versión activa v1 del proyecto original conserva ARN incompatibles. Para laboratorio, copiar la política y restringir `client/`, `topic/` y `topicfilter/` a la identidad y a los tópicos de `chapa-inteligente-9001` o al Thing reservado de la prueba. Mantener el certificado y la política separados de los de la chapa física. |
| [Regla IoT](../aws/config/iot-rule.proposed.json) | La regla desplegada está habilitada y usa `SELECT …, * FROM 'chapa/+/events'`. Usar la proyección explícita local para derivar `thingName` y `ts` y preservar `eventId` sin incorporar campos no previstos. En laboratorio restringir el filtro a `chapa/<THING_LAB>/events` y dirigir la acción solo a la Lambda de laboratorio. Su permiso de invocación debe apuntar a esa regla. |
| [Roles Lambda](../aws/config/lambda-roles.proposed.json) | Las dos funciones del proyecto original continúan con `PowerUserRole`. Usar la propuesta como referencia para limitar la Lambda de almacenamiento a la tabla y logs del laboratorio. La prueba de telemetría requiere la Lambda de histórico corregida. El backend Alexa desplegado sigue en la versión auditada y queda fuera de esta prueba. |
| [Cliente virtual](../aws/config/virtual-device.example.json) | Crear una copia ignorada `virtual-device.local.json` con endpoint ATS, puerto 8883, Thing de laboratorio y rutas locales a la CA y al par X.509 de ese cliente. |
| Shadow y DynamoDB | Usar Shadow clásico del Thing virtual y conservar el esquema PK `thingName` / SK `eventKey`, con TTL `ttl` en la tabla de histórico. Usar una tabla exclusiva del entorno aislado. La Lambda admite `HISTORY_TABLE` para seleccionarla. La tabla de vinculaciones y el modelo Alexa no requieren cambios para ensayar telemetría. |

**Bloqueo de evidencia.** Los hashes de los dos paquetes desplegados coinciden con la auditoría anterior. La deduplicación condicional y el contrato corregido de Alexa se comprobaron con las fuentes locales. Una fila en el despliegue original no demostraría que las correcciones nuevas estén activas.

## 2. Validación real de telemetría, tras autorización

1. Registrar el proyecto y la región autorizados, el endpoint ATS, el Thing virtual reservado `chapa-inteligente-9xxx`, la política, la regla, el hash de la Lambda de histórico y el nombre de su tabla. Verificar que no pertenecen a la chapa física. El proyecto aislado debe disponer del código corregido antes de medir deduplicación. Mantener el SG90 desconectado y usar la identidad de laboratorio para todo el tráfico.
2. Leer el Shadow del Thing virtual. Preparar solo configuración, por ejemplo `desired.modo=manual` y `desired.autocierre_s=21`. La prueba de telemetría no necesita órdenes de movimiento ni modificaciones en Alexa.
3. Ejecutar una vez `python -B aws/virtual_device/telemetry.py aws/config/virtual-device.local.json --allow-isolated-aws`. Guardar el `eventId` emitido, la hora UTC, la aceptación del Shadow y el acuse MQTT. La aceptación del Shadow y el PUBACK comprueban etapas distintas.
4. Leer el Shadow por una vía independiente. Confirmar `reported.modo=manual`, `autocierre_s=21`, `reportado_ms` reciente, `puerta=cerrada` simulada y `cerradura=desconocida`. Corroborar que el cliente no escribió órdenes ni resultados de movimiento.
5. Consultar DynamoDB por el Thing y por el ID del paso 3. Registrar errores de la regla o Lambda si la fila falta. Medir la latencia hasta la primera lectura consistente que devuelve la fila, indicando el intervalo de sondeo y que ese valor acota la latencia de persistencia. Conservar las métricas y muestras anonimizadas para los anexos.

### Persistencia y deduplicación

| Paso | Acción y criterio de aceptación |
|---|---|
| Primera publicación | Obtener el ítem con lectura consistente. La PK es el Thing virtual y la SK es el prefijo numérico de `eventId` convertido a ISO 8601 UTC con milisegundos, seguido de `#` y el sufijo hexadecimal del mismo ID. Verificar `eventId`, `tipo=lectura`, `origen=edge`, `resultado=ejecutado`, puerta, hora de ocurrencia, `ingresoTs` y `ttl`. |
| Reenvío real | Tras cerrar el cliente virtual, republicar con un cliente de laboratorio autorizado en `chapa/<THING_LAB>/events`. Usar el **mismo** `eventId` y payload original. Para la lectura del cliente virtual, el cuerpo incluye `eventId`, `tipo=lectura`, `origen=edge`, `resultado=ejecutado` y `puerta=cerrada`. Una segunda ejecución normal del cliente genera un ID distinto. |
| Confirmación | Obtener otro PUBACK y observar una segunda ejecución de regla y Lambda en la ventana aislada sin tráfico ajeno. Volver a leer la fila de forma consistente y consultar las páginas de la partición del Thing en el rango de prueba. Debe aparecer **un solo registro con ese ID**. `eventKey`, `ingresoTs` y `ttl` deben permanecer iguales. Sin evidencia de la segunda invocación, registrar la deduplicación real como inconclusa. |
| Controles | Publicar otra lectura válida con un ID nuevo y verificar una segunda fila. Publicar un mensaje inválido con otro ID, por ejemplo una lectura sin `puerta`, y verificar el descarte y la ausencia de fila. Medir IDs válidos únicos persistidos / publicados, duplicados aparte y latencia máxima frente a las referencias de 99 % y 5 s. |

## 3. Acreditación con la evidencia actual

| RF | Implementación y evidencia acreditable hoy | Pendiente de validación real |
|---|---|---|
| RF1 | Captura Reed/RFID implementada y cubierta por pruebas de firmware con sustitutos. | Comparar lecturas y presentaciones con el montaje. |
| RF2 | El cliente virtual publica un snapshot válido por MQTT/TLS en loopback. | Adaptador del ESP32 y reporte físico periódico de sensores y estado del actuador. |
| RF3 | El firmware confirma cierre por ESP-NOW en pruebas de escritorio. La demo local confirma órdenes con un actuador virtual. | Integrar consumidor Shadow con el controlador. El recorrido remoto con actuación física queda fuera de la prueba aislada y requiere una autorización distinta. |
| RF4 | El backend local lee el Reed vigente en español. | Verificar frases en Alexa Console y lectura vigente alimentada por el objeto. |
| RF5 | El backend local escribe `desired` y el actuador virtual ejecuta la orden válida. | Alexa Console y recorrido remoto integrado. Actuación física supeditada a autorización específica. |
| RF6 | El backend local lee la estimación de cerradura y distingue estados desconocidos. | Probar frases y comparar el estado reportado con la posición real. |
| RF7 | Alta por número entero y persistencia verificadas localmente con el sustituto de DynamoDB. | Verificar persistencia entre sesiones de Alexa. |
| RF8 | Resolución por usuario, mensaje sin vínculo y separación de dos usuarios verificados localmente. | Confirmar en Alexa Console con identidades y Things autorizados. |
| RF9 | Existencia, alta, reemplazo y desvinculación verificados localmente. | Mostrar esos cambios con registros autorizados de la demo. |
| RF10 | Regla habilitada y permiso constatados en auditoría. Proyección SQL probada en simulación. | Publicación real del cliente de laboratorio y evidencia de invocación. |
| RF11 | Validación, descarte, enriquecimiento y escritura condicional verificados con sustitutos. | Persistencia, latencia y deduplicación según §2. |
| RF12 | Eventos de voz, RFID y Edge preservan origen en pruebas locales de histórico. | Capturar movimientos supervisados y verificar sus registros. |
| RF13 | Autocierre y confirmación ESP-NOW implementados y probados con reloj, radio y actuador simulados. | Medir autocierre y verificar interbloqueos bajo supervisión local. |
| RF14 | En loopback se aplicaron parámetros del Shadow y se publicaron los valores aplicados. Las Lambda locales verifican la consulta y el cambio. | Validar Shadow AWS real, configuración en el ESP32 y frases de Alexa. |
| RF15 | **Acreditable como entregable de diseño.** [Mockups y consultas](mockups-reportes.md) trazados a la tabla existente. | Adjuntar muestras anonimizadas como evidencia global de la práctica. |

## 4. Lista de comprobación física bajo supervisión

1. **Aislar el control remoto.** Mantener el SG90 sin alimentación y sin acople al mecanismo durante las pruebas de radio, TLS y Shadow. Las placas no usan certificado, Thing ni tópico del cliente virtual. Mantener cualquier consumidor de comandos remotos deshabilitado en la placa.
2. **Confirmar montaje y radio.** Revisar pines, 3,3 V del RC522, Reed y alimentación propuesta del servo. Confirmar MAC STA y canal de ambos peers sin copiar identificadores a las evidencias. Confirmar el canal del AP fijo de 2,4 GHz. Cuando exista el adaptador, asociar solo la placa de control al AP y anotar canal efectivo y continuidad de Reed, latidos y confirmaciones ESP-NOW antes, durante y después de reconectar.
3. **Medir sensores.** Abrir y cerrar en una serie declarada de ciclos. Registrar concordancia con posición observada, rebote y reporte de desconocido al desconectar el nodo. Ensayar tarjeta, llavero, UID desconocido y repetición. Anotar aciertos, omisiones y duplicados sin registrar UID crudos.
4. **Probar servo solo por eventos locales.** Con supervisión y movimiento libre, comprobar alimentación regulada de 5 V y GND común. Verificar recorrido y retención. Probar RFID autorizado, modo bloqueado, autocierre y rechazo si se abre la puerta o vence la confirmación de 750 ms. Mantener el consumidor remoto deshabilitado durante toda esta serie. Correlacionar posición observada con la estimación del firmware.
5. **Medir continuidad.** Operar al menos diez minutos como referencia. Cortar WiFi de infraestructura y confirmar que el enlace local, RFID y autocierre siguen operativos con lecturas vigentes. Reiniciar el nodo sensor y comprobar recuperación y rechazo de sesiones antiguas. Tras disponer del adaptador, comprobar también restauración TLS, publicación de telemetría y reconciliación de parámetros permitidos, con SG90 aislado durante cualquier ensayo remoto.
6. **Guardar evidencia.** Registrar hora UTC, escenario, resultados y limitaciones. Medir la latencia de voz hasta Shadow o hasta el resultado virtual por separado. La latencia de extremo a extremo hasta actuación física sigue pendiente mientras se mantenga el aislamiento remoto. No presentar mediciones virtuales como físicas.

## 5. Revisión de datos sensibles

La revisión por patrones cubrió 69 archivos del árbol de trabajo y 176 blobs históricos de las referencias Git locales. No detectó claves AWS, asignaciones de secretos, certificados, claves privadas ni contenedores de claves. Las plantillas incluyen marcadores y rutas vacías. Los archivos de configuración privada y los formatos PEM, KEY y CRT están excluidos.

**Hallazgo preexistente.** El README incluye una MAC observada y el README y la configuración de control incluyen UID RFID de prototipo. Son identificadores del montaje y los UID forman parte de su autorización local. Tratar los UID como sensibles si corresponden a tarjetas reales. Para distribuir el repositorio, considerar muestras ficticias y provisión local y reemplazar las tarjetas registradas si sus UID se reutilizan. Este cierre conserva el firmware y el historial existente.

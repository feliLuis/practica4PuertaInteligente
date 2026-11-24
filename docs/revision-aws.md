# Revisión de AWS en solo lectura

Usar el proyecto **Greenfield Adventures**, perfil `puerta-inteligente` y región
`us-east-2`. Comparar lo que muestra AWS con la
[exportación sanitizada](../aws/config/deployed-sanitized.json). Los archivos
`*.proposed.json` describen cambios locales pendientes de aprobación.

| Consola | Qué revisar |
|---|---|
| **IoT Core → Things → chapa-inteligente-01** | ID de cliente, certificado activo y política asociada. En **Device Shadows**, comparar `desired`, `reported`, `delta` y versión. Registrar órdenes de prueba antiguas sin editarlas. |
| **IoT Core → Security → Policies** | Consultar la versión predeterminada de `chapa-inteligente-01-policy`. Comparar `Connect` con un ARN `client/`, `Publish` y `Receive` con `topic/` y `Subscribe` con `topicfilter/`. La versión auditada contiene ARN incompatibles con esos recursos. |
| **IoT Core → Message routing → Rules** | Verificar que `ChapaEventosRule` esté habilitada, filtre `chapa/+/events` y apunte a `chapa-almacenar-evento`. Comparar SQL y permisos de invocación con la exportación. |
| **Lambda → Functions → Configuration** | Revisar runtime, handler, rol y permiso de invocación de ambas funciones. El histórico se invoca desde la regla IoT y el backend de voz desde Alexa Skills Kit. Evitar mostrar valores de entorno o enlaces de descarga del código en las evidencias. |
| **DynamoDB → Tables** | `AlexaThingBindings` usa `userId`. `ChapaHistory` usa `thingName` y `eventKey`, con TTL en `ttl`. Ambas son bajo demanda. Consultar el histórico por Thing y rango, sin incluir datos personales en el informe. |
| **Alexa Developer Console → Build** | Seleccionar `es-MX` y revisar la invocación **chapa inteligente**, los intents y el endpoint contra el [modelo original](../aws/alexa/interaction-model/es-MX.json). |

### Comprobaciones rápidas por CLI

```powershell
$env:AWS_PROFILE = 'puerta-inteligente'
$env:AWS_DEFAULT_REGION = 'us-east-2'
aws iot describe-thing --thing-name chapa-inteligente-01
aws iot get-policy --policy-name chapa-inteligente-01-policy
aws iot get-topic-rule --rule-name ChapaEventosRule
aws lambda get-function-configuration --function-name chapa-almacenar-evento --query '{Runtime:Runtime,Handler:Handler,Role:Role}'
aws lambda get-function-configuration --function-name chapa-skill-alexa --query '{Runtime:Runtime,Handler:Handler,Role:Role}'
aws dynamodb describe-table --table-name AlexaThingBindings --query 'Table.KeySchema'
aws dynamodb describe-table --table-name ChapaHistory --query 'Table.KeySchema'
aws dynamodb describe-time-to-live --table-name ChapaHistory
```

Para preparar evidencia de la revisión, registrar el recurso, el campo y el
resultado, y omitir IDs de cuenta, usuarios y credenciales. Para la demo local,
volver a las [instrucciones del README](../README.md#ejecución-y-demostración-simulada).

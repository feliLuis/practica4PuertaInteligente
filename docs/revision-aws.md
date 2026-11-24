# Revisión de AWS en solo lectura

Usar el proyecto **Greenfield Adventures**, perfil `puerta-inteligente` y región
`us-east-2`. Comparar lo que muestra AWS con la
[exportación sanitizada](../aws/config/deployed-sanitized.json). Los archivos
`*.proposed.json` describen cambios locales pendientes de aprobación.

| Consola | Qué revisar |
|---|---|
| **IoT Core → Things → chapa-inteligente-01** | ID de cliente, certificado activo y política asociada. En **Device Shadows**, comparar `desired`, `reported`, `delta` y versión. Registrar órdenes de prueba antiguas sin editarlas. |
| **IoT Core → Security → Policies** | Consultar la versión predeterminada de `chapa-inteligente-01-policy`. La v2 observada usa `Connect` sobre `client/`, `Publish` sobre los dos tópicos del Thing y no concede `Receive` ni `Subscribe`. Mantener ese alcance de solo publicación. |
| **IoT Core → Message routing → Rules** | Verificar que `ChapaEventosRule` esté habilitada, filtre `chapa/+/events` y apunte a `chapa-almacenar-evento`. Comparar SQL y permisos de invocación con la exportación. |
| **Lambda → Functions → Configuration** | Revisar runtime, handler, rol y permiso de invocación de ambas funciones. El histórico se invoca desde la regla IoT y el backend de voz desde Alexa Skills Kit. Evitar mostrar valores de entorno o enlaces de descarga del código en las evidencias. |
| **DynamoDB → Tables** | `AlexaThingBindings` usa `userId`. `ChapaHistory` usa `thingName` y `eventKey`, con TTL en `ttl`. Ambas son bajo demanda. Consultar el histórico por Thing y rango, sin incluir datos personales en el informe. |
| **Alexa Developer Console → Build** | Seleccionar `es-MX` y revisar la invocación **chapa inteligente**, los intents y el endpoint contra el [modelo original](../aws/alexa/interaction-model/es-MX.json). |

### Comprobaciones rápidas por CLI

En este equipo `aws` no está en el `PATH`. Invocarlo por ruta absoluta; todas
las órdenes siguientes son de solo lectura:

```powershell
$aws = "$env:ProgramFiles\Amazon\AWSCLIV2\aws.exe"
$profileArgs = @('--profile', 'puerta-inteligente', '--region', 'us-east-2')

& $aws sts get-caller-identity @profileArgs
& $aws iot describe-thing --thing-name chapa-inteligente-01 @profileArgs
& $aws iot get-policy --policy-name chapa-inteligente-01-policy @profileArgs
& $aws iot get-topic-rule --rule-name ChapaEventosRule @profileArgs
& $aws lambda get-function-configuration --function-name chapa-almacenar-evento --query '{Runtime:Runtime,Handler:Handler,Role:Role,CodeSha256:CodeSha256}' @profileArgs
& $aws lambda get-function-configuration --function-name chapa-skill-alexa --query '{Runtime:Runtime,Handler:Handler,Role:Role,CodeSha256:CodeSha256}' @profileArgs
& $aws dynamodb describe-table --table-name ChapaHistory --query 'Table.{Status:TableStatus,KeySchema:KeySchema}' @profileArgs
& $aws dynamodb describe-time-to-live --table-name ChapaHistory @profileArgs
```

`get-thing-shadow` exige un archivo de salida local; usar una ruta temporal y
leerla después. No modifica el Shadow:

```powershell
$salida = "$env:TEMP\shadow-chapa.json"
& $aws iot-data get-thing-shadow --thing-name chapa-inteligente-01 $salida @profileArgs
Get-Content $salida
```

Para verificar datos recientes y la cadena de persistencia:

```powershell
$desde = [long][math]::Floor((Get-Date).ToUniversalTime().AddHours(-1).Subtract([datetime]'1970-01-01').TotalMilliseconds)
& $aws dynamodb query --table-name ChapaHistory `
  --key-condition-expression 'thingName = :t' `
  --expression-attribute-values '{\":t\":{\"S\":\"chapa-inteligente-01\"}}' `
  --no-scan-index-forward --limit 20 --consistent-read @profileArgs
& $aws logs filter-log-events --log-group-name /aws/lambda/chapa-almacenar-evento `
  --start-time $desde --limit 20 @profileArgs
& $aws logs filter-log-events --log-group-name /aws/lambda/chapa-skill-alexa `
  --start-time $desde --limit 20 @profileArgs
```

### Resultado observado el 24/11/2026

| Recurso | Valor observado (UTC) |
|---|---|
| Shadow | Versión 28, `reported.reportado_ms=1795557376854` (`21:56:17.654Z`), `puerta=desconocida`, `cerradura=desconocida`, `modo=automatico`, `autocierre_s=10`. `desired` con `modo=bloqueado` y `cerradura=desbloqueada`, con `delta` pendiente |
| `ChapaHistory` | 31 filas `lectura`/`edge` entre `21:44:18.290Z` y `22:01:46.970Z`, con `eventId`, `ingresoTs` y `ttl`. Tres filas antiguas `acceso`/`rfid` |
| CloudWatch | Invocaciones de `chapa-almacenar-evento` desde `21:44:20.604Z`; sin invocaciones de `chapa-skill-alexa` después de `03:30Z` |
| Política | Versión 2: `iot:Connect` sobre `client/chapa-inteligente-01` y `iot:Publish` sobre el tópico de eventos y `shadow/update`; no incluye `Receive` ni `Subscribe` |
| Regla | Activa, SQL explícita con `thingName`, `ts`, `eventId`, `tipo`, `origen`, `resultado`, `puerta`, `cerradura`, `modo`, `motivo`, `credentialId`, `seq`, `diferido`, `autocierre_s`, `simulado`, filtrada a `chapa/chapa-inteligente-01/events` |
| Lambdas | `chapa-almacenar-evento`: `python3.13`, handler `lambda_function.lambda_handler`, `CodeSha256=OwGJItvs…`, modificado el 24/11 a las 20:37Z. `chapa-skill-alexa`: mismo runtime y handler, `CodeSha256=M9wpEQB7…`, sin cambios desde el 24/11 a las 02:41Z |

Estos datos prueban recepción reciente y persistencia, no la identidad del
publicador ni una lectura física vigente. La política vigente impide suscribirse
a `delta` con ese certificado; el firmware tampoco implementa esa suscripción.
Para preparar evidencia, registrar el recurso, el campo y el resultado con hora
UTC, y omitir IDs de cuenta, usuarios y credenciales. Para la demo local, volver
a las [instrucciones del README](../README.md#ejecución-y-demostración-simulada).

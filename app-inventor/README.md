# App del celular — MIT App Inventor

La aplicación es la **interfaz de operador**: muestra las variables del proceso
y acciona los equipos. No contiene lógica de control: la lógica está en las
placas, que es donde debe estar.

## Documentos de esta carpeta

| Archivo | Contenido |
|---|---|
| `01-guia-paso-a-paso.md` | construcción completa de la app desde cero: pantalla, componentes y bloques |
| `02-bloques-detallados.md` | todos los bloques escritos uno por uno, incluidos los que hay que agregar, con comentarios |
| `03-camara-en-la-app.md` | foto fija y video de la ESP32-CAM dentro de la app |

## Extensión necesaria

**`UrsPahoMqttClient`** — de <https://ullisroboterseite.de/android-AI2-PahoMQTT-en.html>

Descargar el `.aix` e importarlo: en la paleta, **Extension → Import extension**.

### Nombres de bloques que suelen confundirse

| Bloque | Argumentos | Nota |
|---|---|---|
| `Publish` | `Topic`, `Message` | **solo dos** |
| `PublishEx` | `Topic`, `Message`, `QoS`, `Retain` | para publicar con retención |
| `Subscribe` | `Topic`, `QoS` | |
| `MessageReceived` | evento | **este** es el evento correcto |
| `PublishedReceived` | — | de la extensión **vieja** `UrsAI2Mqtt`; no existe acá |

## Componentes de la pantalla

| Componente | Para qué |
|---|---|
| `UrsPahoMqttClient1` | conexión al broker |
| `Label` de temperatura y humedad por sector | lectura |
| `Button` por actuador | comando |
| `Image` | foto de la cámara |
| `WebViewer` | video MJPEG |
| `Clock` | reintento de conexión |
| `Chart` + `ChartData2D` | gráfico de tendencia |

## Configuración del cliente MQTT

| Propiedad | MODO A | MODO B |
|---|---|---|
| `BrokerAddress` | IP de la PC con Mosquitto | igual |
| `BrokerPort` | 1883 | 1883 |
| `ClientID` | `AppITLM` *(único por celular)* | igual |
| `UserName` | *(vacío)* | `appmovil` |
| `Password` | *(vacío)* | la clave de `mosquitto_passwd` |

> Si dos celulares usan el mismo `ClientID`, el broker expulsa a uno cada vez
> que el otro se conecta. En el aula conviene `AppITLM_<nombre>`.

## El patrón de los botones

Tres reglas que resuelven la mayoría de los problemas de la app:

1. **Al pulsar** se publica el comando (`ON` / `OFF`) y **no** se cambia el color
2. **El color** se cambia en `MessageReceived`, cuando llega el tópico `…/estado`
3. **El comando no se publica retenido**; el estado sí lo publica la placa

Sin esto, el botón queda verde aunque la placa esté desconectada y nadie se
entera de que la orden no llegó.

## Sobre la generación automática de `.aia`

No se incluye un `.aia` armado por programa. El formato de bloques de App
Inventor (`.bky`, Blockly) es muy estricto: si un atributo no cuadra, el editor
rechaza **todo** el archivo con el mensaje *"El área de bloques no se ha cargado
correctamente"* y solo carga las variables globales. Se intentó y falla.

Por eso los bloques están escritos paso a paso en
`02-bloques-detallados.md`: se arman en la interfaz, que además es lo que los
alumnos tienen que aprender a hacer.

## Errores frecuentes

Ver `docs/03-errores-frecuentes.md`, sección **E**. El más común:

```
The operation lookup in pairs cannot accept the arguments: , ["v"], [24.8], ["0"]
```

Significa que la app esperaba un diccionario JSON y recibió un número suelto:
esa placa todavía publica texto plano. Se arregla **en la placa**, cargándole el
firmware de este repositorio.

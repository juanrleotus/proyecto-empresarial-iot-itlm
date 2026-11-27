# Arquitectura del sistema

## 1. El diagrama completo

```
┌──────────────────── CAMPO (las placas) ────────────────────┐
│                                                            │
│  ESP32 DevKit        ESP8266 NodeMCU     ESP32-S3-WROOM    │
│  PLANTA              TANQUE              OFICINA           │
│  DHT11 + aire        DS18B20 + bomba     DHT11 + luces     │
│       │                   │                    │           │
│  ESP32-CAM AI Thinker (ALMACEN)                            │
│  camara + flash + servidor HTTP propio                     │
│       │                   │                    │           │
└───────┼───────────────────┼────────────────────┼───────────┘
        │        Wi-Fi 2,4 GHz / TCP 1883        │
        └───────────────────┼────────────────────┘
                            ▼
              ┌──────────────────────────┐
              │   MOSQUITTO  (broker)    │   puerto 1883
              │   MODO A: anonimo        │
              │   MODO B: usuario+clave  │
              └────┬───────────┬─────────┘
                   │           │
       ┌───────────┘           └───────────┬──────────────┐
       ▼                                   ▼              ▼
┌─────────────┐                    ┌──────────────┐  ┌──────────┐
│  NODE-RED   │                    │ APP DEL      │  │   MQTT   │
│  traduce y  │                    │ CELULAR      │  │ EXPLORER │
│  valida     │                    │ App Inventor │  │ (diagn.) │
└──────┬──────┘                    │ o Android St.│  └──────────┘
       ▼                           └──────────────┘
┌─────────────┐
│ POSTGRESQL  │  base itlm_iot
│ mediciones  │  eventos
│ alarmas     │
└──────┬──────┘
       ▼
┌─────────────┐
│  POWER BI   │  tablero de gestion
└─────────────┘

Camino aparte, SIN pasar por el broker:
   ESP32-CAM  ──HTTP :80/capture──>  app / navegador   (foto)
   ESP32-CAM  ──HTTP :81/stream───>  app / navegador   (video)
```

## 2. Por qué está armado así

**Las placas no se hablan entre sí.** Si la ESP32 tuviera que avisarle algo a la
ESP8266, necesitaría conocer su IP, y las IP cambian cada vez que el router
reparte direcciones. Con MQTT cada placa solo conoce **la dirección del
broker**. Agregar una quinta placa no obliga a tocar las otras cuatro.

**El video no pasa por el broker.** MQTT está pensado para mensajes cortos. Un
cuadro de video VGA pesa unos 30 kB y a 10 cuadros por segundo son 300 kB/s:
saturaría el broker y arruinaría la latencia de los comandos. Por eso la
ESP32-CAM levanta su **propio servidor HTTP** y por MQTT solo manda la URL.
Esta es la decisión de diseño más importante del proyecto: **cada protocolo para
lo que sirve**.

**Node-RED está en el medio y no en las placas.** Si cada ESP escribiera directo
en PostgreSQL, cada placa necesitaría la contraseña de la base de datos y
cualquier cambio en una columna obligaría a reprogramar las cuatro. Con
Node-RED en el medio, el conocimiento de la base está en un solo lugar.

**Power BI no toca el broker.** Lee la base. El tablero no participa de la
operación: si Power BI se cae, la planta sigue funcionando. Es la separación
clásica entre **nivel de campo** y **nivel de gestión** que aparece en la
pirámide de automatización.

## 3. Las cuatro capas

| Capa | Pieza | Qué hace | Si se cae… |
|---|---|---|---|
| Campo | ESP32 / ESP8266 | mide y acciona | ese sector queda sin datos ni control |
| Comunicación | Mosquitto | reparte mensajes | nadie se entera de nada, pero los relés quedan en su último estado |
| Supervisión | Node-RED + PostgreSQL | valida y guarda | se pierde la historia; la operación sigue |
| Gestión | Power BI | analiza | nadie lo nota en la planta |

Esta tabla es la respuesta a la pregunta "¿qué pasa si falla X?", que es la
pregunta que se hace en toda puesta en marcha real.

## 4. El contrato: los tópicos

| Función | Tópico | Sentido | Placa |
|---|---|---|---|
| Temp. planta | `empresa/planta/temperatura` | placa → | ESP32 |
| Hum. planta | `empresa/planta/humedad` | placa → | ESP32 |
| Temp. oficina | `empresa/oficina/temperatura` | placa → | ESP32-S3 |
| Hum. oficina | `empresa/oficina/humedad` | placa → | ESP32-S3 |
| Temp. tanque | `empresa/tanque/temperatura` | placa → | ESP8266 |
| Temp. almacén | `empresa/almacen/temperatura` | placa → | opcional |
| Aire | `empresa/planta/aire/cmd` → · `…/estado` ← | ambos | ESP32 |
| Bomba | `empresa/tanque/rele/cmd` → · `…/estado` ← | ambos | ESP8266 |
| Luces | `empresa/oficina/luces/cmd` → · `…/estado` ← | ambos | ESP32-S3 |
| Cámara | `empresa/camara/cmd` → · `…/estado` · `…/evento` ← | ambos | ESP32-CAM |

Nombre en minúsculas y en singular, de lo general a lo particular. MQTT
**distingue mayúsculas**: `empresa/Planta/temperatura` es un tópico distinto de
`empresa/planta/temperatura`, y es la causa más frecuente de "no llegan los
datos de un sector".

## 5. El formato del mensaje

```json
{"v": 24.8, "u": "C", "ts": 123456}
```

| Clave | Qué es |
|---|---|
| `v` | valor: número en sensores, texto `"ON"`/`"OFF"`/`"ESPERA"` en actuadores |
| `u` | unidad: `C`, `%`, `bool`, `evt` |
| `ts` | `millis()` de la placa |

El dato viaja **con su unidad**. Un `"24.8"` suelto obliga a cada consumidor a
suponer de qué unidad se trata, y las suposiciones son lo que produce accidentes
en instrumentación.

El campo `ts` **no es la fecha**: es el tiempo desde que la placa arrancó, porque
la placa no tiene reloj ni batería. La fecha real la pone PostgreSQL al insertar.

## 6. Comando y confirmación

```
App ──"ON"──> empresa/planta/aire/cmd
                      │
                ESP32 lo recibe, mueve el relé
                      │
 app ◀──retained "ON"── empresa/planta/aire/estado
                      │
              ahí el botón se pone verde
```

El botón de la app **no cambia de color al pulsarlo**: cambia cuando la planta
confirma. Si la placa está apagada, el botón no cambia, y eso es información:
el operario ve que algo anda mal en lugar de creer que accionó algo.

Los `/estado` se publican **retained**: el broker guarda el último valor y lo
entrega a quien se suscriba después, así que la app abre mostrando el estado
real sin preguntar nada. Además, cada placa deja un **Last Will**: si pierde la
alimentación, el broker publica `OFF` en su estado por ella.

## 7. Direcciones y puertos

| Servicio | Puerto | Dónde corre |
|---|---|---|
| Mosquitto | 1883/TCP | PC servidor |
| Node-RED | 1880/TCP | PC servidor |
| PostgreSQL | 5432/TCP | PC servidor |
| ESP32-CAM foto | 80/TCP | la propia cámara |
| ESP32-CAM video | 81/TCP | la propia cámara |

> **Todo en la misma red.** Si la PC está en la red del instituto por cable y
> las placas en el punto de acceso de un celular, no hay camino entre ellas: son
> dos redes distintas. La PC tiene que estar **en la misma Wi-Fi que las
> placas**, y en el firmware se usa la IP del **adaptador Wi-Fi** de la PC, no
> la del Ethernet.

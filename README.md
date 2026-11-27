# Proyecto Empresarial IoT ITLM

Sistema de supervisión y control industrial construido con placas ESP32 y
ESP8266, broker MQTT, Node-RED, PostgreSQL, Power BI y una aplicación para
celular.

Material didáctico de la **Tecnicatura Superior en Mecatrónica** — ITLM.
Todo el proyecto está documentado para que un alumno pueda reproducirlo de cero.

---

## Qué hace

Cuatro placas distribuidas en una planta ficticia miden temperatura y humedad,
accionan equipos y vigilan un depósito con una cámara. Todo pasa por un broker
MQTT. Node-RED guarda la historia en PostgreSQL, Power BI la analiza y una app
en el celular opera el sistema.

| Sector | Placa | Mide | Acciona |
|---|---|---|---|
| **Planta** | ESP32 DevKit v1 | temperatura y humedad (DHT11) | aire acondicionado |
| **Tanque** | ESP8266 NodeMCU v3 | temperatura del agua (DS18B20) | bomba de recirculación |
| **Oficina** | ESP32-S3-WROOM | temperatura y humedad (DHT11) | iluminación |
| **Almacén** | ESP32-CAM AI Thinker | imagen (OV2640) | flash |

```
┌──────── CAMPO ────────┐
│ ESP32  ESP8266        │
│ ESP32-S3  ESP32-CAM   │
└───────────┬───────────┘
            │ Wi-Fi · TCP 1883
            ▼
    ┌───────────────┐
    │   MOSQUITTO   │ ──────┬──────────────┐
    └───────┬───────┘       │              │
            ▼               ▼              ▼
      ┌──────────┐    ┌──────────┐   ┌──────────┐
      │ NODE-RED │    │   APP    │   │   MQTT   │
      └────┬─────┘    │ CELULAR  │   │ EXPLORER │
           ▼          └──────────┘   └──────────┘
     ┌────────────┐
     │ POSTGRESQL │
     └─────┬──────┘
           ▼
     ┌────────────┐
     │  POWER BI  │
     └────────────┘

El video de la camara NO pasa por el broker:
ESP32-CAM ──HTTP :81/stream──> app / navegador
```

El diagrama completo y las razones de cada decisión están en
[`docs/01-arquitectura.md`](docs/01-arquitectura.md).

---

## Dos versiones del sistema

El proyecto se entrega en **dos modos de autenticación**, que se eligen cambiando
un archivo de configuración del broker y dos líneas de las credenciales. **No hay
dos versiones del código.**

| | **MODO A** — sin autenticación | **MODO B** — con usuario y contraseña |
|---|---|---|
| Configuración del broker | `broker/mosquitto-sin-auth.conf` | `broker/mosquitto-con-auth.conf` |
| En el firmware | `MQTT_USUARIO ""` | `MQTT_USUARIO "esp32planta"` |
| Flujo de Node-RED | `node-red/flows-sin-auth.json` | `node-red/flows-con-auth.json` |
| Permisos por tópico | no tiene | `broker/aclfile.ejemplo` |
| Quién puede operar la planta | **cualquiera en la red** | solo usuarios autorizados |
| Para qué sirve | primeras clases, laboratorio aislado | modo recomendado |

El firmware decide en tiempo de ejecución:

```cpp
if (strlen(MQTT_USUARIO) == 0) {
    conectado = mqtt.connect(ID_CLIENTE, NULL, NULL, ...);          // MODO A
} else {
    conectado = mqtt.connect(ID_CLIENTE, MQTT_USUARIO, MQTT_CLAVE, ...);  // MODO B
}
```

> **Criterio didáctico.** Se arranca en MODO A para ver el sistema andando, y se
> pasa a MODO B como clase específica de seguridad — después de demostrar, con
> dos comandos, que en MODO A cualquiera puede encender la bomba o inyectarle
> una temperatura falsa al tablero. Ver
> [`docs/02-seguridad-y-credenciales.md`](docs/02-seguridad-y-credenciales.md).

---

## Antes de empezar: las credenciales

**Ninguna contraseña está en este repositorio, y ninguna debe agregarse.**

Cada carpeta de firmware trae `credenciales.ejemplo.h`. Hay que copiarlo como
`credenciales.h` y completarlo. Ese archivo está en `.gitignore`.

```cpp
const char* WIFI_SSID[]  = { "MiRed",  "HotspotCelular" };
const char* WIFI_CLAVE[] = { "clave1", "clave2" };

#define MQTT_SERVIDOR  "192.168.1.33"     // IP de la PC con Mosquitto
#define MQTT_PUERTO    1883

#define MQTT_USUARIO   ""                 // vacio = MODO A
#define MQTT_CLAVE     ""
```

Git guarda **todo el historial**: una clave subida por error queda recuperable
para siempre, aunque después se borre el archivo. La única solución real es
cambiar la contraseña. Esto es contenido de la materia, no un trámite.

---

## Puesta en marcha

Lista de verificación completa, paso por paso y con una comprobación en cada
etapa: [`docs/00-puesta-en-marcha.md`](docs/00-puesta-en-marcha.md).

Resumen del orden, que **no** conviene alterar:

1. Mosquitto escuchando en 1883 y el puerto abierto en el Firewall
2. `mosquitto_sub` y `mosquitto_pub` hablándose entre sí
3. MQTT Explorer conectado
4. `credenciales.h` en cada carpeta de firmware
5. Una placa publicando — **comprobado en MQTT Explorer**
6. Las cuatro placas
7. Los relés respondiendo a los comandos
8. PostgreSQL con la base `itlm_iot` y las tablas creadas
9. Node-RED con `node-red-contrib-postgresql`, flujo importado, **connected**
10. Filas creciendo en `mediciones`
11. App del celular
12. Power BI
13. **Pasar al MODO B**

Cada paso se comprueba antes de seguir: así, cuando algo no funciona, se sabe
exactamente en qué capa está el problema.

---

## Estructura del repositorio

```
proyecto-empresarial-iot-itlm/
│
├── README.md                      este archivo
│
├── broker/                        MOSQUITTO
│   ├── README.md                  instalacion, servicio, firewall, mosquitto_passwd
│   ├── mosquitto-sin-auth.conf    MODO A
│   ├── mosquitto-con-auth.conf    MODO B
│   └── aclfile.ejemplo            permisos por dispositivo
│
├── firmware/                      LAS CUATRO PLACAS
│   ├── README.md                  topicos, JSON, bibliotecas, codigos de error
│   ├── _comun/credenciales.ejemplo.h
│   ├── esp32_planta/              DHT11 + aire acondicionado
│   ├── esp8266_tanque/            DS18B20 + bomba
│   ├── esp32s3_oficina/           DHT11 + luces
│   └── esp32cam_almacen/          camara + servidor HTTP propio
│
├── base-de-datos/                 POSTGRESQL
│   ├── README.md                  instalacion, usuarios, mantenimiento
│   ├── 01-crear-tablas.sql        tablas, indices y vistas
│   └── 02-consultas-utiles.sql    14 consultas de diagnostico
│
├── node-red/                      EL TRADUCTOR
│   ├── README.md                  instalacion, importacion, diagnostico
│   ├── flows-sin-auth.json        MODO A
│   ├── flows-con-auth.json        MODO B
│   ├── funcion-clasificar-y-guardar.js
│   └── funcion-detectar-alarmas.js
│
├── app-inventor/                  APP DEL CELULAR
│   ├── 01-guia-paso-a-paso.md
│   ├── 02-bloques-detallados.md
│   └── 03-camara-en-la-app.md
│
├── app-android/                   VERSION NATIVA EN KOTLIN
│   └── ProyectoIoT_ITLM_AndroidStudio.zip
│
├── power-bi/                      TABLERO
│   ├── README.md                  Npgsql, conexion, tablero sugerido
│   └── medidas-dax.md             medidas listas para copiar
│
├── scripts/
│   └── registrador_csv.py         graba todo el broker en CSV
│
└── docs/
    ├── 00-puesta-en-marcha.md     lista de verificacion de cero a funcionando
    ├── 01-arquitectura.md         diagrama, topicos, decisiones de diseno
    ├── 02-seguridad-y-credenciales.md
    ├── 03-errores-frecuentes.md   ordenado por sintoma
    ├── 04-guia-para-el-aula.md    16 clases, 5 TP, evaluacion
    ├── 05-publicar-en-github.md   verificacion de seguridad y push
    └── esquemas/                  diagramas de conexion
```

---

## Tabla de tópicos

Es el **contrato** entre el firmware, Node-RED y la app. Si se cambia un tópico,
hay que cambiarlo en los tres lugares.

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

MQTT **distingue mayúsculas**: `empresa/Planta/temperatura` y
`empresa/planta/temperatura` son tópicos distintos. Es la causa más frecuente de
"no llegan los datos de un sector".

### Formato de los mensajes

```json
{"v": 24.8, "u": "C", "ts": 123456}
```

`v` es el valor, `u` la unidad y `ts` los milisegundos desde que la placa
arrancó. El dato viaja **con su unidad**: un `"24.8"` suelto obliga a cada
consumidor a suponer de qué unidad se trata, y las suposiciones son lo que
produce accidentes en instrumentación.

Los comandos son texto plano: `ON`, `OFF`, `TOGGLE`, `FOTO`.

---

## Decisiones de diseño que vale la pena mirar

**El video no pasa por el broker.** Un cuadro VGA pesa unos 30 kB; a 10 cuadros
por segundo son 300 kB/s, que saturarían el broker y arruinarían la latencia de
los comandos. La ESP32-CAM levanta su propio servidor HTTP y por MQTT solo manda
la URL. Cada protocolo para lo que sirve.

**El botón de la app no cambia de color al pulsarlo.** Publica el comando y
espera. El color cambia cuando llega la confirmación de la placa en el tópico
`…/estado`, publicado **retenido**. Si la placa está apagada, el botón no cambia,
y eso es información: el operario ve que la orden no llegó.

**Node-RED está en el medio y no en las placas.** Si cada ESP escribiera directo
en PostgreSQL, cada una necesitaría la contraseña de la base y cualquier cambio
de columna obligaría a reprogramar las cuatro.

**La fecha la pone PostgreSQL.** El `ts` de la placa son milisegundos desde el
arranque: la ESP32 no tiene reloj ni batería.

**El aire acondicionado tiene tres estados.** `ON`, `OFF` y `ESPERA`. Un pedido
de encendido antes de que pase el tiempo mínimo de reposo del compresor no se
rechaza: queda pendiente y se ejecuta cuando corresponde.

---

## Si algo no funciona

[`docs/03-errores-frecuentes.md`](docs/03-errores-frecuentes.md) está ordenado
por síntoma y cubre los errores reales que aparecieron armando este proyecto:
desde el servicio de Mosquitto que arranca y se detiene solo, hasta la ESP32-CAM
que no transmite por falta de corriente.

Los dos más frecuentes:

| Síntoma | Causa casi siempre |
|---|---|
| `[MQTT] ERROR rc=-2` en el monitor serie | la PC y las placas están en **redes distintas**, o el Firewall está cerrado |
| Llegan los datos de un solo sector | las otras placas no publican: ver MQTT Explorer antes de tocar Node-RED |

**Regla general: diagnosticar de afuera hacia adentro.** Broker → placas →
Node-RED → base de datos → tablero. Casi siempre el problema está en la capa
anterior a donde se nota.

---

## Uso didáctico

[`docs/04-guia-para-el-aula.md`](docs/04-guia-para-el-aula.md) propone una
secuencia de 16 clases, cinco trabajos prácticos y una evaluación integradora,
con las preguntas de defensa oral y un catálogo de fallas para sembrar en el TP
de diagnóstico.

Dos condiciones de aprobación que no se negocian: nada de claves subidas al
repositorio, y nada de cableado de 220 V sobre protoboard.

---

## Licencia

MIT — ver [`LICENSE`](LICENSE). Uso educativo libre, con mención de la fuente.

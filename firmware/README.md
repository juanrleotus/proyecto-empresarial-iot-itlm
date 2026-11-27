# Firmware de las placas

Cuatro placas, **un mismo estilo de programa**. Quien entiende una entiende las
cuatro: cambian el sensor, el actuador y los tópicos.

| Carpeta | Placa | Sector | Sensor | Actuador |
|---|---|---|---|---|
| `esp32_planta/` | ESP32 DevKit v1 | Planta | DHT11 (GPIO4) | Aire acond. (GPIO26) |
| `esp8266_tanque/` | ESP8266 NodeMCU v3 | Tanque | DS18B20 (D4/GPIO2) | Bomba (D1/GPIO5) |
| `esp32s3_oficina/` | ESP32-S3-WROOM | Oficina | DHT11 (GPIO4) | Luces (GPIO5) |
| `esp32cam_almacen/` | ESP32-CAM AI Thinker | Almacén | Cámara OV2640 | Flash (GPIO4) |

---

## 1. Lo primero: crear `credenciales.h`

Cada carpeta trae un archivo `credenciales.ejemplo.h`. **En cada carpeta**:

1. Copiar `credenciales.ejemplo.h`
2. Renombrar la copia a `credenciales.h`
3. Completar los datos reales

```cpp
const char* WIFI_SSID[]  = { "MiRed",  "HotspotCelular" };
const char* WIFI_CLAVE[] = { "clave1", "clave2" };

#define MQTT_SERVIDOR  "192.168.1.33"
#define MQTT_PUERTO    1883

#define MQTT_USUARIO   ""     // vacio = MODO A (sin autenticacion)
#define MQTT_CLAVE     ""
```

> **Por qué este archivo está separado.** `credenciales.h` figura en
> `.gitignore`: nunca se sube. Una contraseña subida a GitHub queda en el
> historial de git **para siempre**, aunque después se borre el archivo y se
> haga un commit nuevo. Cambiar la clave del Wi-Fi es la única solución real.
> Esto es parte del contenido de la materia, no un detalle administrativo.

---

## 2. Los dos modos de autenticación, con un solo código

No hay dos versiones del firmware. El programa decide en tiempo de ejecución:

```cpp
if (strlen(MQTT_USUARIO) == 0) {
  // MODO A: conexion anonima
  conectado = mqtt.connect(ID_CLIENTE, NULL, NULL, ...);
} else {
  // MODO B: con usuario y clave
  conectado = mqtt.connect(ID_CLIENTE, MQTT_USUARIO, MQTT_CLAVE, ...);
}
```

| Modo | `MQTT_USUARIO` | Broker |
|---|---|---|
| **A** sin autenticación | `""` | `mosquitto-sin-auth.conf` |
| **B** con usuario y clave | `"esp32planta"` | `mosquitto-con-auth.conf` + `aclfile` |

El usuario sugerido para cada placa está comentado en su propio
`credenciales.ejemplo.h` y listado en `broker/README.md`.

---

## 3. Bibliotecas a instalar

Gestor de Bibliotecas del IDE de Arduino (`Programa → Incluir biblioteca → Administrar bibliotecas`):

| Biblioteca | Autor | La usan |
|---|---|---|
| **PubSubClient** | Nick O'Leary | las cuatro |
| **DHT sensor library** | Adafruit | planta, oficina |
| **Adafruit Unified Sensor** | Adafruit | dependencia de la anterior |
| **OneWire** | Paul Stoffregen | tanque |
| **DallasTemperature** | Miles Burton | tanque |

Y los soportes de placa (`Archivo → Preferencias → URLs adicionales`):

```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
https://arduino.esp8266.com/stable/package_esp8266com_index.json
```

La ESP32-CAM no necesita biblioteca de cámara: `esp_camera.h` viene incluido en
el soporte de placa ESP32.

---

## 4. Selección de placa en el IDE

| Carpeta | Placa en el IDE | Notas |
|---|---|---|
| `esp32_planta` | ESP32 Dev Module | 115200 baudios |
| `esp8266_tanque` | NodeMCU 1.0 (ESP-12E Module) | 115200 baudios |
| `esp32s3_oficina` | ESP32S3 Dev Module | **USB CDC On Boot: Enabled**, si no el monitor serie no imprime |
| `esp32cam_almacen` | AI Thinker ESP32-CAM | Partition Scheme: **Huge APP (3MB No OTA)** |

---

## 5. Tabla única de tópicos del proyecto

Esta tabla es el **contrato** entre firmware, Node-RED y la app. Si se cambia un
tópico hay que cambiarlo en los tres lugares.

| Función | Tópico | Sentido | Placa |
|---|---|---|---|
| Temp. planta | `empresa/planta/temperatura` | placa → | ESP32 |
| Hum. planta | `empresa/planta/humedad` | placa → | ESP32 |
| Temp. oficina | `empresa/oficina/temperatura` | placa → | ESP32-S3 |
| Hum. oficina | `empresa/oficina/humedad` | placa → | ESP32-S3 |
| Temp. tanque | `empresa/tanque/temperatura` | placa → | ESP8266 |
| Temp. almacén | `empresa/almacen/temperatura` | placa → | *(opcional)* |
| Aire acond. | `empresa/planta/aire/cmd` → / `…/estado` ← | ambos | ESP32 |
| Bomba | `empresa/tanque/rele/cmd` → / `…/estado` ← | ambos | ESP8266 |
| Luces | `empresa/oficina/luces/cmd` → / `…/estado` ← | ambos | ESP32-S3 |
| Cámara | `empresa/camara/cmd` → / `…/estado` ← / `…/evento` ← | ambos | ESP32-CAM |

### Formato de los mensajes (JSON)

Un único formato para todo el proyecto:

```json
{"v": 24.8, "u": "C", "ts": 123456}
```

| Clave | Significado |
|---|---|
| `v` | valor (número en los sensores, texto `"ON"`/`"OFF"` en los actuadores) |
| `u` | unidad: `"C"`, `"%"`, `"bool"`, `"evt"` |
| `ts` | milisegundos desde que arrancó la placa (`millis()`) |

> **Por qué JSON y no texto suelto.** Un `"24.8"` suelto obliga a cada
> consumidor a adivinar la unidad. Con JSON el dato viaja con su unidad y su
> marca de tiempo, y agregar un campo nuevo no rompe a nadie.
> Si una placa quedó publicando texto plano, la app falla con
> `lookup in pairs cannot accept the arguments: ["v"], [24.8]`.

### Comandos aceptados

| Tópico | Mensaje (texto plano, **no** JSON) |
|---|---|
| `empresa/planta/aire/cmd` | `ON` · `OFF` · `TOGGLE` |
| `empresa/tanque/rele/cmd` | `ON` · `OFF` · `TOGGLE` |
| `empresa/oficina/luces/cmd` | `ON` · `OFF` · `TOGGLE` |
| `empresa/camara/cmd` | `FOTO` · `FLASH_ON` · `FLASH_OFF` · `ESTADO` |

---

## 6. Patrón comando / confirmación

Un botón de la app **no** cambia de color al pulsarlo. Hace esto:

```
App ──publica "ON"──> empresa/planta/aire/cmd
                               │
                         ESP32 lo recibe,
                         mueve el relé,
                               │
    app ◀──retained "ON"── empresa/planta/aire/estado
                               │
                     ahí el botón se pone verde
```

Así el color del botón refleja **lo que de verdad pasó en la planta**, no lo que
el usuario quiso. Si la placa está apagada, el botón no cambia: el operario ve
que algo anda mal.

Los tópicos `/estado` se publican **retained**: el broker guarda el último valor
y lo entrega a quien se suscriba después. Por eso la app muestra los botones en
su color correcto al abrirse, sin preguntar nada.

Además, cada placa deja un **Last Will**: si se corta su alimentación, el broker
publica `OFF` en su tópico de estado por ella.

---

## 7. Protecciones incluidas

**Pulso de arranque del relé.** En los tres sketches con relé:

```cpp
digitalWrite(PIN_RELE, RELE_OFF);   // primero el nivel
pinMode(PIN_RELE, OUTPUT);          // despues la direccion
digitalWrite(PIN_RELE, RELE_OFF);
```

Si se configura `pinMode(OUTPUT)` primero, el pin pasa por 0 V y el relé da un
golpe al encender la placa. Con una lámpara no importa; con un motor, sí.

**Arranque corto del compresor** (`esp32_planta`). Un aire acondicionado no debe
rearrancar antes de unos minutos de haberse apagado. El firmware guarda el
momento del apagado, y si llega un `ON` antes de `TIEMPO_MIN_REPOSO` (3 min) no
lo rechaza: lo deja **pendiente**, publica el estado `ESPERA` y lo ejecuta solo
cuando se cumple el reposo.

```cpp
const unsigned long TIEMPO_MIN_REPOSO = 180000UL;   // 3 min
```

Por eso el estado del aire tiene **tres** valores: `ON`, `OFF` y `ESPERA`
(mostrarlo en amarillo en la app).

**Nada de `while` bloqueante.** La reconexión al broker se intenta cada 3 s
dentro del `loop()`, sin quedarse encerrada:

```cpp
if (!mqtt.connected()) {
  if (millis() - ultimoIntento > 3000) { ultimoIntento = millis(); conectarMqtt(); }
} else {
  mqtt.loop();
}
```

Con un `while (!client.connected())` la placa deja de leer sensores y de atender
comandos mientras el broker está caído.

---

## 8. Códigos de error de `mqtt.state()`

Lo primero que hay que mirar en el monitor serie.

| Código | Significado | Qué revisar |
|---|---|---|
| `0` | conectado | — |
| `-4` | timeout | el broker tarda; keepalive |
| `-3` | conexión perdida | Wi-Fi inestable, alimentación |
| `-2` | **falló la conexión de red** | IP del broker mal, PC en otra red, firewall cerrado |
| `-1` | desconectado | normal antes de conectar |
| `1` | versión de protocolo mala | — |
| `2` | ID de cliente rechazado | dos placas con el mismo `ID_CLIENTE` |
| `3` | servidor no disponible | Mosquitto detenido |
| `4` | **usuario o contraseña incorrectos** | modo B: revisar `credenciales.h` |
| `5` | **no autorizado** | modo B: falta el usuario en `passwd`, o lo prohíbe la ACL |

> `rc=2` tiene una causa típica en el aula: dos alumnos cargan el mismo sketch
> sin cambiar `ID_CLIENTE` y se expulsan mutuamente del broker en bucle.

---

## 9. Alimentación — leer antes de culpar al código

| Placa | Consumo de pico | Alimentación mínima |
|---|---|---|
| ESP32 DevKit | ~250 mA | USB de PC alcanza |
| ESP8266 NodeMCU | ~200 mA | USB de PC alcanza |
| ESP32-S3 | ~300 mA | USB de PC alcanza |
| **ESP32-CAM** | **~500 mA** | **fuente de 5 V / 1 A + capacitor de 470 µF** |
| Módulo de relé | 70 mA por relé activado | desde 5 V, **no** desde 3V3 |

La bobina del relé y la cámara son cargas inductivas y pulsantes. Alimentar un
relé desde el pin de 3V3 de la placa hace que la ESP se reinicie justo al
accionar: parece un error de programa y es un problema de fuente.

> **Nunca** conectar los 220 V a la protoboard. El contacto del relé va cableado
> con borneras y cable de sección adecuada, y el circuito de potencia se arma
> con el tablero **desenergizado y consignado**.

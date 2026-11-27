# Esquemas de conexión — Sistema domótico ITLM

Tres nodos: **ESP32** (planta), **ESP8266** (tanque) y **ESP32-CAM** (almacén).

Los diagramas están en los archivos:

- `esq_esp32_planta.png`
- `esq_esp8266_tanque.png`
- `esq_esp32cam.png`

---

## Reglas que valen para los tres nodos

**GND común.** El negativo de la fuente, el GND de la placa y el GND del módulo relé van todos unidos. Sin esto la señal del GPIO no tiene referencia y el relé conmuta de forma errática o directamente no responde. Es el olvido más frecuente.

**Nunca alimentar el relé desde el pin 3V3.** El regulador interno de la placa no entrega la corriente de la bobina (unos 70 mA). El síntoma típico es que la placa se reinicia justo en el momento de conmutar.

**Siempre usar el contacto NA (normalmente abierto).** Si la placa se apaga o se cuelga, la carga queda desenergizada. Es el criterio de falla segura.

**Módulo relé con optoacoplador.** Se reconoce por un chip de 4 u 8 patas junto a la entrada. Aísla eléctricamente la placa de la etapa de potencia. Los módulos sin optoacoplador conectan el GPIO casi directo al transistor de la bobina.

---

## 1. ESP32 DevKit — Nodo PLANTA

Sensor DHT11 (temperatura y humedad) + relé del aire acondicionado.

### Conexiones

| Pin ESP32 | Va a | Color en el diagrama |
|---|---|---|
| `3V3` | DHT11 `VCC` | Naranja |
| `GND` | DHT11 `GND` | Negro |
| `GPIO4` | DHT11 `DATA` | Azul |
| `GPIO26` | Relé `IN` | Azul |
| `VIN (5V)` | Fuente `+5V` | Rojo |
| `GND` | Fuente `GND` y relé `GND` | Negro |
| — | Relé `VCC` ← Fuente `+5V` | Rojo |

### Resistencia

**10 kΩ entre `DATA` y `3V3`** del DHT11 (pull-up). Sin ella la lectura falla o devuelve valores erráticos.

Muchos módulos DHT11 de 3 pines ya la traen incorporada. Si el tuyo tiene 3 pines en lugar de 4, fijate si hay una resistencia en la plaquita antes de agregar otra.

### Por qué GPIO26

En el ESP32 hay que evitar:

| Pines | Motivo |
|---|---|
| GPIO 0, 2, 12, 15 | Definen el modo de arranque. Un relé ahí puede impedir que la placa butee |
| GPIO 6 a 11 | Conectados a la memoria flash interna |
| GPIO 34 a 39 | Solo entrada, no pueden manejar una salida |

GPIO26 es libre y no tiene ninguna función especial. Otros seguros: 25, 27, 32, 33.

---

## 2. ESP8266 NodeMCU — Nodo TANQUE

Sensor DS18B20 (temperatura) + relé de la bomba.

### Conexiones

| Pin NodeMCU | Va a | Color |
|---|---|---|
| `3V3` | DS18B20 `VDD` | Naranja |
| `GND` | DS18B20 `GND` | Negro |
| `D4 (GPIO2)` | DS18B20 `DATA` | Azul |
| `D1 (GPIO5)` | Relé `IN` | Azul |
| `VIN / VU (5V)` | Fuente `+5V` | Rojo |
| `GND` | Fuente `GND` y relé `GND` | Negro |
| — | Relé `VCC` ← Fuente `+5V` | Rojo |

El pin de 5 V cambia de nombre según el modelo: **VIN** en NodeMCU V3, **VU** en algunas versiones LoLin, **5V** en Wemos D1 mini. Es el mismo punto.

### Resistencia

**4,7 kΩ entre `DATA` y `3V3`** del DS18B20. Es obligatoria: el protocolo 1-Wire no funciona sin ella.

> Es el valor distinto al del DHT11 (10 kΩ). Si ponés 10 kΩ en el DS18B20, suele dar lecturas intermitentes; si ponés 4,7 kΩ en el DHT11, también anda. Pero conviene respetar cada valor.

**Si ves −127 °C**, es el valor que devuelve el DS18B20 cuando no se puede leer: casi siempre falta la pull-up o el cable DATA está flojo.

### Por qué D1 para el relé

En el ESP8266 conviene evitar **D0, D3, D4 y D8**, que participan del arranque. D4 ya está ocupado por el sensor (y funciona bien ahí, pero no admite un relé). **D1, D2, D5, D6 y D7** son los seguros.

---

## 3. ESP32-CAM AI Thinker — Nodo ALMACÉN

Esta placa no lleva sensores ni relé: la cámara ya viene soldada y el flash está en GPIO4 interno. Lo que cambia es que **tiene dos modos de conexión distintos**.

### Modo programación (solo para cargar el sketch)

| ESP32-CAM | FTDI |
|---|---|
| `5V` | `VCC` (jumper del FTDI en **5 V**) |
| `GND` | `GND` |
| `U0R (RX)` | `TX` ← cruzado |
| `U0T (TX)` | `RX` ← cruzado |
| `GPIO0` | `GND` ← puente |

El puente **GPIO0 → GND** pone la placa en modo de carga. Al terminar de subir el sketch: quitar el puente y presionar **RESET**.

Si tenés la placa base **ESP32-CAM-MB**, no hace falta nada de esto: se conecta por micro-USB y el botón BOOT reemplaza al puente.

### Modo normal (funcionamiento)

Solo `5V` y `GND` desde la fuente. Sin puente en GPIO0.

### La alimentación es crítica

Es la causa número uno de fallas con esta placa:

| Síntoma | Causa |
|---|---|
| `Brownout detector was triggered` | Alimentación insuficiente |
| Se reinicia justo al capturar | Pico de corriente de la cámara |
| `Camera init failed 0x105` | Poca corriente, o cable flex flojo |
| Conecta al WiFi pero no captura | Alcanza para el WiFi, no para la cámara |

Necesita **5 V con 500 mA reales** y el pico al capturar supera los 300 mA. No la alimentes desde el USB de la notebook ni desde un hub.

En el IDE verificá además: **PSRAM = Enabled** y **Partition Scheme = Huge APP**.

---

## Presupuesto de potencia

| Elemento | Típico | Pico |
|---|---|---|
| ESP32 DevKit | 100 mA | 500 mA |
| ESP8266 NodeMCU | 80 mA | 300 mA |
| ESP32-CAM | 180 mA | 400 mA |
| Módulo relé (bobina activa) | 70 mA | 80 mA |
| DHT11 | 2 mA | 2 mA |
| DS18B20 | 1,5 mA | 1,5 mA |

Con una fuente de **5 V / 2 A por nodo** vas sobrado. El margen importa: los picos de transmisión WiFi coinciden a veces con la conmutación del relé, y ahí es cuando se producen los reinicios.

---

## Lado de potencia

Los contactos del relé (`COM` / `NA`) van a la carga, no a la placa. Esa parte está eléctricamente aislada del resto.

### Para la clase

Usá una **lámpara o tira LED de 12 V** con su propia fuente. Se ve perfecto, es inofensiva y no exige justificar ninguna instalación.

### Si la práctica exige 220 V

- Todo el lado de potencia dentro de **caja cerrada**. Nada de contactos expuestos en el banco.
- Los módulos comunes son 10 A / 250 V AC, pero ese valor es para **carga resistiva**.
- Para motores, bombas, electroválvulas o un aire acondicionado: el relé comanda la **bobina de un contactor**, y el contactor maneja la carga. Un compresor arranca con varias veces su corriente nominal.
- Desenergizar siempre antes de tocar el cableado.

Normativa aplicable: IEC 60204-1 (equipamiento eléctrico de máquinas), AEA 90364 y Ley 19.587.

---

## Checklist antes de energizar

- [ ] GND común entre fuente, placa y relé
- [ ] Resistencia pull-up puesta y con el valor correcto (10 kΩ DHT11 / 4,7 kΩ DS18B20)
- [ ] El relé alimentado desde los 5 V, nunca desde 3V3
- [ ] Sin cortocircuito entre 5 V y GND (verificar con multímetro en modo continuidad)
- [ ] Carga conectada a COM y NA, no a NC
- [ ] Lado de 220 V desconectado de la red durante el montaje
- [ ] Fuente de 5 V / 2 A, no el USB de la notebook

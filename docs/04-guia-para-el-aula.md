# Guía para el aula

Propuesta de secuencia didáctica para la **Tecnicatura Superior en Mecatrónica**.
El proyecto es uno solo y se construye por capas: cada clase deja algo que
funciona.

---

## 1. Ubicación curricular

El proyecto integra contenidos de varios módulos:

| Módulo | Qué aporta al proyecto |
|---|---|
| Electricidad y electrónica | alimentación, divisores, pull-up, relés, cargas inductivas |
| Sensores y actuadores | DHT11, DS18B20, relé, criterios de selección |
| Programación de microcontroladores | C++ en ESP32/ESP8266, máquinas de estado |
| Redes industriales | MQTT, pub/sub, QoS, topología, direccionamiento IP |
| Supervisión y adquisición | Node-RED, base de datos, tablero |
| Mantenimiento | horas de marcha, arranques, alarmas, análisis de fallas |
| Seguridad e higiene | trabajo con 220 V, consignación, protección de personas |

---

## 2. Secuencia de 16 clases

### Bloque 1 — Campo (clases 1 a 5)

| # | Tema | Resultado concreto |
|---|---|---|
| 1 | Qué es IoT. Arquitectura del proyecto. Pub/sub frente a cliente-servidor | el alumno dibuja el diagrama de bloques completo |
| 2 | ESP32: GPIO, alimentación, entorno de desarrollo. Primer sketch | LED parpadeando y "Hola" por el monitor serie |
| 3 | DHT11: protocolo de un hilo, pull-up, limitaciones. Lectura por serie | temperatura y humedad en el monitor serie |
| 4 | Wi-Fi en la ESP32. Direccionamiento IP, DHCP, redes privadas | la placa se conecta y muestra su IP |
| 5 | MQTT: broker, tópicos, publicación. Mosquitto y MQTT Explorer | la placa publica y se ve en MQTT Explorer |

### Bloque 2 — Actuadores y segunda placa (clases 6 a 9)

| # | Tema | Resultado concreto |
|---|---|---|
| 6 | Relés: bobina, contactos, aislación, cargas inductivas. **Seguridad con 220 V** | relé accionado desde MQTT Explorer |
| 7 | Suscripción y `callback`. Patrón comando/confirmación. Mensajes retenidos | el relé confirma su estado y el estado sobrevive al reinicio |
| 8 | ESP8266 y DS18B20: bus OneWire, direccionamiento de sondas | segunda placa publicando en el mismo broker |
| 9 | Protecciones: arranque corto del compresor, pulso de arranque del relé, reconexión sin bloquear | el aire responde `ESPERA` y no castiga el compresor |

### Bloque 3 — Supervisión (clases 10 a 13)

| # | Tema | Resultado concreto |
|---|---|---|
| 10 | JSON: por qué el dato viaja con su unidad. Formato del proyecto | las placas publican JSON y la app lo interpreta |
| 11 | Node-RED: flujos, nodos, función. Validación del dato | el flujo recibe y clasifica los mensajes |
| 12 | Bases de datos relacionales. Modelo largo. Índices | filas apareciendo en `mediciones` |
| 13 | Consultas SQL de diagnóstico. Vistas. Alarmas por cambio de estado | el alumno responde preguntas sobre el proceso con SQL |

### Bloque 4 — Interfaz y cierre (clases 14 a 16)

| # | Tema | Resultado concreto |
|---|---|---|
| 14 | App Inventor: componentes, bloques, eventos. Interfaz de operador | app que muestra y acciona |
| 15 | ESP32-CAM: por qué el video no va por MQTT. HTTP y MJPEG | foto y video en la app |
| 16 | **Seguridad**: demostración del MODO A, paso al MODO B, ACL, mínimo privilegio | sistema con autenticación y permisos por dispositivo |

> La clase 16 es la más importante del proyecto y conviene no dejarla afuera por
> falta de tiempo. Un sistema que funciona pero que cualquiera puede accionar no
> es un sistema terminado.

---

## 3. Trabajos prácticos

### TP 1 — Caracterización de un sensor (individual)

Comparar el DHT11 con un termómetro patrón en cinco puntos. Construir la tabla
de error, calcular el error medio y la dispersión, y decidir si el sensor sirve
para controlar una cámara de frío de 2 a 8 °C.

*Se evalúa:* medición correcta, tratamiento del error, conclusión fundamentada.

### TP 2 — Agregar un sector nuevo (grupos de dos)

Incorporar un quinto sector al sistema, con su sensor y su actuador. Hay que
tocar: firmware, tabla de tópicos, ACL del broker, función de Node-RED y app.

*Se evalúa:* que el resto del sistema **no** haya que modificarlo. Si para
agregar un sector hubo que cambiar las otras placas, el diseño está mal
entendido.

### TP 3 — Control automático con histéresis (grupos de dos)

Que el aire arranque solo cuando la temperatura pasa de 28 °C y pare en 25 °C,
respetando la protección del compresor. Registrar un ciclo completo y graficarlo
desde la base de datos.

*Se evalúa:* la banda de histéresis, el conteo de arranques por hora, y la
justificación de por qué no se usa un solo umbral.

### TP 4 — Diagnóstico de una falla sembrada (individual, evaluación práctica)

El docente introduce **una** falla sin avisar cuál. Catálogo posible:

| Falla sembrada | Síntoma que verá el alumno |
|---|---|
| Quitar el pull-up del DHT11 | `Lectura fallida del DHT11` |
| Cambiar un carácter del tópico en una placa | esa placa se ve en MQTT Explorer pero no entra en la base |
| Poner `allow_anonymous false` sin crear usuarios | todas las placas con `rc=5` |
| Cambiar la IP del broker en una placa | solo esa placa con `rc=-2` |
| Detener el servicio PostgreSQL | Node-RED en rojo, el broker sigue bien |
| Invertir `RELE_ON` / `RELE_OFF` | el relé funciona al revés |
| Alimentar el relé desde 3V3 | la placa se reinicia al accionar |
| Usar un pull-up de 10 kΩ en el DS18B20 | `getDeviceCount() = 0`, valor −127 |

*Se evalúa:* **el método**, no la velocidad. Se pide entregar la secuencia de
comprobaciones hecha, en orden, con lo que se descartó en cada paso. Un alumno
que encuentra la falla por casualidad y no puede explicar cómo, no aprobó este TP.

### TP 5 — Tablero de mantenimiento (grupos de tres)

Tablero en Power BI que responda, sin que nadie explique nada:

1. ¿Qué sensores están en línea ahora?
2. ¿Cuántas horas trabajó la bomba esta semana?
3. ¿Qué día hubo más alarmas y de qué tipo?
4. ¿La temperatura del tanque tiene un patrón diario?

*Se evalúa:* que un encargado de mantenimiento pueda usarlo sin capacitación.

---

## 4. Evaluación integradora

**Consigna:** entregar el sistema funcionando y defenderlo oralmente.

| Dimensión | Peso | Qué se mira |
|---|---|---|
| Funcionamiento | 25 % | las 4 placas publicando, actuadores respondiendo, datos en la base |
| Documentación | 15 % | README propio, esquemas, tabla de tópicos actualizada |
| Seguridad eléctrica | 15 % | cableado, aislación, borneras, protecciones, prolijidad |
| Seguridad informática | 15 % | MODO B activo, ACL por dispositivo, nada de claves en el repositorio |
| Diagnóstico | 20 % | resuelve una falla sembrada en el momento, explicando el método |
| Defensa oral | 10 % | justifica las decisiones de diseño: por qué MQTT, por qué JSON, por qué el video por HTTP |

**Condición de aprobación:** no se aprueba con claves subidas al repositorio ni
con el cableado de 220 V hecho sobre protoboard. Son dos cosas que en el trabajo
real no se discuten.

---

## 5. Preguntas de defensa oral

Sirven para distinguir quien armó el sistema de quien lo copió.

1. ¿Por qué la cámara no manda el video por MQTT?
2. ¿Por qué el botón de la app no cambia de color al pulsarlo?
3. ¿Qué es un mensaje retenido y qué problema concreto resuelve acá?
4. ¿Por qué la fecha la pone PostgreSQL y no la ESP32?
5. ¿Qué pasa si se apaga el broker? ¿Y si se apaga Power BI?
6. ¿Por qué el relé se alimenta de 5 V y no de 3V3?
7. ¿Para qué sirve la ACL si ya hay usuario y contraseña?
8. Mostrame cómo le inyectarías una temperatura falsa al tablero en MODO A.
9. ¿Qué protege el `TIEMPO_MIN_REPOSO` y qué pasaría sin él?
10. ¿Por qué el pull-up del DS18B20 es de 4,7 kΩ y el del DHT11 de 10 kΩ?

---

## 6. Material por grupo

| Elemento | Cantidad | Observación |
|---|---|---|
| ESP32 DevKit v1 | 1 | |
| ESP8266 NodeMCU v3 | 1 | |
| ESP32-S3-WROOM | 1 | opcional |
| ESP32-CAM AI Thinker + FTDI | 1 | **fuente de 5 V / 1 A aparte** |
| DHT11 | 2 | |
| DS18B20 sumergible | 1 | |
| Módulo de relé de 1 canal | 3 | 5 V, verificar si es activo en bajo |
| Resistencias 10 kΩ y 4,7 kΩ | 2 de cada | |
| Capacitor 470 µF / 16 V | 1 | para la cámara |
| Protoboard y cables | — | cables cortos: los largos dan problemas |
| Fuente de 5 V / 2 A | 1 | |

**Software** (todo libre o gratuito): Arduino IDE, Mosquitto, MQTT Explorer,
Node-RED, PostgreSQL + pgAdmin, Power BI Desktop, MIT App Inventor (en línea).

---

## 7. Advertencias de seguridad para el taller

> **220 V.** El contacto del relé se cablea con borneras y cable de sección
> adecuada, **nunca** sobre protoboard. El circuito de potencia se arma con el
> tablero **desenergizado y consignado**, y se energiza recién con el docente
> presente. Para las prácticas de clase alcanza con una lámpara de 12 V o un
> LED indicador: el aprendizaje es el mismo y el riesgo es cero.

> **Cargas inductivas.** Motores, bombas y compresores devuelven un pico de
> tensión al cortar. Diodo volante en continua, varistor o red RC en alterna.
> Sin eso, el contacto del relé se pega y el equipo queda encendido sin que
> nadie lo haya ordenado.

> **Baterías y fuentes.** Nunca alimentar la placa por USB y por el pin de 5 V
> al mismo tiempo. Es la forma más rápida de quemar el regulador.

---

## 8. Ritmo sugerido

Para un cuatrimestre de 16 clases de 4 horas:

```
Clases  1-5    Bloque 1    +  TP 1
Clases  6-9    Bloque 2    +  TP 2
Clases 10-13   Bloque 3    +  TP 3  y  TP 4
Clases 14-16   Bloque 4    +  TP 5  y evaluacion integradora
```

Si el tiempo se acorta, lo que se recorta es la ESP32-S3 (clase 8 se hace con una
sola placa adicional) y Power BI (el tablero de Node-RED alcanza). **No** se
recorta la clase 16 ni el TP 4.

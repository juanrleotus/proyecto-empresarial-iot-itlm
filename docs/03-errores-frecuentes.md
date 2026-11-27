# Errores frecuentes — ordenado por síntoma

Todos los que aparecen acá se vieron realmente al armar este proyecto. Están con
la causa y la solución, no con una lista de cosas para probar al azar.

---

## A. El broker Mosquitto

### A1. El servicio arranca y se detiene solo

```
El servicio Mosquitto Broker en Equipo local se inició y se detuvo.
```

**Causa:** `log_dest` apunta a una carpeta donde el servicio no puede escribir,
típicamente `C:\Program Files\mosquitto\mosquitto.log`.

**Solución:** usar una carpeta escribible.

```
log_dest file C:/ProgramData/mosquitto/mosquitto.log
```

Y crear la carpeta: `mkdir C:\ProgramData\mosquitto`

---

### A2. `Solo se permite un uso de cada dirección de socket`

**Causa:** el archivo de configuración tiene **las dos** directivas:
`bind_address` (obsoleta) y `listener 1883`. Mosquitto intenta abrir el puerto
dos veces.

**Solución:** borrar o comentar `bind_address`. Dejar solo:

```
listener 1883 0.0.0.0
```

---

### A3. `netstat -an | findstr 1883` no devuelve nada

**Causa:** falta `listener`. Mosquitto 2.x, a diferencia de 1.x, **no escucha en
la red por defecto**: solo acepta conexiones desde `127.0.0.1`.

**Solución:** agregar `listener 1883 0.0.0.0` y reiniciar el servicio.

---

### A4. `Warning: Unused config option bind_address`

Es un aviso, no un error, pero conviene hacerle caso: `bind_address` está
obsoleta y se eliminará. Reemplazarla por `listener`.

---

### A5. `localhost` funciona pero las placas no conectan

**Causa:** el puerto 1883 está cerrado en el Firewall de Windows.

**Solución** (CMD como Administrador):

```cmd
netsh advfirewall firewall add rule name="MQTT 1883 ITLM" dir=in action=allow protocol=TCP localport=1883
```

---

## B. Las placas ESP32 / ESP8266

### B1. `[MQTT] ERROR rc=-2`

El más frecuente. **Causa:** la placa no llega al puerto 1883 del broker.

Revisar, en este orden:

1. **¿La IP es la correcta?** `ipconfig` en la PC. Si hay varios adaptadores,
   tomar el de la red **a la que están conectadas las placas**.
2. **¿La PC y las placas están en la misma red?** Este es el caso que costó una
   tarde en el instituto: notebook en la red del instituto por **cable**
   (`192.168.0.195`), placas en el punto de acceso del celular. Son dos redes
   distintas y no hay camino entre ellas. En `ipconfig` el adaptador Wi-Fi decía
   `Medios desconectados`. La solución es conectar la notebook **también** al
   punto de acceso y usar la IP del adaptador Wi-Fi.
3. **¿El Firewall está abierto?** Ver A5.
4. **¿Mosquitto está corriendo?** `net start mosquitto`

---

### B2. `[MQTT] ERROR rc=4` — usuario o contraseña incorrectos

**Causa:** modo B, y `MQTT_USUARIO` / `MQTT_CLAVE` en `credenciales.h` no
coinciden con lo que se creó con `mosquitto_passwd`.

**Solución:** volver a crear ese usuario (sin `-c`, que borraría el archivo):

```cmd
mosquitto_passwd passwd esp32planta
```

---

### B3. `[MQTT] ERROR rc=5` — no autorizado

**Causa:** el usuario existe pero la **ACL** no le permite ese tópico; o el
`aclfile` no se cargó (en Windows, suele haberse guardado como `aclfile.txt`).

**Solución:** revisar `aclfile` y el `mosquitto.log`, que dice exactamente qué
tópico se denegó.

---

### B4. `[MQTT] ERROR rc=2` — identificador de cliente rechazado

**Causa:** dos placas con el mismo `ID_CLIENTE`. El broker solo admite uno por
identificador: cada una expulsa a la otra, en bucle.

**Solución:** cambiar `ID_CLIENTE` en una de las dos. Pasa siempre en el aula
cuando varios alumnos cargan el mismo sketch.

---

### B5. `[ERROR] Lectura fallida del DHT11`

**Causas, en orden de frecuencia:**

1. Falta la **resistencia de pull-up de 10 kΩ** entre DATA y 3V3
2. El DHT11 está alimentado con 5 V en lugar de 3V3
3. Se lee más rápido que una vez por segundo (el DHT11 no da más)
4. Cables largos de protoboard (más de 20 cm): el flanco se deforma
5. El sensor está roto (pasa: son muy sensibles a la humedad extrema)

---

### B6. El DS18B20 devuelve `-127`

`-127` es `DEVICE_DISCONNECTED_C`: el bus OneWire no encuentra la sonda.

**Causa:** falta la resistencia de **4,7 kΩ** entre el pin de datos y 3V3, o es
de 10 kΩ. El bus OneWire necesita 4,7 kΩ.

**Comprobación:** en el `setup()`, `sensor.getDeviceCount()` tiene que devolver
`1`. Si devuelve `0`, es el pull-up.

---

### B7. La placa se reinicia justo al accionar el relé

**Causa:** el módulo de relé está alimentado desde el pin de **3V3** de la
placa. La bobina consume unos 70 mA y produce un pico al conmutar; la ESP se
queda sin tensión y hace reset.

**Solución:** alimentar el relé desde **5 V (Vin)**, con GND común. Si persiste,
usar una fuente aparte para el relé.

---

### B8. El relé da un golpe al encender la placa

**Causa:** se configuró `pinMode(PIN, OUTPUT)` antes de fijar el nivel; el pin
pasa por 0 V y, con un módulo activo en bajo, eso es "encender".

**Solución** (está en los tres sketches con relé):

```cpp
digitalWrite(PIN_RELE, RELE_OFF);   // primero el nivel
pinMode(PIN_RELE, OUTPUT);          // despues la direccion
digitalWrite(PIN_RELE, RELE_OFF);
```

---

### B9. El relé funciona al revés

**Causa:** el módulo es activo en **alto** y el código supone activo en bajo.

**Solución:** invertir dos líneas en el sketch.

```cpp
#define RELE_ON   HIGH
#define RELE_OFF  LOW
```

---

### B10. La ESP32-S3 no imprime nada en el monitor serie

**Causa:** la S3 usa USB nativo, no un conversor USB-serie.

**Solución:** en el IDE, **USB CDC On Boot: Enabled**, volver a compilar y
cargar.

---

## C. La ESP32-CAM

### C1. No aparece en el broker y `:81/stream` no responde

**Causa número uno: la alimentación.** La ESP32-CAM consume picos de ~500 mA. El
regulador de 3V3 de muchos adaptadores FTDI entrega 50 mA.

**Solución:**
- Alimentar por el pin **5 V** con una fuente de 5 V / 1 A
- Capacitor electrolítico de **470 µF** entre 5 V y GND
- Cable USB corto y de buena sección

**Causa dos:** el navegador. Algunas extensiones bloquean el MJPEG de un origen
HTTP sin cifrar. Probar en una ventana de incógnito o con otro navegador.

**Causa tres:** el `ctrl_port` de los dos servidores HTTP. Si los dos usan el
mismo, el segundo no arranca y el video nunca aparece. En el firmware de este
repositorio son 32768 y 32769.

---

### C2. `Camera init failed with error 0x20004`

**Causa:** el sensor no responde. Conector flex mal insertado o sucio, o
alimentación insuficiente.

**Solución:** desconectar, revisar que el flex entre derecho y que la traba
quede cerrada, volver a probar. Es un conector delicado.

---

### C3. La foto en la app es siempre la misma

**Causa:** el celular guarda la imagen en caché.

**Solución:** la URL tiene que cambiar en cada foto. El firmware ya agrega
`?t=millis()`:

```
http://192.168.1.50/capture?t=123456
```

---

### C4. No se puede cargar el programa en la ESP32-CAM

**Causa:** falta el puente **GPIO0 → GND**, o el FTDI está a 3,3 V en lugar de
5 V, o TX/RX no están cruzados.

**Secuencia correcta:** poner el puente → pulsar RESET → Subir → al terminar,
quitar el puente → RESET otra vez.

---

## D. Node-RED

### D1. El nodo MQTT dice `connecting` para siempre

**Causa:** la IP del broker no responde. Mismo diagnóstico que B1.

---

### D2. El nodo MQTT dice `disconnected`

**Causa:** modo B y faltan las credenciales en la pestaña **Security** del nodo
broker; o Mosquitto está detenido.

---

### D3. `relation "mediciones" does not exist`

**Causa:** el script SQL se ejecutó sobre la base `postgres` en lugar de
`itlm_iot`.

**Solución:** abrir el Query Tool **sobre `itlm_iot`** y volver a ejecutar
`01-crear-tablas.sql`.

---

### D4. `invalid input syntax for type double precision`

**Causa:** llegó texto donde la columna espera un número. Casi siempre es una
placa que quedó publicando **texto plano** (`24.8`) en lugar del JSON del
proyecto.

**Solución:** cargarle a esa placa el firmware de este repositorio. La función
de Node-RED ya avisa: `No es JSON en empresa/… -> 24.8`.

---

### D5. Solo llegan los datos de un sector

**Causa:** las demás placas no están publicando. **No es un problema de
Node-RED.**

**Diagnóstico:** abrir MQTT Explorer. Si el árbol `empresa/` solo tiene una
rama, el problema está en las placas: apagadas, sin llegar al broker
(ver `rc=` en el monitor serie), o publicando un tópico que **no coincide
exactamente** con la tabla de la función. MQTT distingue mayúsculas:
`empresa/Planta/temperatura` ≠ `empresa/planta/temperatura`.

---

### D6. La barra de depuración se llena de mensajes

**Causa:** la función tiene un `node.warn()` en el camino normal. Con 5 sensores
cada 5 segundos son 60 mensajes por minuto y lo importante se pierde.

**Solución:** usar `node.status()` para el estado normal y reservar `node.warn()`
para los problemas. Las funciones de este repositorio ya están así.

---

### D7. El nodo PostgreSQL aparece como *unknown*

**Causa:** está instalado otro paquete parecido
(`node-red-contrib-postgres`, `…-postgrestor`), que usa otros nombres de
propiedades.

**Solución:** instalar exactamente **`node-red-contrib-postgresql`**.

---

## E. App Inventor

### E1. `The operation lookup in pairs cannot accept the arguments: , ["v"], [24.8], ["0"]`

**Causa:** la app espera un **diccionario** y le llegó un número. Esa placa
publica texto plano, no JSON.

**Solución:** cargarle el firmware JSON. Como defensa adicional, envolver el
`lookup` en un `¿es una lista?`.

---

### E2. Importar un `.aia` generado da "El área de bloques no se ha cargado correctamente"

**Causa:** el archivo `.bky` (los bloques) se escribió a mano. El formato
Blockly de App Inventor es muy estricto y rechaza el archivo completo si algo no
cuadra; entonces solo carga las variables globales.

**Solución:** no generar `.bky` a mano. Los bloques se arman en la interfaz
siguiendo `app-inventor/02-bloques-detallados.md`.

---

### E3. El botón no cambia de color

**Causa:** el color se está cambiando al **pulsar** en lugar de al recibir la
confirmación.

**Solución:** ver el patrón comando/confirmación en `01-arquitectura.md` § 6. El
color se cambia en el evento `MessageReceived` del tópico `…/estado`, nunca en
`Click`.

---

### E4. El evento no se dispara

**Causa:** la extensión `UrsPahoMqttClient` usa el evento **`MessageReceived`**.
Las guías viejas mencionan `PublishedReceived`, de la extensión anterior
(`UrsAI2Mqtt`).

---

### E5. `Publish` no acepta cuatro argumentos

En `UrsPahoMqttClient`:

| Bloque | Argumentos |
|---|---|
| `Publish` | `Topic`, `Message` |
| `PublishEx` | `Topic`, `Message`, `QoS`, `Retain` |

Para publicar un comando alcanza `Publish`. Para publicar con retención hay que
usar `PublishEx`.

---

### E6. La app no conecta desde datos móviles

**Causa:** el broker tiene una IP privada (`192.168.x.x`), que no existe en
Internet.

**Solución:** el celular tiene que estar en la **misma Wi-Fi**. Para acceso
desde afuera hace falta un broker en Internet con TLS, que es otro proyecto.

---

## F. Power BI

### F1. "Esta conexión requiere uno o más componentes adicionales"

**Causa:** falta el proveedor **Npgsql**, o se instaló sin la opción
**Npgsql GAC Installation**, que viene desactivada.

**Solución:** reinstalar Npgsql activando esa opción y reiniciar Power BI.

---

### F2. Los gráficos están vacíos

**Causa:** la tabla no tiene filas. Power BI no es el problema.

**Solución:** `SELECT COUNT(*) FROM mediciones;`. Si da 0, el problema está en
Node-RED.

---

### F3. El eje de tiempo agrupa por año

**Causa:** Power BI tomó la fecha como jerarquía automática.

**Solución:** en el visual, hacer clic en la flecha del campo de fecha y elegir
el campo **sin jerarquía**.

---

### F4. Las fechas están desfasadas varias horas

**Causa:** la columna es `TIMESTAMP` sin zona horaria y la medida usa `UTCNOW()`.

**Solución:** usar `NOW()` en la medida, o guardar la fecha como `TIMESTAMPTZ`
en PostgreSQL. La segunda es la correcta; la primera es la rápida.

---

## G. Android Studio

### G1. `Incompatible Gradle JVM version` / `Gradle 8.7 supports Java versions between 1.8 and 21`

**Causa:** el JDK instalado es más nuevo que el que soporta ese Gradle.

**Solución A** (la recomendada): instalar un **JDK 17** y seleccionarlo en
*Settings → Build, Execution, Deployment → Build Tools → Gradle → Gradle JDK*.

**Solución B** (cuando la red del instituto bloquea la descarga del JDK y
aparece *"No JDK packages are available for download"*): subir la versión de
Gradle. En `gradle/wrapper/gradle-wrapper.properties`:

```
distributionUrl=https\://services.gradle.org/distributions/gradle-8.14-bin.zip
```

Después, *File → Sync Project with Gradle Files*.

---

### G2. La app compila pero no conecta al broker

**Causa:** Android 9 y posteriores bloquean el tráfico HTTP y TCP sin cifrar.

**Solución:** `network_security_config.xml` con `cleartextTrafficPermitted="true"`
para la red local, referenciado desde el `AndroidManifest.xml`. Ya viene en el
proyecto de `app-android/`.

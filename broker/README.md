# Broker MQTT — Mosquitto

El broker es el **cartero** del sistema: nadie habla directamente con nadie.
Cada placa *publica* en un tópico, y quien quiera ese dato se *suscribe*.

```
   ESP32 planta ──┐                    ┌── App del celular
   ESP8266 tanque ├──> [ MOSQUITTO ] <─┤
   ESP32-S3 ──────┤      :1883         └── Node-RED ─> PostgreSQL ─> Power BI
   ESP32-CAM ─────┘                        MQTT Explorer (diagnóstico)
```

Este proyecto se entrega en **dos modos**, que se eligen cambiando un único
archivo de configuración:

| Modo | Archivo | Para qué sirve |
|---|---|---|
| **A — sin autenticación** | `mosquitto-sin-auth.conf` | Primera clase, laboratorio aislado. Cualquiera se conecta. |
| **B — con usuario y contraseña** | `mosquitto-con-auth.conf` | Modo recomendado. Cada placa tiene su propio usuario y permisos. |

> **Importante didáctico:** el modo A no es "el modo fácil", es **el modo
> insegura**. Conviene arrancar en A para que los alumnos vean el sistema
> funcionando, y pasar a B como clase específica de seguridad.

---

## 1. Instalación de Mosquitto

### Windows

1. Descargar el instalador desde <https://mosquitto.org/download/> (versión x64).
2. Instalar aceptando la ruta por defecto: `C:\Program Files\mosquitto\`.
3. Marcar la opción **Service** durante la instalación (instala el servicio de Windows).
4. Crear la carpeta de datos y registro:

```cmd
mkdir C:\ProgramData\mosquitto
```

> Esta carpeta es obligatoria. El servicio de Windows **no puede escribir**
> dentro de `C:\Program Files\`, así que si el `log_dest` apunta ahí el
> servicio arranca y se detiene de inmediato.

### Linux (Debian / Ubuntu / Raspberry Pi OS)

```bash
sudo apt update
sudo apt install mosquitto mosquitto-clients
sudo systemctl enable mosquitto
```

---

## 2. Abrir el puerto 1883 en el Firewall de Windows

Sin esta regla las placas ven el puerto cerrado aunque Mosquitto esté corriendo.

**Opción rápida (CMD como Administrador):**

```cmd
netsh advfirewall firewall add rule name="MQTT 1883 ITLM" dir=in action=allow protocol=TCP localport=1883
```

**Opción gráfica:** Panel de control → Firewall de Windows Defender →
Configuración avanzada → Reglas de entrada → Nueva regla → Puerto → TCP →
Puertos locales específicos: `1883` → Permitir la conexión → marcar los tres
perfiles → Nombre: `MQTT 1883 ITLM`.

Comprobación:

```cmd
netstat -an | findstr 1883
```

Debe aparecer:

```
TCP    0.0.0.0:1883      0.0.0.0:0      LISTENING
```

Si **no aparece nada**, el servicio no está corriendo o falta `listener 1883`
en el archivo de configuración.

---

## 3. MODO A — Broker sin autenticación

```cmd
copy "mosquitto-sin-auth.conf" "C:\Program Files\mosquitto\mosquitto.conf"
net stop mosquitto
net start mosquitto
```

En Linux:

```bash
sudo cp mosquitto-sin-auth.conf /etc/mosquitto/conf.d/itlm.conf
sudo systemctl restart mosquitto
```

### Prueba de funcionamiento (dos ventanas de CMD)

Ventana 1 — se suscribe a todo:

```cmd
cd "C:\Program Files\mosquitto"
mosquitto_sub -h localhost -t "empresa/#" -v
```

Ventana 2 — publica una prueba:

```cmd
cd "C:\Program Files\mosquitto"
mosquitto_pub -h localhost -t "empresa/prueba" -m "hola ITLM"
```

En la ventana 1 debe aparecer `empresa/prueba hola ITLM`.

En el firmware, este modo corresponde a dejar **vacías** las credenciales MQTT
en `credenciales.h`:

```cpp
#define MQTT_USUARIO  ""
#define MQTT_CLAVE    ""
```

---

## 4. MODO B — Broker con usuario y contraseña

### 4.1 Crear el archivo de contraseñas

> Las contraseñas **nunca** se escriben en un archivo de configuración ni en el
> código. `mosquitto_passwd` guarda únicamente un *hash*.

Abrir CMD **como Administrador**:

```cmd
cd "C:\Program Files\mosquitto"

:: El parametro -c CREA el archivo (borra lo anterior). Solo la primera vez.
mosquitto_passwd -c passwd esp32planta

:: A partir de aqui, SIN -c, para ir agregando usuarios.
mosquitto_passwd passwd esp8266tanque
mosquitto_passwd passwd esp32s3oficina
mosquitto_passwd passwd esp32cam
mosquitto_passwd passwd nodered
mosquitto_passwd passwd appmovil
mosquitto_passwd passwd docente
```

Cada comando pide la contraseña dos veces y no muestra nada al escribir: es
normal.

En Linux es idéntico, cambiando la ruta:

```bash
sudo mosquitto_passwd -c /etc/mosquitto/passwd esp32planta
sudo mosquitto_passwd    /etc/mosquitto/passwd esp8266tanque
# ...
```

### 4.2 Instalar la ACL

```cmd
copy "aclfile.ejemplo" "C:\Program Files\mosquitto\aclfile"
```

El archivo **no lleva extensión**. Si Windows lo guarda como `aclfile.txt`,
Mosquitto no lo encuentra y rechaza todas las conexiones.

### 4.3 Instalar la configuración y reiniciar

```cmd
copy "mosquitto-con-auth.conf" "C:\Program Files\mosquitto\mosquitto.conf"
net stop mosquitto
net start mosquitto
```

### 4.4 Prueba

Sin credenciales debe **fallar**:

```cmd
mosquitto_sub -h localhost -t "empresa/#" -v
```

→ `Connection error: Connection Refused: not authorised.`

Con credenciales debe **funcionar**:

```cmd
mosquitto_sub -h localhost -t "empresa/#" -v -u docente -P LA_CLAVE
```

Y la ACL se comprueba así (la app no debe poder escribir en un tópico de sensor):

```cmd
mosquitto_pub -h localhost -t "empresa/planta/temperatura" -m "99" -u appmovil -P LA_CLAVE
```

→ el mensaje se descarta y en `mosquitto.log` aparece una línea de ACL denegada.

En el firmware, este modo corresponde a completar las credenciales:

```cpp
#define MQTT_USUARIO  "esp32planta"
#define MQTT_CLAVE    "la-clave-que-definiste"
```

---

## 5. Tabla de usuarios sugerida

| Usuario | Lo usa | Permisos |
|---|---|---|
| `esp32planta` | ESP32 DevKit (Planta) | escribe temp/hum/aire-estado; lee aire/cmd |
| `esp8266tanque` | ESP8266 NodeMCU (Tanque) | escribe temp/rele-estado; lee rele/cmd |
| `esp32s3oficina` | ESP32-S3-WROOM (Oficina) | escribe temp/hum/luces-estado; lee luces/cmd |
| `esp32cam` | ESP32-CAM AI Thinker | escribe estado/evento; lee camara/cmd |
| `nodered` | Node-RED | lee `empresa/#` |
| `appmovil` | App Inventor / Android | lee sensores y estados; escribe `/cmd` |
| `docente` | MQTT Explorer | lectura y escritura total |

---

## 6. Errores frecuentes

| Síntoma | Causa | Solución |
|---|---|---|
| El servicio arranca y se detiene solo | `log_dest` apunta a `C:\Program Files\...` (no escribible) | usar `C:/ProgramData/mosquitto/mosquitto.log` |
| `Solo se permite un uso de cada dirección de socket` | están presentes `bind_address` **y** `listener 1883` | borrar o comentar `bind_address` |
| `netstat` no muestra 1883 | falta `listener 1883` (Mosquitto 2.x no escucha en red por defecto) | agregar `listener 1883 0.0.0.0` |
| Las placas no conectan pero `localhost` sí | falta la regla de Firewall | abrir TCP 1883 |
| `Connection Refused: not authorised` | modo B sin usuario/clave, o mal escritos | completar `credenciales.h` |
| El broker acepta la conexión pero no llegan mensajes | la ACL no autoriza ese tópico | revisar `aclfile` y el log |
| `rc=-2` en el monitor serie de la ESP | IP del broker incorrecta o PC en otra red | verificar con `ipconfig` la IP **de la placa de red conectada a la misma Wi-Fi** |

> **Nota sobre dos redes:** si la notebook está en la red del instituto por
> cable y las placas en un punto de acceso del celular, no hay camino entre
> ellas. La notebook debe estar **en la misma red Wi-Fi** que las placas, y en
> el firmware se usa la IP del **adaptador Wi-Fi**, no la del Ethernet.

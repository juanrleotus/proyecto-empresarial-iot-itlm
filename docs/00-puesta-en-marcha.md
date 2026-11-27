# Puesta en marcha — de cero a funcionando

Orden obligatorio. Cada paso se **comprueba** antes de pasar al siguiente: así,
cuando algo no funciona, se sabe exactamente dónde está.

> Tiempo estimado la primera vez: 2 a 3 horas. Después, 20 minutos.

---

## Lista de verificación

```
[ ]  1. Mosquitto instalado y escuchando en 1883
[ ]  2. Puerto 1883 abierto en el Firewall
[ ]  3. mosquitto_sub / mosquitto_pub hablan entre si
[ ]  4. MQTT Explorer conectado al broker
[ ]  5. credenciales.h creado en cada carpeta de firmware
[ ]  6. Una placa publicando: se ve en MQTT Explorer
[ ]  7. Las cuatro placas publicando
[ ]  8. Comandos: los reles responden desde MQTT Explorer
[ ]  9. PostgreSQL instalado, base itlm_iot creada
[ ] 10. Tablas creadas con 01-crear-tablas.sql
[ ] 11. Node-RED instalado con node-red-contrib-postgresql
[ ] 12. Flujo importado y conectado (dice "connected")
[ ] 13. Filas apareciendo en la tabla mediciones
[ ] 14. App del celular leyendo y accionando
[ ] 15. Power BI conectado y mostrando el tablero
[ ] 16. (opcional) Pasar el broker al MODO B con autenticacion
```

---

## Paso 1 a 4 — El broker

Seguir `broker/README.md`, secciones 1 a 3.

**Comprobación** — dos ventanas de CMD:

```cmd
cd "C:\Program Files\mosquitto"
mosquitto_sub -h localhost -t "empresa/#" -v
```

```cmd
cd "C:\Program Files\mosquitto"
mosquitto_pub -h localhost -t "empresa/prueba" -m "hola"
```

Si en la primera ventana aparece `empresa/prueba hola`, el broker funciona.
**No seguir hasta que esto funcione.**

Después, conectar **MQTT Explorer** (<https://mqtt-explorer.com/>):

| Campo | Valor |
|---|---|
| Name | `ITLM IoT` |
| Protocol | `mqtt://` |
| Host | IP de la PC (no `localhost` si se prueba desde otra máquina) |
| Port | `1883` |
| Username / Password | vacíos en MODO A |

MQTT Explorer es **el instrumento de medición** de este proyecto. Lo que no se
ve ahí, no está pasando.

---

## Paso 5 a 7 — Las placas

1. Instalar las bibliotecas y los soportes de placa (`firmware/README.md`, § 3 y 4)
2. En cada carpeta de firmware: copiar `credenciales.ejemplo.h` → `credenciales.h` y completarlo
3. Averiguar la IP de la PC: `ipconfig` → tomar la del adaptador **conectado a la misma Wi-Fi que las placas**
4. Cargar una sola placa y abrir el **Monitor Serie a 115200 baudios**

**Comprobación** — en el monitor serie debe aparecer:

```
[WiFi] Conectado a MiRed
[WiFi] IP de la placa: 192.168.1.41
[MQTT] Conectando a 192.168.1.33:1883  (modo anonimo)
[MQTT] Conectado
[MQTT -> ] empresa/tanque/temperatura {"v":27.7,"u":"C","ts":5123}
```

Y en MQTT Explorer tiene que aparecer el árbol `empresa/` con ese valor.

Si dice `[MQTT] ERROR rc=-2`, el problema es de red: IP mal, firewall cerrado,
o la PC en otra red. Ver la tabla de códigos en `firmware/README.md` § 8.

Recién cuando una placa funciona, cargar la segunda. **De una en una.**

---

## Paso 8 — Los actuadores

Desde MQTT Explorer, publicar:

| Tópico | Mensaje |
|---|---|
| `empresa/planta/aire/cmd` | `ON` |
| `empresa/tanque/rele/cmd` | `ON` |
| `empresa/oficina/luces/cmd` | `ON` |
| `empresa/camara/cmd` | `FOTO` |

**Comprobación**: se escucha el clic del relé **y** aparece el tópico
`…/estado` con `{"v":"ON",…}`. Las dos cosas: el clic solo no alcanza, porque
sin el estado la app no puede pintar el botón.

> El aire acondicionado puede contestar `ESPERA` en lugar de `ON`: es la
> protección de arranque corto del compresor. Está bien. Esperar 3 minutos.

---

## Paso 9 y 10 — La base de datos

Seguir `base-de-datos/README.md`, secciones 1 a 3.

**Comprobación** — en el Query Tool de `itlm_iot`:

```sql
INSERT INTO mediciones (dispositivo, variable, valor, unidad)
VALUES ('Planta', 'temperatura', 25.8, 'C');

SELECT * FROM mediciones;
```

Debe aparecer la fila de prueba. Borrarla después:
`DELETE FROM mediciones WHERE valor = 25.8;`

---

## Paso 11 a 13 — Node-RED

Seguir `node-red/README.md`, secciones 1 a 3.

**Comprobación 1**: debajo del nodo `MQTT empresa/#` dice **connected** en verde.

**Comprobación 2**: debajo de `Clasificar y guardar` aparece un texto verde que
cambia con cada medición, por ejemplo `Tanque temperatura = 27.7C`.

**Comprobación 3**: en pgAdmin,

```sql
SELECT * FROM mediciones ORDER BY id DESC LIMIT 10;
```

Las filas tienen que estar creciendo. Ejecutar la consulta dos veces con unos
segundos de diferencia: el `id` más alto debe haber aumentado.

---

## Paso 14 — La app del celular

Seguir `app-inventor/01-guia-paso-a-paso.md`.

El celular tiene que estar **en la misma Wi-Fi**. Datos móviles no sirven: el
broker tiene una IP privada que no existe en Internet.

**Comprobación**: en MQTT Explorer, en el nodo raíz, se ve aumentar el número
de clientes conectados cuando la app se conecta. Y al pulsar un botón de la app
aparece el mensaje en `…/cmd`.

---

## Paso 15 — Power BI

Seguir `power-bi/README.md`.

**Comprobación**: la tarjeta `Estado del sistema` dice `NORMAL`.

---

## Paso 16 — Pasar al MODO B (con autenticación)

Esto se hace **al final**, cuando todo funciona en MODO A. Así, si algo deja de
andar, se sabe que la causa es la autenticación y no otra cosa.

1. `broker/README.md` § 4: crear `passwd` con `mosquitto_passwd` e instalar el `aclfile`
2. Cambiar la configuración a `mosquitto-con-auth.conf` y reiniciar el servicio
3. En cada `credenciales.h`, completar `MQTT_USUARIO` y `MQTT_CLAVE`, y recargar las cuatro placas
4. En Node-RED, pestaña **Security** del broker: usuario `nodered` y su clave → **Deploy**
5. En la app, completar usuario y contraseña en el componente MQTT
6. En MQTT Explorer, usuario `docente`

**Comprobación de que la seguridad realmente está puesta**: intentar conectarse
**sin** credenciales. Tiene que fallar.

```cmd
mosquitto_sub -h localhost -t "empresa/#" -v
```

→ `Connection error: Connection Refused: not authorised.`

Si eso **no** falla, la configuración con autenticación no se cargó: el servicio
sigue usando el archivo anterior.

---

## Si algo no funciona

Ir a `03-errores-frecuentes.md`. Está ordenado por síntoma.

La regla general: **diagnosticar de afuera hacia adentro**. Primero el broker,
después las placas, después Node-RED, después la base, al final el tablero. Casi
siempre el problema está en la capa anterior a donde se nota.

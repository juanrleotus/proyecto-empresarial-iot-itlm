# Node-RED — el traductor del sistema

Node-RED es la pieza que está **en el medio**: escucha el broker, entiende los
tópicos, valida los datos y los escribe en PostgreSQL.

```
MQTT (empresa/#) ──> [ Clasificar y guardar ] ──> PostgreSQL (mediciones / eventos)
                 └─> [ Detectar alarmas    ] ──> PostgreSQL (alarmas)
```

> **Por qué no publica la ESP32 directo a PostgreSQL.** Podría, con una
> biblioteca y una conexión TCP más. Pero entonces cada placa necesitaría la
> contraseña de la base de datos, y cambiar el nombre de una columna obligaría a
> reprogramar las cuatro placas. Con Node-RED en el medio, las placas solo saben
> publicar un JSON y el conocimiento de la base queda en un único lugar.

---

## 1. Instalación

Requiere Node.js 18 o superior.

```cmd
npm install -g --unsafe-perm node-red
node-red
```

Queda disponible en <http://localhost:1880>.

Para que arranque con Windows, lo más simple es un acceso directo a `node-red`
en la carpeta `shell:startup`.

---

## 2. Instalar el nodo de PostgreSQL

Menú ☰ → **Manage palette** → pestaña **Install** → buscar e instalar:

```
node-red-contrib-postgresql
```

> **Ojo con el nombre.** Hay varios paquetes parecidos
> (`node-red-contrib-postgres`, `…-postgrestor`, `…-postgres-variable`) y **no**
> usan los mismos nombres de propiedades. Los flujos de este repositorio están
> hechos para `node-red-contrib-postgresql`; con otro paquete el nodo aparece
> como *unknown*.

Si antes se usó SQLite (`node-red-node-sqlite`), conviene **dejarlo instalado**
hasta comprobar que PostgreSQL guarda bien, y solo entonces desactivar ese
flujo.

---

## 3. Importar el flujo

| Archivo | Cuándo usarlo |
|---|---|
| `flows-sin-auth.json` | broker en **MODO A** (sin usuario) |
| `flows-con-auth.json` | broker en **MODO B** (con usuario y contraseña) |

Menú ☰ → **Import** → **select a file to import** → elegir el archivo →
**Import**.

### Lo que hay que completar a mano después de importar

Las contraseñas **nunca** se exportan en un archivo de flujos. Esto es una
medida de seguridad de Node-RED, no un olvido: hay que escribirlas.

**a) Broker MQTT** — doble clic en el nodo `MQTT empresa/#` → lápiz junto a
*Server*:

| Pestaña | Campo | Valor |
|---|---|---|
| Connection | Server | IP real de la PC con Mosquitto |
| Connection | Port | 1883 |
| Security | Username | `nodered` *(solo MODO B)* |
| Security | Password | la clave de `mosquitto_passwd` *(solo MODO B)* |

En MODO A la pestaña **Security** queda vacía.

**b) PostgreSQL** — doble clic en cualquier nodo `PostgreSQL` → lápiz junto a
*Config*:

| Campo | Valor |
|---|---|
| Host | `127.0.0.1` |
| Port | `5432` |
| Database | `itlm_iot` |
| User | `nodered` (o `postgres`) |
| Password | la contraseña de ese usuario |

Los tres nodos PostgreSQL comparten la misma configuración: se completa una vez.

Luego **Deploy**.

---

## 4. Qué trae el flujo

| Nodo | Función |
|---|---|
| `MQTT empresa/#` | se suscribe a todos los tópicos del proyecto |
| `Ver lo que llega del broker` | debug desactivado; activarlo para diagnosticar |
| `Clasificar y guardar` | identifica el tópico y arma el `INSERT` |
| `Guardar en PostgreSQL` | ejecuta el `INSERT` en `mediciones` o `eventos` |
| `Detectar alarmas` | compara contra los límites y registra los cambios de estado |
| `Guardar alarma` | ejecuta el `INSERT` en `alarmas` |
| `Probar la base` → `SELECT de prueba` | consulta `v_ultimos_valores` sin esperar a las placas |
| Botones `Aire ON`, `Bomba OFF`, `Camara FOTO`… | publican comandos para probar los actuadores **sin la app del celular** |

Esos botones son la herramienta de diagnóstico más útil del proyecto: si el
relé responde al botón de Node-RED pero no a la app, el problema está en la app,
no en la placa.

---

## 5. El código de las funciones

Los dos archivos `.js` de esta carpeta son el contenido de la pestaña
**On Message** de cada nodo `function`. Están sueltos para poder leerlos,
corregirlos y versionarlos con git; el flujo ya los trae incorporados.

### `funcion-clasificar-y-guardar.js`

Decide, según el tópico, qué tabla corresponde:

| Tópico | Tabla | Observación |
|---|---|---|
| `…/temperatura`, `…/humedad` | `mediciones` | valida que el valor sea un número finito |
| `…/aire/estado`, `…/rele/estado`, `…/luces/estado`, `camara/estado` | `eventos` | acepta `ON`, `OFF` y `ESPERA` |
| `camara/evento` | `eventos` | guarda la URL de la foto en `detalle` |
| `…/cmd` | — | **se descarta**: es una orden, no un dato medido |
| cualquier otro | — | se avisa con `node.warn` y no se guarda |

Tres validaciones que evitan basura en la base:

```js
if (!Number.isFinite(valor)) { ... return null; }     // no es un numero
if (valor < -50 || valor > 150) { ... return null; }  // fuera de escala fisica
if (topico.endsWith("/cmd")) return null;             // es un comando
```

La segunda atrapa el `-127` del DS18B20, que es su manera de decir "no estoy
conectado". Sin ese filtro, el gráfico de Power BI aparece con un pico hacia
abajo imposible de interpretar.

> **Nota sobre `node.warn`.** La versión original avisaba en cada inserción
> correcta. Con 5 sensores a 5 segundos eso son 60 mensajes por minuto en la
> barra de depuración y deja de verse lo importante. Acá el estado normal se
> muestra con `node.status()` debajo del nodo, y `node.warn` queda reservado
> para los problemas reales.

### `funcion-detectar-alarmas.js`

Límites configurables al principio del archivo:

```js
const LIMITES = {
  "empresa/planta/temperatura": { dispositivo:"Planta", variable:"temperatura", min:5, max:35 },
  ...
};
```

Registra **solo el cambio de estado**: la entrada en alarma y la normalización.
Si guardara cada medición fuera de rango, una tarde de calor generaría miles de
filas idénticas. El estado anterior se recuerda con `flow.get` / `flow.set`.

---

## 6. Diagnóstico: "no veo los datos"

Orden de revisión, de afuera hacia adentro.

**Paso 1 — ¿el nodo MQTT está conectado?** Debajo del nodo `MQTT empresa/#`
debe decir **connected** en verde.

| Dice | Significa |
|---|---|
| `connected` | bien, seguir al paso 2 |
| `connecting` | la IP no responde: IP mal, firewall cerrado, otra red |
| `disconnected` | credenciales rechazadas (MODO B) o broker detenido |

**Paso 2 — ¿llegan mensajes?** Activar el debug `Ver lo que llega del broker`
(clic en el cuadradito a su derecha) y mirar la barra lateral de depuración.
Si no aparece nada, el problema está en las placas o en el broker, no en
Node-RED. Comprobarlo con MQTT Explorer.

**Paso 3 — ¿la función los acepta?** Mirar el texto debajo de
`Clasificar y guardar`:

| Estado | Significa |
|---|---|
| verde `Tanque temperatura = 27.7C` | correcto |
| gris `topico sin uso` | el tópico no está en la tabla de la función |
| avisos `No es JSON en …` | esa placa publica texto plano: cargarle el firmware de este repo |

**Paso 4 — ¿PostgreSQL acepta el INSERT?** Si el nodo `Guardar en PostgreSQL`
se pone en rojo, el texto del error lo dice todo:

| Error | Causa |
|---|---|
| `password authentication failed` | usuario o clave de la base |
| `relation "mediciones" does not exist` | falta ejecutar `01-crear-tablas.sql`, o está en otra base |
| `connection refused` | servicio PostgreSQL detenido |
| `invalid input syntax for type double precision` | llegó texto donde se espera número |

**Paso 5 — ¿la fila está en la base?**

```sql
SELECT * FROM mediciones ORDER BY id DESC LIMIT 10;
```

> **El caso "solo veo el Tanque".** Si una sola placa aparece en la base, no es
> un problema de Node-RED: las otras no están publicando. Suele ser que están
> apagadas, que no llegaron al broker (ver `rc=` en su monitor serie), o que
> publican un tópico que no coincide **exactamente** con la tabla de la función.
> `empresa/Planta/temperatura` y `empresa/planta/temperatura` son tópicos
> distintos: MQTT distingue mayúsculas.

---

## 7. Tablero opcional (node-red-dashboard)

Para mostrar el sistema sin Power BI, instalar `node-red-dashboard` y agregar
nodos `gauge` y `chart` a la salida del `mqtt in`. Queda en
<http://localhost:1880/ui>.

No está incluido en los flujos de este repositorio para que el archivo pueda
importarse sin instalar nada más que el nodo de PostgreSQL.

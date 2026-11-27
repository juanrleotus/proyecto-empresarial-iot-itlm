# Base de datos — PostgreSQL

MQTT **no guarda historia**. El broker entrega el mensaje y lo olvida (salvo el
último valor de los tópicos *retained*). Si se quiere saber qué temperatura
tenía el tanque el martes a las 3 de la tarde, hace falta una base de datos.

```
MQTT ──> Node-RED ──> PostgreSQL ──> Power BI
         (traduce)     (guarda)      (muestra)
```

## Por qué PostgreSQL y no SQLite

El proyecto arrancó con SQLite (`C:\IoT\sensores.db`) y funciona, pero:

| | SQLite | PostgreSQL |
|---|---|---|
| Instalación | ninguna, es un archivo | servidor |
| Acceso simultáneo | **un escritor a la vez** | muchos |
| Power BI | sin conector nativo | conector nativo |
| Acceso desde otra PC | no | sí, por red |
| Usuarios y permisos | no tiene | sí |

El problema concreto: con SQLite, si Node-RED está escribiendo y Power BI quiere
leer, uno de los dos recibe `database is locked`. En el aula, con varios alumnos
consultando a la vez, eso pasa todo el tiempo.

> SQLite no es "peor": es la herramienta correcta para un solo proceso. Conviene
> conservar `sensores.db` como respaldo hasta que PostgreSQL esté comprobado.

---

## 1. Instalación

1. Descargar PostgreSQL de <https://www.postgresql.org/download/windows/>
2. Durante la instalación anotar la **contraseña del usuario `postgres`**
3. Dejar el puerto **5432**
4. Instalar también **pgAdmin 4** (viene en el mismo instalador)

---

## 2. Crear la base

En pgAdmin 4: clic derecho en **Databases → Create → Database…**

| Campo | Valor |
|---|---|
| Database | `itlm_iot` |
| Owner | `postgres` |
| Encoding | `UTF8` |

O por SQL, desde el Query Tool de la base `postgres`:

```sql
CREATE DATABASE itlm_iot WITH ENCODING 'UTF8' TEMPLATE template0;
```

---

## 3. Crear las tablas

Abrir el Query Tool **sobre `itlm_iot`** (no sobre `postgres`) y ejecutar
`01-crear-tablas.sql` completo.

Crea:

| Objeto | Tipo | Para qué |
|---|---|---|
| `mediciones` | tabla | lecturas numéricas de los sensores |
| `eventos` | tabla | cambios de estado de actuadores y avisos de cámara |
| `alarmas` | tabla | mediciones fuera de rango |
| `v_ultimos_valores` | vista | último valor de cada sensor |
| `v_promedio_horario` | vista | promedio/mín/máx por hora |
| `v_horas_marcha` | vista | horas de marcha de cada actuador por día |

### Estructura de `mediciones`

| Campo | Tipo | Descripción |
|---|---|---|
| `id` | BIGINT | identificador automático |
| `fecha` | TIMESTAMP | momento de la inserción (lo pone PostgreSQL) |
| `dispositivo` | VARCHAR(50) | Planta, Oficina, Almacen, Tanque |
| `variable` | VARCHAR(50) | temperatura, humedad |
| `valor` | DOUBLE PRECISION | la medición |
| `unidad` | VARCHAR(10) | C, % |

> **Dos decisiones de diseño que vale explicar en clase.**
>
> *La fecha la pone la base, no la placa.* El campo `ts` que envía la ESP32 son
> milisegundos desde que la placa arrancó: no sirve como fecha real, porque la
> placa no tiene reloj ni batería. PostgreSQL sella el momento de la inserción,
> que a 5 segundos de diferencia es exacto de sobra.
>
> *Una fila por medición, no una columna por sensor.* Así, agregar un sensor
> nuevo no exige modificar la tabla: es una fila más con otro `dispositivo`.
> Este formato se llama *largo* o *EAV* y es el que Power BI prefiere.

---

## 4. Índices

`01-crear-tablas.sql` los crea. Sin ellos todo funciona… hasta que la tabla
tiene medio millón de filas y el tablero tarda veinte segundos en abrir.

```sql
CREATE INDEX idx_mediciones_fecha            ON mediciones (fecha DESC);
CREATE INDEX idx_mediciones_disp_var_fecha   ON mediciones (dispositivo, variable, fecha DESC);
```

Una consulta como "temperatura del tanque de la última hora" filtra por
`dispositivo`, `variable` y `fecha`: el segundo índice cubre las tres y la
consulta no recorre la tabla entera.

---

## 5. Usuario propio para Node-RED (recomendado)

Node-RED no necesita ser administrador de la base. Solo inserta.

```sql
CREATE USER nodered WITH PASSWORD 'cambiar-esta-clave';

GRANT CONNECT ON DATABASE itlm_iot TO nodered;
GRANT USAGE   ON SCHEMA public     TO nodered;

GRANT INSERT, SELECT ON mediciones, eventos, alarmas TO nodered;
GRANT USAGE ON ALL SEQUENCES IN SCHEMA public TO nodered;
```

Y un usuario de **solo lectura** para Power BI:

```sql
CREATE USER powerbi WITH PASSWORD 'cambiar-esta-clave';
GRANT CONNECT ON DATABASE itlm_iot TO powerbi;
GRANT USAGE   ON SCHEMA public     TO powerbi;
GRANT SELECT  ON ALL TABLES IN SCHEMA public TO powerbi;
```

> Mismo criterio que la ACL del broker: cada pieza del sistema recibe el
> **mínimo permiso** que necesita. Si mañana el tablero queda abierto en una PC
> del taller, con el usuario `powerbi` nadie puede borrar la historia.

---

## 6. Acceso desde otra PC

Por defecto PostgreSQL solo escucha en `localhost`. Para que Power BI o Node-RED
entren desde otra máquina:

1. En `postgresql.conf`: `listen_addresses = '*'`
2. En `pg_hba.conf`, agregar la red del aula:
   ```
   host    itlm_iot    nodered    192.168.1.0/24    scram-sha-256
   ```
3. Abrir el puerto 5432 en el Firewall
4. Reiniciar el servicio `postgresql-x64-16`

---

## 7. Consultas de diagnóstico

`02-consultas-utiles.sql` trae 14 consultas listas. Las dos primeras son las que
se usan todos los días:

**¿Está entrando algo?**

```sql
SELECT * FROM mediciones ORDER BY id DESC LIMIT 20;
```

**¿Qué placa se cayó?**

```sql
SELECT dispositivo, variable, fecha,
       ROUND(EXTRACT(EPOCH FROM (CURRENT_TIMESTAMP - fecha))::numeric, 0) AS segundos_sin_datos
FROM   v_ultimos_valores
ORDER  BY segundos_sin_datos DESC;
```

Si una fila muestra 400 segundos y las demás 4, esa placa dejó de publicar.

---

## 8. Mantenimiento

A 5 segundos por muestra y 5 sensores son unas **86.000 filas por día**. No es
un problema inmediato, pero conviene enseñar la rutina:

```sql
DELETE FROM mediciones WHERE fecha < CURRENT_DATE - INTERVAL '30 days';
VACUUM ANALYZE mediciones;
```

Para no perder la historia, el resumen por hora se puede volcar a una tabla
antes de borrar el detalle. Es un buen trabajo práctico.

Copia de seguridad:

```cmd
"C:\Program Files\PostgreSQL\16\bin\pg_dump" -U postgres -d itlm_iot -F c -f C:\IoT\itlm_iot.backup
```

Restauración:

```cmd
"C:\Program Files\PostgreSQL\16\bin\pg_restore" -U postgres -d itlm_iot -c C:\IoT\itlm_iot.backup
```

---

## 9. Errores frecuentes

| Síntoma | Causa | Solución |
|---|---|---|
| `relation "mediciones" does not exist` | el Query Tool está abierto sobre la base `postgres` | abrirlo sobre `itlm_iot` |
| `password authentication failed` | clave mal en el nodo de Node-RED | revisar la configuración del nodo, **no** el código JavaScript |
| `connection refused` en el puerto 5432 | servicio detenido, o `listen_addresses` en `localhost` | arrancar el servicio / editar `postgresql.conf` |
| La tabla existe pero está vacía | Node-RED no llega al broker, o la función descarta el tópico | ver `node-red/README.md` |
| `invalid input syntax for type double precision` | llegó texto donde se espera número | la placa publica texto plano, no JSON |
| `duplicate key value violates unique constraint` | se intentó insertar un `id` a mano | no incluir `id` en el INSERT |

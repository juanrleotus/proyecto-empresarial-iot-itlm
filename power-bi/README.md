# Power BI — el tablero de la dirección

Power BI es la última capa: no toca el broker ni las placas, solo **lee la base
de datos**. Si el tablero no muestra nada, el problema está antes.

```
PostgreSQL (itlm_iot) ──> Power BI Desktop ──> tablero
```

---

## 1. Requisito: el proveedor Npgsql

Power BI Desktop necesita el proveedor de PostgreSQL para .NET. Sin él, al
elegir la base de datos aparece el aviso *"Esta conexión requiere uno o más
componentes adicionales"*.

1. Descargar **Npgsql** de <https://github.com/npgsql/Npgsql/releases>
   — el archivo `Npgsql-x.y.z.msi` (instalador, **no** el .zip)
2. Durante la instalación, abrir el árbol de opciones y activar
   **Npgsql GAC Installation** (viene desactivada por defecto)
3. **Cerrar y volver a abrir Power BI Desktop**

> El paso 2 es el que falla siempre. Si no se instala en la GAC, Power BI no
> encuentra el proveedor aunque Npgsql esté instalado.

---

## 2. Conectar

**Inicio → Obtener datos → Base de datos PostgreSQL**

| Campo | Valor |
|---|---|
| Servidor | `localhost` (o la IP del servidor) |
| Base de datos | `itlm_iot` |

Modo de conectividad de datos: **Importar**.

Credenciales: **Base de datos** → usuario `powerbi` (el de solo lectura creado
en `base-de-datos/README.md`) y su contraseña.

---

## 3. Qué tablas traer

En el navegador, marcar:

| Objeto | Para qué |
|---|---|
| `v_ultimos_valores` | las tarjetas con el valor actual de cada sensor |
| `v_promedio_horario` | los gráficos de tendencia |
| `eventos` | historial de actuadores |
| `alarmas` | tabla de alarmas |
| `mediciones` | **solo si hace falta el detalle** |

> **Por qué conviene no traer `mediciones` entera.** A 5 segundos por muestra y
> 5 sensores son unas 86.000 filas por día: en un mes, 2,6 millones. Power BI
> las importa, pero el archivo pesa y los gráficos tardan. Las vistas ya vienen
> resumidas por hora, que es la resolución que un tablero de gestión necesita.
>
> Si se necesita el detalle, conviene filtrarlo al importar: *Transformar datos*
> → filtrar `fecha` por los últimos 7 días.

---

## 4. Modo Importar vs DirectQuery

| | Importar | DirectQuery |
|---|---|---|
| Velocidad del tablero | rápido | depende de la base |
| Datos | los del último refresco | siempre actuales |
| Carga sobre PostgreSQL | solo al refrescar | en cada interacción |
| Medidas DAX | todas | limitadas |

Para el aula: **Importar**, con refresco manual o cada 15 minutos
(*Transformar datos → Configuración del origen de datos*). Para un tablero que
tiene que mostrar el valor del minuto, el camino correcto no es DirectQuery sino
el tablero de Node-RED (`/ui`), que recibe el dato en el momento.

---

## 5. Tablero sugerido

**Página 1 — Estado actual**

| Visual | Datos |
|---|---|
| 4 tarjetas grandes | `v_ultimos_valores`: temperatura de Planta, Oficina, Almacén, Tanque |
| 2 medidores (gauge) | humedad de Planta y Oficina, mínimo 0, máximo 100 |
| Tabla | `v_ultimos_valores` con la medida `Segundos sin datos` |
| Indicador KPI | medida `Sensores en línea` |

**Página 2 — Tendencias**

| Visual | Datos |
|---|---|
| Gráfico de líneas | eje `hora` de `v_promedio_horario`, valores `promedio`, leyenda `dispositivo` |
| Segmentación | `variable` (temperatura / humedad) |
| Segmentación de fecha | `hora` |
| Gráfico de columnas | máximo y mínimo por día |

**Página 3 — Actuadores y alarmas**

| Visual | Datos |
|---|---|
| Gráfico de columnas | arranques por día desde `eventos` |
| Tabla | `alarmas` sin reconocer |
| Tarjeta | medida `Horas de marcha` |

---

## 6. Medidas DAX

Están en `medidas-dax.md`, listas para copiar.

---

## 7. Errores frecuentes

| Síntoma | Causa | Solución |
|---|---|---|
| "Esta conexión requiere componentes adicionales" | falta Npgsql o no se instaló en la GAC | reinstalar activando *Npgsql GAC Installation* |
| `No se pudo conectar al servidor` | PostgreSQL escucha solo en `localhost` | editar `postgresql.conf` y `pg_hba.conf` |
| Conecta pero no aparecen las vistas | se conectó a otra base | verificar `itlm_iot` |
| Los gráficos están vacíos | la tabla no tiene filas | revisar Node-RED, no Power BI |
| El eje de tiempo agrupa por año | Power BI tomó `fecha` como jerarquía | en el visual, elegir la fecha "sin jerarquía" |
| Los números salen como texto | la columna llegó como `character varying` | *Transformar datos* → cambiar el tipo a Número decimal |

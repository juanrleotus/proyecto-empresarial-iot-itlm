-- ============================================================
--  PROYECTO EMPRESARIAL IoT ITLM
--  PostgreSQL  -  Creacion de la base de datos
-- ------------------------------------------------------------
--  Ejecutar en pgAdmin 4:
--    Servers > PostgreSQL > Databases > itlm_iot > Query Tool
--
--  Si la base todavia no existe, crearla primero:
--    CREATE DATABASE itlm_iot
--      WITH ENCODING 'UTF8' TEMPLATE template0;
--  (y volver a abrir el Query Tool SOBRE itlm_iot)
-- ============================================================


-- ════════════════════════════════════════════════════════════
--  TABLA 1: mediciones
--  Una fila por cada lectura numerica de un sensor.
-- ════════════════════════════════════════════════════════════
CREATE TABLE IF NOT EXISTS mediciones (
    id          BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    fecha       TIMESTAMP        NOT NULL DEFAULT CURRENT_TIMESTAMP,
    dispositivo VARCHAR(50)      NOT NULL,   -- Planta, Oficina, Almacen, Tanque
    variable    VARCHAR(50)      NOT NULL,   -- temperatura, humedad
    valor       DOUBLE PRECISION NOT NULL,
    unidad      VARCHAR(10)                  -- C, %
);

COMMENT ON TABLE  mediciones            IS 'Lecturas numericas de los sensores del sistema IoT ITLM';
COMMENT ON COLUMN mediciones.dispositivo IS 'Sector de la planta de donde proviene el dato';
COMMENT ON COLUMN mediciones.variable    IS 'Magnitud medida';
COMMENT ON COLUMN mediciones.unidad      IS 'Unidad de ingenieria del valor';


-- ════════════════════════════════════════════════════════════
--  TABLA 2: eventos
--  Una fila por cada cambio de estado de un actuador o aviso
--  de la camara. No son mediciones: no tienen unidad fisica.
-- ════════════════════════════════════════════════════════════
CREATE TABLE IF NOT EXISTS eventos (
    id          BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    fecha       TIMESTAMP   NOT NULL DEFAULT CURRENT_TIMESTAMP,
    dispositivo VARCHAR(50) NOT NULL,   -- Planta, Tanque, Oficina, Camara
    actuador    VARCHAR(50) NOT NULL,   -- aire, bomba, luces, camara
    estado      VARCHAR(20) NOT NULL,   -- ON, OFF, ESPERA, FOTO
    detalle     TEXT                    -- URL de la foto, observaciones
);

COMMENT ON TABLE eventos IS 'Cambios de estado de actuadores y avisos de la camara';


-- ════════════════════════════════════════════════════════════
--  TABLA 3: alarmas
--  Se llena desde Node-RED cuando una medicion sale de rango.
-- ════════════════════════════════════════════════════════════
CREATE TABLE IF NOT EXISTS alarmas (
    id          BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    fecha       TIMESTAMP        NOT NULL DEFAULT CURRENT_TIMESTAMP,
    dispositivo VARCHAR(50)      NOT NULL,
    variable    VARCHAR(50)      NOT NULL,
    valor       DOUBLE PRECISION,
    limite      DOUBLE PRECISION,
    mensaje     TEXT             NOT NULL,
    reconocida  BOOLEAN          NOT NULL DEFAULT FALSE
);

COMMENT ON TABLE alarmas IS 'Mediciones fuera de los limites definidos en Node-RED';


-- ════════════════════════════════════════════════════════════
--  INDICES
--  Sin ellos, Power BI tarda cada vez mas a medida que la
--  tabla crece: una consulta "ultima hora" tiene que recorrer
--  TODAS las filas para encontrar las ultimas.
-- ════════════════════════════════════════════════════════════
CREATE INDEX IF NOT EXISTS idx_mediciones_fecha
    ON mediciones (fecha DESC);

CREATE INDEX IF NOT EXISTS idx_mediciones_disp_var_fecha
    ON mediciones (dispositivo, variable, fecha DESC);

CREATE INDEX IF NOT EXISTS idx_eventos_fecha
    ON eventos (fecha DESC);

CREATE INDEX IF NOT EXISTS idx_alarmas_fecha
    ON alarmas (fecha DESC);


-- ════════════════════════════════════════════════════════════
--  VISTA 1: ultimo valor de cada sensor
--  Para los indicadores grandes del tablero.
--  DISTINCT ON es propio de PostgreSQL: devuelve la primera
--  fila de cada grupo segun el ORDER BY.
-- ════════════════════════════════════════════════════════════
CREATE OR REPLACE VIEW v_ultimos_valores AS
SELECT DISTINCT ON (dispositivo, variable)
       dispositivo,
       variable,
       valor,
       unidad,
       fecha
FROM   mediciones
ORDER  BY dispositivo, variable, fecha DESC;


-- ════════════════════════════════════════════════════════════
--  VISTA 2: promedio por hora
--  Power BI no deberia graficar un punto cada 5 segundos: en
--  un dia son 17.280 puntos por sensor. Esta vista promedia
--  por hora y deja el grafico legible y rapido.
-- ════════════════════════════════════════════════════════════
CREATE OR REPLACE VIEW v_promedio_horario AS
SELECT date_trunc('hour', fecha) AS hora,
       dispositivo,
       variable,
       unidad,
       ROUND(AVG(valor)::numeric, 2) AS promedio,
       ROUND(MIN(valor)::numeric, 2) AS minimo,
       ROUND(MAX(valor)::numeric, 2) AS maximo,
       COUNT(*)                      AS muestras
FROM   mediciones
GROUP  BY 1, 2, 3, 4
ORDER  BY 1 DESC;


-- ════════════════════════════════════════════════════════════
--  VISTA 3: horas de marcha de cada actuador por dia
--  Util para mantenimiento: cuantas horas trabajo la bomba.
--  LEAD() mira la fila siguiente para saber cuanto duro el
--  estado actual.
-- ════════════════════════════════════════════════════════════
CREATE OR REPLACE VIEW v_horas_marcha AS
WITH tramos AS (
    SELECT dispositivo,
           actuador,
           estado,
           fecha                                           AS desde,
           LEAD(fecha) OVER (PARTITION BY dispositivo, actuador
                             ORDER BY fecha)               AS hasta
    FROM   eventos
    WHERE  estado IN ('ON', 'OFF')
)
SELECT dispositivo,
       actuador,
       date_trunc('day', desde) AS dia,
       ROUND(SUM(EXTRACT(EPOCH FROM (hasta - desde)) / 3600.0)::numeric, 2)
           AS horas_encendido
FROM   tramos
WHERE  estado = 'ON'
  AND  hasta IS NOT NULL
GROUP  BY 1, 2, 3
ORDER  BY 3 DESC, 1, 2;


-- ════════════════════════════════════════════════════════════
--  COMPROBACION
-- ════════════════════════════════════════════════════════════
SELECT table_name, table_type
FROM   information_schema.tables
WHERE  table_schema = 'public'
ORDER  BY table_type, table_name;

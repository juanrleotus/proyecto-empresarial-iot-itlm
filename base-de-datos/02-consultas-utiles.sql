-- ============================================================
--  PROYECTO EMPRESARIAL IoT ITLM
--  Consultas de diagnostico y de clase
-- ============================================================


-- ── 1. Llegan datos? Las ultimas 20 mediciones ──────────────
SELECT id, fecha, dispositivo, variable, valor, unidad
FROM   mediciones
ORDER  BY id DESC
LIMIT  20;


-- ── 2. Cuanto hace que no llega nada de cada sensor ─────────
--  Si "segundos_sin_datos" pasa de ~30, esa placa esta caida.
SELECT dispositivo,
       variable,
       valor,
       unidad,
       fecha,
       ROUND(EXTRACT(EPOCH FROM (CURRENT_TIMESTAMP - fecha))::numeric, 0)
           AS segundos_sin_datos
FROM   v_ultimos_valores
ORDER  BY segundos_sin_datos DESC;


-- ── 3. Cuantas filas guardo cada sensor ─────────────────────
SELECT dispositivo,
       variable,
       COUNT(*)        AS registros,
       MIN(fecha)      AS primera,
       MAX(fecha)      AS ultima,
       ROUND(AVG(valor)::numeric, 2) AS promedio
FROM   mediciones
GROUP  BY dispositivo, variable
ORDER  BY dispositivo, variable;


-- ── 4. Temperatura de la planta en la ultima hora ───────────
SELECT fecha, valor
FROM   mediciones
WHERE  dispositivo = 'Planta'
  AND  variable    = 'temperatura'
  AND  fecha >= CURRENT_TIMESTAMP - INTERVAL '1 hour'
ORDER  BY fecha;


-- ── 5. Maximo y minimo del dia, por sensor ──────────────────
SELECT dispositivo,
       variable,
       ROUND(MIN(valor)::numeric, 1) AS minimo,
       ROUND(MAX(valor)::numeric, 1) AS maximo,
       ROUND(AVG(valor)::numeric, 1) AS promedio
FROM   mediciones
WHERE  fecha::date = CURRENT_DATE
GROUP  BY dispositivo, variable
ORDER  BY dispositivo, variable;


-- ── 6. Promedio por hora (para graficar) ────────────────────
SELECT * FROM v_promedio_horario
WHERE  dispositivo = 'Tanque'
ORDER  BY hora DESC
LIMIT  48;


-- ── 7. Mediciones fuera de rango (sin tabla de alarmas) ─────
SELECT fecha, dispositivo, variable, valor, unidad
FROM   mediciones
WHERE  (variable = 'temperatura' AND (valor < 5  OR valor > 35))
   OR  (variable = 'humedad'     AND (valor < 20 OR valor > 80))
ORDER  BY fecha DESC
LIMIT  50;


-- ── 8. Historial de un actuador ─────────────────────────────
SELECT fecha, dispositivo, actuador, estado, detalle
FROM   eventos
WHERE  actuador = 'bomba'
ORDER  BY fecha DESC
LIMIT  30;


-- ── 9. Horas de marcha por dia (mantenimiento) ──────────────
SELECT * FROM v_horas_marcha
WHERE  dia >= CURRENT_DATE - INTERVAL '7 days';


-- ── 10. Cuantas veces arranco cada actuador hoy ─────────────
--  Un numero alto de arranques en pocas horas indica un ciclo
--  corto: el equipo se esta castigando.
SELECT dispositivo,
       actuador,
       COUNT(*) AS arranques_hoy
FROM   eventos
WHERE  estado = 'ON'
  AND  fecha::date = CURRENT_DATE
GROUP  BY dispositivo, actuador
ORDER  BY arranques_hoy DESC;


-- ── 11. Alarmas sin reconocer ───────────────────────────────
SELECT id, fecha, dispositivo, variable, valor, limite, mensaje
FROM   alarmas
WHERE  reconocida = FALSE
ORDER  BY fecha DESC;

-- Reconocer una alarma:
-- UPDATE alarmas SET reconocida = TRUE WHERE id = 12;


-- ── 12. Tamano que ocupa cada tabla ─────────────────────────
SELECT relname AS tabla,
       pg_size_pretty(pg_total_relation_size(relid)) AS tamano
FROM   pg_catalog.pg_statio_user_tables
ORDER  BY pg_total_relation_size(relid) DESC;


-- ── 13. MANTENIMIENTO: borrar datos viejos ──────────────────
--  A 5 s por muestra y 5 sensores son ~86.000 filas por dia.
--  Conviene conservar el detalle 30 dias y el resumen aparte.
--  CUIDADO: esto borra datos de forma definitiva.
-- DELETE FROM mediciones WHERE fecha < CURRENT_DATE - INTERVAL '30 days';
-- VACUUM ANALYZE mediciones;


-- ── 14. Insercion manual de prueba ──────────────────────────
--  Para verificar la base sin tener las placas encendidas.
-- INSERT INTO mediciones (dispositivo, variable, valor, unidad)
-- VALUES ('Planta', 'temperatura', 25.8, 'C');

-- INSERT INTO eventos (dispositivo, actuador, estado, detalle)
-- VALUES ('Tanque', 'bomba', 'ON', 'prueba manual');

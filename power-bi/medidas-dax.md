# Medidas DAX — Proyecto Empresarial IoT ITLM

Copiar cada bloque en **Modelado → Nueva medida**.

Las medidas de este archivo suponen que están importadas las vistas
`v_ultimos_valores` y `v_promedio_horario`, y las tablas `eventos` y `alarmas`.

> **Diferencia entre columna calculada y medida.** Una columna calculada se
> evalúa fila por fila al refrescar y ocupa espacio en el modelo. Una medida se
> evalúa en el momento, según los filtros del visual. Todo lo que es un
> agregado — promedios, conteos, máximos — va como **medida**.

---

## 1. Valores actuales

```dax
Temperatura Planta =
CALCULATE(
    MAX ( v_ultimos_valores[valor] ),
    v_ultimos_valores[dispositivo] = "Planta",
    v_ultimos_valores[variable]    = "temperatura"
)
```

```dax
Humedad Planta =
CALCULATE(
    MAX ( v_ultimos_valores[valor] ),
    v_ultimos_valores[dispositivo] = "Planta",
    v_ultimos_valores[variable]    = "humedad"
)
```

```dax
Temperatura Tanque =
CALCULATE(
    MAX ( v_ultimos_valores[valor] ),
    v_ultimos_valores[dispositivo] = "Tanque",
    v_ultimos_valores[variable]    = "temperatura"
)
```

```dax
Temperatura Oficina =
CALCULATE(
    MAX ( v_ultimos_valores[valor] ),
    v_ultimos_valores[dispositivo] = "Oficina",
    v_ultimos_valores[variable]    = "temperatura"
)
```

```dax
Temperatura Almacen =
CALCULATE(
    MAX ( v_ultimos_valores[valor] ),
    v_ultimos_valores[dispositivo] = "Almacen",
    v_ultimos_valores[variable]    = "temperatura"
)
```

> Se usa `MAX` y no `SUM` porque la vista devuelve **una sola fila** por sensor:
> `MAX` toma ese valor. Con `SUM`, si por cualquier motivo llegaran dos filas,
> el tablero mostraría la suma de dos temperaturas — un número sin sentido
> físico que nadie detectaría a simple vista.

---

## 2. Vigilancia del sistema

```dax
Segundos sin datos =
VAR UltimaFecha = MAX ( v_ultimos_valores[fecha] )
RETURN
    IF (
        ISBLANK ( UltimaFecha ),
        BLANK (),
        DATEDIFF ( UltimaFecha, UTCNOW (), SECOND )
    )
```

> `UTCNOW()` devuelve la hora universal y `NOW()` la del equipo. PostgreSQL
> guardó la fecha con la hora local del servidor. Si el tablero muestra un
> desfasaje de varias horas, cambiar `UTCNOW()` por `NOW()`, o guardar la fecha
> como `TIMESTAMPTZ` en la base. Es un buen ejercicio de clase.

```dax
Sensores en linea =
VAR Limite = 60      -- segundos
RETURN
    COUNTROWS (
        FILTER (
            v_ultimos_valores,
            DATEDIFF ( v_ultimos_valores[fecha], UTCNOW (), SECOND ) <= Limite
        )
    )
```

```dax
Sensores totales = COUNTROWS ( v_ultimos_valores )
```

```dax
Estado del sistema =
VAR EnLinea = [Sensores en linea]
VAR Total   = [Sensores totales]
RETURN
    SWITCH (
        TRUE (),
        Total   = 0,       "SIN DATOS",
        EnLinea = Total,   "NORMAL",
        EnLinea = 0,       "SISTEMA CAIDO",
        "DEGRADADO (" & EnLinea & "/" & Total & ")"
    )
```

Esta última medida va en una tarjeta grande arriba del tablero. Es la que mira
un encargado de mantenimiento antes que cualquier gráfico.

---

## 3. Tendencias

```dax
Promedio del periodo = AVERAGE ( v_promedio_horario[promedio] )
```

```dax
Maximo del periodo = MAX ( v_promedio_horario[maximo] )
```

```dax
Minimo del periodo = MIN ( v_promedio_horario[minimo] )
```

```dax
Amplitud termica = [Maximo del periodo] - [Minimo del periodo]
```

```dax
Promedio ultimas 24 h =
CALCULATE (
    AVERAGE ( v_promedio_horario[promedio] ),
    v_promedio_horario[hora] >= UTCNOW () - 1
)
```

```dax
Variacion vs hora anterior =
VAR HoraActual = MAX ( v_promedio_horario[hora] )
VAR Actual =
    CALCULATE ( AVERAGE ( v_promedio_horario[promedio] ),
                v_promedio_horario[hora] = HoraActual )
VAR Anterior =
    CALCULATE ( AVERAGE ( v_promedio_horario[promedio] ),
                v_promedio_horario[hora] = HoraActual - ( 1 / 24 ) )
RETURN
    IF ( ISBLANK ( Anterior ), BLANK (), Actual - Anterior )
```

---

## 4. Actuadores

```dax
Arranques del dia =
CALCULATE (
    COUNTROWS ( eventos ),
    eventos[estado] = "ON",
    eventos[fecha] >= TODAY ()
)
```

```dax
Arranques Bomba =
CALCULATE (
    COUNTROWS ( eventos ),
    eventos[estado]   = "ON",
    eventos[actuador] = "bomba"
)
```

```dax
Fotos tomadas =
CALCULATE (
    COUNTROWS ( eventos ),
    eventos[estado] = "FOTO"
)
```

```dax
Veces en espera del compresor =
CALCULATE (
    COUNTROWS ( eventos ),
    eventos[estado] = "ESPERA"
)
```

> Esta última medida tiene valor técnico: cada `ESPERA` es un pedido de
> encendido que la protección del compresor tuvo que retener. Un número alto
> indica que alguien está golpeando el botón, o que la lógica de control pide
> arranques demasiado seguidos.

---

## 5. Alarmas

```dax
Alarmas sin reconocer =
CALCULATE ( COUNTROWS ( alarmas ), alarmas[reconocida] = FALSE () )
```

```dax
Alarmas del dia =
CALCULATE ( COUNTROWS ( alarmas ), alarmas[fecha] >= TODAY () )
```

```dax
Ultima alarma = MAX ( alarmas[fecha] )
```

---

## 6. Formato condicional por semáforo

Para pintar de rojo una tarjeta de temperatura cuando se pasa de límite:

```dax
Color Temperatura Planta =
VAR T = [Temperatura Planta]
RETURN
    SWITCH (
        TRUE (),
        ISBLANK ( T ), "#6B7280",   -- gris: sin datos
        T > 35,        "#DC2626",   -- rojo: alta
        T > 30,        "#F59E0B",   -- ambar: atencion
        T < 5,         "#2563EB",   -- azul: baja
        "#16A34A"                   -- verde: normal
    )
```

Después, en el visual: **Formato → Color del valor → fx → Formato por: Valor de
campo → Campo: `Color Temperatura Planta`**.

---

## 7. Tabla de fechas (recomendada)

Para que las segmentaciones de tiempo y el *time intelligence* funcionen bien,
conviene una tabla de calendario propia:

```dax
Calendario =
ADDCOLUMNS (
    CALENDAR ( DATE ( 2026, 1, 1 ), DATE ( 2027, 12, 31 ) ),
    "Anio",       YEAR ( [Date] ),
    "Mes",        FORMAT ( [Date], "MMMM" ),
    "MesNumero",  MONTH ( [Date] ),
    "Dia",        DAY ( [Date] ),
    "DiaSemana",  FORMAT ( [Date], "dddd" ),
    "Semana",     WEEKNUM ( [Date] )
)
```

Luego, en **Modelo**, relacionar `Calendario[Date]` con la fecha de
`v_promedio_horario` y marcar la tabla como tabla de fechas
(*Herramientas de tabla → Marcar como tabla de fechas*).

# Scripts auxiliares

## `registrador_csv.py`

Graba en un archivo CSV **todo** lo que pasa por el broker, un archivo por día.
Sirve para dos cosas:

- **Clase de adquisición de datos** sin montar PostgreSQL: el CSV se abre en
  Excel y se grafica en cinco minutos.
- **Diagnóstico**: queda un registro con hora de PC de todo lo que publicó cada
  placa, útil para encontrar cortes intermitentes.

No requiere tocar el firmware de ninguna placa: se suscribe a `empresa/#` como
un cliente más.

### Instalación

```cmd
pip install paho-mqtt
```

### Uso — MODO A (broker sin autenticación)

```cmd
python registrador_csv.py
```

### Uso — MODO B (broker con usuario y contraseña)

La contraseña **no** se escribe en el script: se pasa por variable de entorno.

```cmd
set MQTT_USUARIO=docente
set MQTT_CLAVE=la-clave
python registrador_csv.py
```

En Linux o macOS:

```bash
MQTT_USUARIO=docente MQTT_CLAVE=la-clave python3 registrador_csv.py
```

### Configuración

Al principio del archivo:

| Constante | Valor por defecto | Qué es |
|---|---|---|
| `BROKER` | `localhost` | IP del broker |
| `PUERTO` | `1883` | |
| `TOPICO` | `empresa/#` | `#` captura todas las ramas |
| `CARPETA` | `registros` | se crea sola; está en `.gitignore` |
| `DELIMITADOR` | `;` | Excel en español usa `;` |

### Columnas del CSV

```
fecha_hora ; topico ; zona ; magnitud ; valor ; unidad ; ts_placa ; retenido
```

`fecha_hora` es la hora de la PC, que es la fecha real. `ts_placa` son los
milisegundos desde que arrancó la placa: sirve para detectar que una placa se
reinició (el número vuelve a empezar de cero).

### Detener

`Ctrl + C`. El archivo queda completo: se abre y se cierra en cada mensaje, así
que un corte de luz no pierde lo grabado.

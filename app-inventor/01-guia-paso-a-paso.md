# Armado paso a paso — App MQTT para ESP32 / ESP8266

**Proyecto:** `ESP32_Conexion`
**Extensión:** `UrsPahoMqttClient` (de.ullisroboterseite.ursai2pahomqtt)
**Objetivo:** conectarse a un broker MQTT de la red local, suscribirse a cuatro zonas, mostrar los valores recibidos en JSON y graficarlos.

---

## Antes de empezar

Trabajá sobre tu **proyecto original** (el que ya tenías en App Inventor). No hace falta importar nada.

Lo primero es **borrar los bloques viejos**, porque todos tienen errores y van a estorbar:

1. Entrá a la pestaña **Bloques**.
2. Seleccioná cada grupo de bloques y arrastralo al **tacho de basura** (abajo a la derecha).
3. Tienen que quedar los 7 grupos siguientes, que son los que estaban mal:
   - `cuando Gráfico1.EntryClick`
   - `cuando btbEnviar.Click`
   - `cuando UrsPahoMqttClient1.ConnectionStateChanged`
   - Los cuatro `cuando subscripcion___.Changed`

El área de bloques tiene que quedar **completamente vacía**.

---

## PARTE 1 — Cambios en el Diseñador

### 1.1 Cliente MQTT

Seleccioná `UrsPahoMqttClient1` en la lista de **Componentes** y en **Propiedades**:

| Propiedad | Valor | Por qué |
|---|---|---|
| `Broker` | *(dejar vacío)* | La IP la va a cargar la caja de texto en tiempo de ejecución |
| `Port` | `1883` | Puerto MQTT sin cifrar |

> Tu proyecto tenía `Broker = 192.1`, una IP incompleta. Vaciala.

### 1.2 Etiqueta de estado

1. En **Paleta → Interfaz de usuario**, arrastrá una **Etiqueta** dentro de `DisposiciónVertical1`, **debajo** de la imagen `CONECTADO`.
2. Renombrala a `lblEstado` (botón *Cambiar nombre*).
3. Propiedades: `Texto` = `Desconectado`, `Negrita` = tildado.

### 1.3 El gráfico y sus cuatro series

Este es el punto que impedía que el gráfico funcionara: **un Chart sin series no dibuja nada**.

1. Seleccioná `Gráfico1` y poné la propiedad `Tipo` = **línea**.
2. En **Paleta → Gráficos**, arrastrá un **ChartData2D** y soltalo **encima del `Gráfico1`** (tiene que quedar anidado adentro, no al lado).
3. Repetí hasta tener **cuatro**.
4. Renombralos y configuralos:

| Nombre | Propiedad `Etiqueta` | Propiedad `Color` |
|---|---|---|
| `DatosPlanta` | `Planta` | Rojo |
| `DatosOficina` | `Oficina` | Azul |
| `DatosAlmacen` | `Almacén` | Verde |
| `DatosTanque` | `Tanque` | Naranja |

Verificá en el árbol de **Componentes** que queden así:

```
Gráfico1
   ├── DatosPlanta
   ├── DatosOficina
   ├── DatosAlmacen
   └── DatosTanque
```

Si aparecen al mismo nivel que `Gráfico1` en vez de adentro, arrastralos de nuevo.

### 1.4 Etiquetas indicadoras

Seleccioná `indicadorPlanta`, `indicadorOficina`, `indicadorAlmacen` e `indicadorTanque` y poné en cada una `Texto` = `—` (un guion largo).

### 1.5 Componentes no visibles

Confirmá que estén presentes: `Reloj1` (Sensores → Reloj) y `Web1` (Conectividad → Web).

`Web1` **no** se usa para pedir páginas: lo necesitamos solo por su bloque para decodificar JSON.

---

## PARTE 2 — Variables globales

Estas ya las tenés cargadas (se importaron bien). Si las borraste, rehacelas.

De **Bloques → Variables**, arrastrá `inicializar global ___ como` siete veces.
El valor de texto sale de **Bloques → Texto** (el bloque `" "`), y el numérico de **Bloques → Matemáticas** (el bloque `0`).

| Nombre de la variable | Tipo de bloque | Valor |
|---|---|---|
| `topicPlanta` | Texto | `planta/temperatura` |
| `topicOficina` | Texto | `oficina/temperatura` |
| `topicAlmacen` | Texto | `almacen/temperatura` |
| `topicTanque` | Texto | `tanque/nivel` |
| `datos` | Texto | *(vacío)* |
| `valor` | Matemáticas | `0` |
| `marca` | Matemáticas | `0` |

> **Esto es lo importante:** los nombres de los tópicos quedan en un solo lugar. Si mañana cambiás `planta/temperatura` por otra cosa, lo tocás acá y toda la app se adapta sola.

---

## PARTE 3 — `cuando Screen1.Initialize`

Se ejecuta una sola vez al abrir la app. Sirve para dejar todo en un estado conocido.

**Dónde está el bloque:** en el árbol de Bloques, clic en `Screen1` → `cuando Screen1.Initialize`.

Adentro, en orden:

```
cuando Screen1.Initialize
  ejecutar
    poner UrsPahoMqttClient1.Port  a  [ 1883 ]
    poner UrsPahoMqttClient1.ClientID  a  [ unir ─┬─ "AppESP32_"
                                                  └─ llamar Reloj1.SystemTime ]
    poner lblEstado.Texto  a  [ "Desconectado" ]
    poner CONECTADO.Visible  a  [ falso ]
```

**Cómo armar cada pieza:**

1. `poner UrsPahoMqttClient1.Port a` → clic en `UrsPahoMqttClient1` en el árbol, buscá el bloque verde oscuro `poner ... a` y elegí `Port` en el desplegable. El `1883` sale de **Matemáticas**.
2. El bloque `unir` sale de **Texto**. Tiene dos huecos.
3. `llamar Reloj1.SystemTime` → clic en `Reloj1`. Devuelve los milisegundos actuales.
4. `falso` sale de **Lógica**.

> **Por qué el ClientID:** MQTT exige que cada cliente tenga un identificador único. Si dos alumnos se conectan al mismo broker con el mismo ID, el broker expulsa a uno. Pegarle la hora del sistema garantiza que nunca se repita.

---

## PARTE 4 — `cuando btbEnviar.Click`

Acá estaba uno de los errores más importantes del proyecto original: conectaba **antes** de cargar el broker, y además hacía `poner Broker a obtener Broker`, que se asigna a sí mismo y no hace nada.

El orden correcto es **primero configurar, después conectar**:

```
cuando btbEnviar.Click
  ejecutar
    poner UrsPahoMqttClient1.Broker  a  [ obtener direccionIP.Texto ]
    llamar UrsPahoMqttClient1.Connect
        CleanSession  [ verdadero ]
```

**Cómo armar:**

1. `obtener direccionIP.Texto` → clic en `direccionIP` en el árbol, bloque `obtener ... ` y elegir `Texto`.
2. `llamar UrsPahoMqttClient1.Connect` → clic en `UrsPahoMqttClient1`.
3. `verdadero` sale de **Lógica**.

> **CleanSession = verdadero** significa que el broker no guarda suscripciones viejas de este cliente. Para un proyecto de aula es lo que querés.

---

## PARTE 5 — `cuando UrsPahoMqttClient1.ConnectionStateChanged`

Este evento trae dos parámetros: `NewState` (un número) y `StateString` (el nombre del estado en texto).

```
cuando UrsPahoMqttClient1.ConnectionStateChanged (NewState, StateString)
  ejecutar
    poner lblEstado.Texto  a  [ obtener StateString ]
    si  [ obtener NewState  =  2 ]
      entonces   poner CONECTADO.Visible  a  [ verdadero ]
      si no      poner CONECTADO.Visible  a  [ falso ]
```

**Cómo armar:**

1. El bloque `si / entonces` sale de **Control**. Para agregarle el `si no`, hacé clic en el **engranaje azul** del bloque y arrastrá un `si no` dentro del `si` en el mini-panel que se abre.
2. El bloque `=` sale de **Lógica**.
3. `obtener NewState` y `obtener StateString`: pasá el mouse **por encima del nombre del parámetro** en la cabecera del evento y aparece un globito con `obtener NewState` para arrastrar. Es la única forma de conseguirlos.
4. El `2` sale de **Matemáticas**.

| `NewState` | Significado |
|---|---|
| 0 | Desconectado |
| 1 | Conectando |
| 2 | **Conectado** |
| 3 | Desconectando |
| 4 | Conexión abortada |

> El proyecto original pisaba `direccionIP.Texto` acá, borrando lo que había escrito el usuario. Ahora solo escribe en `lblEstado`.

---

## PARTE 6 — `cuando UrsPahoMqttClient1.ErrorOccurred`

Bloque nuevo, no estaba. Es el que te va a ahorrar horas de diagnóstico en clase, porque muestra el motivo exacto del fallo.

Parámetros: `ActionName`, `ErrorCode`, `ErrorMessage`.

```
cuando UrsPahoMqttClient1.ErrorOccurred (ActionName, ErrorCode, ErrorMessage)
  ejecutar
    poner lblEstado.Texto  a  [ unir ─┬─ "Error: "
                                      └─ obtener ErrorMessage ]
```

---

## PARTE 7 — `cuando UrsPahoMqttClient1.MessageReceived`

**Este es el bloque central y el que faltaba por completo.** Sin él la app se suscribe pero nunca escucha: por eso no aparecía ningún dato.

Parámetros: `Topic`, `Payload`, `Message`, `RetainFlag`, `DupFlag`.

### 7.1 Decodificar el JSON (tres líneas)

```
cuando UrsPahoMqttClient1.MessageReceived (Topic, Payload, Message, RetainFlag, DupFlag)
  ejecutar
    poner global datos  a  [ llamar Web1.JsonTextDecode
                                 jsonText  [ obtener Message ] ]

    poner global valor  a  [ buscar en pares  clave        [ "v" ]
                                              pares        [ obtener global datos ]
                                              noEncontrado [ 0 ] ]

    poner global marca  a  [ buscar en pares  clave        [ "ts" ]
                                              pares        [ obtener global datos ]
                                              noEncontrado [ 0 ] ]
```

**Cómo armar:**

1. `poner global datos a` → **Variables** → bloque `poner ... a`, y elegí `global datos` en el desplegable.
2. `llamar Web1.JsonTextDecode` → clic en `Web1` en el árbol.
3. `buscar en pares` → **Listas**. En inglés es *look up in pairs*. Tiene tres huecos: clave, pares, noEncontrado.
4. Las comillas `"v"` y `"ts"` salen de **Texto**.

> **Qué está pasando acá:** la placa manda `{"v":23.4,"u":"C","ts":154302}`. `JsonTextDecode` lo convierte en una lista de parejas: `(("v" 23.4) ("u" "C") ("ts" 154302))`. Después `buscar en pares` saca el valor de la clave que le pidas. El `noEncontrado` es la red de seguridad: si el mensaje viniera mal formado, devuelve `0` en vez de tirar error.

### 7.2 Repartir el dato según la zona (cuatro bloques `si`)

Justo debajo de las tres líneas anteriores, **dentro del mismo evento**, van cuatro bloques `si` uno tras otro. Estos **no** llevan `si no`.

```
    si  [ obtener Topic  =  obtener global topicPlanta ]
      entonces
        poner indicadorPlanta.Texto  a  [ obtener global valor ]
        llamar DatosPlanta.AddEntry
            x  [ obtener global marca ]
            y  [ obtener global valor ]

    si  [ obtener Topic  =  obtener global topicOficina ]
      entonces
        poner indicadorOficina.Texto  a  [ obtener global valor ]
        llamar DatosOficina.AddEntry
            x  [ obtener global marca ]
            y  [ obtener global valor ]

    si  [ obtener Topic  =  obtener global topicAlmacen ]
      entonces
        poner indicadorAlmacen.Texto  a  [ obtener global valor ]
        llamar DatosAlmacen.AddEntry
            x  [ obtener global marca ]
            y  [ obtener global valor ]

    si  [ obtener Topic  =  obtener global topicTanque ]
      entonces
        poner indicadorTanque.Texto  a  [ obtener global valor ]
        llamar DatosTanque.AddEntry
            x  [ obtener global marca ]
            y  [ obtener global valor ]
```

**Truco para no armar cuatro veces lo mismo:** armá el primer bloque `si` completo, hacé **clic derecho → Duplicar**, y en la copia cambiá solo tres cosas: la variable del tópico, la etiqueta indicadora y la serie del gráfico.

> **Atención con las etiquetas.** El proyecto original usaba `Planta`, `Oficina`, etc., que son los **títulos fijos** de cada fila (dicen "PLANTA", "OFICINA"…). Las que tienen que mostrar los valores son `indicadorPlanta`, `indicadorOficina`, etc. Es un error fácil de repetir porque los nombres se parecen.

---

## PARTE 8 — Los cuatro `cuando subscripcion___.Changed`

El original llamaba `Subscribe` pasándole `Planta.Visible` como nombre de tópico — un booleano donde va un texto. Y nunca se desuscribía al destildar.

Versión corregida (la de Planta; las otras tres son idénticas cambiando el nombre):

```
cuando subscripcionPlanta.Changed
  ejecutar
    si  [ obtener subscripcionPlanta.Marcado ]
      entonces
        llamar UrsPahoMqttClient1.Subscribe
            Topic  [ obtener global topicPlanta ]
            QoS    [ 0 ]
      si no
        llamar UrsPahoMqttClient1.Unsubscribe
            Topic  [ obtener global topicPlanta ]
```

Repetí para `subscripcionOficina` → `topicOficina`, `subscripcionAlmacen` → `topicAlmacen`, `subscripcionTanque` → `topicTanque`.

> `Marcado` es la propiedad `Checked` del CheckBox. El evento `Changed` se dispara tanto al tildar como al destildar; por eso el `si / si no` es imprescindible.

---

## PARTE 9 — Resumen de lo que tiene que quedar

| Grupo de bloques | Cantidad |
|---|---|
| `inicializar global` | 7 |
| `cuando Screen1.Initialize` | 1 |
| `cuando btbEnviar.Click` | 1 |
| `cuando ...ConnectionStateChanged` | 1 |
| `cuando ...ErrorOccurred` | 1 |
| `cuando ...MessageReceived` | 1 |
| `cuando subscripcion___.Changed` | 4 |
| **Total de grupos** | **16** |

Abajo a la izquierda el contador tiene que marcar **0 advertencias y 0 errores**. Si marca algo, casi siempre es un hueco vacío.

---

## PARTE 10 — Prueba

### Orden de encendido

1. Arrancá el broker Mosquitto en la PC.
2. Alimentá las placas y confirmá en el Monitor Serie que publican.
3. Abrí **MQTT Explorer** y verificá que llegan los cuatro tópicos con JSON del tipo `{"v":23.4,"u":"C","ts":154302}`.
4. Recién ahí abrí la app.

### Secuencia en la app

1. Escribí la IP del broker en la caja de texto (por ejemplo `192.168.1.100`). **Nunca `localhost`**: eso apuntaría al propio celular.
2. Tocá **ENVIAR**. `lblEstado` debe pasar a `Connected` y aparecer la imagen.
3. Tildá los CheckBox de a uno. Cada indicador debe empezar a actualizarse.
4. El gráfico debe dibujar una línea por cada zona tildada.

---

## PARTE 11 — Si algo no anda

| Síntoma | Causa probable | Qué hacer |
|---|---|---|
| Queda en `ConnectionAborted` | IP mal escrita, puerto distinto de 1883, o el firewall de la PC bloquea el 1883 | Fijate qué dice `lblEstado` gracias al bloque `ErrorOccurred` |
| Conecta pero no llega nada | El texto del tópico no coincide **exactamente** con el del firmware | Copiá el tópico desde MQTT Explorer y pegalo en la variable global |
| Los indicadores muestran `0` | El mensaje no es JSON válido, o esa placa quedó con el firmware viejo | Miralo en MQTT Explorer: si ves `23.4` pelado en vez de `{"v":23.4,...}`, falta reprogramar esa placa |
| El gráfico no dibuja | Los `ChartData2D` quedaron afuera del `Gráfico1` | Revisá el árbol de Componentes (Parte 1.3) |
| Se actualiza la fila equivocada | Se usó la etiqueta de título en vez de la indicadora | Revisá la advertencia de la Parte 7.2 |
| Conecta y a los segundos se cae | Dos dispositivos con el mismo ClientID | Verificá el bloque de la Parte 3 |

---

## Nota sobre el eje X del gráfico

Se está usando `ts`, que es `millis()` de la placa: los milisegundos desde que **esa** placa arrancó.

Funciona perfecto para ver la evolución de una zona. Pero si las cuatro zonas son placas distintas, cada una lleva su propio contador y las series van a quedar corridas entre sí.

Si te molesta para la clase, tenés dos caminos:

- **Simple:** usar un contador de muestras en la app (una variable global que sume 1 con cada mensaje recibido) y pasar eso como `x`.
- **Prolijo:** que las placas sincronicen hora real por NTP y manden un timestamp absoluto en el JSON.

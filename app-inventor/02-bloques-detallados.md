# Programación en bloques — App `ESP32_Conexion`

**Sistema domótico ITLM** · App Inventor + extensión `UrsPahoMqttClient`
Documentación del proyecto al 21/09/2026, con los bloques existentes y los que hay que agregar.

---

## Cómo leer este documento

| Marca | Significado |
|---|---|
| ✅ **YA ESTÁ** | Bloque que ya existe en tu proyecto y funciona. No tocar. |
| 🆕 **AGREGAR** | Bloque nuevo. Hay que armarlo. |
| ⚠️ **REVISAR** | Bloque existente con un problema o una mejora recomendada. |
| 💡 **OPCIONAL** | Ampliación que podés dejar para después. |

Los bloques se escriben con los mismos nombres que aparecen en tu App Inventor en español (`poner ... como`, `tomar global`, `llamar`, `Clic`, `MensajeRecibido`, etc.). La sangría indica qué bloque va encastrado dentro de cuál.

---

## Índice

0. [Diagnóstico del proyecto actual](#0-diagnóstico-del-proyecto-actual)
1. [Diseñador — componentes](#1-diseñador--componentes)
2. [Variables globales](#2-variables-globales)
3. [`Screen1.Inicializar`](#3-cuando-screen1inicializar)
4. [`btbEnviar.Clic`](#4-cuando-btbenviarclic)
5. [`ConnectionStateChanged`](#5-cuando-urspahomqttclient1connectionstatechanged)
6. [`ErrorGenerado`](#6-cuando-urspahomqttclient1errorgenerado)
7. [Los cuatro CheckBox `.Cambiado`](#7-los-cuatro-checkbox-cambiado)
8. [`MensajeRecibido`](#8-cuando-urspahomqttclient1mensajerecibido)
9. [Botones de los actuadores](#9-botones-de-los-actuadores)
10. [Resumen final y orden de trabajo](#10-resumen-final-y-orden-de-trabajo)

---

## 0. Diagnóstico del proyecto actual

La parte de **monitoreo** funciona: conexión, suscripción, recepción de JSON e indicadores. Lo que falta es toda la parte de **actuación**: los cuatro botones están en el diseño pero no tienen ningún bloque.

Además encontré estos puntos:

| # | Dónde | Qué pasa | Gravedad |
|---|---|---|---|
| 1 | Los 4 CheckBox | Usan `.Habilitado` en vez de `.Marcado`. `Habilitado` siempre es verdadero, así que al **destildar** la app se vuelve a suscribir en lugar de desuscribirse. | ⚠️ Corregir |
| 2 | `ConnectionStateChanged` | Solo muestra/oculta `CONECTADO`. La imagen `DESCONECTADO` nunca se oculta ni se vuelve a mostrar. | ⚠️ Corregir |
| 3 | `MensajeRecibido` | Las comparaciones usan el tópico escrito como texto, no las variables globales. Funciona, pero el tópico queda escrito en dos lugares. | ⚠️ Recomendado |
| 4 | `ErrorGenerado` | `"Error"` sin espacio: se lee `ErrorConnection refused`. | ⚠️ Cosmético |
| 5 | `ControlGrafico` | `XFromZero` activado. Como el eje X es `ts` (milisegundos, valores de cientos de miles), los datos quedan aplastados contra el borde derecho. | ⚠️ Recomendado |
| 6 | Botón **Luces** | No hay ninguna placa programada para recibir ese comando todavía. | Ver sección 9.3 |

**Corrección sobre mis respuestas anteriores:** en esta extensión, el bloque `Publish` lleva **solo dos parámetros**, `Topic` y `Message`. Lo verifiqué en el archivo de definición de la extensión dentro de tu `.aia`. La variante con `QoS` y `RetainFlag` se llama `PublishEx`. Donde antes te escribí `Publish` con cuatro parámetros, era un error mío; en este documento ya está corregido.

---

## 1. Diseñador — componentes

### 1.1 ✅ YA ESTÁ

| Componente | Tipo | Función |
|---|---|---|
| `direccionIP` | CampoDeTexto | IP del broker |
| `btbEnviar` | Botón `CONECTAR` | Conecta al broker |
| `LblEstado` | Etiqueta | Estado de la conexión |
| `CONECTADO` / `DESCONECTADO` | Imagen | Indicador visual |
| `subscripcionPlanta` … `subscripcionTanque` | CasillaDeVerificación ×4 | Suscripción por zona |
| `Planta`, `Oficina`, `Almacen`, `Tanque` | Etiqueta ×4 | **Títulos fijos** de cada fila |
| `indicadorPlanta` … `indicadorTanque` | Etiqueta ×4 | **Valores** recibidos |
| `btbBomba4` | Botón `Aire` | Fila Planta |
| `btbBomba3` | Botón `Luces` | Fila Oficina |
| `btbBomba2` | Botón `Cámara` | Fila Almacén |
| `btbBomba` | Botón `Bomba` | Fila Tanque |
| `ControlGrafico` + 4 `ChartData2D` | Gráfico | Historial de temperatura |
| `Reloj1`, `Web1`, `UrsPahoMqttClient1` | No visibles | Reloj, decodificador JSON, cliente MQTT |
| `ReconocimientoDeVoz1` | No visible | Sin uso por ahora |

Los botones se llaman todos `btbBomba…` aunque cada uno comanda algo distinto. Este documento usa **tus nombres actuales** y aclara entre paréntesis qué hace cada uno. Si preferís renombrarlos (`btnAire`, `btnLuces`, etc.), hacelo **antes** de armar los bloques: App Inventor actualiza solo los bloques existentes, pero los de este documento vas a tener que leerlos con el nombre nuevo.

### 1.2 🆕 AGREGAR

| Componente | Paleta | Nombre | Dónde ubicarlo |
|---|---|---|---|
| Imagen | Interfaz de usuario | `imgFoto` | Debajo del gráfico. Ancho: *Ajustar al contenedor*. Alto: 200 píxeles |

### 1.3 💡 OPCIONAL — video en vivo

| Componente | Paleta | Nombre | Dónde ubicarlo |
|---|---|---|---|
| Botón | Interfaz de usuario | `btnVideo` — texto `Ver video` | Junto a `imgFoto` |
| VisorWeb | Interfaz de usuario | `webCamara` | Debajo de `imgFoto`. Alto: 250 píxeles |

### 1.4 ⚠️ REVISAR — ajuste del gráfico

Seleccioná `ControlGrafico` y **destildá `XFromZero`**. Así el eje X se ajusta a los valores reales de `ts` en lugar de arrancar en cero.

---

## 2. Variables globales

Todas se arman con **Variables → `inicializar global ___ como`**. Los textos salen de **Texto** y los números de **Matemáticas**.

### 2.1 ✅ YA ESTÁ

```
inicializar global topicPlanta   como  "empresa/planta/temperatura"
inicializar global topicOficina  como  "empresa/oficina/temperatura"
inicializar global topicAlmacen  como  "empresa/almacen/temperatura"
inicializar global topicTanque   como  "empresa/tanque/temperatura"
inicializar global datos         como  ""
inicializar global valor         como  0
inicializar global marca         como  0
```

### 2.2 🆕 AGREGAR — Bomba (ESP8266, fila Tanque)

```
inicializar global topicBombaCmd     como  "empresa/tanque/rele/cmd"
inicializar global topicBombaEstado  como  "empresa/tanque/rele/estado"
inicializar global estadoBomba       como  "OFF"
```

### 2.3 🆕 AGREGAR — Aire (ESP32, fila Planta)

```
inicializar global topicAireCmd     como  "empresa/planta/aire/cmd"
inicializar global topicAireEstado  como  "empresa/planta/aire/estado"
inicializar global estadoAire       como  "OFF"
```

### 2.4 🆕 AGREGAR — Luces (fila Oficina)

```
inicializar global topicLucesCmd     como  "empresa/oficina/luces/cmd"
inicializar global topicLucesEstado  como  "empresa/oficina/luces/estado"
inicializar global estadoLuces       como  "OFF"
```

### 2.5 🆕 AGREGAR — Cámara (ESP32-CAM, fila Almacén)

```
inicializar global topicCamCmd     como  "empresa/camara/cmd"
inicializar global topicCamEvento  como  "empresa/camara/evento"
```

### 2.6 💡 OPCIONAL — solo si agregás el video

```
inicializar global topicCamEstado  como  "empresa/camara/estado"
inicializar global urlPaginaCam    como  ""
```

Todos estos textos tienen que coincidir **letra por letra** con los del firmware de cada placa. Si alguno no coincide, el botón publica pero nadie escucha.

---

## 3. `cuando Screen1.Inicializar`

### ✅ YA ESTÁ — sin cambios

```
cuando Screen1 .Inicializar
ejecutar
    poner UrsPahoMqttClient1 . Port      como  1883
    poner UrsPahoMqttClient1 . ClientID  como  unir  "AppESP32_"
                                                     llamar Reloj1 .HoraDelSistema
    poner CONECTADO    . Visible  como  falso
    poner DESCONECTADO . Visible  como  verdadero
```

Este bloque está bien. El `ClientID` con la hora del sistema garantiza que dos celulares nunca se pisen en el broker.

---

## 4. `cuando btbEnviar.Clic`

### ✅ YA ESTÁ — sin cambios

```
cuando btbEnviar .Clic
ejecutar
    poner UrsPahoMqttClient1 . Broker  como  direccionIP . Texto
    llamar UrsPahoMqttClient1 .Conectar
        CleanSession  verdadero
```

Primero carga la IP, después conecta. Orden correcto.

---

## 5. `cuando UrsPahoMqttClient1.ConnectionStateChanged`

Acá van dos cambios: arreglar la imagen `DESCONECTADO` y **agregar las suscripciones a los estados de los actuadores**. Esas suscripciones van acá y no en un CheckBox porque el estado de un actuador siempre tiene que verse.

```
cuando UrsPahoMqttClient1 .ConnectionStateChanged   (NewState, StateString)
ejecutar
    ✅ poner LblEstado . Texto  como  tomar StateString

    ✅ si  tomar NewState  =  2
       entonces
           ✅ poner CONECTADO    . Visible  como  verdadero
           ⚠️ poner DESCONECTADO . Visible  como  falso            ← AGREGAR esta línea

           🆕 llamar UrsPahoMqttClient1 .Subscribe
                  Topic  tomar global topicBombaEstado
                  QoS    0
           🆕 llamar UrsPahoMqttClient1 .Subscribe
                  Topic  tomar global topicAireEstado
                  QoS    0
           🆕 llamar UrsPahoMqttClient1 .Subscribe
                  Topic  tomar global topicLucesEstado
                  QoS    0
           🆕 llamar UrsPahoMqttClient1 .Subscribe
                  Topic  tomar global topicCamEvento
                  QoS    0
           💡 llamar UrsPahoMqttClient1 .Subscribe                  ← solo con video
                  Topic  tomar global topicCamEstado
                  QoS    0

       si no (else)
           ✅ poner CONECTADO    . Visible  como  falso
           ⚠️ poner DESCONECTADO . Visible  como  verdadero         ← AGREGAR esta línea
```

**Por qué suscribirse recién cuando `NewState = 2`:** antes de estar conectado, un `Subscribe` falla. Y como los estados de los actuadores se publican **retenidos**, apenas la app se suscribe el broker le entrega el último estado conocido: los botones muestran la situación real sin esperar a que alguien los toque.

**Truco para armar las cuatro suscripciones:** armá la primera, clic derecho → **Duplicar**, y en la copia cambiá solo la variable del tópico.

---

## 6. `cuando UrsPahoMqttClient1.ErrorGenerado`

```
cuando UrsPahoMqttClient1 .ErrorGenerado   (ActionName, ErrorCode, ErrorMessage)
ejecutar
    ✅ poner LblEstado . Texto  como  unir  "Error: "        ← ⚠️ agregar ": " al texto
                                             tomar ErrorMessage
```

Es solo cosmético: con `"Error: "` el mensaje se lee `Error: Connection refused` en lugar de `ErrorConnection refused`.

---

## 7. Los cuatro CheckBox `.Cambiado`

### ⚠️ REVISAR — el mismo arreglo en los cuatro

Hoy están así:

```
cuando subscripcionPlanta .Cambiado
ejecutar
    si  subscripcionPlanta . Habilitado        ← ⚠️ PROBLEMA
    entonces
        llamar UrsPahoMqttClient1 .Subscribe
            Topic  tomar global topicPlanta
            QoS    0
    si no (else)
        llamar UrsPahoMqttClient1 .Unsubscribe
            Topic  tomar global topicPlanta
```

`Habilitado` indica si la casilla se puede tocar, y siempre vale verdadero. Por eso la app **nunca** entra al `else` y nunca se desuscribe: destildar vuelve a suscribir.

**Cómo corregirlo:** en el bloque `subscripcionPlanta . Habilitado`, abrí el desplegable de la propiedad (donde dice `Habilitado`) y elegí **`Marcado`**. No hay que rearmar nada, es un cambio en el desplegable.

Queda así:

```
cuando subscripcionPlanta .Cambiado
ejecutar
    si  subscripcionPlanta . Marcado            ← ✅ corregido
    entonces
        llamar UrsPahoMqttClient1 .Subscribe
            Topic  tomar global topicPlanta
            QoS    0
    si no (else)
        llamar UrsPahoMqttClient1 .Unsubscribe
            Topic  tomar global topicPlanta
```

Repetí el cambio en `subscripcionOficina`, `subscripcionAlmacen` y `subscripcionTanque`.

**Cómo verificar que quedó bien:** con el log de Mosquitto abierto, tildá y destildá una casilla. Tenés que ver una línea de `SUBSCRIBE` y después una de `UNSUBSCRIBE`. Si solo aparecen `SUBSCRIBE`, quedó alguna con `Habilitado`.

---

## 8. `cuando UrsPahoMqttClient1.MensajeRecibido`

Es el bloque central. Se mantiene todo lo que tenés y se agregan **cuatro bloques `si` nuevos al final**, uno por actuador.

### 8.1 ✅ YA ESTÁ — decodificación del JSON

```
cuando UrsPahoMqttClient1 .MensajeRecibido   (Topic, Payload, Message, RetainFlag, DupFlag)
ejecutar
    poner global datos  a  llamar Web1 .DecodificarTextoJson
                               textoJson  tomar Message

    poner global valor  a  buscar por parejas  clave         "v"
                                               parejas       tomar global datos
                                               noEncontrado  "0"

    poner global marca  a  buscar por parejas  clave         "ts"
                                               parejas       tomar global datos
                                               noEncontrado  "0"
```

Estas tres líneas sirven para **todos** los mensajes, también los de los actuadores, porque todas las placas publican en el mismo formato JSON. Para un actuador, `global valor` va a contener `"ON"`, `"OFF"` o `"ESPERA"`.

### 8.2 ✅ YA ESTÁ — las cuatro zonas de temperatura

```
    si  tomar Topic  =  "empresa/planta/temperatura"        ← ⚠️ ver nota
    entonces
        poner indicadorPlanta . Texto  como  tomar global valor
        llamar DatosPlanta .AñadirEntrada
            x  tomar global marca
            y  tomar global valor

    si  tomar Topic  =  "empresa/oficina/temperatura"
    entonces
        poner indicadorOficina . Texto  como  tomar global valor
        llamar DatosOficina .AñadirEntrada
            x  tomar global marca
            y  tomar global valor

    si  tomar Topic  =  "empresa/almacen/temperatura"
    entonces
        poner indicadorAlmacen . Texto  como  tomar global valor
        llamar DatosAlmacen .AñadirEntrada
            x  tomar global marca
            y  tomar global valor

    si  tomar Topic  =  "empresa/tanque/temperatura"
    entonces
        poner indicadorTanque . Texto  como  tomar global valor
        llamar DatosTanque .AñadirEntrada
            x  tomar global marca
            y  tomar global valor
```

**⚠️ Nota recomendada:** a la derecha del `=` tenés el tópico escrito como texto. Funciona, pero el mismo tópico queda escrito dos veces: en la variable global (que usa el CheckBox para suscribirse) y acá. Si algún día lo cambiás en un solo lugar, la suscripción y la comparación dejan de coincidir y la zona se queda en silencio sin dar error.

Para evitarlo, reemplazá cada texto por su variable: `tomar global topicPlanta`, `tomar global topicOficina`, etc. **Ojo:** tiene que ser el bloque naranja `tomar global`, no un bloque de texto rosa que diga `topicPlanta`.

### 8.3 🆕 AGREGAR — Bomba

Pegalo **debajo** del último `si` de temperatura, dentro del mismo `ejecutar`.

```
🆕  si  tomar Topic  =  tomar global topicBombaEstado
    entonces
        poner global estadoBomba  a  tomar global valor
        poner btbBomba . Texto  como  unir  "Bomba "
                                           tomar global valor
        si  tomar global valor  =  "ON"
        entonces
            poner btbBomba . ColorDeFondo  como  Verde
        si no (else)
            poner btbBomba . ColorDeFondo  como  Rojo
```

### 8.4 🆕 AGREGAR — Aire

El aire tiene un tercer estado, `ESPERA`, que aparece cuando el firmware bloquea el encendido para proteger el compresor. Por eso lleva un `si` anidado adentro del `else`.

```
🆕  si  tomar Topic  =  tomar global topicAireEstado
    entonces
        poner global estadoAire  a  tomar global valor
        poner btbBomba4 . Texto  como  unir  "Aire "
                                            tomar global valor
        si  tomar global valor  =  "ON"
        entonces
            poner btbBomba4 . ColorDeFondo  como  Verde
        si no (else)
            si  tomar global valor  =  "ESPERA"
            entonces
                poner btbBomba4 . ColorDeFondo  como  Naranja
            si no (else)
                poner btbBomba4 . ColorDeFondo  como  Rojo
```

### 8.5 🆕 AGREGAR — Luces

```
🆕  si  tomar Topic  =  tomar global topicLucesEstado
    entonces
        poner global estadoLuces  a  tomar global valor
        poner btbBomba3 . Texto  como  unir  "Luces "
                                            tomar global valor
        si  tomar global valor  =  "ON"
        entonces
            poner btbBomba3 . ColorDeFondo  como  Verde
        si no (else)
            poner btbBomba3 . ColorDeFondo  como  Rojo
```

### 8.6 🆕 AGREGAR — Foto de la cámara

La cámara publica un evento cada vez que saca una foto (por el botón o sola, cuando se enciende el aire o la bomba). El evento trae la URL de la imagen en la clave `"url"`.

```
🆕  si  tomar Topic  =  tomar global topicCamEvento
    entonces
        poner imgFoto . Imagen  como  buscar por parejas  clave         "url"
                                                          parejas       tomar global datos
                                                          noEncontrado  ""
```

La foto aparece sola en `imgFoto` cada vez que llega un evento, sin tocar nada.

### 8.7 💡 OPCIONAL — URL del video

Solo si agregaste el video. La cámara publica su propia dirección, así que la app nunca necesita que tipees la IP de la cámara.

```
💡  si  tomar Topic  =  tomar global topicCamEstado
    entonces
        poner global urlPaginaCam  a  buscar por parejas  clave         "pagina"
                                                          parejas       tomar global datos
                                                          noEncontrado  ""
```

### Dónde salen los colores

**Bloques → Colores**. Si no aparece Naranja entre los predefinidos, usá el bloque `crear color` con los valores `255, 165, 0`.

---

## 9. Botones de los actuadores

### El criterio de diseño

Ningún botón cambia su propio texto ni su color al tocarlo. **Solo publica el comando** y espera la confirmación de la placa, que llega por `MensajeRecibido` (sección 8). Si la placa está apagada o el relé falla, el botón no muestra un estado falso.

Los tres botones con estado deciden qué mandar mirando la variable global que actualizó la placa: si está `ON` mandan `OFF`, y en cualquier otro caso mandan `ON`.

### 9.1 🆕 AGREGAR — Bomba

```
🆕 cuando btbBomba .Clic
   ejecutar
       si  tomar global estadoBomba  =  "ON"
       entonces
           llamar UrsPahoMqttClient1 .Publish
               Topic    tomar global topicBombaCmd
               Message  "OFF"
       si no (else)
           llamar UrsPahoMqttClient1 .Publish
               Topic    tomar global topicBombaCmd
               Message  "ON"
```

### 9.2 🆕 AGREGAR — Aire

```
🆕 cuando btbBomba4 .Clic
   ejecutar
       si  tomar global estadoAire  =  "ON"
       entonces
           llamar UrsPahoMqttClient1 .Publish
               Topic    tomar global topicAireCmd
               Message  "OFF"
       si no (else)
           llamar UrsPahoMqttClient1 .Publish
               Topic    tomar global topicAireCmd
               Message  "ON"
```

Si el estado está en `ESPERA`, el botón vuelve a mandar `ON`. La placa ya tiene el pedido registrado y lo ignora, así que no genera ningún problema.

### 9.3 🆕 AGREGAR — Luces

```
🆕 cuando btbBomba3 .Clic
   ejecutar
       si  tomar global estadoLuces  =  "ON"
       entonces
           llamar UrsPahoMqttClient1 .Publish
               Topic    tomar global topicLucesCmd
               Message  "OFF"
       si no (else)
           llamar UrsPahoMqttClient1 .Publish
               Topic    tomar global topicLucesCmd
               Message  "ON"
```

> **⚠️ Importante:** hoy **ninguna placa escucha** `empresa/oficina/luces/cmd`. El bloque va a publicar correctamente (lo vas a ver en MQTT Explorer), pero no se va a encender nada y el botón no va a cambiar de color, porque nadie responde en `empresa/oficina/luces/estado`. Hace falta un segundo relé en alguna placa. La opción más directa es sumarlo a la ESP32 del aire, en el GPIO27.

### 9.4 🆕 AGREGAR — Cámara (sacar foto)

La cámara no tiene estado ON/OFF: cada toque pide una foto.

```
🆕 cuando btbBomba2 .Clic
   ejecutar
       llamar UrsPahoMqttClient1 .Publish
           Topic    tomar global topicCamCmd
           Message  "FOTO"
```

La foto no se muestra acá: llega un instante después como evento y la muestra el bloque de la sección 8.6.

### 9.5 💡 OPCIONAL — Ver video

```
💡 cuando btnVideo .Clic
   ejecutar
       llamar webCamara .IrAUrl
           url  tomar global urlPaginaCam
```

Se usa la **página** (`http://IP/`) y no el stream crudo (`http://IP:81/stream`), porque el VisorWeb de Android muestra mucho mejor el video envuelto en HTML.

### Otros comandos disponibles de la cámara

Con el mismo bloque `Publish` sobre `topicCamCmd` podés mandar `FLASH_ON`, `FLASH_OFF` o `STATUS`, por si querés sumar un botón de flash más adelante.

---

## 10. Resumen final y orden de trabajo

### Cantidad de grupos de bloques

| Grupo | Hoy | 🆕 A sumar | 💡 Opcional | Total |
|---|---|---|---|---|
| Variables globales | 7 | 11 | 2 | 18 – 20 |
| `Screen1.Inicializar` | 1 | — | — | 1 |
| `btbEnviar.Clic` | 1 | — | — | 1 |
| `ConnectionStateChanged` | 1 | *(se amplía)* | — | 1 |
| `ErrorGenerado` | 1 | — | — | 1 |
| `MensajeRecibido` | 1 | *(se amplía)* | — | 1 |
| CheckBox `.Cambiado` | 4 | — | — | 4 |
| Botones `.Clic` | 0 | 4 | 1 | 4 – 5 |
| **Total de grupos** | **16** | | | **31 – 34** |

### Orden recomendado

1. **Corregir primero lo existente** — CheckBox a `Marcado` (sección 7), imagen `DESCONECTADO` (sección 5), `XFromZero` (sección 1.4). Probá que la app siga funcionando igual antes de sumar nada.
2. **Bomba** — variables, suscripción, `si` en `MensajeRecibido`, botón. Probala completa con la ESP8266 antes de seguir.
3. **Aire** — mismo recorrido con la ESP32. Probá el estado `ESPERA` tocando OFF y ON seguido.
4. **Cámara** — componente `imgFoto`, variables, suscripción, `si` y botón.
5. **Luces** — dejalo para cuando haya una placa que lo reciba.

Ir de a un actuador por vez te deja aislar rápido dónde está cualquier problema: si la bomba anda y el aire no, el error está en lo que agregaste para el aire.

### Cómo probar cada actuador sin la app

Desde **MQTT Explorer**, publicá `ON` en el tópico `…/cmd`. Tenés que escuchar el relé y ver aparecer el `…/estado` en el árbol. Si eso funciona y la app no, el problema está en los bloques; si eso tampoco funciona, está en el firmware o en el cableado.

### Tabla de referencia de tópicos

| Actuador | Botón | Comando (app → placa) | Estado (placa → app) | Placa |
|---|---|---|---|---|
| Bomba | `btbBomba` | `empresa/tanque/rele/cmd` | `empresa/tanque/rele/estado` | ESP8266 |
| Aire | `btbBomba4` | `empresa/planta/aire/cmd` | `empresa/planta/aire/estado` | ESP32 |
| Luces | `btbBomba3` | `empresa/oficina/luces/cmd` | `empresa/oficina/luces/estado` | *pendiente* |
| Cámara | `btbBomba2` | `empresa/camara/cmd` | `empresa/camara/evento` | ESP32-CAM |

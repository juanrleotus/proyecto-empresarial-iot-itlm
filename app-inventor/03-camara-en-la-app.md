# Sugerencias de corrección — App `ESP32_Conexion`

Revisión del `.aia` del 21/09/2026 · 48 grupos de bloques · foco en la **cámara**

---

## Resumen

La app avanzó bastante: los CheckBox ya usan `Marcado`, las comparaciones de `MensajeRecibido` usan las variables globales, aparecieron procedimientos y el panel de actuadores está completo.

Lo que no funciona es, sobre todo, la parte de la cámara. Hay **un problema de fondo** (el componente elegido no puede mostrar imágenes remotas) y varios bloques con la lógica anidada en el lugar equivocado.

También hay cosas fuera de la cámara que conviene arreglar: dos botones sin ningún bloque, colores que nunca cambian y bloques sueltos flotando.

### Prioridades

| Prioridad | Qué | Sección |
|---|---|---|
| 🔴 1 | La cámara no puede mostrar la foto: componente equivocado | [1.1](#11-el-problema-de-fondo-imgfoto-es-un-selectordeimagen) |
| 🔴 2 | El bloque de la foto está anidado donde nunca se ejecuta | [1.2](#12-el-bloque-de-la-foto-está-dentro-del-si-equivocado) |
| 🔴 3 | Falta `btbBomba.Clic` y `btnLuces.Clic` | [3.1](#31-faltan-dos-eventos-de-botón) |
| 🟠 4 | URL del video mal armada | [1.4](#14-la-url-del-video) |
| 🟠 5 | `estadoBomba` y `estadoLuces` guardan la marca de tiempo | [3.2](#32-estadobomba-y-estadoluces-guardan-el-dato-equivocado) |
| 🟠 6 | Los colores nunca cambian: verde en las dos ramas | [3.3](#33-los-colores-nunca-cambian) |
| 🟡 7 | `PublishBinary` en vez de `Publish` | [3.4](#34-publishbinary-no-es-el-bloque-correcto) |
| 🟡 8 | La temperatura del tanque nunca se grafica | [3.5](#35-el-tanque-no-se-grafica) |
| 🟡 9 | Bloques sueltos sin conectar | [4](#4-bloques-sueltos) |

---

# 1. La cámara

## 1.1 El problema de fondo: `imgFoto` es un SelectorDeImagen

En el Diseñador, `imgFoto` es un **SelectorDeImagen** (`ImagePicker`). Ese componente **no sirve para mostrar fotos**: es un botón que, al tocarlo, abre la galería del teléfono para que el usuario elija una imagen. Su propiedad `Imagen` es el dibujo de fondo del botón y espera un archivo local del proyecto, no una dirección web.

Por eso la foto nunca aparece, aunque el resto estuviera bien.

### Cómo corregirlo

1. En el Diseñador, seleccioná `imgFoto` y **eliminalo**.
2. De **Paleta → Interfaz de usuario**, arrastrá un componente **Imagen** dentro de `panelCamaras`.
3. Nombralo `imgFoto` (el mismo nombre, así los bloques lo reconocen).
4. Propiedades: `Alto` = *Ajustar al contenedor*, `Ancho` = *Ajustar al contenedor*.

La propiedad que hay que escribir desde los bloques pasa a llamarse **`Imagen`** (en inglés `Picture`). Al rearmar el bloque, elegila del desplegable.

> **Nota sobre el contenedor:** `panelCamaras` es un `DisposiciónHorizontalDesplazable`. Funciona, pero si más adelante querés guardar un historial de varias fotos, ese contenedor es justo el indicado: podés ir agregando componentes Imagen y desplazarlos de costado.

## 1.2 El bloque de la foto está dentro del `si` equivocado

Hoy el bloque que muestra la foto está **anidado adentro** del `si` que atiende `topicCamEstado`:

```
si  tomar Topic  =  tomar global topicCamEstado          ← entra solo si es el ESTADO
entonces
    poner global urlPaginaCam  a  ...
    poner global estadoFlash   a  ...
    poner btnFlash . Texto     como ...

    si  tomar Topic  =  tomar global topicCamEvento      ← ⛔ nunca puede ser verdadero
    entonces
        poner imgFoto . Imagen  como  buscar por parejas "url" ...
```

Un mensaje llega de **un solo** tópico. Si ya entró porque es `empresa/camara/estado`, es imposible que además sea `empresa/camara/evento`. Ese `si` interno nunca se cumple, así que la foto nunca se asigna.

### Cómo corregirlo

Sacá ese `si` interno y dejalo como un bloque independiente, al mismo nivel que los demás:

```
si  tomar Topic  =  tomar global topicCamEvento
entonces
    poner imgFoto . Imagen  como  buscar por parejas  clave         "url"
                                                      parejas       tomar global datos
                                                      noEncontrado  ""
```

Arrastralo fuera del bloque que lo contiene y soltalo abajo, encadenado con los otros `si`.

## 1.3 El `si` con el bloque `o` hay que eliminarlo

Existe este bloque:

```
si  tomar Topic  =  ( tomar global topicCamEstado  o  tomar global topicCamEvento )
entonces
    si  tomar global valor = "ON"  →  btnCamara.ColorDeFondo = Verde
    si no                          →  btnCamara.ColorDeFondo = Verde
```

Tiene dos problemas:

- El bloque `o` devuelve **verdadero/falso**, no un texto. La comparación queda «¿el tópico es igual a *verdadero*?», que siempre da falso.
- Adentro compara `valor = "ON"`, pero la cámara no tiene estado ON/OFF. Y en las dos ramas pone verde, así que aunque funcionara no cambiaría nada.

**Eliminá este bloque completo.** No hace falta reemplazarlo: el botón `FOTO` no necesita color de estado.

> Si quisieras comparar contra dos tópicos, la forma correcta sería:
> `( Topic = topicCamEstado )  o  ( Topic = topicCamEvento )` — con **dos comparaciones completas** unidas por el `o`, no el `o` adentro de una sola comparación.

## 1.4 La URL del video

Dos lugares tienen la dirección mal armada:

| Dónde | Ahora | Problema |
|---|---|---|
| `imgStream.HomeUrl` (Diseñador) | `http://192.168.1.33:81` | El puerto 81 **solo** responde en `/stream`. La raíz de ese puerto no existe → error 404 |
| `global urlPaginaCam` | `http://192.168.1.33:81/stream` | Es el video crudo. El VisorWeb de Android lo muestra mal en muchos equipos |

La dirección correcta para el VisorWeb es la **página** en el puerto 80: `http://192.168.1.33/`. Esa página trae el video envuelto en HTML y se ve en muchos más celulares.

### Cómo corregirlo

1. **Diseñador:** seleccioná `imgStream` y **dejá `HomeUrl` vacío**. La URL la va a cargar la app sola.
2. **Bloques:** cambiá el valor inicial de `global urlPaginaCam` a texto vacío `""`.

No hace falta escribir ninguna IP a mano: el firmware de la ESP32-CAM publica su propia dirección en `empresa/camara/estado`, bajo la clave `pagina`, y el bloque de la sección siguiente la guarda en `urlPaginaCam`. Si en el instituto el hotspot le da otra IP, la app se adapta sola.

## 1.5 El `noEncontrado` de la página

En el `si` de `topicCamEstado`:

```
poner global urlPaginaCam  a  buscar por parejas  clave         "pagina"
                                                  parejas       tomar global datos
                                                  noEncontrado  "ON"     ← ⛔
```

Si por algún motivo faltara la clave `pagina`, `urlPaginaCam` quedaría valiendo `"ON"` y el VisorWeb intentaría abrir una dirección llamada «ON».

**Cambiá `"ON"` por texto vacío `""`.**

## 1.6 Cómo debe quedar la cámara

Tres bloques independientes dentro de `MensajeRecibido`, ninguno anidado dentro de otro:

```
si  tomar Topic  =  tomar global topicCamEvento
entonces
    poner imgFoto . Imagen  como  buscar por parejas  clave         "url"
                                                      parejas       tomar global datos
                                                      noEncontrado  ""


si  tomar Topic  =  tomar global topicCamEstado
entonces
    poner global urlPaginaCam  a  buscar por parejas  clave         "pagina"
                                                      parejas       tomar global datos
                                                      noEncontrado  ""

    poner global estadoFlash   a  buscar por parejas  clave         "flash"
                                                      parejas       tomar global datos
                                                      noEncontrado  "OFF"

    poner btnFlash . Texto  como  unir  "FLASH: "        ← agregar ": "
                                        tomar global estadoFlash
```

Y los dos botones, que ya están bien salvo por el detalle de `PublishBinary` (sección 3.4):

```
cuando btnCamara .Clic
ejecutar
    llamar UrsPahoMqttClient1 .Publish
        Topic    tomar global topicCamCmd
        Message  "FOTO"

cuando btnVIdeo .Clic
ejecutar
    llamar imgStream .IrAUrl
        url  tomar global urlPaginaCam
```

## 1.7 Cómo probar la cámara por partes

Esto separa los problemas de firmware de los de la app:

1. **Navegador de la PC** → `http://IP_CAMARA/` debe mostrar el video. Si no, el problema es del firmware o de la alimentación, no de la app.
2. **MQTT Explorer** → abrí `empresa/camara/estado`. Tiene que estar ahí (es retenido) con la clave `pagina`.
3. **MQTT Explorer** → publicá `FOTO` en `empresa/camara/cmd`. Tiene que aparecer un mensaje nuevo en `empresa/camara/evento` con una clave `url`. Copiá esa URL y abrila en el navegador: si ahí se ve la foto, la cámara está perfecta.
4. **Recién ahí**, probá la app.

Si el paso 3 funciona y en la app no aparece nada, el problema está en los bloques de la sección 1.2 o en el componente de la sección 1.1.

---

# 2. Detalle de cada problema de la cámara, en orden

| # | Qué hacer | Dónde |
|---|---|---|
| 1 | Reemplazar `imgFoto` (SelectorDeImagen) por un componente **Imagen** con el mismo nombre | Diseñador |
| 2 | Vaciar `HomeUrl` de `imgStream` | Diseñador |
| 3 | Sacar el `si` de la foto afuera del `si` del estado | `MensajeRecibido` |
| 4 | Eliminar el `si` que usa el bloque `o` | `MensajeRecibido` |
| 5 | Cambiar `noEncontrado` de `"ON"` a `""` en la clave `pagina` | `MensajeRecibido` |
| 6 | Cambiar el valor inicial de `urlPaginaCam` a `""` | Variables globales |
| 7 | Agregar `": "` al texto del flash | `MensajeRecibido` |
| 8 | Cambiar `PublishBinary` por `Publish` en `btnCamara` | `btnCamara.Clic` |

---

# 3. Fuera de la cámara

## 3.1 Faltan dos eventos de botón

En el proyecto **no existe** `btbBomba.Clic` ni `btnLuces.Clic`. Los botones están en pantalla pero no hacen nada al tocarlos.

```
cuando btbBomba .Clic
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


cuando btnLuces .Clic
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

> **Recordá:** todavía no hay ninguna placa escuchando `empresa/oficina/luces/cmd`. El botón de luces va a publicar bien (lo vas a ver en MQTT Explorer) pero no va a encender nada ni cambiar de color hasta que exista ese firmware.

## 3.2 `estadoBomba` y `estadoLuces` guardan el dato equivocado

```
si  tomar Topic  =  tomar global topicBombaEstado
entonces
    poner global estadoBomba  a  tomar global marca      ← ⛔ marca = "ts"
```

`global marca` es la marca de tiempo, no el estado. El botón de la bomba va a comparar un número contra `"ON"` y nunca va a coincidir.

**Cambiá `tomar global marca` por `tomar global valor`** en los dos casos: bomba y luces.

## 3.3 Los colores nunca cambian

En todos los bloques de color, las dos ramas ponen **verde**:

```
si  tomar global valor = "ON"
entonces   poner btbBomba . ColorDeFondo  como  Verde
si no      poner btbBomba . ColorDeFondo  como  Verde     ← ⛔ debería ser Rojo
```

Pasa igual en `btnLuces`, `btnCamara` y en las tres ramas de `btbAire` (ON / ESPERA / OFF, las tres en verde).

### Cómo debe quedar

| Botón | ON | ESPERA | OFF |
|---|---|---|---|
| `btbBomba` | Verde | — | Rojo |
| `btnLuces` | Verde | — | Rojo |
| `btbAire` | Verde | Naranja | Rojo |

Si el bloque **Naranja** no está entre los colores predefinidos, usá `crear color` con la lista `255, 165, 0`.

## 3.4 `PublishBinary` no es el bloque correcto

`btbAire.Clic` y `btnCamara.Clic` usan **`PublishBinary`**. Ese bloque está pensado para enviar datos binarios (su parámetro se llama `BinaryMessage`), no texto común.

Lo verifiqué en la definición de la extensión, que tiene cuatro variantes:

| Bloque | Parámetros | Para qué |
|---|---|---|
| **`Publish`** | `Topic`, `Message` | **El que hay que usar acá** |
| `PublishEx` | `Topic`, `Message`, `RetainFlag`, `QoS` | Cuando hace falta retener el mensaje |
| `PublishBinary` | `Topic`, `BinaryMessage`, `RetainFlag`, `QoS` | Datos binarios |
| `PublishByteArray` | `Topic`, `ByteArray`, `RetainFlag`, `QoS` | Arreglos de bytes |

**Reemplazá `PublishBinary` por `Publish`** en los dos botones. `btnFlash` ya lo usa bien, podés copiarlo de ahí.

## 3.5 El tanque no se grafica

```
si  tomar Topic  =  tomar global topicTanque
entonces
    poner indicadorTanque . Texto  como  tomar global valor
    si  tomar global valor  =  "ON"        ← ⛔ es una temperatura, nunca vale "ON"
    entonces
        llamar DatosTanque .AñadirEntrada ...
```

El indicador se actualiza, pero el gráfico del tanque queda siempre vacío porque una temperatura nunca es igual a `"ON"`.

**Eliminá ese `si` interno** y dejá el `AñadirEntrada` directo, como en las otras tres zonas.

## 3.6 El botón del aire decide con la variable equivocada

```
cuando btbAire .Clic
ejecutar
    si  tomar global valor  =  "ON"        ← ⛔ debería ser global estadoAire
```

`global valor` cambia con **cada** mensaje que llega: si el último fue la temperatura de la oficina, vale un número. El botón terminaría decidiendo según un dato que no tiene nada que ver con el aire.

**Cambiá `tomar global valor` por `tomar global estadoAire`.** En `btnFlash` ya está bien resuelto: usa `global estadoFlash`.

---

# 4. Bloques sueltos

Hay **12 bloques flotando** fuera de cualquier evento. No se ejecutan nunca y generan advertencias:

| Cantidad | Qué son |
|---|---|
| 6 | Llamadas a `AccionSubscripcion` y `AccionBoton` sin conectar |
| 6 | Bloques de texto con tópicos (`empresa/planta/temperatura`, etc.) |

Los bloques de texto parecen restos de cuando reemplazaste las comparaciones por variables globales. **Borralos**, arrastrándolos al tacho.

## Sobre los procedimientos

Creaste dos procedimientos y están bien pensados: evitan repetir la misma lógica cuatro veces. Pero hoy **ninguno está en uso**: los eventos tienen la lógica escrita directamente adentro, y las llamadas a los procedimientos quedaron sueltas.

Tenés dos caminos:

**A — Borrar los procedimientos y sus llamadas sueltas.** La app queda igual de funcional, con la lógica repetida pero explícita. Es más fácil de seguir para los estudiantes.

**B — Usarlos de verdad.** Reemplazar el contenido de cada evento por la llamada al procedimiento. Por ejemplo:

```
cuando subscripcionPlanta .Cambiado
ejecutar
    llamar AccionSubscripcion
        Topic              tomar global topicPlanta
        CheckSubscripcion  subscripcionPlanta . Marcado
```

`AccionSubscripcion` está correcto y funcionaría tal cual está.

> **`AccionBoton` sí tiene un error de lógica.** Compara `globalEstado = unir(mensaje, "ON")`. Para el flash eso da `"FLASH_ON"`, pero `estadoFlash` vale `"ON"` a secas, así que nunca coincide. Si elegís el camino B, ese procedimiento hay que rehacerlo: debería recibir el estado y el tópico, comparar el estado contra `"ON"` a secas, y publicar `"ON"` u `"OFF"`. El caso del flash es distinto a los demás (usa `FLASH_ON` / `FLASH_OFF`), así que conviene dejarlo con su propio bloque.

Para avanzar rápido, el camino A alcanza. El B queda como mejora cuando el sistema ya esté andando entero.

---

# 5. Orden de trabajo sugerido

1. **Limpiar** — borrar los 12 bloques sueltos (sección 4). Deja la pantalla de bloques legible y el contador de advertencias en cero.
2. **Cámara** — los 8 pasos de la sección 2. Probá con el navegador antes que con la app.
3. **Botones faltantes** — `btbBomba.Clic` y `btnLuces.Clic` (sección 3.1).
4. **Correcciones de datos** — `estadoBomba` / `estadoLuces` (3.2), botón del aire (3.6), gráfico del tanque (3.5).
5. **Colores** (3.3) y **`Publish`** (3.4). Son los últimos porque son cosméticos o no impiden que funcione.

Después de cada paso, probá antes de seguir. Si algo deja de andar, sabés exactamente qué tocaste.

---

# 6. Tabla de referencia

| Actuador | Botón | Comando (app → placa) | Estado (placa → app) | Placa |
|---|---|---|---|---|
| Aire | `btbAire` | `empresa/planta/aire/cmd` | `empresa/planta/aire/estado` | ESP32 |
| Bomba | `btbBomba` | `empresa/tanque/rele/cmd` | `empresa/tanque/rele/estado` | ESP8266 |
| Luces | `btnLuces` | `empresa/oficina/luces/cmd` | `empresa/oficina/luces/estado` | *pendiente* |
| Foto | `btnCamara` | `empresa/camara/cmd` → `FOTO` | `empresa/camara/evento` (clave `url`) | ESP32-CAM |
| Flash | `btnFlash` | `empresa/camara/cmd` → `FLASH_ON` / `FLASH_OFF` | `empresa/camara/estado` (clave `flash`) | ESP32-CAM |
| Video | `btnVIdeo` | — | `empresa/camara/estado` (clave `pagina`) | ESP32-CAM |

**Direcciones de la ESP32-CAM:**

| Para | URL |
|---|---|
| VisorWeb de la app | `http://IP_CAMARA/` |
| Video crudo (navegador) | `http://IP_CAMARA:81/stream` |
| Foto del momento | `http://IP_CAMARA/capture` |
| Última foto guardada | `http://IP_CAMARA/foto` |

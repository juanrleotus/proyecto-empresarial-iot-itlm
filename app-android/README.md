# App nativa — Android Studio

Versión nativa de la app, en Kotlin. Hace lo mismo que la de App Inventor, pero
con control total de la interfaz: botones con estado real, imagen de la cámara
que se refresca y video MJPEG embebido.

| Archivo | Contenido |
|---|---|
| `ProyectoIoT_ITLM_AndroidStudio.zip` | proyecto completo, listo para abrir |

## Abrir el proyecto

1. Descomprimir el ZIP
2. Android Studio → **Open** → elegir la carpeta `DomoticaITLM` (la que tiene `settings.gradle.kts`)
3. Esperar el *Gradle Sync*
4. Conectar el celular con **Depuración USB** activada → **Run**
5. La **IP del broker se escribe en la app**, en el campo de arriba de la
   pantalla: no hay que recompilar para cambiarla

Estructura del proyecto:

| Archivo | Contenido |
|---|---|
| `MainActivity.kt` | pantalla, botones, imagen y video |
| `MqttManager.kt` | cliente Paho: conexión, suscripciones, publicación |
| `Topicos.kt` | **todos los tópicos en un solo lugar** |
| `res/xml/network_security_config.xml` | permiso de tráfico sin cifrar |

`Topicos.kt` usa exactamente los mismos tópicos que el firmware y que Node-RED.
Si se cambia uno, se cambia ahí y en ningún otro lado del código Android.

## Requisitos

| Elemento | Versión |
|---|---|
| Android Studio | Ladybug o posterior |
| JDK | **17** |
| Gradle | 8.7 (o 8.14 si se usa un JDK más nuevo) |
| minSdk | 24 (Android 7) |

## Bibliotecas

| Biblioteca | Para qué |
|---|---|
| `org.eclipse.paho:org.eclipse.paho.client.mqttv3` | cliente MQTT |
| `io.coil-kt:coil` | carga de la imagen de la cámara |

> Se usa el cliente **Java** de Paho, no `paho.android.service`, que está
> discontinuado y falla en Android 12 y posteriores por el requisito de
> `PendingIntent.FLAG_IMMUTABLE`.

## Autenticación del broker

Para el **MODO B**, en `MqttManager.kt` se agregan dos constantes y tres líneas
en las opciones de conexión.

Arriba de la clase:

```kotlin
// MODO A (broker sin autenticacion): dejar los dos vacios.
// MODO B (broker con usuario y clave): completarlos.
private const val MQTT_USUARIO = ""
private const val MQTT_CLAVE   = ""
```

Y dentro de `conectar()`, en el bloque `opciones`:

```kotlin
val opciones = MqttConnectOptions().apply {
    isCleanSession = true
    connectionTimeout = 10
    keepAliveInterval = 60
    isAutomaticReconnect = true

    // Un solo codigo para los dos modos
    if (MQTT_USUARIO.isNotEmpty()) {
        userName = MQTT_USUARIO
        password = MQTT_CLAVE.toCharArray()
    }
}
```

Mismo criterio que el firmware: si el usuario está vacío, se conecta de forma
anónima. El usuario que corresponde a la app es `appmovil`, según la ACL de
`broker/aclfile.ejemplo`.

> Una contraseña escrita en el código Kotlin queda dentro del `.apk`, y un
> `.apk` se puede descompilar. Para un trabajo de taller está bien; para una
> instalación real, la clave se pide al usuario y se guarda cifrada con
> `EncryptedSharedPreferences`. Y en cualquier caso, **sin TLS la clave viaja en
> claro por la red**: ver `docs/02-seguridad-y-credenciales.md` § 5.

## Tráfico sin cifrar

Android 9 y posteriores bloquean HTTP y TCP en claro. El proyecto incluye
`res/xml/network_security_config.xml` y lo referencia desde el manifiesto:

```xml
<application
    android:networkSecurityConfig="@xml/network_security_config"
    ... >
```

Sin eso, la conexión al broker y la imagen de la cámara fallan **sin mensaje de
error claro**.

## Problemas al compilar

Ver `docs/03-errores-frecuentes.md`, sección **G**. El más frecuente:

```
Incompatible Gradle JVM version
Gradle 8.7 supports Java versions between 1.8 and 21
```

Se resuelve instalando un JDK 17, o — si la red del instituto bloquea la
descarga — subiendo Gradle a 8.14 en
`gradle/wrapper/gradle-wrapper.properties`.

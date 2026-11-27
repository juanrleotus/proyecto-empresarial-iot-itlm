# Seguridad y manejo de credenciales

Este documento es **contenido de la materia**, no una formalidad
administrativa. Un sistema IoT mal asegurado es un sistema que cualquiera en la
red puede accionar.

---

## 1. La regla del repositorio

> **Ninguna contraseña se escribe en un archivo que se sube a GitHub.**

Por qué: git guarda **todo el historial**. Si se sube un archivo con una clave y
después se borra el archivo y se hace otro commit, la clave **sigue estando** en
el historial y cualquiera puede recuperarla con un comando. La única solución
real es **cambiar la contraseña**.

GitHub, además, indexa los repositorios públicos: hay programas automáticos que
buscan claves subidas por error y las encuentran en minutos.

### Qué está en `.gitignore`

```
firmware/**/credenciales.h     # claves de Wi-Fi y del broker
broker/passwd                  # hash de las contrasenas de Mosquitto
broker/*.log                   # los log pueden contener IP y nombres de usuario
.env                           # variables de entorno
flows_cred.json                # Node-RED guarda aqui las claves cifradas
scripts/registros/             # datos capturados
```

### Qué sí se sube

| Archivo | Contenido |
|---|---|
| `credenciales.ejemplo.h` | la **plantilla**, con los campos vacíos |
| `aclfile.ejemplo` | la lista de permisos (nombres de usuario, sin claves) |
| `mosquitto-*.conf` | la configuración (apunta al archivo de claves, no las contiene) |
| `flows-*.json` | los flujos de Node-RED (Node-RED no exporta las claves) |

El patrón `archivo.ejemplo` + `archivo` en `.gitignore` es el estándar de la
industria. Quien clona el repositorio copia el ejemplo, lo completa y trabaja;
sus claves nunca salen de su máquina.

---

## 2. Qué hacer si ya se subió una clave

Pasa, y hay que saber reaccionar:

1. **Cambiar la contraseña inmediatamente.** Es lo primero y lo único
   indispensable. La clave vieja hay que considerarla comprometida para siempre.
2. Borrar el archivo del repositorio y agregarlo a `.gitignore`.
3. Si el repositorio es privado y recién subido, se puede reescribir el historial
   (`git filter-repo`), pero **eso no recupera la clave**: si alguien ya la
   copió, el daño está hecho.

En el caso del Wi-Fi de una casa: cambiar la contraseña del router y volver a
conectar los dispositivos. Es media hora de trabajo y evita un problema real.

---

## 3. Los dos modos del broker

| | MODO A — sin autenticación | MODO B — con usuario y clave |
|---|---|---|
| Archivo | `mosquitto-sin-auth.conf` | `mosquitto-con-auth.conf` |
| `allow_anonymous` | `true` | `false` |
| Quién puede publicar | **cualquiera en la red** | solo usuarios creados |
| Quién puede leer | **cualquiera en la red** | solo usuarios creados |
| Control por tópico | no | sí, con `aclfile` |
| Para qué sirve | primera clase, laboratorio aislado | todo lo demás |

### Lo que se puede hacer contra un broker en MODO A

Esto conviene demostrarlo en clase. Desde cualquier PC de la red:

```cmd
:: Leer TODO lo que pasa por el sistema
mosquitto_sub -h 192.168.1.33 -t "#" -v

:: Encender la bomba del tanque sin permiso de nadie
mosquitto_pub -h 192.168.1.33 -t "empresa/tanque/rele/cmd" -m "ON"

:: Mentirle al tablero: inyectar una temperatura falsa
mosquitto_pub -h 192.168.1.33 -t "empresa/planta/temperatura" -m "{\"v\":99,\"u\":\"C\",\"ts\":0}"
```

Las tres funcionan y no dejan rastro de quién las hizo. La tercera es la más
instructiva: la medición falsa entra en PostgreSQL y aparece en Power BI como un
dato legítimo. Nadie puede distinguirla de una real.

Después de ver esto, pasar al MODO B deja de ser un trámite.

---

## 4. Principio de mínimo privilegio

La misma idea se aplica en tres lugares del proyecto:

| Pieza | Herramienta | Qué permite |
|---|---|---|
| Broker | `aclfile` | cada placa escribe **solo sus** tópicos y lee **solo su** comando |
| PostgreSQL | `GRANT` | `nodered` solo inserta; `powerbi` solo lee |
| Red | Firewall | solo los puertos 1883, 1880 y 5432 |

Ejemplo de la ACL:

```
user esp8266tanque
topic write empresa/tanque/temperatura
topic write empresa/tanque/rele/estado
topic read  empresa/tanque/rele/cmd
```

Esa placa **no puede** publicar en `empresa/planta/temperatura` ni siquiera por
un error de programación. La ACL no es solo defensa contra un atacante: es
defensa contra un error propio.

Comprobarlo:

```cmd
mosquitto_pub -h localhost -t "empresa/planta/temperatura" -m "99" -u esp8266tanque -P LA_CLAVE
```

El broker acepta la conexión y **descarta** el mensaje. En `mosquitto.log`
aparece la línea de ACL denegada.

---

## 5. Lo que este proyecto NO tiene

Honestidad técnica: hay que saber qué queda afuera.

| Falta | Consecuencia | Cómo se resuelve en la industria |
|---|---|---|
| **Cifrado (TLS)** | usuario y clave viajan en claro; cualquiera con Wireshark en la red los lee | `listener 8883` + certificados; en las placas, `WiFiClientSecure` |
| Certificados de cliente | cualquiera con la clave es "la placa" | autenticación mutua con certificado por dispositivo |
| Firma de los mensajes | un mensaje puede ser falsificado | HMAC en el payload |
| Separación de redes | las placas están en la misma red que las PC | VLAN para dispositivos IoT |
| Registro de auditoría | no se sabe quién accionó qué | log centralizado |

> **MODO B protege contra el curioso, no contra el atacante.** Sin TLS, un
> usuario y una contraseña que viajan en texto plano por el aire son un
> candado con la llave colgada al lado. Es muchísimo mejor que nada y es el
> nivel correcto para un trabajo de taller, pero una instalación real en
> producción necesita el puerto 8883 con certificados.

TLS en la ESP32 es perfectamente posible (`WiFiClientSecure` + el certificado de
la CA) y es un buen trabajo práctico final para los alumnos avanzados.

---

## 6. Cambiar una contraseña del sistema

| Qué | Dónde se cambia | Qué hay que actualizar después |
|---|---|---|
| Clave del Wi-Fi | router | `credenciales.h` de las 4 placas + recargar |
| Clave de un usuario MQTT | `mosquitto_passwd passwd <usuario>` | la pieza que usa ese usuario |
| Clave de PostgreSQL | `ALTER USER nodered WITH PASSWORD '…';` | nodo PostgreSQL en Node-RED, Power BI |

Siempre: cambiar, y después **probar la reconexión**. Una clave cambiada y no
actualizada en una placa produce `rc=4` en su monitor serie.

---

## 7. Checklist antes de hacer público el repositorio

```
[ ] No existe ningun archivo credenciales.h en el repositorio
      git ls-files | findstr credenciales.h      (no debe devolver nada)
[ ] No existe broker/passwd
[ ] Ningun .ino tiene un SSID o una clave real escrita
      git grep -niE "password|passwd|clave *=" -- "*.ino" "*.h"
[ ] Ningun .json de Node-RED tiene una clave
      git grep -ni "password" -- "*.json"
[ ] Las IP que quedan son privadas (192.168.x.x) y no identifican un lugar
[ ] El historial esta limpio: revisar los primeros commits
      git log --stat --oneline | more
```

El primer comando es el importante. Si devuelve algo, hay que detenerse y
arreglarlo antes de publicar.

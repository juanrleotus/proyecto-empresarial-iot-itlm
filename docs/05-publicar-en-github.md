# Publicar el repositorio en GitHub

El repositorio ya viene con `git` inicializado y un primer commit hecho. Solo
falta crearlo en GitHub y subirlo.

---

## 1. Antes de subir: la verificación de seguridad

**Hacerla siempre**, aunque el repositorio vaya a ser privado.

```cmd
cd proyecto-empresarial-iot-itlm

:: 1) Ningun archivo de credenciales debe estar versionado
git ls-files | findstr credenciales.h
git ls-files | findstr passwd

:: 2) Ningun SSID o clave escrito en el firmware
git grep -niE "password|passwd|clave *=" -- "*.ino" "*.h"

:: 3) Ninguna clave en los flujos de Node-RED
git grep -ni "password" -- "*.json"
```

Los tres comandos tienen que devolver **nada** (o solo comentarios y
plantillas). Si alguno devuelve una clave real, detenerse y arreglarlo antes de
subir: una vez publicada, la clave queda en el historial para siempre.

En Linux o macOS, reemplazar `findstr` por `grep`.

---

## 2. Crear el repositorio en GitHub

Desde el navegador, en <https://github.com/new>:

| Campo | Valor |
|---|---|
| Repository name | `proyecto-empresarial-iot-itlm` |
| Description | Sistema IoT con ESP32, MQTT, Node-RED, PostgreSQL y Power BI — Tecnicatura en Mecatrónica |
| Visibilidad | **Private** al principio; pasarlo a Public cuando esté revisado |
| Add a README file | **NO** marcar |
| Add .gitignore | **NO** marcar |
| Choose a license | **NO** marcar |

Las tres últimas van sin marcar porque el repositorio ya las trae: si GitHub las
crea, el primer `push` choca.

---

## 3. Subirlo

GitHub muestra los comandos al crear el repositorio. Son estos:

```cmd
cd proyecto-empresarial-iot-itlm

git remote add origin https://github.com/juanrleotus/proyecto-empresarial-iot-itlm.git
git branch -M main
git push -u origin main
```

La primera vez pide autenticación. GitHub **ya no acepta la contraseña de la
cuenta**: hay que usar un *Personal Access Token*.

### Crear el token

Settings → Developer settings → Personal access tokens → **Tokens (classic)** →
Generate new token:

| Campo | Valor |
|---|---|
| Note | `PC del instituto` |
| Expiration | 90 días |
| Scopes | marcar **`repo`** |

Copiar el token **en ese momento**: después no se vuelve a mostrar. Cuando git
pida la contraseña, pegar el token.

> El token es una contraseña. No va en ningún archivo del proyecto. Si se
> filtra, se revoca desde la misma pantalla y se genera otro.

---

## 4. Trabajo normal, después del primer push

```cmd
git status                      :: que cambio
git add .
git commit -m "Agrego el sector de secado"
git push
```

Y para traer los cambios de otra máquina:

```cmd
git pull
```

---

## 5. Cómo lo usan los alumnos

Lo clonan:

```cmd
git clone https://github.com/juanrleotus/proyecto-empresarial-iot-itlm.git
cd proyecto-empresarial-iot-itlm
```

Y lo primero que hacen, en cada carpeta de firmware, es crear su propio archivo
de credenciales:

```cmd
cd firmware\esp32_planta
copy credenciales.ejemplo.h credenciales.h
```

Después lo completan con **sus** redes y su IP de broker. Ese archivo está en
`.gitignore`, así que nunca se sube: cada alumno trabaja con sus datos y el
repositorio queda limpio.

> Vale explicarles por qué el repositorio está armado así. Es el mismo patrón
> que usan los proyectos profesionales, y la razón es concreta: el historial de
> git es permanente.

---

## 6. Si cada alumno va a entregar su versión

Dos caminos, según lo que se quiera evaluar:

**Fork.** Cada alumno hace *Fork* del repositorio en su propia cuenta y trabaja
ahí. Entrega el enlace a su fork. Sirve cuando se evalúa el trabajo individual.

**Ramas.** Se los agrega como colaboradores y cada grupo trabaja en su rama
(`git checkout -b grupo-3`). Entregan un *Pull Request*. Sirve cuando se quiere
enseñar trabajo en equipo y revisión de código, que es más cercano a la
realidad profesional.

El segundo camino da más trabajo al docente y enseña bastante más.

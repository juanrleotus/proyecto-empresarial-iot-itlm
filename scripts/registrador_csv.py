# -*- coding: utf-8 -*-
"""
============================================================
 REGISTRADOR CSV — Sistema domotico ITLM
------------------------------------------------------------
 Se suscribe a TODOS los topicos de "empresa/" y va guardando
 cada mensaje en un archivo CSV, con la fecha y hora de la PC.

 No hay que modificar el firmware de ninguna placa: el broker
 ya recibe todo, este script solo escucha y anota.

 INSTALACION (una sola vez):
     pip install paho-mqtt

 USO:
     python registrador_csv.py
     (Ctrl+C para terminar)
============================================================
"""

import csv
import json
import os
from datetime import datetime

import paho.mqtt.client as mqtt

# ── CONFIGURACION ───────────────────────────────────────────
BROKER = "localhost"        # si corre en la misma PC del broker
PUERTO = 1883
TOPICO = "empresa/#"        # el # captura todas las ramas

# ── AUTENTICACION DEL BROKER ────────────────────────────────
#  MODO A (broker sin autenticacion): dejar los dos vacios.
#  MODO B (broker con usuario y clave): NO escribir la clave
#  aqui. Pasarla por variable de entorno antes de ejecutar:
#
#    Windows (CMD):
#      set MQTT_USUARIO=docente
#      set MQTT_CLAVE=la-clave
#      python registrador_csv.py
#
#    Linux / macOS:
#      MQTT_USUARIO=docente MQTT_CLAVE=la-clave python3 registrador_csv.py
#
#  Asi la clave nunca queda escrita en un archivo del repositorio.
USUARIO = os.environ.get("MQTT_USUARIO", "")
CLAVE   = os.environ.get("MQTT_CLAVE", "")

# Un archivo por dia: datos_2026-09-25.csv
CARPETA = "registros"
DELIMITADOR = ";"           # Excel en espanol usa ; — si te abre
                            # todo en una columna, probar con ","

COLUMNAS = [
    "fecha_hora",      # de la PC, con segundos
    "topico",          # empresa/planta/temperatura
    "zona",            # planta
    "magnitud",        # temperatura
    "valor",           # 24.8
    "unidad",          # C
    "ts_placa",        # milisegundos desde que arranco la placa
    "retenido",        # 1 si el broker lo tenia guardado
]


def archivo_de_hoy():
    """Devuelve la ruta del CSV del dia, creando la carpeta si hace falta."""
    os.makedirs(CARPETA, exist_ok=True)
    nombre = "datos_" + datetime.now().strftime("%Y-%m-%d") + ".csv"
    return os.path.join(CARPETA, nombre)


def asegurar_encabezado(ruta):
    """Escribe la fila de titulos solo si el archivo es nuevo."""
    if not os.path.exists(ruta) or os.path.getsize(ruta) == 0:
        with open(ruta, "w", newline="", encoding="utf-8-sig") as f:
            csv.writer(f, delimiter=DELIMITADOR).writerow(COLUMNAS)


def partir_topico(topico):
    """
    empresa/planta/temperatura   -> ("planta", "temperatura")
    empresa/tanque/rele/estado   -> ("tanque", "rele/estado")
    empresa/camara/estado        -> ("camara", "estado")
    """
    partes = topico.split("/")
    zona = partes[1] if len(partes) > 1 else ""
    magnitud = "/".join(partes[2:]) if len(partes) > 2 else ""
    return zona, magnitud


def interpretar(texto):
    """
    Acepta los dos formatos que pueden llegar:
      JSON  -> {"v":24.8,"u":"C","ts":52341}
      plano -> 24.8   o   ON
    Devuelve (valor, unidad, ts_placa).
    """
    texto = texto.strip()
    try:
        d = json.loads(texto)
        if isinstance(d, dict):
            return d.get("v", ""), d.get("u", ""), d.get("ts", "")
    except (json.JSONDecodeError, ValueError):
        pass
    return texto, "", ""      # no era JSON: se guarda tal cual


def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"[OK] Conectado al broker {BROKER}:{PUERTO}")
        client.subscribe(TOPICO, qos=0)
        print(f"[OK] Suscrito a  {TOPICO}")
        print("[OK] Grabando... (Ctrl+C para terminar)\n")
    else:
        ayuda = {
            1: "version de protocolo incorrecta",
            2: "identificador de cliente rechazado",
            3: "broker no disponible",
            4: "usuario o contrasena incorrectos (MQTT_USUARIO / MQTT_CLAVE)",
            5: "no autorizado: falta el usuario en passwd, o lo prohibe la ACL",
        }.get(rc, "causa desconocida")
        print(f"[ERROR] No se pudo conectar. Codigo {rc}: {ayuda}")


def on_message(client, userdata, msg):
    texto = msg.payload.decode("utf-8", errors="replace")
    valor, unidad, ts = interpretar(texto)
    zona, magnitud = partir_topico(msg.topic)

    fila = [
        datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        msg.topic,
        zona,
        magnitud,
        valor,
        unidad,
        ts,
        1 if msg.retain else 0,
    ]

    ruta = archivo_de_hoy()          # si cambia el dia, cambia el archivo
    asegurar_encabezado(ruta)

    # Se abre y se cierra en cada mensaje: asi el CSV queda siempre
    # completo aunque se corte la luz o cierres el script de golpe.
    with open(ruta, "a", newline="", encoding="utf-8-sig") as f:
        csv.writer(f, delimiter=DELIMITADOR).writerow(fila)

    print(f"{fila[0]}  {msg.topic:38s} {valor} {unidad}")


def main():
    cliente = mqtt.Client(client_id="RegistradorCSV")
    cliente.on_connect = on_connect
    cliente.on_message = on_message

    # Un solo codigo para los dos modos: si no hay usuario, se
    # conecta de forma anonima (MODO A).
    if USUARIO:
        cliente.username_pw_set(USUARIO, CLAVE)
        print(f"[MQTT] Modo con autenticacion, usuario: {USUARIO}")
    else:
        print("[MQTT] Modo anonimo (broker sin autenticacion)")

    try:
        cliente.connect(BROKER, PUERTO, keepalive=60)
    except Exception as e:
        print(f"[ERROR] No se pudo conectar a {BROKER}:{PUERTO}")
        print(f"        {e}")
        print("        Verificar que Mosquitto este corriendo.")
        return

    try:
        cliente.loop_forever()
    except KeyboardInterrupt:
        print("\n[FIN] Registro detenido.")
        print(f"[FIN] Archivo: {archivo_de_hoy()}")
        cliente.disconnect()


if __name__ == "__main__":
    main()

// ============================================================
//  PROYECTO EMPRESARIAL IoT ITLM
//  credenciales.ejemplo.h   -   PLANTILLA
// ------------------------------------------------------------
//  COMO SE USA
//  1. Copiar este archivo DENTRO de la carpeta del sketch.
//  2. Renombrarlo a   credenciales.h
//  3. Completar los datos reales.
//
//  El archivo  credenciales.h  esta incluido en .gitignore:
//  NUNCA se sube al repositorio. Este archivo .ejemplo.h si,
//  pero siempre con los campos VACIOS.
//
//  Por que: una clave subida a GitHub queda para siempre en el
//  historial de git, incluso si despues se borra el archivo.
// ============================================================

#ifndef CREDENCIALES_H
#define CREDENCIALES_H

// ── 1. REDES WiFi ───────────────────────────────────────────
// Se intentan en orden. Dejar solo las que se vayan a usar.
// Las dos listas deben tener la MISMA cantidad de elementos.
const char* WIFI_SSID[] = {
  "",            // red principal
  ""             // red de respaldo (punto de acceso del celular)
};

const char* WIFI_CLAVE[] = {
  "",
  ""
};

const int WIFI_CANTIDAD = sizeof(WIFI_SSID) / sizeof(WIFI_SSID[0]);

// ── 2. BROKER MQTT ──────────────────────────────────────────
// IP de la PC donde corre Mosquitto, vista DESDE LA PLACA.
// Se obtiene con  ipconfig  (Windows) o  ip addr  (Linux),
// tomando la IP del adaptador conectado a la MISMA red Wi-Fi
// que las placas.
#define MQTT_SERVIDOR  "192.168.1.33"
#define MQTT_PUERTO    1883

// ── 3. AUTENTICACION DEL BROKER ─────────────────────────────
//  MODO A (sin autenticacion): dejar ambos campos VACIOS ""
//  MODO B (con usuario y clave): completar los dos campos
//
//  El firmware detecta el modo en tiempo de ejecucion:
//  si MQTT_USUARIO esta vacio se conecta de forma anonima.
// MODO B: usuario sugerido para esta placa -> "esp8266tanque"
#define MQTT_USUARIO   ""
#define MQTT_CLAVE     ""

#endif  // CREDENCIALES_H

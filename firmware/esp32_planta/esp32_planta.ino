/* ============================================================
   PROYECTO EMPRESARIAL IoT ITLM
   PLACA 1 - ESP32 DevKit v1  ->  SECTOR "PLANTA"


   Mide:    temperatura y humedad con DHT11
   Controla: aire acondicionado por rele (salida digital)

   Topicos que PUBLICA:
     empresa/planta/temperatura     {"v":24.8,"u":"C","ts":123456}
     empresa/planta/humedad         {"v":62.0,"u":"%","ts":123456}
     empresa/planta/aire/estado     {"v":"ON","u":"bool","ts":...}   (retained)

   Topico al que se SUSCRIBE:
     empresa/planta/aire/cmd        "ON" / "OFF" / "TOGGLE"

   Conexiones:
     DHT11  VCC -> 3V3 | GND -> GND | DATA -> GPIO4
            resistencia pull-up de 10 kohm entre DATA y 3V3
     RELE   VCC -> 5V (Vin) | GND -> GND | IN -> GPIO26
            modulo de rele de 1 canal, entrada ACTIVA EN BAJO

   Bibliotecas: PubSubClient (Nick O'Leary), DHT sensor library (Adafruit)

   Funciona con broker SIN y CON autenticacion: lo define
   credenciales.h  (ver credenciales.ejemplo.h)
   ============================================================ */

#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include "credenciales.h"

// ── IDENTIFICACION DE LA PLACA ──────────────────────────────
#define ID_CLIENTE   "ITLM_ESP32_PLANTA"

// ── SENSOR ──────────────────────────────────────────────────
#define PIN_DHT      4
#define TIPO_DHT     DHT11
DHT dht(PIN_DHT, TIPO_DHT);

// ── ACTUADOR (aire acondicionado) ───────────────────────────
#define PIN_RELE     26
// La mayoria de los modulos de rele chinos son ACTIVOS EN BAJO:
// LOW cierra el contacto (enciende). Si tu modulo es activo en
// alto, invertir estas dos lineas y nada mas.
#define RELE_ON      LOW
#define RELE_OFF     HIGH

// Proteccion del compresor: un aire acondicionado no debe
// volver a arrancar antes de que pasen unos minutos desde que
// se apago, o se daña el compresor.
const unsigned long TIEMPO_MIN_REPOSO = 180000UL;   // 3 min
unsigned long       momentoApagado    = 0;
bool                pedidoPendiente   = false;
bool                aireEncendido     = false;

// ── TOPICOS ─────────────────────────────────────────────────
const char* TOPIC_TEMP      = "empresa/planta/temperatura";
const char* TOPIC_HUM       = "empresa/planta/humedad";
const char* TOPIC_AIRE_CMD  = "empresa/planta/aire/cmd";
const char* TOPIC_AIRE_EST  = "empresa/planta/aire/estado";

// ── OBJETOS DE RED ──────────────────────────────────────────
WiFiClient    clienteWifi;
PubSubClient  mqtt(clienteWifi);

// ── TEMPORIZADORES ──────────────────────────────────────────
unsigned long ultimaLectura   = 0;
const unsigned long INTERVALO = 5000;     // 5 s entre mediciones

// ════════════════════════════════════════════════════════════
//   WiFi
// ════════════════════════════════════════════════════════════
void conectarWiFi() {
  Serial.println(F("\n[WiFi] Buscando red..."));

  for (int i = 0; i < WIFI_CANTIDAD; i++) {
    if (strlen(WIFI_SSID[i]) == 0) continue;     // ranura vacia

    Serial.print(F("[WiFi] Probando: "));
    Serial.println(WIFI_SSID[i]);

    WiFi.begin(WIFI_SSID[i], WIFI_CLAVE[i]);

    int intentos = 0;
    while (WiFi.status() != WL_CONNECTED && intentos < 15) {
      delay(500);
      Serial.print('.');
      intentos++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.print(F("\n[WiFi] Conectado a "));
      Serial.println(WIFI_SSID[i]);
      Serial.print(F("[WiFi] IP de la placa: "));
      Serial.println(WiFi.localIP());
      return;
    }

    Serial.println(F("\n[WiFi] Sin exito en esta red"));
    WiFi.disconnect();
  }

  Serial.println(F("[WiFi] ERROR: ninguna red disponible"));
}

// ════════════════════════════════════════════════════════════
//   PUBLICACION EN FORMATO JSON
//   Se arma a mano para no depender de ArduinoJson.
//   Formato unico de todo el proyecto:
//     {"v":<valor>,"u":"<unidad>","ts":<millis>}
// ════════════════════════════════════════════════════════════
void publicarNumero(const char* topico, float valor, const char* unidad) {
  String carga = "{\"v\":";
  carga += String(valor, 1);
  carga += ",\"u\":\"";
  carga += unidad;
  carga += "\",\"ts\":";
  carga += String(millis());
  carga += "}";

  mqtt.publish(topico, carga.c_str());

  Serial.print(F("[MQTT -> ] "));
  Serial.print(topico);
  Serial.print(' ');
  Serial.println(carga);
}

void publicarTexto(const char* topico, const char* texto, bool retenido) {
  String carga = "{\"v\":\"";
  carga += texto;
  carga += "\",\"u\":\"bool\",\"ts\":";
  carga += String(millis());
  carga += "}";

  // retenido = true  -> el broker GUARDA el ultimo valor y lo
  // entrega a quien se suscriba despues. Asi la app muestra el
  // estado correcto de los botones al abrirse.
  mqtt.publish(topico, carga.c_str(), retenido);

  Serial.print(F("[MQTT -> ] "));
  Serial.print(topico);
  Serial.print(' ');
  Serial.println(carga);
}

// ════════════════════════════════════════════════════════════
//   CONTROL DEL AIRE ACONDICIONADO
// ════════════════════════════════════════════════════════════
void aplicarEstadoAire(bool encender) {
  if (encender) {
    // Proteccion anti arranque corto del compresor
    if (momentoApagado != 0 &&
        (millis() - momentoApagado) < TIEMPO_MIN_REPOSO) {

      pedidoPendiente = true;
      unsigned long faltan =
        (TIEMPO_MIN_REPOSO - (millis() - momentoApagado)) / 1000;

      Serial.print(F("[AIRE] En ESPERA, faltan "));
      Serial.print(faltan);
      Serial.println(F(" s de reposo del compresor"));

      publicarTexto(TOPIC_AIRE_EST, "ESPERA", true);
      return;
    }

    digitalWrite(PIN_RELE, RELE_ON);
    aireEncendido   = true;
    pedidoPendiente = false;
    Serial.println(F("[AIRE] ENCENDIDO"));
    publicarTexto(TOPIC_AIRE_EST, "ON", true);

  } else {
    digitalWrite(PIN_RELE, RELE_OFF);
    if (aireEncendido) momentoApagado = millis();
    aireEncendido   = false;
    pedidoPendiente = false;
    Serial.println(F("[AIRE] APAGADO"));
    publicarTexto(TOPIC_AIRE_EST, "OFF", true);
  }
}

void atenderPedidoPendiente() {
  if (!pedidoPendiente) return;
  if ((millis() - momentoApagado) >= TIEMPO_MIN_REPOSO) {
    Serial.println(F("[AIRE] Reposo cumplido, ejecuto el pedido en espera"));
    pedidoPendiente = false;
    aplicarEstadoAire(true);
  }
}

// ════════════════════════════════════════════════════════════
//   LLEGADA DE UN MENSAJE MQTT
// ════════════════════════════════════════════════════════════
void alRecibir(char* topico, byte* carga, unsigned int largo) {
  // El contenido NO termina en '\0': hay que copiarlo.
  char mensaje[16];
  unsigned int n = (largo < sizeof(mensaje) - 1) ? largo : sizeof(mensaje) - 1;
  memcpy(mensaje, carga, n);
  mensaje[n] = '\0';

  Serial.print(F("[MQTT <- ] "));
  Serial.print(topico);
  Serial.print(' ');
  Serial.println(mensaje);

  if (strcmp(topico, TOPIC_AIRE_CMD) == 0) {
    if      (strcmp(mensaje, "ON")     == 0) aplicarEstadoAire(true);
    else if (strcmp(mensaje, "OFF")    == 0) aplicarEstadoAire(false);
    else if (strcmp(mensaje, "TOGGLE") == 0) aplicarEstadoAire(!aireEncendido);
    else Serial.println(F("[AIRE] Comando no reconocido"));
  }
}

// ════════════════════════════════════════════════════════════
//   CONEXION AL BROKER
//   Un mismo codigo sirve para los dos modos:
//   si MQTT_USUARIO esta vacio -> conexion anonima (MODO A)
//   si tiene contenido         -> con usuario y clave (MODO B)
// ════════════════════════════════════════════════════════════
bool conectarMqtt() {
  Serial.print(F("[MQTT] Conectando a "));
  Serial.print(MQTT_SERVIDOR);
  Serial.print(':');
  Serial.print(MQTT_PUERTO);

  bool conectado;

  // Last Will: si la placa se desconecta de golpe, el broker
  // publica "OFF" en el topico de estado por nosotros.
  if (strlen(MQTT_USUARIO) == 0) {
    Serial.println(F("  (modo anonimo)"));
    conectado = mqtt.connect(ID_CLIENTE,
                             NULL, NULL,
                             TOPIC_AIRE_EST, 0, true,
                             "{\"v\":\"OFF\",\"u\":\"bool\",\"ts\":0}");
  } else {
    Serial.print(F("  (usuario "));
    Serial.print(MQTT_USUARIO);
    Serial.println(')');
    conectado = mqtt.connect(ID_CLIENTE,
                             MQTT_USUARIO, MQTT_CLAVE,
                             TOPIC_AIRE_EST, 0, true,
                             "{\"v\":\"OFF\",\"u\":\"bool\",\"ts\":0}");
  }

  if (conectado) {
    Serial.println(F("[MQTT] Conectado"));
    mqtt.subscribe(TOPIC_AIRE_CMD);
    Serial.print(F("[MQTT] Suscrito a "));
    Serial.println(TOPIC_AIRE_CMD);
    // Publicar el estado real al reconectar
    publicarTexto(TOPIC_AIRE_EST, aireEncendido ? "ON" : "OFF", true);
    return true;
  }

  Serial.print(F("[MQTT] ERROR rc="));
  Serial.print(mqtt.state());
  Serial.println(F("   (-2 red/IP | -4 timeout | 4 usuario o clave mal | 5 no autorizado)"));
  return false;
}

// ════════════════════════════════════════════════════════════
//   SETUP
// ════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(F("\n=== ITLM - ESP32 PLANTA ==="));

  // El rele se deja en reposo ANTES de configurar el pin como
  // salida: asi no se produce un pulso de arranque al encender.
  digitalWrite(PIN_RELE, RELE_OFF);
  pinMode(PIN_RELE, OUTPUT);
  digitalWrite(PIN_RELE, RELE_OFF);

  dht.begin();
  Serial.println(F("[OK] DHT11 iniciado"));

  conectarWiFi();
  mqtt.setServer(MQTT_SERVIDOR, MQTT_PUERTO);
  mqtt.setCallback(alRecibir);
  mqtt.setKeepAlive(30);
}

// ════════════════════════════════════════════════════════════
//   LOOP
// ════════════════════════════════════════════════════════════
void loop() {
  // 1. Red
  if (WiFi.status() != WL_CONNECTED) conectarWiFi();

  // 2. Broker (sin while bloqueante: el loop debe seguir corriendo)
  static unsigned long ultimoIntento = 0;
  if (!mqtt.connected()) {
    if (millis() - ultimoIntento > 3000) {
      ultimoIntento = millis();
      conectarMqtt();
    }
  } else {
    mqtt.loop();          // obligatorio: atiende los mensajes entrantes
  }

  // 3. Pedido de encendido que quedo esperando el reposo
  atenderPedidoPendiente();

  // 4. Medicion periodica
  if (millis() - ultimaLectura >= INTERVALO) {
    ultimaLectura = millis();

    float temperatura = dht.readTemperature();
    float humedad     = dht.readHumidity();

    if (isnan(temperatura) || isnan(humedad)) {
      Serial.println(F("[ERROR] Lectura fallida del DHT11"));
      Serial.println(F("        revisar pull-up de 10k y cableado"));
      return;
    }

    if (mqtt.connected()) {
      publicarNumero(TOPIC_TEMP, temperatura, "C");
      publicarNumero(TOPIC_HUM,  humedad,     "%");
    }
  }
}

/* ============================================================
   PROYECTO EMPRESARIAL IoT ITLM
   PLACA 3 - ESP32-S3-WROOM  ->  SECTOR "OFICINA"

   Mide:     temperatura y humedad con DHT11
   Controla: iluminacion por rele

   Topicos que PUBLICA:
     empresa/oficina/temperatura    {"v":23.4,"u":"C","ts":...}
     empresa/oficina/humedad        {"v":55.0,"u":"%","ts":...}
     empresa/oficina/luces/estado   {"v":"ON","u":"bool","ts":...}  (retained)

   Topico al que se SUSCRIBE:
     empresa/oficina/luces/cmd      "ON" / "OFF" / "TOGGLE"

   Conexiones:
     DHT11  VCC -> 3V3 | GND -> GND | DATA -> GPIO4
            pull-up de 10 kohm entre DATA y 3V3
     RELE   VCC -> 5V | GND -> GND | IN -> GPIO5

   AVISO sobre la ESP32-S3:
   - En el IDE elegir placa "ESP32S3 Dev Module".
   - Activar  USB CDC On Boot: "Enabled"  para ver el monitor
     serie por el puerto USB nativo. Si no, el Serial no imprime.
   - Los GPIO 19/20 son USB y los 26..32 son la memoria flash:
     NO usarlos.
   ============================================================ */

#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include "credenciales.h"

#define ID_CLIENTE   "ITLM_ESP32S3_OFICINA"

// ── SENSOR ──────────────────────────────────────────────────
#define PIN_DHT      4
#define TIPO_DHT     DHT11
DHT dht(PIN_DHT, TIPO_DHT);

// ── ACTUADOR (luces) ────────────────────────────────────────
#define PIN_RELE     5
#define RELE_ON      LOW
#define RELE_OFF     HIGH
bool lucesEncendidas = false;

// ── TOPICOS ─────────────────────────────────────────────────
const char* TOPIC_TEMP      = "empresa/oficina/temperatura";
const char* TOPIC_HUM       = "empresa/oficina/humedad";
const char* TOPIC_LUCES_CMD = "empresa/oficina/luces/cmd";
const char* TOPIC_LUCES_EST = "empresa/oficina/luces/estado";

WiFiClient    clienteWifi;
PubSubClient  mqtt(clienteWifi);

unsigned long ultimaLectura   = 0;
const unsigned long INTERVALO = 5000;

// ════════════════════════════════════════════════════════════
void conectarWiFi() {
  Serial.println(F("\n[WiFi] Buscando red..."));

  for (int i = 0; i < WIFI_CANTIDAD; i++) {
    if (strlen(WIFI_SSID[i]) == 0) continue;

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
  mqtt.publish(topico, carga.c_str(), retenido);
  Serial.print(F("[MQTT -> ] "));
  Serial.print(topico);
  Serial.print(' ');
  Serial.println(carga);
}

// ════════════════════════════════════════════════════════════
void aplicarEstadoLuces(bool encender) {
  digitalWrite(PIN_RELE, encender ? RELE_ON : RELE_OFF);
  lucesEncendidas = encender;
  Serial.print(F("[LUCES] "));
  Serial.println(encender ? F("ENCENDIDAS") : F("APAGADAS"));
  publicarTexto(TOPIC_LUCES_EST, encender ? "ON" : "OFF", true);
}

void alRecibir(char* topico, byte* carga, unsigned int largo) {
  char mensaje[16];
  unsigned int n = (largo < sizeof(mensaje) - 1) ? largo : sizeof(mensaje) - 1;
  memcpy(mensaje, carga, n);
  mensaje[n] = '\0';

  Serial.print(F("[MQTT <- ] "));
  Serial.print(topico);
  Serial.print(' ');
  Serial.println(mensaje);

  if (strcmp(topico, TOPIC_LUCES_CMD) == 0) {
    if      (strcmp(mensaje, "ON")     == 0) aplicarEstadoLuces(true);
    else if (strcmp(mensaje, "OFF")    == 0) aplicarEstadoLuces(false);
    else if (strcmp(mensaje, "TOGGLE") == 0) aplicarEstadoLuces(!lucesEncendidas);
    else Serial.println(F("[LUCES] Comando no reconocido"));
  }
}

// ════════════════════════════════════════════════════════════
bool conectarMqtt() {
  Serial.print(F("[MQTT] Conectando a "));
  Serial.print(MQTT_SERVIDOR);
  Serial.print(':');
  Serial.print(MQTT_PUERTO);

  bool conectado;
  const char* testamento = "{\"v\":\"OFF\",\"u\":\"bool\",\"ts\":0}";

  if (strlen(MQTT_USUARIO) == 0) {
    Serial.println(F("  (modo anonimo)"));
    conectado = mqtt.connect(ID_CLIENTE, NULL, NULL,
                             TOPIC_LUCES_EST, 0, true, testamento);
  } else {
    Serial.print(F("  (usuario "));
    Serial.print(MQTT_USUARIO);
    Serial.println(')');
    conectado = mqtt.connect(ID_CLIENTE, MQTT_USUARIO, MQTT_CLAVE,
                             TOPIC_LUCES_EST, 0, true, testamento);
  }

  if (conectado) {
    Serial.println(F("[MQTT] Conectado"));
    mqtt.subscribe(TOPIC_LUCES_CMD);
    publicarTexto(TOPIC_LUCES_EST, lucesEncendidas ? "ON" : "OFF", true);
    return true;
  }

  Serial.print(F("[MQTT] ERROR rc="));
  Serial.print(mqtt.state());
  Serial.println(F("   (-2 red/IP | -4 timeout | 4 usuario o clave mal | 5 no autorizado)"));
  return false;
}

// ════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(F("\n=== ITLM - ESP32-S3 OFICINA ==="));

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

void loop() {
  if (WiFi.status() != WL_CONNECTED) conectarWiFi();

  static unsigned long ultimoIntento = 0;
  if (!mqtt.connected()) {
    if (millis() - ultimoIntento > 3000) {
      ultimoIntento = millis();
      conectarMqtt();
    }
  } else {
    mqtt.loop();
  }

  if (millis() - ultimaLectura >= INTERVALO) {
    ultimaLectura = millis();

    float temperatura = dht.readTemperature();
    float humedad     = dht.readHumidity();

    if (isnan(temperatura) || isnan(humedad)) {
      Serial.println(F("[ERROR] Lectura fallida del DHT11"));
      return;
    }

    if (mqtt.connected()) {
      publicarNumero(TOPIC_TEMP, temperatura, "C");
      publicarNumero(TOPIC_HUM,  humedad,     "%");
    }
  }
}

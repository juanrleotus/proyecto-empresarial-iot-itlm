/* ============================================================
   PROYECTO EMPRESARIAL IoT ITLM
   PLACA 2 - ESP8266 NodeMCU v3  ->  SECTOR "TANQUE"

   Mide:     temperatura del agua con DS18B20 (sonda sumergible)
   Controla: bomba de recirculacion por rele

   Topicos que PUBLICA:
     empresa/tanque/temperatura     {"v":27.7,"u":"C","ts":123456}
     empresa/tanque/rele/estado     {"v":"ON","u":"bool","ts":...}  (retained)

   Topico al que se SUSCRIBE:
     empresa/tanque/rele/cmd        "ON" / "OFF" / "TOGGLE"

   Conexiones:
     DS18B20  rojo -> 3V3 | negro -> GND | amarillo -> D4 (GPIO2)
              resistencia pull-up de 4,7 kohm entre D4 y 3V3
              (4,7 k, NO 10 k: el bus OneWire lo exige)
     RELE     VCC -> Vin (5V) | GND -> GND | IN -> D1 (GPIO5)

   Bibliotecas: PubSubClient, OneWire (Paul Stoffregen),
                DallasTemperature (Miles Burton)

   OJO con los nombres de pines del NodeMCU:
     D1 = GPIO5   D2 = GPIO4   D4 = GPIO2
   En el codigo se usan los NUMEROS DE GPIO.
   ============================================================ */

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "credenciales.h"

// ── IDENTIFICACION DE LA PLACA ──────────────────────────────
#define ID_CLIENTE   "ITLM_ESP8266_TANQUE"

// ── SENSOR ──────────────────────────────────────────────────
#define PIN_ONEWIRE  2            // D4 del NodeMCU
OneWire            bus(PIN_ONEWIRE);
DallasTemperature  sensor(&bus);

// ── ACTUADOR (bomba) ────────────────────────────────────────
#define PIN_RELE     5            // D1 del NodeMCU
#define RELE_ON      LOW          // modulo activo en bajo
#define RELE_OFF     HIGH
bool bombaEncendida = false;

// ── TOPICOS ─────────────────────────────────────────────────
const char* TOPIC_TEMP      = "empresa/tanque/temperatura";
const char* TOPIC_BOMBA_CMD = "empresa/tanque/rele/cmd";
const char* TOPIC_BOMBA_EST = "empresa/tanque/rele/estado";

// ── OBJETOS DE RED ──────────────────────────────────────────
WiFiClient    clienteWifi;
PubSubClient  mqtt(clienteWifi);

unsigned long ultimaLectura   = 0;
const unsigned long INTERVALO = 5000;

// ════════════════════════════════════════════════════════════
//   WiFi
// ════════════════════════════════════════════════════════════
void conectarWiFi() {
  Serial.println(F("\n[WiFi] Buscando red..."));
  WiFi.mode(WIFI_STA);

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
//   PUBLICACION JSON
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
//   CONTROL DE LA BOMBA
// ════════════════════════════════════════════════════════════
void aplicarEstadoBomba(bool encender) {
  digitalWrite(PIN_RELE, encender ? RELE_ON : RELE_OFF);
  bombaEncendida = encender;

  Serial.print(F("[BOMBA] "));
  Serial.println(encender ? F("ENCENDIDA") : F("APAGADA"));

  publicarTexto(TOPIC_BOMBA_EST, encender ? "ON" : "OFF", true);
}

// ════════════════════════════════════════════════════════════
//   LLEGADA DE UN MENSAJE MQTT
// ════════════════════════════════════════════════════════════
void alRecibir(char* topico, byte* carga, unsigned int largo) {
  char mensaje[16];
  unsigned int n = (largo < sizeof(mensaje) - 1) ? largo : sizeof(mensaje) - 1;
  memcpy(mensaje, carga, n);
  mensaje[n] = '\0';

  Serial.print(F("[MQTT <- ] "));
  Serial.print(topico);
  Serial.print(' ');
  Serial.println(mensaje);

  if (strcmp(topico, TOPIC_BOMBA_CMD) == 0) {
    if      (strcmp(mensaje, "ON")     == 0) aplicarEstadoBomba(true);
    else if (strcmp(mensaje, "OFF")    == 0) aplicarEstadoBomba(false);
    else if (strcmp(mensaje, "TOGGLE") == 0) aplicarEstadoBomba(!bombaEncendida);
    else Serial.println(F("[BOMBA] Comando no reconocido"));
  }
}

// ════════════════════════════════════════════════════════════
//   CONEXION AL BROKER (sirve para los dos modos)
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
                             TOPIC_BOMBA_EST, 0, true, testamento);
  } else {
    Serial.print(F("  (usuario "));
    Serial.print(MQTT_USUARIO);
    Serial.println(')');
    conectado = mqtt.connect(ID_CLIENTE, MQTT_USUARIO, MQTT_CLAVE,
                             TOPIC_BOMBA_EST, 0, true, testamento);
  }

  if (conectado) {
    Serial.println(F("[MQTT] Conectado"));
    mqtt.subscribe(TOPIC_BOMBA_CMD);
    publicarTexto(TOPIC_BOMBA_EST, bombaEncendida ? "ON" : "OFF", true);
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
  Serial.println(F("\n=== ITLM - ESP8266 TANQUE ==="));

  digitalWrite(PIN_RELE, RELE_OFF);
  pinMode(PIN_RELE, OUTPUT);
  digitalWrite(PIN_RELE, RELE_OFF);

  sensor.begin();
  Serial.print(F("[OK] Sondas DS18B20 detectadas: "));
  Serial.println(sensor.getDeviceCount());
  if (sensor.getDeviceCount() == 0) {
    Serial.println(F("[AVISO] Ninguna sonda. Revisar el pull-up de 4,7k en D4"));
  }

  conectarWiFi();
  mqtt.setServer(MQTT_SERVIDOR, MQTT_PUERTO);
  mqtt.setCallback(alRecibir);
  mqtt.setKeepAlive(30);
}

// ════════════════════════════════════════════════════════════
//   LOOP
// ════════════════════════════════════════════════════════════
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

    sensor.requestTemperatures();
    float temperatura = sensor.getTempCByIndex(0);

    // DEVICE_DISCONNECTED_C vale -127: sonda ausente o mal cableada
    if (temperatura == DEVICE_DISCONNECTED_C || temperatura < -50) {
      Serial.println(F("[ERROR] DS18B20 no responde (revisar pull-up 4,7k)"));
      return;
    }

    if (mqtt.connected()) {
      publicarNumero(TOPIC_TEMP, temperatura, "C");
    }
  }
}

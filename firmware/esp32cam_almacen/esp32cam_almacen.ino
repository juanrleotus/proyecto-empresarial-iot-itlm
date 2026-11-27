/* ============================================================
   PROYECTO EMPRESARIAL IoT ITLM
   PLACA 4 - ESP32-CAM AI THINKER  ->  SECTOR "ALMACEN"

   Hace tres cosas AL MISMO TIEMPO:
     1. Servidor HTTP en el puerto 80  -> foto fija  /capture
     2. Servidor HTTP en el puerto 81  -> video MJPEG /stream
     3. Cliente MQTT -> recibe ordenes y avisa cuando saca foto

   Topicos que PUBLICA:
     empresa/camara/estado   (retained)
        {"v":"ON","u":"bool","ts":...,
         "ip":"192.168.1.50",
         "foto":"http://192.168.1.50/capture",
         "video":"http://192.168.1.50:81/stream"}
     empresa/camara/evento
        {"v":"FOTO","u":"evt","ts":...,
         "url":"http://192.168.1.50/capture?t=123456"}

   Topico al que se SUSCRIBE:
     empresa/camara/cmd   ->  "FOTO" | "FLASH_ON" | "FLASH_OFF" | "ESTADO"

   La app del celular lee la clave "url" del evento para
   refrescar la imagen, y la clave "video" del estado para
   abrir el video.

   --------------------------------------------------------
   COMO SE PROGRAMA (la ESP32-CAM no tiene puerto USB)
   --------------------------------------------------------
     Adaptador FTDI/USB-TTL a 5V:
       FTDI 5V   -> ESP32-CAM 5V
       FTDI GND  -> ESP32-CAM GND
       FTDI TX   -> ESP32-CAM U0R   (cruzado)
       FTDI RX   -> ESP32-CAM U0T   (cruzado)
       PUENTE: GPIO0 -> GND   (solo para cargar el programa)

     IDE: Placa "AI Thinker ESP32-CAM" | Partition Scheme:
          "Huge APP (3MB No OTA/1MB SPIFFS)" | 115200 baudios.

     Secuencia: poner el puente GPIO0-GND, pulsar RESET, Subir.
     Al terminar: QUITAR el puente y pulsar RESET otra vez.

   --------------------------------------------------------
   ALIMENTACION  (causa nro 1 de que "no transmita")
   --------------------------------------------------------
   La ESP32-CAM consume picos de ~500 mA al transmitir video.
   El regulador de 3V3 de muchos FTDI entrega 50 mA: la placa
   se reinicia sola o nunca llega al broker.
   -> Alimentar por el pin 5V con una fuente de 5V / 1A, o con
      un USB-TTL que entregue 5V de verdad, y agregar un
      capacitor electrolitico de 470 uF entre 5V y GND.
   ============================================================ */

#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include <WiFi.h>
#include <PubSubClient.h>

// Desactiva el reinicio por bajo voltaje. ATENCION: esto NO
// arregla una alimentacion mala, solo evita que la placa se
// reinicie en bucle mientras se diagnostica.
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#include "credenciales.h"

#define ID_CLIENTE   "ITLM_ESP32CAM_ALMACEN"

// ── PINOUT FIJO DEL MODELO AI THINKER ───────────────────────
// No modificar: viene dado por el circuito impreso de la placa.
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#define PIN_FLASH          4      // LED blanco de la placa
#define PIN_LED_ROJO      33      // LED rojo (logica invertida)

// ── TOPICOS ─────────────────────────────────────────────────
const char* TOPIC_CAM_CMD = "empresa/camara/cmd";
const char* TOPIC_CAM_EST = "empresa/camara/estado";
const char* TOPIC_CAM_EVT = "empresa/camara/evento";

// ── OBJETOS ─────────────────────────────────────────────────
WiFiClient    clienteWifi;
PubSubClient  mqtt(clienteWifi);

httpd_handle_t servidorWeb    = NULL;    // puerto 80
httpd_handle_t servidorStream = NULL;    // puerto 81

String ipPlaca   = "0.0.0.0";
bool   flashOn   = false;

// ════════════════════════════════════════════════════════════
//   MJPEG: constantes del formato multipart
// ════════════════════════════════════════════════════════════
#define LIMITE "itlmframe"
static const char* STREAM_TIPO     = "multipart/x-mixed-replace;boundary=" LIMITE;
static const char* STREAM_LIMITE   = "\r\n--" LIMITE "\r\n";
static const char* STREAM_PARTE    = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

// ════════════════════════════════════════════════════════════
//   MANEJADOR  /capture   (foto fija)
// ════════════════════════════════════════════════════════════
static esp_err_t manejarCaptura(httpd_req_t* req) {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println(F("[CAM] No se pudo capturar el cuadro"));
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=itlm.jpg");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  // Sin cache: si no, el celular muestra siempre la misma foto.
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");

  esp_err_t res = httpd_resp_send(req, (const char*)fb->buf, fb->len);
  esp_camera_fb_return(fb);
  return res;
}

// ════════════════════════════════════════════════════════════
//   MANEJADOR  /stream   (video MJPEG, puerto 81)
// ════════════════════════════════════════════════════════════
static esp_err_t manejarStream(httpd_req_t* req) {
  camera_fb_t* fb  = NULL;
  esp_err_t    res = ESP_OK;
  char   cabecera[64];

  res = httpd_resp_set_type(req, STREAM_TIPO);
  if (res != ESP_OK) return res;
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  Serial.println(F("[CAM] Cliente conectado al stream"));

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) { res = ESP_FAIL; break; }

    size_t largoCabecera = snprintf(cabecera, sizeof(cabecera), STREAM_PARTE, fb->len);

    res = httpd_resp_send_chunk(req, STREAM_LIMITE, strlen(STREAM_LIMITE));
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, cabecera, largoCabecera);
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len);

    esp_camera_fb_return(fb);
    if (res != ESP_OK) break;      // el cliente cerro la pagina
  }

  Serial.println(F("[CAM] Cliente desconectado del stream"));
  return res;
}

// ════════════════════════════════════════════════════════════
//   MANEJADOR  /   (pagina de prueba)
// ════════════════════════════════════════════════════════════
static esp_err_t manejarInicio(httpd_req_t* req) {
  String ip = ipPlaca;
  String html =
    "<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Camara ITLM - Almacen</title>"
    "<style>body{font-family:sans-serif;background:#10213a;color:#fff;"
    "text-align:center;margin:0;padding:16px}img{max-width:100%;"
    "border-radius:8px;border:2px solid #2d6cdf}a{color:#8ab4ff}</style>"
    "</head><body><h2>Camara ITLM - Almacen</h2>"
    "<img src='http://" + ip + ":81/stream'>"
    "<p><a href='/capture'>Descargar foto fija</a></p>"
    "</body></html>";

  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, html.c_str(), html.length());
}

// ════════════════════════════════════════════════════════════
//   ARRANQUE DE LOS DOS SERVIDORES
// ════════════════════════════════════════════════════════════
void iniciarServidores() {
  // ── Servidor del puerto 80 ──
  httpd_config_t cfg80 = HTTPD_DEFAULT_CONFIG();
  cfg80.server_port = 80;
  cfg80.ctrl_port   = 32768;

  httpd_uri_t uriInicio  = { "/",        HTTP_GET, manejarInicio,  NULL };
  httpd_uri_t uriCaptura = { "/capture", HTTP_GET, manejarCaptura, NULL };

  if (httpd_start(&servidorWeb, &cfg80) == ESP_OK) {
    httpd_register_uri_handler(servidorWeb, &uriInicio);
    httpd_register_uri_handler(servidorWeb, &uriCaptura);
    Serial.println(F("[HTTP] Puerto 80 activo  (/ y /capture)"));
  } else {
    Serial.println(F("[HTTP] ERROR al abrir el puerto 80"));
  }

  // ── Servidor del puerto 81 (solo video) ──
  // ctrl_port DEBE ser distinto del anterior, o el segundo
  // servidor no arranca y el video nunca aparece.
  httpd_config_t cfg81 = HTTPD_DEFAULT_CONFIG();
  cfg81.server_port = 81;
  cfg81.ctrl_port   = 32769;

  httpd_uri_t uriStream = { "/stream", HTTP_GET, manejarStream, NULL };

  if (httpd_start(&servidorStream, &cfg81) == ESP_OK) {
    httpd_register_uri_handler(servidorStream, &uriStream);
    Serial.println(F("[HTTP] Puerto 81 activo  (/stream)"));
  } else {
    Serial.println(F("[HTTP] ERROR al abrir el puerto 81"));
  }
}

// ════════════════════════════════════════════════════════════
//   CAMARA
// ════════════════════════════════════════════════════════════
bool iniciarCamara() {
  // Se inicializa en cero: la estructura tiene campos que no se
  // asignan aqui y, sin esto, quedarian con basura de memoria.
  camera_config_t cfg = {};
  cfg.ledc_channel = LEDC_CHANNEL_0;
  cfg.ledc_timer   = LEDC_TIMER_0;
  cfg.pin_d0       = Y2_GPIO_NUM;
  cfg.pin_d1       = Y3_GPIO_NUM;
  cfg.pin_d2       = Y4_GPIO_NUM;
  cfg.pin_d3       = Y5_GPIO_NUM;
  cfg.pin_d4       = Y6_GPIO_NUM;
  cfg.pin_d5       = Y7_GPIO_NUM;
  cfg.pin_d6       = Y8_GPIO_NUM;
  cfg.pin_d7       = Y9_GPIO_NUM;
  cfg.pin_xclk     = XCLK_GPIO_NUM;
  cfg.pin_pclk     = PCLK_GPIO_NUM;
  cfg.pin_vsync    = VSYNC_GPIO_NUM;
  cfg.pin_href     = HREF_GPIO_NUM;
  cfg.pin_sccb_sda = SIOD_GPIO_NUM;
  cfg.pin_sccb_scl = SIOC_GPIO_NUM;
  cfg.pin_pwdn     = PWDN_GPIO_NUM;
  cfg.pin_reset    = RESET_GPIO_NUM;
  cfg.xclk_freq_hz = 20000000;
  cfg.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    // Con PSRAM se puede usar mas resolucion y doble buffer.
    cfg.frame_size   = FRAMESIZE_VGA;    // 640x480
    cfg.jpeg_quality = 12;               // 10 = mejor, 63 = peor
    cfg.fb_count     = 2;
    cfg.grab_mode    = CAMERA_GRAB_LATEST;
    cfg.fb_location  = CAMERA_FB_IN_PSRAM;
    Serial.println(F("[CAM] PSRAM detectada -> VGA"));
  } else {
    cfg.frame_size   = FRAMESIZE_QVGA;   // 320x240
    cfg.jpeg_quality = 15;
    cfg.fb_count     = 1;
    cfg.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;
    cfg.fb_location  = CAMERA_FB_IN_DRAM;
    Serial.println(F("[CAM] SIN PSRAM -> QVGA"));
  }

  esp_err_t err = esp_camera_init(&cfg);
  if (err != ESP_OK) {
    Serial.print(F("[CAM] ERROR de inicializacion 0x"));
    Serial.println(err, HEX);
    Serial.println(F("      0x20004 = sensor no detectado: revisar el"));
    Serial.println(F("      conector flex de la camara y la alimentacion"));
    return false;
  }

  // Ajustes de imagen. El sensor OV2640 viene espejado.
  sensor_t* s = esp_camera_sensor_get();
  if (s) {
    s->set_vflip(s, 1);
    s->set_hmirror(s, 0);
    s->set_brightness(s, 1);
    s->set_saturation(s, 0);
  }

  Serial.println(F("[CAM] Camara lista"));
  return true;
}

// ════════════════════════════════════════════════════════════
//   WiFi
// ════════════════════════════════════════════════════════════
void conectarWiFi() {
  Serial.println(F("\n[WiFi] Buscando red..."));
  WiFi.mode(WIFI_STA);
  // El ahorro de energia del WiFi corta el video a mitad de cuadro.
  WiFi.setSleep(false);

  for (int i = 0; i < WIFI_CANTIDAD; i++) {
    if (strlen(WIFI_SSID[i]) == 0) continue;

    Serial.print(F("[WiFi] Probando: "));
    Serial.println(WIFI_SSID[i]);
    WiFi.begin(WIFI_SSID[i], WIFI_CLAVE[i]);

    int intentos = 0;
    while (WiFi.status() != WL_CONNECTED && intentos < 20) {
      delay(500);
      Serial.print('.');
      intentos++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      ipPlaca = WiFi.localIP().toString();
      Serial.print(F("\n[WiFi] Conectado a "));
      Serial.println(WIFI_SSID[i]);
      Serial.println(F("--------------------------------------------"));
      Serial.print(F("  Foto  : http://"));  Serial.print(ipPlaca); Serial.println(F("/capture"));
      Serial.print(F("  Video : http://"));  Serial.print(ipPlaca); Serial.println(F(":81/stream"));
      Serial.print(F("  Prueba: http://"));  Serial.println(ipPlaca);
      Serial.println(F("--------------------------------------------"));
      return;
    }

    Serial.println(F("\n[WiFi] Sin exito en esta red"));
    WiFi.disconnect();
  }

  Serial.println(F("[WiFi] ERROR: ninguna red disponible"));
}

// ════════════════════════════════════════════════════════════
//   PUBLICACIONES MQTT
// ════════════════════════════════════════════════════════════
void publicarEstado() {
  String carga = "{\"v\":\"ON\",\"u\":\"bool\",\"ts\":";
  carga += String(millis());
  carga += ",\"ip\":\"" + ipPlaca + "\"";
  carga += ",\"foto\":\"http://" + ipPlaca + "/capture\"";
  carga += ",\"video\":\"http://" + ipPlaca + ":81/stream\"}";

  mqtt.publish(TOPIC_CAM_EST, carga.c_str(), true);   // retenido
  Serial.print(F("[MQTT -> ] "));
  Serial.print(TOPIC_CAM_EST);
  Serial.print(' ');
  Serial.println(carga);
}

void publicarEventoFoto() {
  // El parametro ?t= cambia en cada foto para que el celular
  // no muestre la imagen vieja guardada en cache.
  String carga = "{\"v\":\"FOTO\",\"u\":\"evt\",\"ts\":";
  carga += String(millis());
  carga += ",\"url\":\"http://" + ipPlaca + "/capture?t=" + String(millis()) + "\"}";

  mqtt.publish(TOPIC_CAM_EVT, carga.c_str());
  Serial.print(F("[MQTT -> ] "));
  Serial.print(TOPIC_CAM_EVT);
  Serial.print(' ');
  Serial.println(carga);
}

// ════════════════════════════════════════════════════════════
//   ORDENES QUE LLEGAN POR MQTT
// ════════════════════════════════════════════════════════════
void alRecibir(char* topico, byte* carga, unsigned int largo) {
  char mensaje[24];
  unsigned int n = (largo < sizeof(mensaje) - 1) ? largo : sizeof(mensaje) - 1;
  memcpy(mensaje, carga, n);
  mensaje[n] = '\0';

  Serial.print(F("[MQTT <- ] "));
  Serial.print(topico);
  Serial.print(' ');
  Serial.println(mensaje);

  if (strcmp(topico, TOPIC_CAM_CMD) != 0) return;

  if (strcmp(mensaje, "FOTO") == 0) {
    // Se descarta un cuadro: el primero suele venir con la
    // exposicion del momento anterior.
    camera_fb_t* fb = esp_camera_fb_get();
    if (fb) esp_camera_fb_return(fb);
    publicarEventoFoto();
  }
  else if (strcmp(mensaje, "FLASH_ON") == 0) {
    flashOn = true;
    digitalWrite(PIN_FLASH, HIGH);
    Serial.println(F("[CAM] Flash encendido"));
  }
  else if (strcmp(mensaje, "FLASH_OFF") == 0) {
    flashOn = false;
    digitalWrite(PIN_FLASH, LOW);
    Serial.println(F("[CAM] Flash apagado"));
  }
  else if (strcmp(mensaje, "ESTADO") == 0) {
    publicarEstado();
  }
  else {
    Serial.println(F("[CAM] Comando no reconocido"));
  }
}

// ════════════════════════════════════════════════════════════
//   CONEXION AL BROKER (los dos modos)
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
                             TOPIC_CAM_EST, 0, true, testamento);
  } else {
    Serial.print(F("  (usuario "));
    Serial.print(MQTT_USUARIO);
    Serial.println(')');
    conectado = mqtt.connect(ID_CLIENTE, MQTT_USUARIO, MQTT_CLAVE,
                             TOPIC_CAM_EST, 0, true, testamento);
  }

  if (conectado) {
    Serial.println(F("[MQTT] Conectado"));
    mqtt.subscribe(TOPIC_CAM_CMD);
    publicarEstado();
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
  // Silencia el reinicio por bajo voltaje (ver aviso del encabezado)
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  Serial.setDebugOutput(false);
  delay(500);
  Serial.println(F("\n=== ITLM - ESP32-CAM ALMACEN ==="));

  pinMode(PIN_FLASH, OUTPUT);
  digitalWrite(PIN_FLASH, LOW);
  pinMode(PIN_LED_ROJO, OUTPUT);
  digitalWrite(PIN_LED_ROJO, HIGH);      // apagado (logica invertida)

  if (!iniciarCamara()) {
    Serial.println(F("[FATAL] Reinicio en 5 s"));
    delay(5000);
    ESP.restart();
  }

  conectarWiFi();
  iniciarServidores();

  mqtt.setServer(MQTT_SERVIDOR, MQTT_PUERTO);
  mqtt.setCallback(alRecibir);
  mqtt.setKeepAlive(30);
  // El estado lleva las dos URL: no entra en los 256 bytes que
  // PubSubClient trae por defecto y el publish se descarta en
  // silencio. Por eso se agranda el buffer.
  mqtt.setBufferSize(512);
}

// ════════════════════════════════════════════════════════════
//   LOOP
// ════════════════════════════════════════════════════════════
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    conectarWiFi();
  }

  static unsigned long ultimoIntento = 0;
  if (!mqtt.connected()) {
    if (millis() - ultimoIntento > 5000) {
      ultimoIntento = millis();
      conectarMqtt();
    }
  } else {
    mqtt.loop();
  }

  // Latido: reconfirma el estado cada 60 s. Sirve para ver en
  // MQTT Explorer que la placa sigue viva y para actualizar la
  // IP si el router se la cambio.
  static unsigned long ultimoLatido = 0;
  if (mqtt.connected() && millis() - ultimoLatido > 60000) {
    ultimoLatido = millis();
    publicarEstado();
  }

  delay(10);     // deja trabajar a las tareas del servidor HTTP
}

#include <Adafruit_BMP280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <BH1750.h>
#include <esp_crt_bundle.h>
#include <esp_wifi.h>
#include <WiFiManager.h>
#include <Wire.h>
#include <Preferences.h>
#include "mqtt_client.h"

// ============================================
// Configuration Hardware
// ============================================
#define PIN_SOIL 4
#define PIN_BUTTON 0 
#define PIN_TOUCH 13 
#define PIN_BUZZER 17
#define PIN_NEOPIXEL 48
#define NUM_PIXELS 1
#define FACTORY_RESET_HOLD_MS 5000
#define CONFIG_PORTAL_TIMEOUT_SEC 300
#define CONFIG_AP_SSID "Station-Plante"
#define MQTT_RECONNECT_INTERVAL_MS 15000
#define MQTT_PUBLISH_INTERVAL_MS 3000

#define I2C_SDA 9
#define I2C_SCL 8

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

// Seuils pour le score de bonheur
#define SOIL_DRY 3000
#define SOIL_WET 1500

#ifndef STATION_ID
#define STATION_ID "adi_4"
#endif

// ============================================
// Variables Globales
// ============================================
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_BMP280 bmp;
BH1750 lightMeter;
Adafruit_NeoPixel pixel(NUM_PIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
Preferences prefs;

// Valeurs capteurs
float temperature = 0;
float pressure = 0;
float altitude = 0;
float soilHumidity = 0;
float lightLevel = 0;
int soilADC = 0;

// Variables de logique d'état
int currentScreen = 0;
const int NUM_NORMAL_SCREENS = 3; // 0:Scene, 1:Needs, 2:Journal
const int NUM_DEBUG_SCREENS = 4;  // 0:Capteurs, 1:Réseau, 2:Système, 3:Reset

bool debugMode = false;
int debugScreen = 0;
unsigned long touchPressStart = 0;
bool lastTouchStateIsPressed = false;
bool touchLongPressHandled = false; // Evite le skip d'écran au relâchement après long press
unsigned long lastTouchOverlayRefresh = 0; // Timer pour rafraîchir l'overlay
#define TOUCH_LONG_HOLD_MS 5000
#define TOUCH_OVERLAY_WARN_MS 2000 // Délai avant affichage avertissement debug

// MQTT Secure (WSS)
const char *MQTT_URI = "wss://iot.youcloud.ovh/mqtt";
const char *MQTT_TOPIC = "station/plantes/data";
const char *MQTT_CA_CERT = R"PEM(-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----)PEM";

esp_mqtt_client_handle_t mqtt_client = NULL;
bool mqtt_connected = false;
char mqtt_status_text[24] = "OFFLINE";
unsigned long lastMqttReconnectAttempt = 0;
unsigned long lastMqttTime = 0;

// Variables pour le score et streak
float smoothedScore = 3.0; // Score de 0 à 5
unsigned long lastStateChangeMs = 0;
unsigned long streakStartSec = 0; 
int currentProblemCode = 0; // 0=OK, 1=Soif, 2=Noyée, 3=Sombre, 4=Froid, 5=Chaud

// Historique NVS
#define HIST_SIZE 20
uint8_t scores_history[HIST_SIZE] = {0};
uint8_t histIndex = 0;

// Timers
unsigned long lastReadTime = 0;
unsigned long lastButtonTime = 0;
unsigned long buttonPressStart = 0;
unsigned long lastNvsSaveTime = 0;
unsigned long lastSceneRefreshTime = 0;
unsigned long uptimeSec = 0;
unsigned long lastUptimeUpdateMs = 0;
int lastButtonState = HIGH;
int lastTouchState = LOW;
// factoryResetArmed supprimé — reset géré uniquement via mode debug tactile

// ============================================
// Prototypes
// ============================================
void readSensors();
void updateHappinessScore();
void saveHistoryNVS();
void loadHistoryNVS();
void addHistoryPoint(int stars);
void refreshDisplay();
void ensureMQTTConnection();
void initMQTT();
void setMqttStatus(const char *status);
void formatDuration(unsigned long seconds, char* buffer);
void getProblemText(int code, char* buffer, unsigned long durationSec);
void refreshDisplay(unsigned long touchElapsedMs);
void drawNeedsScreen();
void drawResetProgressBar(unsigned long elapsedMs);
void drawWaterDrop(int x, int y);
void drawSunIcon(int x, int y);
void drawThermometerIcon(int x, int y);
void drawDebugSensorsScreen();
void drawDebugNetworkScreen();
void drawDebugSystemScreen();
void drawDebugResetScreen();
void drawLongPressOverlay(unsigned long elapsedMs);

// ============================================
// MQTT Events
// ============================================
void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        mqtt_connected = true;
        setMqttStatus("OK");
        break;
    case MQTT_EVENT_DISCONNECTED:
        mqtt_connected = false;
        setMqttStatus("DISC");
        break;
    case MQTT_EVENT_ERROR:
        mqtt_connected = false;
        setMqttStatus("ERROR");
        break;
    default:
        break;
    }
}

void setMqttStatus(const char *status) {
  if (status) snprintf(mqtt_status_text, sizeof(mqtt_status_text), "%s", status);
}

void initMQTT() {
  esp_mqtt_client_config_t mqtt_cfg = {};
  mqtt_cfg.uri = MQTT_URI;
  mqtt_cfg.cert_pem = MQTT_CA_CERT;
  mqtt_cfg.client_id = STATION_ID;
  mqtt_cfg.reconnect_timeout_ms = MQTT_RECONNECT_INTERVAL_MS;
  mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
  esp_mqtt_client_register_event(mqtt_client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
  setMqttStatus("CONN...");
  esp_mqtt_client_start(mqtt_client);
}

void ensureMQTTConnection() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (mqtt_connected || mqtt_client == NULL) return;
  if (millis() - lastMqttReconnectAttempt < MQTT_RECONNECT_INTERVAL_MS) return;
  lastMqttReconnectAttempt = millis();
  setMqttStatus("RETRY");
  esp_mqtt_client_reconnect(mqtt_client);
}

// ============================================
// Core Logic : Tamagotchi Score & NVS
// ============================================
void loadHistoryNVS() {
  prefs.begin("plant", true); // read-only
  size_t len = prefs.getBytes("scores", scores_history, HIST_SIZE);
  if (len == 0) {
    for(int i=0; i<HIST_SIZE; i++) scores_history[i] = 3; // Default
  }
  histIndex = prefs.getUChar("idx", 0);
  streakStartSec = prefs.getUInt("streakT", 0); // en temps absolu ? Non, l'ESP n'a pas de RTC par défaut
  // Si pas de RTC, on va utiliser l'uptime et stocker un diff lors du boot.
  // Pour la simplicité ici, on va recréer le streak à 0 au boot, car sans WiFi+NTP on ne connaît pas l'heure réelle.
  // Pour faire mieux, on pourrait stocker la durée totale, mais restons simple :
  uptimeSec = prefs.getUInt("upSec", 0);
  streakStartSec = prefs.getUInt("streakStart", uptimeSec); 
  prefs.end();
}

void saveHistoryNVS() {
  prefs.begin("plant", false); // rw
  prefs.putBytes("scores", scores_history, HIST_SIZE);
  prefs.putUChar("idx", histIndex);
  prefs.putUInt("upSec", uptimeSec);
  prefs.putUInt("streakStart", streakStartSec);
  prefs.end();
}

void addHistoryPoint(int stars) {
  scores_history[histIndex] = (uint8_t)stars;
  histIndex = (histIndex + 1) % HIST_SIZE;
  saveHistoryNVS();
}

void updateHappinessScore() {
  // Score Eau (0.0 à 1.0)
  float sEau = 0;
  int probEau = 0;
  if (soilHumidity >= 40 && soilHumidity <= 70) { sEau = 1.0; probEau = 0; }
  else if (soilHumidity >= 30 && soilHumidity <= 85) { sEau = 0.5; probEau = (soilHumidity < 40) ? 1 : 2; }
  else { sEau = 0.0; probEau = (soilHumidity < 30) ? 1 : 2; }

  // Score Lumière
  float sLum = 0;
  int probLum = 0;
  if (lightLevel >= 200) { sLum = 1.0; probLum = 0; }
  else if (lightLevel >= 50) { sLum = 0.5; probLum = 3; }
  else { sLum = 0.0; probLum = 3; }

  // Score Température
  float sTemp = 0;
  int probTemp = 0;
  if (temperature >= 18 && temperature <= 26) { sTemp = 1.0; probTemp = 0; }
  else if ((temperature >= 15 && temperature < 18) || (temperature > 26 && temperature <= 30)) { sTemp = 0.5; probTemp = (temperature < 18) ? 4 : 5; }
  else { sTemp = 0.0; probTemp = (temperature < 15) ? 4 : 5; }

  float rawScore = ((sEau + sLum + sTemp) / 3.0) * 5.0; // Sur 5
  
  // Déterminer le problème prioritaire (0 = OK)
  int newProblem = 0;
  if (probEau == 1) newProblem = 1; // Soif prime
  else if (probTemp == 5) newProblem = 5; // Chaud
  else if (probTemp == 4) newProblem = 4; // Froid
  else if (probEau == 2) newProblem = 2; // Noyée
  else if (probLum == 3) newProblem = 3; // Sombre

  // Lissage du score
  smoothedScore = (0.3 * rawScore) + (0.7 * smoothedScore);
  
  // Gestion du Streak
  if (newProblem != currentProblemCode) {
    currentProblemCode = newProblem;
    streakStartSec = uptimeSec;
    saveHistoryNVS();
  }
}

void formatDuration(unsigned long seconds, char* buffer) {
  if (seconds < 60) snprintf(buffer, 16, "%lu sec", seconds);
  else if (seconds < 3600) snprintf(buffer, 16, "%lu min", seconds / 60);
  else if (seconds < 86400) snprintf(buffer, 16, "%lu h", seconds / 3600);
  else snprintf(buffer, 16, "%lu j", seconds / 86400);
}

void getProblemText(int code, char* buffer, unsigned long durationSec) {
  char timeStr[16];
  formatDuration(durationSec, timeStr);
  switch (code) {
    case 0: snprintf(buffer, 32, "Je suis bien depuis %s", timeStr); break;
    case 1: snprintf(buffer, 32, "J'ai soif depuis %s", timeStr); break;
    case 2: snprintf(buffer, 32, "Je me noie depuis %s", timeStr); break;
    case 3: snprintf(buffer, 32, "Je suis a l'ombre depuis %s", timeStr); break;
    case 4: snprintf(buffer, 32, "J'ai froid depuis %s", timeStr); break;
    case 5: snprintf(buffer, 32, "J'ai chaud depuis %s", timeStr); break;
    default: snprintf(buffer, 32, "Etat inconnu"); break;
  }
}

// ============================================
// drawStar — 7x5px, lisible sur SSD1306
// ============================================
void drawStar(int x, int y, bool filled) {
  // Forme en losange-étoile 7px de large
  display.drawPixel(x+3, y,   SSD1306_WHITE);
  display.drawPixel(x+2, y+1, SSD1306_WHITE);
  display.drawPixel(x+3, y+1, SSD1306_WHITE);
  display.drawPixel(x+4, y+1, SSD1306_WHITE);
  if (filled) {
    display.drawLine(x,   y+2, x+6, y+2, SSD1306_WHITE);
    display.drawLine(x+1, y+3, x+5, y+3, SSD1306_WHITE);
  } else {
    display.drawPixel(x,   y+2, SSD1306_WHITE);
    display.drawPixel(x+6, y+2, SSD1306_WHITE);
    display.drawPixel(x+1, y+3, SSD1306_WHITE);
    display.drawPixel(x+5, y+3, SSD1306_WHITE);
  }
  display.drawPixel(x+2, y+4, SSD1306_WHITE);
  display.drawPixel(x+4, y+4, SSD1306_WHITE);
}

// ============================================
// drawSky — confiné à x < 42 pour ne pas
// déborder sur la zone plante (cx=62)
// ============================================
void drawSky(int lux) {
  if (lux > 500) {
    // Soleil
    int anim = (millis() / 500) % 2;
    display.drawCircle(16, 10, 5, SSD1306_WHITE);
    if (anim) {
      display.drawLine(16, 2,  16, 0,  SSD1306_WHITE);
      display.drawLine(16, 18, 16, 20, SSD1306_WHITE);
      display.drawLine(8,  10, 6,  10, SSD1306_WHITE);
      display.drawLine(24, 10, 26, 10, SSD1306_WHITE);
    } else {
      display.drawLine(10, 4,  8,  2,  SSD1306_WHITE);
      display.drawLine(22, 16, 24, 18, SSD1306_WHITE);
      display.drawLine(10, 16, 8,  18, SSD1306_WHITE);
      display.drawLine(22, 4,  24, 2,  SSD1306_WHITE);
    }
  } else if (lux > 50) {
    // Nuages (max x=40)
    display.fillCircle(18, 11, 4, SSD1306_WHITE);
    display.fillCircle(24, 9,  6, SSD1306_WHITE);
    display.fillCircle(30, 11, 4, SSD1306_WHITE);
    display.fillRect(18, 11, 12, 5, SSD1306_WHITE);
  } else {
    // Lune croissant
    display.fillCircle(16, 11, 5, SSD1306_WHITE);
    display.fillCircle(19, 8,  5, SSD1306_BLACK);
  }
}

// ============================================
// drawGiantPlant — cx=62, ySol=50
// Fix: fRad cappé pour éviter dessin hors écran
// ============================================
void drawGiantPlant(int cx, int baseY, int stars, int problemCode) {
  // stemTop ajusté : jamais < 10 pour garder fleur visible
  int stemTop = 10;
  if (problemCode == 1) stemTop = 20; // Soif: plante affaissée
  if (problemCode == 4) stemTop = 23; // Froid: plante ramassée

  // Pot
  int potW = 28, potH = 10;
  int potX  = cx - potW/2;
  int potTop = baseY - potH;
  display.fillRect(potX, potTop, potW, potH, SSD1306_WHITE);
  display.fillRect(potX+2, potTop+2, potW-4, potH-4, SSD1306_BLACK);
  display.fillRect(potX-3, potTop-3, potW+6, 4, SSD1306_WHITE);
  display.fillRect(potX-1, potTop-2, potW+2, 2, SSD1306_BLACK);

  int stemBase = potTop - 3;

  // Tige (inclinée si soif)
  int cxStem = cx;
  if (problemCode == 1) {
    display.drawLine(cx,   stemBase, cx-7, stemTop, SSD1306_WHITE);
    display.drawLine(cx+1, stemBase, cx-6, stemTop, SSD1306_WHITE);
    display.drawLine(cx-1, stemBase, cx-8, stemTop, SSD1306_WHITE);
    cxStem = cx - 7;
  } else {
    display.drawLine(cx-1, stemBase, cx-1, stemTop, SSD1306_WHITE);
    display.drawLine(cx,   stemBase, cx,   stemTop, SSD1306_WHITE);
    display.drawLine(cx+1, stemBase, cx+1, stemTop, SSD1306_WHITE);
  }

  // Feuilles
  int leafY = stemBase - (stemBase - stemTop) / 3;
  if (stars >= 4) {
    display.fillCircle(cxStem-14, leafY-2, 7, SSD1306_WHITE);
    display.fillCircle(cxStem-9,  leafY+1, 5, SSD1306_WHITE);
    display.fillCircle(cxStem+14, leafY-4, 7, SSD1306_WHITE);
    display.fillCircle(cxStem+9,  leafY-1, 5, SSD1306_WHITE);
    // Tige redessinée par-dessus les feuilles
    display.drawLine(cx-1, stemBase, cx-1, stemTop, SSD1306_WHITE);
    display.drawLine(cx,   stemBase, cx,   stemTop, SSD1306_WHITE);
    display.drawLine(cx+1, stemBase, cx+1, stemTop, SSD1306_WHITE);
  } else if (problemCode == 1 || problemCode == 5) {
    display.fillCircle(cxStem-8, leafY+6, 4, SSD1306_WHITE);
    display.fillCircle(cxStem+8, leafY+5, 4, SSD1306_WHITE);
  } else {
    display.fillCircle(cxStem-11, leafY,   5, SSD1306_WHITE);
    display.fillCircle(cxStem+11, leafY-2, 5, SSD1306_WHITE);
  }

  // Fleur — fRad cappé pour rester dans l'écran
  int fRad = (stars >= 4) ? 9 : (stars >= 3 ? 6 : 4);
  fRad = min(fRad, stemTop - 2); // JAMAIS hors écran

  if (problemCode == 1 || stars < 2) {
    // Bouton fermé / fanée
    display.drawCircle(cxStem, stemTop, fRad, SSD1306_WHITE);
    display.drawPixel(cxStem-2, stemTop+fRad+2, SSD1306_WHITE);
    display.drawPixel(cxStem+2, stemTop+fRad+3, SSD1306_WHITE);
  } else {
    // Fleur épanouie 6 pétales
    int pr = fRad - 1;
    display.fillCircle(cxStem,        stemTop-fRad,    pr, SSD1306_WHITE);
    display.fillCircle(cxStem+fRad,   stemTop-fRad/2,  pr, SSD1306_WHITE);
    display.fillCircle(cxStem+fRad,   stemTop+fRad/2,  pr, SSD1306_WHITE);
    display.fillCircle(cxStem,        stemTop+fRad,    pr, SSD1306_WHITE);
    display.fillCircle(cxStem-fRad,   stemTop+fRad/2,  pr, SSD1306_WHITE);
    display.fillCircle(cxStem-fRad,   stemTop-fRad/2,  pr, SSD1306_WHITE);
    display.fillCircle(cxStem, stemTop, fRad,    SSD1306_BLACK);
    display.fillCircle(cxStem, stemTop, fRad/2+1, SSD1306_WHITE);
  }
}

// ============================================
// drawSceneScreen — layout retravaillé
//   ySol=50  |  text y=52  |  dots y=61
// ============================================
void drawSceneScreen() {
  int stars = constrain(round(smoothedScore), 0, 5);
  int ySol  = 55;

  // Ciel (zone gauche, x < 42)
  drawSky((int)lightLevel);

  // Etoiles (haut droite, 7px + 2px gap = 9px/étoile)
  for (int i = 0; i < 5; i++) drawStar(83 + i*9, 1, i < stars);

  // Plante centrée sur cx=62
  drawGiantPlant(62, ySol, stars, currentProblemCode);

  // Sol
  display.drawLine(0, ySol,   128, ySol,   SSD1306_WHITE);
  display.drawLine(0, ySol+1, 128, ySol+1, SSD1306_WHITE);

  // Texte statut centré à y=52 (bottom y=59, dots top y=59 → juste tangent)
  unsigned long duration = uptimeSec > streakStartSec ? (uptimeSec - streakStartSec) : 0;
  char buf[32]; char timeStr[10];
  formatDuration(duration, timeStr);
  switch (currentProblemCode) {
    case 0: snprintf(buf, sizeof(buf), "Heureuse: %s", timeStr); break;
    case 1: snprintf(buf, sizeof(buf), "J'ai soif: %s", timeStr); break;
    case 2: snprintf(buf, sizeof(buf), "Je me noie: %s", timeStr); break;
    case 3: snprintf(buf, sizeof(buf), "A l'ombre: %s", timeStr); break;
    case 4: snprintf(buf, sizeof(buf), "J'ai froid: %s", timeStr); break;
    case 5: snprintf(buf, sizeof(buf), "J'ai chaud: %s", timeStr); break;
    default: snprintf(buf, sizeof(buf), "..."); break;
  }
  int tw = strlen(buf) * 6;
  display.setCursor((128 - tw) / 2, 57);
  display.print(buf);
}

// ============================================
// drawJournalScreen — yBot remonté à 50
// ============================================
void drawJournalScreen() {
  const char* title = "HUMEUR (4H)";
  int tw2 = strlen(title) * 6;
  display.setCursor((128-tw2)/2, 0);
  display.print(title);
  display.drawLine(0, 9, 128, 9, SSD1306_WHITE);

  int xS = 10, yBot = 50, gW = 108, gH = 36;
  display.drawLine(xS, yBot,    xS+gW, yBot,    SSD1306_WHITE);
  display.drawLine(xS, yBot,    xS,    yBot-gH, SSD1306_WHITE);
  display.setCursor(0, yBot-gH); display.print("+");
  display.setCursor(0, yBot-6);  display.print("-");

  int stepX = gW / HIST_SIZE;
  for(int i=0; i<HIST_SIZE; i++) {
    int idx  = (histIndex + i) % HIST_SIZE;
    int val  = min((int)scores_history[idx], 5);
    int x    = xS + 2 + (i * stepX);
    int y    = yBot - 2 - ((val * (gH-4)) / 5);
    display.fillCircle(x, y, 1, SSD1306_WHITE);
    if(i > 0) {
      int pIdx = (histIndex + i - 1) % HIST_SIZE;
      int pVal = min((int)scores_history[pIdx], 5);
      int pX   = xS + 2 + ((i-1) * stepX);
      int pY   = yBot - 2 - ((pVal * (gH-4)) / 5);
      display.drawLine(pX, pY, x, y, SSD1306_WHITE);
    }
  }
}

void drawWaterDrop(int x, int y) {
  display.drawPixel(x + 3, y,     SSD1306_WHITE);
  display.drawPixel(x + 3, y + 1, SSD1306_WHITE);
  display.drawLine(x + 2, y + 2, x + 4, y + 2, SSD1306_WHITE);
  display.drawLine(x + 1, y + 3, x + 5, y + 3, SSD1306_WHITE);
  display.drawLine(x,     y + 4, x + 6, y + 4, SSD1306_WHITE);
  display.drawLine(x,     y + 5, x + 6, y + 5, SSD1306_WHITE);
  display.drawLine(x + 1, y + 6, x + 5, y + 6, SSD1306_WHITE);
  display.drawLine(x + 2, y + 7, x + 4, y + 7, SSD1306_WHITE);
}

void drawSunIcon(int x, int y) {
  display.drawCircle(x + 4, y + 4, 2, SSD1306_WHITE);
  display.drawPixel(x + 4, y,     SSD1306_WHITE);
  display.drawPixel(x + 4, y + 8, SSD1306_WHITE);
  display.drawPixel(x,     y + 4, SSD1306_WHITE);
  display.drawPixel(x + 8, y + 4, SSD1306_WHITE);
  display.drawPixel(x + 1, y + 1, SSD1306_WHITE);
  display.drawPixel(x + 7, y + 1, SSD1306_WHITE);
  display.drawPixel(x + 1, y + 7, SSD1306_WHITE);
  display.drawPixel(x + 7, y + 7, SSD1306_WHITE);
}

void drawThermometerIcon(int x, int y) {
  display.drawRoundRect(x, y, 5, 10, 2, SSD1306_WHITE);
  display.drawPixel(x + 2, y + 2, SSD1306_WHITE);
  display.drawPixel(x + 2, y + 4, SSD1306_WHITE);
  display.drawPixel(x + 2, y + 6, SSD1306_WHITE);
  display.fillCircle(x + 2, y + 7, 1, SSD1306_WHITE);
}

void drawDebugResetScreen() {
  display.setCursor(15, 0);
  display.print(F("REINITIALISATION"));
  display.drawLine(0, 9, 128, 9, SSD1306_WHITE);

  display.setCursor(0, 16);
  display.println(F("Appui tactile long (5s)"));
  display.println(F("pour reset le WiFi."));
  
  display.setCursor(0, 40);
  display.println(F("Appui tactile court"));
  display.println(F("pour quitter le debug."));
}

// ============================================
// drawNeedsScreen — Liste des besoins
// ============================================
void drawNeedsScreen() {
  const char* title = "HUMEUR";
  int tw = strlen(title) * 6;
  display.setCursor((128 - tw)/2, 0);
  display.print(title);
  display.drawLine(0, 9, 128, 9, SSD1306_WHITE);

  // Eau
  drawWaterDrop(4, 13);
  display.setCursor(18, 14);
  bool eauBad = (currentProblemCode == 1 || currentProblemCode == 2);
  if (eauBad) display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  if (currentProblemCode == 1) display.print(F(" J'ai soif ! "));
  else if (currentProblemCode == 2) display.print(F(" Je me noie ! "));
  else if (soilHumidity < 40) display.print(F("Un peu soif"));
  else if (soilHumidity > 70) display.print(F("Trop d'eau !"));
  else display.print(F("Parfait"));
  display.setTextColor(SSD1306_WHITE);

  // Lumiere
  drawSunIcon(3, 27);
  display.setCursor(18, 28);
  if (currentProblemCode == 3) display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  if (currentProblemCode == 3) display.print(F(" A l'ombre ! "));
  else if (lightLevel < 200) display.print(F("Besoin de soleil"));
  else display.print(F("Parfait"));
  display.setTextColor(SSD1306_WHITE);

  // Temperature
  drawThermometerIcon(5, 41);
  display.setCursor(18, 42);
  bool tmpBad = (currentProblemCode == 4 || currentProblemCode == 5);
  if (tmpBad) display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  if (currentProblemCode == 4) display.print(F(" J'ai froid ! "));
  else if (currentProblemCode == 5) display.print(F(" J'ai chaud ! "));
  else display.print(F("Parfait"));
  display.setTextColor(SSD1306_WHITE);
}

// ============================================
// DEBUG SCREEN 0 : Capteurs
// ============================================
void drawDebugSensorsScreen() {
  display.setCursor(22, 0);
  display.print(F("[DBG] CAPTEURS"));
  display.drawLine(0, 9, 128, 9, SSD1306_WHITE);

  display.setCursor(0, 13);
  display.printf("Temp  : %.1f C", temperature);
  display.setCursor(0, 24);
  display.printf("Pres  : %.0f hPa", pressure);
  display.setCursor(0, 35);
  display.printf("Sol   : %.0f %%", soilHumidity);
  display.setCursor(0, 46);
  display.printf("Lux   : %.0f lx", lightLevel);
  display.setCursor(0, 57);
  display.printf("ADC   : %d", soilADC);
}

// ============================================
// DEBUG SCREEN 1 : Réseau
// ============================================
void drawDebugNetworkScreen() {
  display.setCursor(25, 0);
  display.print(F("[DBG] RESEAU"));
  display.drawLine(0, 9, 128, 9, SSD1306_WHITE);

  bool wifiOk = (WiFi.status() == WL_CONNECTED);
  display.setCursor(0, 13);
  display.printf("WiFi  : %s", wifiOk ? "CONNECTE" : "OFFLINE");
  display.setCursor(0, 24);
  display.print("IP    : ");
  display.print(wifiOk ? WiFi.localIP().toString().c_str() : "N/A");
  display.setCursor(0, 35);
  display.printf("MQTT  : %s", mqtt_status_text);
  display.setCursor(0, 46);
  display.printf("RSSI  : %d dBm", wifiOk ? WiFi.RSSI() : 0);
  display.setCursor(0, 57);
  display.printf("ID    : %s", STATION_ID);
}

// ============================================
// DEBUG SCREEN 2 : Système
// ============================================
void drawDebugSystemScreen() {
  display.setCursor(22, 0);
  display.print(F("[DBG] SYSTEME"));
  display.drawLine(0, 9, 128, 9, SSD1306_WHITE);

  char upStr[16];
  formatDuration(uptimeSec, upStr);
  display.setCursor(0, 13);
  display.printf("Uptime : %s", upStr);
  display.setCursor(0, 24);
  display.printf("Score  : %.2f / 5", smoothedScore);
  display.setCursor(0, 35);
  display.printf("Etat   : code %d", currentProblemCode);
  display.setCursor(0, 46);
  char streakStr[16];
  formatDuration(uptimeSec > streakStartSec ? uptimeSec - streakStartSec : 0, streakStr);
  display.printf("Streak : %s", streakStr);
  display.setCursor(0, 57);
  display.printf("RAM    : %lu B", ESP.getFreeHeap());
}

// ============================================
// drawResetProgressBar — Barre de progression
// ============================================
void drawResetProgressBar(unsigned long elapsedMs) {
  display.clearDisplay();

  // Titre
  display.setCursor(10, 4);
  display.print(F("RESET WIFI EN COURS"));

  // Icône attention
  display.setCursor(55, 18);
  display.print(F("[ ! ]"));

  // Barre de progression
  int barX = 8, barY = 38, barW = 112, barH = 10;
  display.drawRect(barX, barY, barW, barH, SSD1306_WHITE);
  int fill = (int)((elapsedMs * (barW - 4)) / TOUCH_LONG_HOLD_MS);
  fill = constrain(fill, 0, barW - 4);
  display.fillRect(barX + 2, barY + 2, fill, barH - 4, SSD1306_WHITE);

  // Pourcentage
  int pct = (int)((elapsedMs * 100) / TOUCH_LONG_HOLD_MS);
  char pctBuf[6];
  snprintf(pctBuf, sizeof(pctBuf), "%d%%", pct);
  int tw = strlen(pctBuf) * 6;
  display.setCursor((128 - tw) / 2, 52);
  display.print(pctBuf);

  display.display();
}

// ============================================
// drawLongPressOverlay — Overlay avertissement
// s'affiche après 2s d'appui, compte à rebours
// ============================================
void drawLongPressOverlay(unsigned long elapsedMs) {
  // Rectangle de fond noir avec bordure
  display.fillRect(2, 48, 124, 14, SSD1306_BLACK);
  display.drawRect(2, 48, 124, 14, SSD1306_WHITE);

  // Message
  int secsLeft = (int)((TOUCH_LONG_HOLD_MS - elapsedMs) / 1000) + 1;
  char buf[28];
  snprintf(buf, sizeof(buf), "DEBUG dans %d sec...", secsLeft);
  int tw = strlen(buf) * 6;
  display.setCursor((128 - tw) / 2, 52);
  display.print(buf);
}

// ============================================
// refreshDisplay — dots à y=61 (était 62)
// Cercle r=2 → top=59, bottom=63 : dans les 64px
// ============================================
void refreshDisplay() {
  refreshDisplay(0); // Pas d'overlay par défaut
}

void refreshDisplay(unsigned long touchElapsedMs) {
  display.clearDisplay();

  if (debugMode) {
    switch (debugScreen) {
      case 0: drawDebugSensorsScreen(); break;
      case 1: drawDebugNetworkScreen(); break;
      case 2: drawDebugSystemScreen();  break;
      case 3: drawDebugResetScreen();   break;
    }
  } else {
    switch (currentScreen) {
      case 0: drawSceneScreen();   break;
      case 1: drawNeedsScreen();   break;
      case 2: drawJournalScreen(); break;
    }
  }

  // Navigation dots - small 1-3px max
  if (debugMode) {
    int totalDotsW = NUM_DEBUG_SCREENS * 3 + (NUM_DEBUG_SCREENS - 1) * 4;
    int dotX = (128 - totalDotsW) / 2;
    for (int i = 0; i < NUM_DEBUG_SCREENS; i++) {
      int x = dotX + i * 7;
      if (i == debugScreen) display.fillRect(x, 62, 3, 1, SSD1306_WHITE);
      else                  display.drawPixel(x + 1, 62, SSD1306_WHITE);
    }
  } else if (currentScreen != 0) {
    int totalDotsW = NUM_NORMAL_SCREENS * 3 + (NUM_NORMAL_SCREENS - 1) * 4;
    int dotX = (128 - totalDotsW) / 2;
    for (int i = 0; i < NUM_NORMAL_SCREENS; i++) {
      int x = dotX + i * 7;
      if (i == currentScreen) display.fillRect(x, 62, 3, 1, SSD1306_WHITE);
      else                  display.drawPixel(x + 1, 62, SSD1306_WHITE);
    }
  }

  // Overlay avertissement long press (uniquement en mode normal, après 2s)
  if (!debugMode && touchElapsedMs >= TOUCH_OVERLAY_WARN_MS && touchElapsedMs < TOUCH_LONG_HOLD_MS) {
    drawLongPressOverlay(touchElapsedMs);
  }

  display.display();
}

// ============================================
// Setup
// ============================================
void setup() {
  Serial.begin(115200);
  delay(1000); 

  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_TOUCH, INPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  pixel.begin();
  pixel.setBrightness(20);
  pixel.setPixelColor(0, pixel.Color(0, 50, 0));
  pixel.show();

  Wire.begin(I2C_SDA, I2C_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED Fail");
    while(1) delay(100);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 20);
  display.println(F("BOOTING..."));
  display.display();

  if (!bmp.begin(0x76)) Serial.println("BMP280 Fail");
  if (!lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) Serial.println("BH1750 Fail");

  loadHistoryNVS();

  WiFiManager wm;
  wm.setConnectTimeout(15);
  wm.setConfigPortalTimeout(CONFIG_PORTAL_TIMEOUT_SEC);

  display.clearDisplay();
  display.setCursor(10, 20);
  display.println(F("WIFI CONNECT..."));
  display.display();

  if (!wm.autoConnect(CONFIG_AP_SSID)) {
    Serial.println("WiFi Fail");
    delay(2000);
  } else {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    display.clearDisplay();
    display.setCursor(10, 20);
    display.println(F("SYNC TIME..."));
    display.display();
    time_t now = time(nullptr);
    int retries = 0;
    while (now < 1000000000 && retries < 20) {
      delay(500);
      now = time(nullptr);
      retries++;
    }
    initMQTT();
  }

  readSensors();
  updateHappinessScore();
  refreshDisplay();
}

void readSensors() {
  temperature = bmp.readTemperature();
  pressure = bmp.readPressure() / 100.0F;
  float instantAlt = bmp.readAltitude(1013.25);
  if (altitude == 0) altitude = instantAlt;
  else altitude = (instantAlt * 0.2) + (altitude * 0.8);

  soilADC = analogRead(PIN_SOIL);
  soilHumidity = map(soilADC, SOIL_DRY, SOIL_WET, 0, 100);
  soilHumidity = constrain(soilHumidity, 0, 100);
  lightLevel = lightMeter.readLightLevel();
}

// ============================================
// Loop
// ============================================
void loop() {
  unsigned long now = millis();

  // Uptime update
  if (now - lastUptimeUpdateMs >= 1000) {
    uptimeSec += (now - lastUptimeUpdateMs) / 1000;
    lastUptimeUpdateMs = now;
  }

  // Navigation Bouton & Touch
  int btnState = digitalRead(PIN_BUTTON);
  int touchState = digitalRead(PIN_TOUCH);
  bool touchIsPressed = (touchState == HIGH);

  // Bouton Physique Logic
  if (btnState == LOW && lastButtonState == HIGH) {
    buttonPressStart = now;
  }
  if (btnState == HIGH && lastButtonState == LOW) {
    if (now - buttonPressStart < 1000) { // Court appui
      if (debugMode) {
        debugScreen = (debugScreen + 1) % NUM_DEBUG_SCREENS;
        if (debugScreen == 0) { debugMode = false; } // Sort après le tour complet
      } else {
        currentScreen = (currentScreen + 1) % NUM_NORMAL_SCREENS;
      }
      refreshDisplay();
    }
  }

  // Capteur Tactile Logic
  if (touchIsPressed && !lastTouchStateIsPressed) {
    // Front montant : début d'un appui
    touchPressStart = now;
    lastTouchStateIsPressed = true;
    touchLongPressHandled = false;
  } else if (!touchIsPressed && lastTouchStateIsPressed) {
    // Front descendant : relâchement
    unsigned long elapsed = now - touchPressStart;
    if (!touchLongPressHandled && elapsed < TOUCH_LONG_HOLD_MS) {
      // Court appui
      if (now - lastButtonTime > 250) {
        if (debugMode) {
          debugScreen = (debugScreen + 1) % NUM_DEBUG_SCREENS;
          if (debugScreen == 0) { debugMode = false; } // Sort du debug après avoir fait le tour
        } else {
          currentScreen = (currentScreen + 1) % NUM_NORMAL_SCREENS;
        }
        lastButtonTime = now;
        refreshDisplay();
      }
    }
    lastTouchStateIsPressed = false;
    touchLongPressHandled = false;
  }

  // Vérification de l'appui long tactile en continu
  if (touchIsPressed && lastTouchStateIsPressed && !touchLongPressHandled) {
    unsigned long elapsed = now - touchPressStart;
    if (elapsed >= TOUCH_LONG_HOLD_MS) {
      // Long press atteint
      touchLongPressHandled = true;
      if (!debugMode) {
        // Passage en mode debug
        debugMode = true;
        debugScreen = 0;
        refreshDisplay();
      } else if (debugScreen == NUM_DEBUG_SCREENS - 1) {
        // Reset d'usine depuis l'écran Reset
        display.clearDisplay();
        display.setCursor(10, 28);
        display.println(F("RESET WIFI..."));
        display.display();
        WiFiManager wm;
        wm.resetSettings();
        delay(1000);
        ESP.restart();
      }
    } else if (debugMode && debugScreen == NUM_DEBUG_SCREENS - 1 && elapsed >= TOUCH_OVERLAY_WARN_MS) {
      // Barre de progression pendant l'appui sur l'écran Reset
      drawResetProgressBar(elapsed);
    } else if (!debugMode && elapsed >= TOUCH_OVERLAY_WARN_MS) {
      // Overlay avertissement en mode normal — rafraîchir toutes les 200ms
      if (now - lastTouchOverlayRefresh > 200) {
        lastTouchOverlayRefresh = now;
        refreshDisplay(elapsed);
      }
    }
  }

  lastButtonState = btnState;
  lastTouchState = touchState;

  // Lecture Capteurs (2s)
  if (now - lastReadTime > 2000) {
    readSensors();
    updateHappinessScore();
    lastReadTime = now;
    
    // LED Statut
    if (currentProblemCode == 1) pixel.setPixelColor(0, pixel.Color(50, 20, 0)); // Orange/Soif
    else if (currentProblemCode == 4) pixel.setPixelColor(0, pixel.Color(0, 0, 50)); // Bleu/Froid
    else pixel.setPixelColor(0, pixel.Color(0, 20, 0)); // Vert/OK
    pixel.show();

    if (!debugMode && currentScreen == 0) refreshDisplay();
  }

  // Animation Scène fluide (si écran 0)
  if (!debugMode && currentScreen == 0 && (now - lastSceneRefreshTime > 250)) {
    lastSceneRefreshTime = now;
    refreshDisplay();
  }

  // Historique NVS sauvegarde score par intervalles de 12 mins (720s)
  if (uptimeSec - lastNvsSaveTime > 720) {
    addHistoryPoint(round(smoothedScore));
    lastNvsSaveTime = uptimeSec;
  }

  // MQTT (3s)
  if (now - lastMqttTime > MQTT_PUBLISH_INTERVAL_MS) {
    if (mqtt_connected) {
      StaticJsonDocument<256> doc;
      doc["station_id"] = STATION_ID;
      doc["temperature"] = temperature;
      doc["pressure"] = pressure;
      doc["soil_humidity"] = soilHumidity;
      doc["light_level"] = lightLevel;
      doc["happiness"] = smoothedScore;
      doc["problem_code"] = currentProblemCode;
      
      char buffer[256];
      serializeJson(doc, buffer);
      esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC, buffer, 0, 1, 0);
    } else {
      ensureMQTTConnection();
    }
    lastMqttTime = now;
  }

  delay(10);
}

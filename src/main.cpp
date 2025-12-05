#include <WiFi.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <Update.h>
#include <Arduino.h>
#include <Preferences.h>
#include "webpage.h"

// =========================
// CONFIGURATIONS
// =========================
const char* WIFI_SSID = "uaifai_IoT";   // default
const char* WIFI_PASS = "supersuper";

const char* MQTT_HOST = "192.168.0.127";
const uint16_t MQTT_PORT = 1883;

const char* TOPIC    = "casa/lavanderia/lampada";
const char* HOSTNAME = "lampada_lavanderia";

// =========================
// PIN DEFINITIONS (Mini R4)
// =========================
#define PIN_RELAY   26
#define PIN_LED     19
#define PIN_SWITCH  27
#define DEBOUNCE_MS 50

// =========================
// GLOBAL STATE
// =========================
volatile int lampState = 0;   // 0=off, 1=on
unsigned long lastDebounce = 0;
int lastStableState = HIGH;
int lastReading = HIGH;

// =========================
// OBJECTS
// =========================
WiFiClient   espClient;
PubSubClient mqtt(espClient);
WebServer    server(80);
WebPage      page(&server);

// =========================
// FORWARD DECLARATIONS
// =========================
void connectWiFiWithFallback();
void startFallbackAP();

void syncRelay();
void publishState();
void toggleLamp();

// =========================
// TOGGLE LAMP (WEB + MQTT + FÍSICO)
// =========================
void toggleLamp() {
    lampState = !lampState;
    syncRelay();

    // Só publica no MQTT se estiver conectado
    if (WiFi.getMode() == WIFI_MODE_STA &&
        WiFi.status() == WL_CONNECTED &&
        mqtt.connected()) {
        publishState();
    }

    page.setStatus(lampState);
    Serial.println("[ACTION] Toggle -> estado = " + String(lampState));
}

// =========================
// MQTT CALLBACK
// =========================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String msg;
    for (unsigned int i = 0; i < length; i++) {
        msg += (char)payload[i];
    }
    msg.trim();

    if (msg == "0")      lampState = 0;
    else if (msg == "1") lampState = 1;

    syncRelay();
    page.setStatus(lampState);

    Serial.println("[MQTT] Novo estado recebido: " + msg);
}

// =========================
// WIFI CONNECT OR FALLBACK AP
// =========================
bool tryConnectWiFi(String ssid, String pass, uint16_t timeoutSec) {
    Serial.println("[WiFi] Tentando conectar em: " + ssid);

    WiFi.setHostname(HOSTNAME);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED &&
           (millis() - start) < (timeoutSec * 1000UL)) {
        delay(300);
        Serial.print(".");
    }
    Serial.println();

    bool ok = (WiFi.status() == WL_CONNECTED);
    Serial.println(ok ? "[WiFi] STA conectado." : "[WiFi] STA falhou.");
    return ok;
}

void startFallbackAP() {
    Serial.println("\n[WiFi] Falha ao conectar. Iniciando AP...");
    WiFi.mode(WIFI_AP);

    const char* AP_SSID = "lampada_lavanderia";
    const char* AP_PASS = "12345678";

    bool ok = WiFi.softAP(AP_SSID, AP_PASS);
    if (!ok) {
        Serial.println("[AP] Falha ao iniciar AP!");
    } else {
        Serial.println("[AP] AP ativo!");
        Serial.println("SSID: lampada_lavanderia");
        Serial.println("Senha: 12345678");
        Serial.print("IP AP: ");
        Serial.println(WiFi.softAPIP());
    }

    // Se quiser scan depois, faça assíncrono:
    // WiFi.scanNetworks(true);
}

void connectWiFiWithFallback() {
    Preferences prefs;
    prefs.begin("wifi");

    String ssid = prefs.getString("ssid", WIFI_SSID);
    String pass = prefs.getString("pass", WIFI_PASS);

    prefs.end();

    if (tryConnectWiFi(ssid, pass, 12)) {
        Serial.print("[WiFi] Conectado! IP: ");
        Serial.println(WiFi.localIP());
        return;
    }

    startFallbackAP();
}

// =========================
// STATE HELPERS
// =========================
void syncRelay() {
    digitalWrite(PIN_RELAY, lampState ? HIGH : LOW);
}

void publishState() {
    mqtt.publish(TOPIC, lampState ? "1" : "0", true);
    Serial.println("[MQTT] Publicado estado: " + String(lampState));
}

// =========================
// SETUP
// =========================
void setup() {
    Serial.begin(115200);
    delay(5000);
    Serial.println("\n=== Boot Lâmpada Lavanderia ===");

    pinMode(PIN_RELAY,  OUTPUT);
    pinMode(PIN_LED,    OUTPUT);
    pinMode(PIN_SWITCH, INPUT_PULLUP);

    digitalWrite(PIN_LED, LOW);
    syncRelay();

    // ======= WIFI (STA + FALLBACK AP) =======
    connectWiFiWithFallback();

    // ======= MQTT (sempre configura; só conecta em STA) =======
    mqtt.setServer(MQTT_HOST, MQTT_PORT);
    mqtt.setCallback(mqttCallback);

    // ======= Web UI =======
    IPAddress ip;
    if (WiFi.getMode() == WIFI_MODE_AP || WiFi.status() != WL_CONNECTED) {
        ip = WiFi.softAPIP();   // 192.168.4.1 normalmente
    } else {
        ip = WiFi.localIP();    // IP em modo STA
    }

    Serial.print("[WEB] IP para UI: ");
    Serial.println(ip);

    page.setNetworkInfo(ip, WiFi.macAddress());
    page.onToggle(toggleLamp);
    page.setupRoutes();

    server.begin();
    Serial.println("[WEB] WebServer iniciado.");

    digitalWrite(PIN_LED, HIGH);
    Serial.println("=== Setup concluído ===");
}

// =========================
// MAIN LOOP
// =========================
void loop() {
    // MQTT só roda quando estiver em modo STA e conectado
    if (WiFi.getMode() == WIFI_MODE_STA && WiFi.status() == WL_CONNECTED) {
        if (!mqtt.connected()) {
            if (mqtt.connect(HOSTNAME)) {
                Serial.println("[MQTT] Conectado.");
                // publica estado inicial
                publishState();
                mqtt.subscribe(TOPIC);
            } else {
                // não travar o loop com tentativas infinitas
                static unsigned long lastRetry = 0;
                if (millis() - lastRetry > 5000) {
                    Serial.println("[MQTT] Falha ao conectar, tentando novamente...");
                    lastRetry = millis();
                }
            }
        }
        mqtt.loop();
    }

    server.handleClient();

    // ======= BOTÃO FÍSICO S2 =======
    int reading = digitalRead(PIN_SWITCH);

    if (reading != lastReading) {
        lastDebounce = millis();  // começou a mudar → inicia debounce
    }

    if ((millis() - lastDebounce) > DEBOUNCE_MS) {
        if (reading != lastStableState) {
            // Chegou em um novo estado ESTÁVEL (aberto OU fechado)
            // → qualquer mudança de estado dispara toggle
            toggleLamp();

            Serial.print("[S2] Mudança de estado: ");
            Serial.println(reading == LOW ? "FECHADO" : "ABERTO");

            lastStableState = reading;
        }
    }

    lastReading = reading;
}

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_now.h>
#include <WebServer.h>
#include <Preferences.h>

// struct esp now
typedef struct __attribute__((packed)) struct_message {
  char ipwebserver[16];
  char station[32];
  char linie[10];
  char richtung[32];
  int minuten;
} __attribute__((packed)) struct_message;

// sleep vars
RTC_DATA_ATTR struct_message meineDaten; 
RTC_DATA_ATTR int bootCount = 0; 
RTC_DATA_ATTR char aktuelleRblChar[8] = "1252";
const unsigned long ZYKLUS_DAUER_S = 60; // Alle 60s
const unsigned long WAKEUP_BUFFER_S = 2; // Getter wacht 2s früher auf

// struct, server, prefs init
struct_message meineDaten;
WebServer server(80);
Preferences preferences;

// wlan
const char* ssid = "[WLAN NAME]";
const char* password = "[WLAN PASSWORT]";

// mac für esp now
uint8_t broadcastAddress[] = {0x1C, 0xC3, 0xAB, 0x3C, 0xB7, 0xF8}; 

// history
String historyLog[5]; 
int historyIndex = 0;

const int ledPin = 2;
String aktuelleRbl = "1252"; // derzeit neustift am walde 35a spittelau

unsigned long lastTime = 0;
unsigned long timerDelay = 20000;

// dbg help (esp now)
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLetzter Sendestatus: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Zustellung Erfolgreich" : "Zustellung Fehlgeschlagen");
}

// HTML
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<style>body{font-family:sans-serif; padding:20px; background:#f4f4f4;} ";
  html += "input{padding:10px; width:80%; max-width:300px; border:1px solid #ccc; border-radius:5px;} ";
  html += ".btn{background:#e30613; color:white; border:none; padding:10px 20px; cursor:pointer; border-radius:5px; margin-top:10px;} ";
  html += ".card{background:white; padding:20px; border-radius:10px; box-shadow:0 2px 5px rgba(0,0,0,0.1);}</style>";
  html += "</head><body>";
  html += "<div class='card'>";
  html += "<h1>Wiener Linien Monitor</h1>";
  html += "<p>Aktuelle Station: <b>" + String(meineDaten.station) + "</b> (RBL: " + aktuelleRbl + ")</p>";
  html += "<form action='/setrbl' method='GET'>";
  html += "Neue RBL Nummer: <br><input type='text' name='rbl' placeholder='z.B. 1252'><br>";
  html += "<input type='submit' class='btn' value='Speichern & Aktualisieren'>";
  html += "</form>";
  html += "<h2>Letzte Abfragen:</h2><ul>";
  for(int i = 0; i < 5; i++) {
    if(historyLog[i].length() > 0) html += "<li>" + historyLog[i] + "</li>";
  }
  html += "</ul>";
  html += "</div></body></html>";
  server.send(200, "text/html", html);
}

// save rbl per web interface
void handleSetRbl() {
  String newRbl = server.arg("rbl");
  if(newRbl.length() > 0) {
    aktuelleRbl = newRbl;
    // save in prefs
    preferences.begin("wl-config", false);
    preferences.putString("rbl", newRbl);
    preferences.end();
    
    server.send(200, "text/html", "RBL wurde auf " + newRbl + " gesetzt! <a href='/'>Zurueck</a>");
    lastTime = millis() + timerDelay; 
    holeUndSendeDaten();
  }
}

// main get/send (api, espnow)
void holeUndSendeDaten() {
  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(ledPin, HIGH);
    
    HTTPClient http;
    String url = "https://www.wienerlinien.at/ogd_realtime/monitor?rbl=" + aktuelleRbl;
    http.begin(url);
    
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      
      StaticJsonDocument<400> filter;
      filter["data"]["monitors"][0]["locationStop"]["properties"]["title"] = true;
      filter["data"]["monitors"][0]["lines"][0]["name"] = true;
      filter["data"]["monitors"][0]["lines"][0]["towards"] = true;
      filter["data"]["monitors"][0]["lines"][0]["departures"]["departure"][0]["departureTime"]["countdown"] = true;
      
      DynamicJsonDocument doc(4096); 
      DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter), DeserializationOption::NestingLimit(20));
      
      if (!error) {
        JsonObject monitor = doc["data"]["monitors"][0];
        JsonObject line = monitor["lines"][0];

        if (!line.isNull()) {
          // send structur
          strlcpy(meineDaten.ipwebserver, WiFi.localIP().toString().c_str(), sizeof(meineDaten.ipwebserver));
          strlcpy(meineDaten.station, monitor["locationStop"]["properties"]["title"] | "Unbekannt", sizeof(meineDaten.station));
          strlcpy(meineDaten.linie, line["name"] | "N/A", sizeof(meineDaten.linie));
          strlcpy(meineDaten.richtung, line["towards"] | "N/A", sizeof(meineDaten.richtung));
          meineDaten.minuten = line["departures"]["departure"][0]["departureTime"]["countdown"] | 0;

          // history
          String entry = String(meineDaten.station) + ": " + String(meineDaten.linie) + " -> " + String(meineDaten.richtung) + " (" + String(meineDaten.minuten) + "m)";
          historyLog[historyIndex] = entry;
          historyIndex = (historyIndex + 1) % 5;

          // espnow
          esp_now_send(broadcastAddress, (uint8_t *) &meineDaten, sizeof(meineDaten));
          Serial.println("Gesendet: " + entry);
        } else {
          Serial.println("Keine Daten gefunden.");
        }
      } else {
        Serial.print("JSON Fehler: ");
        Serial.println(error.f_str());
      }
    }
    http.end();
    digitalWrite(ledPin, LOW); // LED aus also http Fertig
  }
}

void setup() {
  Serial.begin(115200);
  bootCount++; // Zählt Neustarts
  Serial.println("Sender aufgewacht, Boot: " + String(bootCount));

  pinMode(ledPin, OUTPUT);
  
  preferences.begin("wl-config", true);
  aktuelleRbl = preferences.getString("rbl", "1252");
  preferences.end();

  // wlan
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Verbinde WLAN...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  if(WiFi.status() == WL_CONNECTED) {
    strlcpy(meineDaten.ipwebserver, WiFi.localIP().toString().c_str(), sizeof(meineDaten.ipwebserver));
  }
  IPAddress ip = WiFi.localIP(); 
  sprintf(meineDaten.ipwebserver, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
  Serial.println("\nVerbunden! IP: " + WiFi.localIP().toString());
  Serial.print("Sende auf Kanal: "); Serial.println(WiFi.channel());

  // espnow
  if (esp_now_init() != ESP_OK) {
    Serial.println("Fehler bei ESP NOW Init");
    return;
  }

  esp_now_register_send_cb((esp_now_send_cb_t)OnDataSent);
  
  // Peer (getter)
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = WiFi.channel(); 
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Fehler beim Hinzufuegen des Peers");
    return;
  }

  // wbsrv
  server.on("/", handleRoot);
  server.on("/setrbl", handleSetRbl);
  server.begin();

  holeUndSendeDaten();
}

void loop() {
  // Webserver Anfragen bearbeiten (immer aktiv)
  server.handleClient();

  // Zeitgesteuerter Abruf ohne den Loop zu blockieren (non-blocking)
  unsigned long currentMillis = millis();
  if (currentMillis - lastTime >= timerDelay) {
    lastTime = currentMillis;
    holeUndSendeDaten();
  }
}
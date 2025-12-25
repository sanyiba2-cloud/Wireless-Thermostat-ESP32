// --- KRITIKUS JAVÍTÁS A NÉVÜTKÖZÉS ELLEN ---
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h> // Előbb az Async, utána az Elegant!
#define ELEGANTOTA_USE_ASYNC_WEBSERVER 1
#include <ElegantOTA.h>

#include <esp_now.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <ArduinoOTA.h>
#include <Preferences.h>

// --- KONFIGURÁCIÓ ---
Adafruit_BME280 bme;
AsyncWebServer server(80);
Preferences pref;

float Temperature = 0.0, TargetTemp = 22.0, TempSetting = 22.0;
String RelayState = "KÉSZENLÉT";

struct Schedule { int h; int m; float t; };
Schedule weekProg[7][4];

void loadSettings() {
  pref.begin("thermo", true);
  for (int d = 0; d < 7; d++) {
    for (int s = 0; s < 4; s++) {
      String key = "d" + String(d) + "s" + String(s);
      weekProg[d][s].h = pref.getInt((key + "h").c_str(), 8);
      weekProg[d][s].m = pref.getInt((key + "m").c_str(), 0);
      weekProg[d][s].t = pref.getFloat((key + "t").c_str(), 21.0);
    }
  }
  TargetTemp = pref.getFloat("target", 22.0);
  TempSetting = TargetTemp;
  pref.end();
}

String getCSS() {
  return "<style>body{background:#FFF;font-family:Arial;margin:0;text-align:center;padding-bottom:60px;}"
         ".navbar{background:#003399;display:flex;align-items:center;border-bottom:4px solid #CC0000;position:sticky;top:0;z-index:100;}"
         ".nav-btn{color:#FFF;text-decoration:none;padding:15px;font-weight:bold;flex:1;font-size:11px;}"
         ".sig{color:#FFF;font-size:10px;padding-right:10px;}"
         ".circle-container{width:180px;height:180px;margin:20px auto;border-radius:50%;border:10px solid #003399;display:flex;align-items:center;justify-content:center;font-size:40px;font-weight:bold;color:#003399;box-shadow:0 5px 15px rgba(0,0,0,0.2);}"
         ".data-item{background:#f0f4f8;padding:10px;border:2px solid #003399;border-radius:8px;margin:5px;flex:1;color:#003399;font-weight:bold;}"
         "table{width:95%;margin:10px auto;border-collapse:collapse;font-size:12px;} th{background:#003399;color:#FFF;padding:8px;} td{border:1px solid #003399;padding:4px;}"
         "input{width:40px;text-align:center;}"
         ".save-btn{background:#CC0000;color:#FFF;padding:18px;width:90%;border:none;font-weight:bold;border-radius:5px;margin:20px 0;}"
         ".footer{background:#CC0000;color:#FFF;padding:8px;position:fixed;bottom:0;width:100%;font-size:11px;}</style>";
}

String wrapPage(String title, String content) {
  String sig = String(WiFi.RSSI()) + " dBm";
  String h = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'><meta charset='UTF-8'>" + getCSS() + "</head><body>";
  h += "<div class='navbar'><a href='/' class='nav-btn'>STATUS</a><a href='/program' class='nav-btn'>SCHEDULE</a><a href='/graph' class='nav-btn'>GRAPH</a><div class='sig'>" + sig + "</div></div>";
  h += "<h2>" + title + "</h2>" + content;
  h += "<div class='footer'>WIRELESS THERMOSTAT v2.9.4 | <a href='/update' style='color:white'>Update</a></div></body></html>";
  return h;
}

void setup() {
  Serial.begin(115200);
  if (!bme.begin(0x76)) { Serial.println("BME280 nem található!"); }
  loadSettings();

  WiFi.mode(WIFI_STA);
  WiFi.begin("Don't Touch Me", "Ed06e239");
  int c = 0;
  while(WiFi.status() != WL_CONNECTED && c < 15) { delay(500); Serial.print("."); c++; }
  if(WiFi.status() != WL_CONNECTED) {
    WiFi.begin("Szabo", "Botond17");
    c = 0;
    while(WiFi.status() != WL_CONNECTED && c < 15) { delay(500); Serial.print("."); c++; }
  }

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req){
    String body = "<div class='circle-container'>" + String(Temperature, 1) + "°</div>";
    body += "<div style='display:flex;padding:10px;'><div class='data-item'>CÉL<br>" + String(TempSetting, 1) + "°</div>";
    body += "<div class='data-item'>KAZÁN<br><span style='color:#CC0000'>" + RelayState + "</span></div></div>";
    body += "<button onclick='location.href=\"/down\"' style='font-size:30px;width:70px;'>-</button>";
    body += "<button onclick='location.href=\"/ok\"' style='font-size:30px;width:70px;background:#CC0000;color:#FFF;border:none;margin:0 10px;'>OK</button>";
    body += "<button onclick='location.href=\"/up\"' style='font-size:30px;width:70px;'>+</button>";
    req->send(200, "text/html", wrapPage("ÁLLAPOT", body));
  });

  server.on("/program", HTTP_GET, [](AsyncWebServerRequest *req){
    String h = "<form action='/save' method='POST'><table><tr><th>Nap</th><th>Idő</th><th>°C</th></tr>";
    const char* napok[] = {"Hé","Ke","Sze","Csü","Pé","Szo","Vas"};
    for(int d=0; d<7; d++) {
      for(int s=0; s<4; s++) {
        h += "<tr>";
        if(s==0) h += "<td rowspan='4'><b>" + String(napok[d]) + "</b></td>";
        String id = String(d) + String(s);
        h += "<td><input type='number' name='h"+id+"' value='"+String(weekProg[d][s].h)+"'>:";
        h += "<input type='number' name='m"+id+"' value='"+String(weekProg[d][s].m)+"'></td>";
        h += "<td><input type='number' step='0.1' name='t"+id+"' value='"+String(weekProg[d][s].t,1)+"'></td></tr>";
      }
    }
    h += "</table><input type='submit' class='save-btn' value='MENTÉS'></form>";
    req->send(200, "text/html", wrapPage("IDŐZÍTÉS", h));
  });

  server.on("/save", HTTP_POST, [](AsyncWebServerRequest *req){
    pref.begin("thermo", false);
    for(int d=0; d<7; d++){
      for(int s=0; s<4; s++){
        String id = String(d) + String(s);
        if(req->hasParam("h"+id, true)) {
          int vH = req->getParam("h"+id, true)->value().toInt();
          int vM = req->getParam("m"+id, true)->value().toInt();
          float vT = req->getParam("t"+id, true)->value().toFloat(); // JAVÍTVA: toFloat()
          weekProg[d][s].h = vH; weekProg[d][s].m = vM; weekProg[d][s].t = vT;
          String k = "d" + id;
          pref.putInt((k + "h").c_str(), vH); pref.putInt((k + "m").c_str(), vM); pref.putFloat((k + "t").c_str(), vT);
        }
      }
    }
    pref.end();
    req->redirect("/program");
  });

  server.on("/graph", HTTP_GET, [](AsyncWebServerRequest *req){
    req->send(200, "text/html", wrapPage("GRAFIKON", "<iframe width='100%' height='400' src='https://io.adafruit.com/termosztat_mester_2025/dashboards/termosztat-grafikon?embed=1'></iframe>"));
  });

  server.on("/up", HTTP_GET, [](AsyncWebServerRequest *req){ TempSetting += 0.5; req->redirect("/"); });
  server.on("/down", HTTP_GET, [](AsyncWebServerRequest *req){ TempSetting -= 0.5; req->redirect("/"); });
  server.on("/ok", HTTP_GET, [](AsyncWebServerRequest *req){ 
    TargetTemp = TempSetting; pref.begin("thermo", false); pref.putFloat("target", TargetTemp); pref.end();
    req->redirect("/"); 
  });

  ElegantOTA.begin(&server); 
  server.begin();
  ArduinoOTA.begin();
}

void loop() {
  ElegantOTA.loop();
  ArduinoOTA.handle();
  static unsigned long lastU = 0;
  if (millis() - lastU > 5000) {
    lastU = millis();
    float t = bme.readTemperature();
    if (!isnan(t)) Temperature = t;
    RelayState = (Temperature < (TargetTemp - 0.2)) ? "FŰTÉS BE" : "KÉSZENLÉT";
  }
}
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <Ticker.h>

// Ayarlar
#define AP_SSID "ACIL-DURUM-DEPREMZEDE"
#define API_URL "https://api.afad.gov.tr/latest-earthquakes"
#define CHECK_INTERVAL 300000  // 5 dakikada bir kontrol

// Global değişkenler
DNSServer dnsServer;
ESP8266WebServer webServer(80);
Ticker ticker;
String lastEarthquake = "Veri bekleniyor...";

// Kişisel bilgiler JSON olarak
const char* personalInfo = R"({
  "name": "Ad Soyad",
  "address": "Açık Adres",
  "blood_type": "Kan Grubu",
  "contact": "Telefon",
  "home_wifi": {
    "ssid": "Ev Wifi SSID",
    "password": "Ev Wifi Şifre"
  }
})";

void setupWiFiAP() {
  WiFi.softAP(AP_SSID);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  
  dnsServer.start(53, "*", myIP);
}

String getEarthquakeData() {
  if (WiFi.status() != WL_CONNECTED) return "Internet yok";
  
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  
  http.begin(client, API_URL);
  http.addHeader("Accept", "application/json");
  
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, payload);
    
    float magnitude = doc["data"][0]["magnitude"];
    String location = doc["data"][0]["location"];
    String date = doc["data"][0]["date"];
    
    return String(magnitude, 1) + " büyüklüğünde - " + location + " (" + date + ")";
  }
  return "API hatası: " + String(httpCode);
}

void checkEarthquake() {
  String newData = getEarthquakeData();
  if (!newData.startsWith("API hatası")) {
    lastEarthquake = newData;
    
    if (newData.indexOf("7.") != -1 || newData.indexOf("8.") != -1 || newData.indexOf("9.") != -1) {
      Serial.println("CRITICAL EARTHQUAKE DETECTED!");
      setupWiFiAP();
    }
  }
}

String generateHTML() {
  DynamicJsonDocument personal(512);
  deserializeJson(personal, personalInfo);
  
  float quakeValue = 0;
  if (lastEarthquake.length() > 3) {
    quakeValue = lastEarthquake.substring(0, 3).toFloat();
  }
  int progressWidth = min(100, (int)(quakeValue * 10));
  
  String html = "<!DOCTYPE html><html lang='tr'><head>";
  html += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>ACİL DURUM - DEPREM BİLGİ SİSTEMİ</title>";
  html += "<link href='https://cdn.jsdelivr.net/npm/bootstrap@5.3.0/dist/css/bootstrap.min.css' rel='stylesheet'>";
  html += "<link rel='stylesheet' href='https://cdn.jsdelivr.net/npm/bootstrap-icons@1.10.0/font/bootstrap-icons.css'>";
  html += "<style>body{background-color:#f8f9fa;}.alert-critical{animation:pulse 2s infinite;}";
  html += "@keyframes pulse{0%{opacity:1;}50%{opacity:0.7;}100%{opacity:1;}}";
  html += ".card{border-radius:15px;box-shadow:0 4px 6px rgba(0,0,0,0.1);}.status-badge{font-size:0.8rem;}</style>";
  html += "</head><body><div class='container py-5'><div class='row justify-content-center'>";
  html += "<div class='col-lg-8'><div class='card mb-4'><div class='card-header bg-danger text-white'>";
  html += "<h4 class='mb-0'><i class='bi bi-exclamation-triangle-fill'></i> ACİL DURUM BİLGİLERİ</h4>";
  html += "</div><div class='card-body'><div class='alert alert-warning d-flex align-items-center mb-4'>";
  html += "<i class='bi bi-info-circle-fill me-2'></i><div>Bu ağ yalnızca acil durumlarda kullanım içindir.</div></div>";
  
  // Kişisel Bilgiler
  html += "<div class='row mb-4'><div class='col-md-6'><div class='card h-100'>";
  html += "<div class='card-header bg-primary text-white'><h5 class='mb-0'><i class='bi bi-person-badge-fill'></i> Kişisel Bilgiler</h5></div>";
  html += "<div class='card-body'><ul class='list-group list-group-flush'>";
  html += "<li class='list-group-item d-flex justify-content-between align-items-center'>";
  html += "<span><i class='bi bi-person-fill me-2'></i>Ad Soyad</span>";
  html += "<span class='badge bg-primary rounded-pill'>" + personal["name"].as<String>() + "</span></li>";
  html += "<li class='list-group-item d-flex justify-content-between align-items-center'>";
  html += "<span><i class='bi bi-geo-alt-fill me-2'></i>Adres</span>";
  html += "<span class='badge bg-primary rounded-pill'>" + personal["address"].as<String>() + "</span></li>";
  html += "<li class='list-group-item d-flex justify-content-between align-items-center'>";
  html += "<span><i class='bi bi-droplet-fill me-2'></i>Kan Grubu</span>";
  html += "<span class='badge bg-primary rounded-pill'>" + personal["blood_type"].as<String>() + "</span></li>";
  html += "<li class='list-group-item d-flex justify-content-between align-items-center'>";
  html += "<span><i class='bi bi-telephone-fill me-2'></i>İletişim</span>";
  html += "<span class='badge bg-primary rounded-pill'>" + personal["contact"].as<String>() + "</span></li></ul></div></div></div>";
  
  // WiFi Bilgileri
  html += "<div class='col-md-6'><div class='card h-100'><div class='card-header bg-success text-white'>";
  html += "<h5 class='mb-0'><i class='bi bi-wifi'></i> WiFi Bilgileri</h5></div><div class='card-body'>";
  html += "<div class='alert alert-info mb-3'><i class='bi bi-router me-2'></i><strong>" + personal["home_wifi"]["ssid"].as<String>() + "</strong>";
  html += "<div class='mt-2'><input type='password' class='form-control' value='" + personal["home_wifi"]["password"].as<String>() + "' readonly></div></div>";
  html += "<button class='btn btn-outline-success w-100' onclick='copyWifi()'><i class='bi bi-clipboard'></i> Kopyala</button></div></div></div></div>";
  
  // Deprem Durumu
  html += "<div class='card mb-4'><div class='card-header bg-info text-white'>";
  html += "<h5 class='mb-0'><i class='bi bi-activity'></i> Son Deprem Durumu</h5></div><div class='card-body'>";
  html += String("<div id='earthquakeAlert' class='alert ") + String(quakeValue >= 7.0 ? "alert-danger alert-critical" : "alert-info") + "'>";
  html += "<i class='bi bi-shield-exclamation'></i><strong id='earthquakeText'>" + lastEarthquake + "</strong></div>";
  html += String("<div class='progress mb-3' style='height:10px;'><div id='quakeMeter' class='progress-bar ") + String(quakeValue >= 7.0 ? "bg-danger" : "bg-warning") + "' role='progressbar' style='width:" + String(progressWidth) + "%;'></div></div>";
  html += "<small class='text-muted'>Son güncelleme: " + String(millis() / 1000) + " saniye önce</small></div></div>";
  
  // Talimatlar
  html += "<div class='alert alert-secondary'><h5><i class='bi bi-info-circle'></i> Acil Durum Talimatları</h5>";
  html += "<ol class='mt-3'><li>Panik yapmayın</li><li>Güvenli bir yere geçin</li>";
  html += "<li>Bu bilgileri kurtarma ekipleriyle paylaşın</li><li>Çevrenizdekilere yardım edin</li></ol></div>";
  
  // Footer ve Scriptler
  html += "</div></div><div class='text-center text-muted small'><p>ACİL DURUM SİSTEMİ v1.0 - ESP8266</p></div>";
  html += "<script src='https://cdn.jsdelivr.net/npm/bootstrap@5.3.0/dist/js/bootstrap.bundle.min.js'></script>";
  html += "<script>function copyWifi(){const e=document.querySelector('input[type=\"password\"]');e.select(),document.execCommand('copy'),alert('WiFi bilgileri kopyalandı!')}";
  html += "setInterval(()=>{fetch('/update').then(e=>e.text()).then(e=>{if(e){document.getElementById('earthquakeText').innerText=e;";
  html += "const t=parseFloat(e.split(' ')[0]),n=document.getElementById('quakeMeter');";
  html += "n.style.width=Math.min(100,10*t)+'%',n.className=t>=7?'progress-bar bg-danger':'progress-bar bg-warning';";
  html += "const o=document.getElementById('earthquakeAlert');";
  html += "o.className=t>=7?'alert alert-danger alert-critical':'alert alert-info'}})},3e4);</script>";
  html += "</body></html>";
  
  return html;
}

void handleRoot() {
  webServer.send(200, "text/html", generateHTML());
}

void handleUpdate() {
  webServer.send(200, "text/plain", lastEarthquake);
}

void setup() {
  Serial.begin(115200); // Aynı hızı Serial Monitor'de seçtiğinizden emin olun
  
  // WiFi bağlantısı
  DynamicJsonDocument personal(512);
  deserializeJson(personal, personalInfo);
  WiFi.begin(personal["home_wifi"]["ssid"].as<String>(), personal["home_wifi"]["password"].as<String>());
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi bağlandı!");
  
  webServer.on("/", handleRoot);
  webServer.on("/update", handleUpdate);
  
  checkEarthquake();
  ticker.attach_ms(CHECK_INTERVAL, checkEarthquake);
}

void loop() {
  dnsServer.processNextRequest();
  webServer.handleClient();
}
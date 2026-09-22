#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <WiFi.h>
#include <WebServer.h>
#include "mqtt_client.h"   // ESP-IDF'in MQTT istemcisi (esp-mqtt), Arduino çekirdeğinde hazır geliyor
#include "ca_sertifika.h"

// Ekran çözünürlüğü
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Ekran nesnesi (I2C adresi genelde 0x3C'dir)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// DHT Ayarları
#define DHTPIN 15
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// MPU6050 register'ları (datasheet: MPU-6000/6050 Register Map)
#define MPU_ADDR      0x68  // AD0 GND'de ise 0x68, 3V3'te ise 0x69
#define REG_PWR_MGMT1 0x6B  // Güç yönetimi, açılışta uyku modunda gelir
#define REG_ACCEL_X_H 0x3B  // İvme verisinin başladığı register (6 byte: X, Y, Z)
#define REG_WHO_AM_I  0x75  // Kimlik register'ı, 0x68 döner
#define ACCEL_LSB_PER_G 16384.0  // Varsayılan ±2g aralığında 1g = 16384

bool mpuVar = false;

// WiFi: Wokwi'nin sanal erişim noktası, şifresiz, kanal 6
const char *WIFI_AD = "Wokwi-GUEST";
const char *WIFI_SIFRE = "";
WebServer server(80);

// MQTT: herkese açık deneme broker'ı (kullanıcı/şifre: public/public)
// Ağ sadece 80/443 portlarına izin veriyor, MQTT'nin kendi portları (1883, 8883) kapalı.
// Bu yüzden MQTT'yi WebSocket içinde, TLS ile 443'ten gönderiyoruz: wss://
const char *MQTT_URI = "wss://public.cloud.shiftr.io:443";
// Konu adları herkese açık, bu yüzden başkalarıyla karışmasın diye kendine özgü bir ön ek
#define KONU_ONEK   "yavuz-sensordenemesi/esp32"
#define KONU_VERI   KONU_ONEK "/veri"    // ESP32 buraya yayınlar
#define KONU_DURUM  KONU_ONEK "/durum"   // çevrimiçi / çevrimdışı
#define KONU_KOMUT  KONU_ONEK "/komut"   // ESP32 bunu dinler
esp_mqtt_client_handle_t mqtt;

// esp-mqtt kendi FreeRTOS görevinde (task) çalışır; bu değişkenlere iki görev birden erişir
volatile bool mqttBagli = false;
char sonKomut[32] = "";  // komut konusundan gelen son mesaj, OLED'de gösterilir
portMUX_TYPE komutKilidi = portMUX_INITIALIZER_UNLOCKED;  // sonKomut'a aynı anda yazılıp okunmasın

// Son okunan değerler, hem ekran hem web sunucusu bunları kullanır
float nem = NAN, sicaklik = NAN;
float ax = 0, ay = 0, az = 0;

// Tek bir register'a bir byte yaz
void registerYaz(uint8_t adres, uint8_t reg, uint8_t deger) {
  Wire.beginTransmission(adres);
  Wire.write(reg);
  Wire.write(deger);
  Wire.endTransmission();
}

// reg'den başlayarak adet kadar byte oku (sensör register adresini kendisi artırır)
bool registerOku(uint8_t adres, uint8_t reg, uint8_t *tampon, uint8_t adet) {
  Wire.beginTransmission(adres);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;  // false: STOP yerine repeated START
  if (Wire.requestFrom(adres, adet) != adet) return false;
  for (uint8_t i = 0; i < adet; i++) tampon[i] = Wire.read();
  return true;
}

// Hattaki tüm adresleri dene, ACK veren cihazları yazdır
void i2cTara() {
  Serial.println("I2C taramasi basliyor...");
  int bulunan = 0;
  for (uint8_t adres = 1; adres < 127; adres++) {
    Wire.beginTransmission(adres);
    if (Wire.endTransmission() == 0) {  // 0 = cihaz ACK verdi
      Serial.printf("  Cihaz bulundu: 0x%02X\n", adres);
      bulunan++;
    }
  }
  Serial.printf("Toplam %d cihaz\n", bulunan);
}

// Tarayıcıdan "/" istenince: 2 saniyede bir kendini yenileyen basit sayfa
void anaSayfa() {
  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'>"
                "<meta http-equiv='refresh' content='2'>"
                "<meta name='viewport' content='width=device-width'>"
                "<title>ESP32 Sensor</title></head>"
                "<body style='font-family:sans-serif;text-align:center'>"
                "<h1>ESP32 Sensör</h1>";
  html += "<p style='font-size:2em'>🌡 " + String(sicaklik, 1) + " °C</p>";
  html += "<p style='font-size:2em'>💧 %" + String(nem, 1) + "</p>";
  html += "<p>Çalışma süresi: " + String(millis() / 1000) + " sn</p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

// "/veri" istenince: aynı veriler JSON olarak (başka programlar için)
void veriJson() {
  String json = "{\"sicaklik\":" + String(sicaklik, 1) +
                ",\"nem\":" + String(nem, 1) +
                ",\"ax\":" + String(ax, 2) +
                ",\"ay\":" + String(ay, 2) +
                ",\"az\":" + String(az, 2) + "}";
  server.send(200, "application/json", json);
}

void wifiBaglan() {
  Serial.print("WiFi'ye baglaniliyor");
  WiFi.begin(WIFI_AD, WIFI_SIFRE, 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }
  Serial.print("\nBaglandi, IP: ");
  Serial.println(WiFi.localIP());
}

// esp-mqtt her olayda (bağlandı, koptu, mesaj geldi...) bu fonksiyonu kendi görevinden çağırır.
// Yeniden bağlanmayı da kütüphane kendisi yapar, loop'ta kontrol etmemize gerek yok.
void mqttOlay(void *arg, esp_event_base_t taban, int32_t olayId, void *olayVerisi) {
  esp_mqtt_event_handle_t olay = (esp_mqtt_event_handle_t)olayVerisi;
  switch ((esp_mqtt_event_id_t)olayId) {
    case MQTT_EVENT_CONNECTED:
      mqttBagli = true;
      Serial.println("MQTT baglandi");
      // retain = 1: broker bu mesajı saklar, sonradan abone olan da cihazın durumunu görür
      esp_mqtt_client_publish(mqtt, KONU_DURUM, "cevrimici", 0, 1, 1);
      esp_mqtt_client_subscribe(mqtt, KONU_KOMUT, 0);
      break;

    case MQTT_EVENT_DISCONNECTED:
      mqttBagli = false;
      Serial.println("MQTT koptu, kutuphane tekrar baglanmayi deneyecek");
      break;

    case MQTT_EVENT_DATA: {
      // Gelen veri '\0' ile bitmez, uzunluğu ayrıca verilir
      int n = min(olay->data_len, (int)sizeof(sonKomut) - 1);
      portENTER_CRITICAL(&komutKilidi);
      memcpy(sonKomut, olay->data, n);
      sonKomut[n] = '\0';
      portEXIT_CRITICAL(&komutKilidi);
      Serial.printf("MQTT [%.*s]: %.*s\n", olay->topic_len, olay->topic, olay->data_len, olay->data);
      break;
    }

    case MQTT_EVENT_ERROR:
      Serial.printf("MQTT hatasi (tip %d)\n", olay->error_handle->error_type);
      break;

    default:
      break;
  }
}

void mqttBaslat() {
  esp_mqtt_client_config_t ayar = {};
  ayar.uri = MQTT_URI;
  ayar.username = "public";
  ayar.password = "public";
  ayar.cert_pem = CA_SERTIFIKA;  // sunucunun gerçekten o broker olduğunu bu kök sertifikayla doğrula
  // Son vasiyet (Last Will): ESP32 habersizce koparsa broker bu mesajı kendisi yayınlar
  ayar.lwt_topic = KONU_DURUM;
  ayar.lwt_msg = "cevrimdisi";
  ayar.lwt_qos = 1;
  ayar.lwt_retain = 1;
  ayar.keepalive = 15;  // 15 sn ses çıkmazsa broker cihazı kopmuş sayar

  mqtt = esp_mqtt_client_init(&ayar);
  esp_mqtt_client_register_event(mqtt, MQTT_EVENT_ANY, mqttOlay, NULL);
  esp_mqtt_client_start(mqtt);
  Serial.println("MQTT istemcisi basladi");
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);  // SDA, SCL
  dht.begin();

  i2cTara();

  // MPU6050'yi kimliğinden tanı ve uykudan uyandır
  uint8_t kimlik = 0;
  if (registerOku(MPU_ADDR, REG_WHO_AM_I, &kimlik, 1) && kimlik == 0x68) {
    registerYaz(MPU_ADDR, REG_PWR_MGMT1, 0x00);
    mpuVar = true;
    Serial.println("MPU6050 hazir");
  } else {
    Serial.printf("MPU6050 bulunamadi (WHO_AM_I = 0x%02X)\n", kimlik);
  }

  // Ekranı başlat
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 ekran bulunamadı!"));
    for(;;); // Hata varsa kodu burada durdur
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);
  display.println("WiFi baglaniyor...");
  display.display();

  wifiBaglan();

  server.on("/", anaSayfa);
  server.on("/veri", veriJson);
  server.begin();
  Serial.println("Web sunucusu basladi");

  mqttBaslat();
}

void loop() {
  // Gelen HTTP isteklerine cevap ver; bu yüzden loop'ta uzun delay() olmamalı
  server.handleClient();

  // 5 saniyede bir ölçümleri yayınla
  static unsigned long sonYayin = 0;
  if (mqttBagli && millis() - sonYayin >= 5000) {
    sonYayin = millis();
    char json[128];
    snprintf(json, sizeof(json),
             "{\"sicaklik\":%.1f,\"nem\":%.1f,\"ax\":%.2f,\"ay\":%.2f,\"az\":%.2f}",
             sicaklik, nem, ax, ay, az);
    esp_mqtt_client_publish(mqtt, KONU_VERI, json, 0, 0, 0);
  }

  // Sensörleri ve ekranı 500 ms'de bir güncelle
  static unsigned long sonGuncelleme = 0;
  if (millis() - sonGuncelleme < 500) return;
  sonGuncelleme = millis();

  // İvme: 6 byte, her eksen big-endian işaretli 16 bit
  uint8_t ham[6];
  if (mpuVar && registerOku(MPU_ADDR, REG_ACCEL_X_H, ham, 6)) {
    ax = (int16_t)(ham[0] << 8 | ham[1]) / ACCEL_LSB_PER_G;
    ay = (int16_t)(ham[2] << 8 | ham[3]) / ACCEL_LSB_PER_G;
    az = (int16_t)(ham[4] << 8 | ham[5]) / ACCEL_LSB_PER_G;
  }

  // DHT22 en fazla 2 saniyede bir okunabilir, arada son değeri kullan
  static unsigned long sonDht = 0;
  if (millis() - sonDht >= 2000) {
    sonDht = millis();
    nem = dht.readHumidity();
    sicaklik = dht.readTemperature();
    if (isnan(nem) || isnan(sicaklik)) Serial.println("Sensör Hatasi!");
    // Seri port MQTT mesajlarını boğmasın diye sadece DHT okununca yazdır
    Serial.printf("T: %.1f C | Nem: %%%.1f | ax: %.2f ay: %.2f az: %.2f g\n",
                  sicaklik, nem, ax, ay, az);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.printf("Sicaklik: %.1f C", sicaklik);
  display.setCursor(0, 12);
  display.printf("Nem:      %%%.1f", nem);
  display.setCursor(0, 21);
  char komut[sizeof(sonKomut)];
  portENTER_CRITICAL(&komutKilidi);
  strcpy(komut, sonKomut);
  portEXIT_CRITICAL(&komutKilidi);
  if (komut[0] != '\0') {
    display.printf("> %s", komut);
  } else {
    display.print(WiFi.localIP());
    display.print(mqttBagli ? " MQTT" : "");
  }

  display.setCursor(0, 30);
  if (mpuVar) {
    display.printf("ax: %+.2f g", ax);
    display.setCursor(0, 42);
    display.printf("ay: %+.2f g", ay);
    display.setCursor(0, 54);
    display.printf("az: %+.2f g", az);
  } else {
    display.print("MPU6050 yok");
  }
  display.display();
}

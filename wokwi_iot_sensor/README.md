# wokwi_iot_sensor: Sensörden İnternete Bir IoT Cihazı

Bu proje tamamen [Wokwi](https://wokwi.com) simülatöründe yapıldı. Cihaz sensörleri okuyor, OLED
ekrana yazıyor, veriyi web sayfasında gösteriyor ve internetteki bir MQTT broker'ına gönderiyor.
Broker'dan gelen komutları da ekranda gösteriyor.

Bu dosya bir konu anlatımı gibi yazıldı. Her bölüm bir kavramı, o kavramın koddaki yerini ve
nedenini anlatıyor. Sonda kendini test etmen için sorular var.

> **Not:** Bu proje klasik **ESP32 DevKit** üzerinde **Arduino framework** (PlatformIO) kullanıyor.
> Reponun geri kalanı ESP32-S3 ve ESP-IDF ile yazıldı. MQTT kısmı ise zaten ESP-IDF'in kendi
> istemcisini (`esp-mqtt`) kullanıyor.

---

## İçindekiler

0. [Genel resim](#0-genel-resim)
1. [I2C: iki telden birçok cihaz](#1-i2c-iki-telden-birçok-cihaz)
2. [Kütüphanesiz sensör okuma: register'lar](#2-kütüphanesiz-sensör-okuma-registerlar)
3. [DHT22: kendi protokolü olan sensör](#3-dht22-kendi-protokolü-olan-sensör)
4. [`delay()` yerine `millis()`](#4-delay-yerine-millis)
5. [WiFi ve web sunucusu](#5-wifi-ve-web-sunucusu)
6. [MQTT: aracı üzerinden haberleşme](#6-mqtt-aracı-üzerinden-haberleşme)
7. [Engelli ağda hata ayıklama](#7-engelli-ağda-hata-ayıklama)
8. [WebSocket ve TLS](#8-websocket-ve-tls)
9. [İki görev, bir değişken: yarış durumu](#9-iki-görev-bir-değişken-yarış-durumu)
10. [Kendini test et](#10-kendini-test-et)
11. [Çalıştırma](#11-çalıştırma)

---

## 0. Genel resim

```
 DHT22 ──(tek tel, GPIO15)──┐
                            │
 MPU6050 ──┐                ▼
           ├─(I2C: 21/22)─ ESP32 ──WiFi──▶ İnternet ──▶ MQTT Broker ──▶ Tarayıcı / telefon
 OLED ─────┘                │                               ▲
                            │                               │
                            └── Web sunucusu (port 80)      └── Komut: "merhaba" ──▶ OLED
```

| Parça | Bağlantı | Görevi |
|---|---|---|
| DHT22 | GPIO15, 10 kΩ pull-up | Sıcaklık ve nem |
| MPU6050 | I2C, adres 0x68 | İvme (x, y, z) |
| SSD1306 OLED | I2C, adres 0x3C | 128×64 ekran |

Proje üç aşamada kuruldu:
1. **Yerel:** Sensörü oku, ekrana yaz.
2. **Aynı ağ:** Tarayıcıdan ESP32'nin IP adresine gidip veriyi gör.
3. **İnternet:** Veriyi broker'a gönder, dünyanın her yerinden izle ve komut gönder.

---

## 1. I2C: iki telden birçok cihaz

### Kavram
I2C iki telli bir **ortak hat (bus)**:
- **SDA:** veri
- **SCL:** saat, ritmi ESP32 belirler

Bütün cihazlar bu iki tele paralel bağlanır. Her cihazın sabit bir **adresi** vardır. ESP32
**master**, sensörler **slave**'dir. Konuşmayı her zaman master başlatır.

Bir sınıfta yoklama gibi düşünebilirsin:

```
ESP32:   "0x68, orada mısın?"
MPU6050: "Buradayım"          ← buna ACK denir
ESP32:   "0x50, orada mısın?"
(sessizlik)                   ← buna NACK denir, o adreste kimse yok
```

### Pull-up direnci neden var?
I2C hatları **open-drain** çalışır: cihazlar hattı sadece 0'a çekebilir, 1'e itemez. Hattı 1'e
çeken şey pull-up dirençleridir. Direnç yoksa ya da bağlı değilse hat hep 0'da kalır ve
iletişim kurulamaz. Gerçek devrede I2C sorunlarının en sık sebebi budur. Wokwi'de pull-up'lar
hazır geldiği için bu sorun hiç çıkmaz.

### I2C tarayıcı: bir teşhis aracı
```cpp
for (uint8_t adres = 1; adres < 127; adres++) {
  Wire.beginTransmission(adres);
  if (Wire.endTransmission() == 0) {   // 0 = cihaz ACK verdi
    Serial.printf("Cihaz bulundu: 0x%02X\n", adres);
  }
}
```
Tarayıcı her adresi deneyip cevap vereni listeler. Asıl değeri, hatayı **iki katmana ayırmasıdır**:

| Tarayıcı sonucu | Anlamı | Nereye bakmalı |
|---|---|---|
| Cihaz **bulunamadı** | Fiziksel iletişim yok | Kablo, pin, pull-up, besleme, ortak GND |
| Cihaz **bulundu** | Kablolama sağlam | Yazılım: yanlış register, sensör uyandırılmamış... |

**Wokwi'nin rolü:** Simülatörde kablolar kusursuzdur. Kod orada çalışıp gerçek kartta
çalışmıyorsa, sorun donanımdadır. Böylece yazılım ile donanım birbirinden ayrılmış olur.

---

## 2. Kütüphanesiz sensör okuma: register'lar

### Kavram
Sensörün içi numaralı küçük hücrelerden oluşur. Bunlara **register** denir. Bazılarına yazarak
sensörü ayarlarsın, bazılarından okuyarak ölçüm alırsın. Hangi register'ın ne işe yaradığı
**datasheet'te** (MPU-6050 Register Map) yazar.

Kullandığımız register'lar:

| Register | Adres | Ne işe yarıyor |
|---|---|---|
| `WHO_AM_I` | 0x75 | Kimlik. Her zaman 0x68 döner |
| `PWR_MGMT_1` | 0x6B | Güç yönetimi. Sensör açılışta **uyku modunda** gelir |
| `ACCEL_XOUT_H` | 0x3B | İvme verisinin başladığı yer (6 byte: X, Y, Z) |

### Üç adım
**1. Kimlik sor:** Doğru cihazla mı konuşuyorum?
```cpp
registerOku(0x68, 0x75, &kimlik, 1);   // kimlik == 0x68 ise doğru cihaz
```

**2. Uyandır:** Uyku biti temizlenmezse sensör hep 0 döndürür. Bunu datasheet'i okumadan
bilemezsin.
```cpp
registerYaz(0x68, 0x6B, 0x00);
```

**3. Oku ve dönüştür:**
```cpp
registerOku(0x68, 0x3B, ham, 6);
ax = (int16_t)(ham[0] << 8 | ham[1]) / 16384.0;
```
- Her eksen **16 bit**, iki byte'a bölünmüş: önce yüksek byte (H), sonra düşük byte (L).
  `<< 8` ile yüksek byte'ı sola kaydırıp düşük byte ile birleştiriyoruz. Önce yüksek byte'ın
  geldiği bu sıraya **big-endian** denir.
- `(int16_t)`: İvme negatif olabilir, bu yüzden sayı **işaretli** yorumlanmalı.
- `/ 16384.0`: Varsayılan ±2g aralığında 1g = 16384. Bu, ham sayıyı fiziksel birime çevirir.

### Okuma işlemi nasıl yapılıyor? (repeated START)
```cpp
Wire.beginTransmission(adres);
Wire.write(reg);                  // "0x3B'den okumak istiyorum"
Wire.endTransmission(false);      // false: hattı bırakma (STOP gönderme)
Wire.requestFrom(adres, 6);       // şimdi 6 byte oku
```
Önce hangi register'dan okunacağı **yazılır**, sonra hat bırakılmadan **okunur**. Sensör,
register adresini her byte'tan sonra kendisi bir artırır. Bu sayede 6 byte tek seferde gelir.

> Kütüphaneler de arkada tam olarak bunu yapıyor. Datasheet'i okuyup register'a yazabilen biri
> her sensörü kullanabilir.

---

## 3. DHT22: kendi protokolü olan sensör

DHT22, I2C ya da SPI kullanmaz. **Tek telli, kendine özgü** bir protokolü vardır. Burada
kütüphane kullandık, çünkü protokol hassas zamanlama (mikrosaniye) gerektiriyor.

Bilmen gereken iki şey:
- **En fazla 2 saniyede bir okunabilir.** Daha sık okursan hata ya da eski değer alırsın.
- **Veri hattı pull-up ister.** Devredeki 10 kΩ direnç bunun için.

İlk sürümde DHT **GPIO3**'e bağlıydı. GPIO3, seri portun **RX** pini olduğu için
`Serial.begin()` ile çakışıyordu. Bu yüzden GPIO15'e taşındı.
**Ders:** Pin seçmeden önce o pinin başka bir görevi olup olmadığına bak (UART, boot pinleri,
flash pinleri).

---

## 4. `delay()` yerine `millis()`

### Sorun
```cpp
void loop() {
  delay(2000);        // 2 saniye boyunca işlemci HİÇBİR ŞEY yapmaz
  sensorOku();
}
```
Web sunucusu eklenince `delay()` sorun olur. Sunucu gelen istekleri ancak
`server.handleClient()` çağrıldığında işleyebilir. `delay()` bekledikçe tarayıcı da cevap
alamadan bekler.

### Çözüm: beklemek yerine saate bak
```cpp
void loop() {
  server.handleClient();                    // her turda: isteklere cevap ver

  static unsigned long son = 0;
  if (millis() - son < 500) return;         // 500 ms dolmadıysa çık
  son = millis();

  sensorOku();                              // 500 ms'de bir
}
```
`millis()`, kart açıldığından beri geçen milisaniyeyi verir. `loop()` saniyede binlerce kez
döner ama işler yalnızca zamanı geldiğinde yapılır. Birden çok iş farklı aralıklarla
çalışabilir: ivme 500 ms'de, DHT 2 sn'de, MQTT gönderimi 5 sn'de bir.

> `millis() - son` yazımı taşmaya karşı güvenlidir. Sayaç yaklaşık 49 günde bir sıfıra döner
> ama işaretsiz çıkarma bu durumda da doğru sonucu verir.

**İleride:** FreeRTOS'ta aynı sorun, her işi ayrı bir **görev (task)** yaparak çözülür.

---

## 5. WiFi ve web sunucusu

### İstemci ve sunucu
```
Tarayıcı (istemci) ──"GET /"──▶ ESP32 (sunucu, port 80)
Tarayıcı           ◀──HTML───── ESP32
```
- **IP adresi:** Ağdaki cihazın adresi (ör. `10.13.37.2`).
- **Port:** O cihazdaki "kapı numarası". Web için 80 (HTTP) ve 443 (HTTPS) kullanılır.
- **Yol (path):** `/` sayfayı, `/veri` JSON verisini döndürür.

```cpp
server.on("/", anaSayfa);       // "/" istenirse anaSayfa() çalışsın
server.on("/veri", veriJson);   // "/veri" istenirse JSON döndür
```

Neden JSON? İnsan için HTML, **program** için JSON. Başka bir yazılım `/veri` adresinden veriyi
kolayca alıp işleyebilir.

### Wokwi'de port yönlendirme
ESP32'nin IP adresi, Wokwi'nin içindeki sanal ağa ait. Senin bilgisayarın o ağı göremez.
`wokwi.toml` içindeki şu ayar bir köprü kuruyor:
```toml
[[net.forward]]
from = "localhost:8180"   # bilgisayarındaki kapı
to = "target:80"          # simülasyondaki ESP32'nin kapısı
```
> Wokwi `wokwi.toml` dosyasını **sadece simülasyon başlarken** okur. Değiştirirsen
> simülatörü yeniden başlatman gerekir.

### Bu yöntemin sınırı
Sadece **aynı ağdayken** çalışır. ESP32 evdeyken sen okuldaysan ona ulaşamazsın, çünkü ev modemi
dışarıdan gelen bağlantıları içeri almaz. 100 cihaz olsa her birine tek tek sormak da gerekir.
MQTT bu iki sorunu çözüyor.

---

## 6. MQTT: aracı üzerinden haberleşme

### Kavram
Arada bir **aracı sunucu (broker)** var:
```
ESP32 ──yayınla──▶  BROKER  ──ilet──▶  Abone olan herkes
                 (internette)
```
- ESP32 kimsenin sormasını beklemez, veriyi **kendisi gönderir**.
- Broker mesajı, o konuyu dinleyen herkese iletir.
- ESP32 kimin dinlediğini bilmez. Sen de ESP32'nin nerede olduğunu bilmezsin. İkiniz de
  sadece broker'a bağlısınız.

**Neden çalışıyor?** Ağlar içeriden dışarıya giden bağlantılara izin verir, dışarıdan içeriye
gelenleri engeller. Burada hem ESP32 hem sen broker'a **dışarıya doğru** bağlanıyorsunuz.
Broker bir buluşma noktası.

> Benzetme: WhatsApp grubu. ESP32 gruba yazıyor, WhatsApp sunucusu (broker) gruptaki herkese
> iletiyor.

### Temel kavramlar

| Kavram | Anlamı | Bu projede |
|---|---|---|
| **Konu (topic)** | Mesajın gittiği "kanal", `/` ile hiyerarşik | `yavuz-sensordenemesi/esp32/veri` |
| **Yayınla (publish)** | Bir konuya mesaj gönder | ESP32, 5 sn'de bir veri yayınlar |
| **Abone ol (subscribe)** | Bir konuyu dinle | ESP32 `.../komut` konusunu dinler |
| **Joker `#`** | "Bunun altındaki her şey" | `yavuz-sensordenemesi/esp32/#` üç konuyu birden dinler |
| **Joker `+`** | Tek seviye | `+/esp32/veri` |
| **Retained** | Broker son mesajı saklar, yeni gelen abone hemen görür | `durum` mesajı |
| **Last Will (LWT)** | "Habersiz koparsam şunu yayınla" diye broker'a bırakılan mesaj | `cevrimdisi` |
| **Keepalive** | Bu süre içinde ses gelmezse cihaz kopmuş sayılır | 15 sn |
| **QoS** | Teslim garantisi: 0 = en fazla bir kez, 1 = en az bir kez, 2 = tam bir kez | veri 0, durum 1 |

### Bu projedeki konular

| Konu | Yön | İçerik |
|---|---|---|
| `.../veri` | ESP32 → broker | `{"sicaklik":24.0,"nem":40.0,"ax":0.00,"ay":0.00,"az":1.00}` |
| `.../durum` | ESP32 → broker | `cevrimici` / `cevrimdisi` (retained) |
| `.../komut` | broker → ESP32 | Kısa bir metin, OLED'de `> metin` olarak görünür |

### Cihaz takibi: retained ve Last Will birlikte
1. ESP32 bağlanırken broker'a bir vasiyet bırakır: *"Kopursam `durum` konusuna `cevrimdisi` yaz."*
2. Bağlanınca kendisi `cevrimici` yayınlar (retained).
3. Elektrik kesilirse ESP32 veda edemez. Broker 15 saniye ses alamayınca vasiyeti kendisi
   yayınlar.
4. Durum mesajı retained olduğu için sonradan bağlanan biri de cihazın durumunu hemen görür.

### Kütüphanenin yaptığı iş
`esp-mqtt` bağlantı koparsa **kendi kendine yeniden bağlanır**. Olaylar bir fonksiyona gelir:
```cpp
void mqttOlay(..., int32_t olayId, void *olayVerisi) {
  switch (olayId) {
    case MQTT_EVENT_CONNECTED:    // bağlandı → durum yayınla, komuta abone ol
    case MQTT_EVENT_DISCONNECTED: // koptu → kütüphane tekrar deneyecek
    case MQTT_EVENT_DATA:         // abone olunan konuya mesaj geldi
    case MQTT_EVENT_ERROR:        // hata
  }
}
```
Bu yapıya **olay güdümlü (event-driven)** programlama denir. Sürekli "mesaj var mı?" diye
sormazsın, mesaj gelince seni çağırırlar.

---

## 7. Engelli ağda hata ayıklama

Bu bölüm projenin en öğretici kısmı, çünkü gerçek işte de aynı şekilde yaşanır.

### Belirti
```
MQTT'ye baglaniliyor... basarisiz (durum -2)
```
`-2`, **TCP bağlantısının hiç kurulamadığı** anlamına gelir. Broker'a daha "merhaba" bile
denemedi.

### Olası sebepler
- DNS: broker'ın adı IP adresine çevrilemiyor
- Simülasyonun internet çıkışı hiç yok
- Broker kapalı
- Ağ, o portu engelliyor

### Tahmin etmek yerine ölçmek
ESP32'ye açılışta çalışan bir **teşhis kodu** eklendi:
```
DNS: test.mosquitto.org -> 54.36.178.49     ← DNS çalışıyor
  Port 80:   ACIK                           ← internet var
  Port 1883: KAPALI                         ← MQTT portu engelli
  Port 8883: KAPALI                         ← şifreli MQTT portu da engelli
```
Aynı test bilgisayardan da yapıldı (`Test-NetConnection`) ve **aynı sonuç** çıktı. Wokwi'nin
VS Code eklentisi simülasyonun trafiğini bilgisayarın üzerinden çıkardığı için ESP32 de
bilgisayarın bağlı olduğu ağın güvenlik duvarına takılıyor.

**Sonuç:** Kod doğruydu. Ağ sadece web portlarına (80, 443) izin veriyordu. Okul, yurt ve iş
yeri ağlarında bu sık görülür.

### Genel ders
Hata ayıklarken sorunu **katmanlara ayırıp her katmanı ayrı test et**:
```
DNS çalışıyor mu? → İnternet var mı? → Bu port açık mı? → Protokol doğru mu? → Uygulama
```
Her adım bir sonrakinin ön koşulu. I2C tarayıcının yaptığı da aynı şeydi.

---

## 8. WebSocket ve TLS

### WebSocket: MQTT'yi web trafiği gibi göndermek
Sadece 443 portu açık olduğu için MQTT mesajlarını **WebSocket** içine koyup 443'ten gönderdik.
Mesajlar aynı, sadece farklı bir kapıdan geçiyor. Güvenlik duvarları bunu normal web trafiği
olarak görür.

| Adres | Anlamı | Port |
|---|---|---|
| `mqtt://` | Düz MQTT | 1883 |
| `mqtts://` | Şifreli MQTT | 8883 |
| `ws://` | WebSocket içinde MQTT | 80 |
| `wss://` | Şifreli WebSocket içinde MQTT | 443 ← **bizimki** |

Kütüphane değişikliğinin sebebi de bu: PubSubClient WebSocket desteklemiyor. `esp-mqtt` ise
destekliyor ve Arduino çekirdeğinin içinde zaten var.

### TLS: şifreleme ve kimlik doğrulama
TLS iki iş yapar:
1. **Şifreleme:** Aradaki kimse mesajları okuyamaz.
2. **Kimlik doğrulama:** Konuştuğun sunucunun gerçekten o sunucu olduğunu kanıtlar.

İkinci iş **sertifika zinciri** ile yapılır:
```
public.cloud.shiftr.io  (sunucunun sertifikası)
        ↑ imzalayan
Let's Encrypt YR1       (ara sertifika)
        ↑ imzalayan
ISRG Root X1            (KÖK sertifika ← ESP32'nin güvendiği tek şey)
```
ESP32'ye sadece **kök sertifikayı** verdik (`include/ca_sertifika.h`). Sunucu bağlanırken
zincirin geri kalanını kendisi gönderir, ESP32 de zincirin bu köke ulaşıp ulaşmadığını
kontrol eder.

### `setInsecure()` neden kötü?
Bir ara `setInsecure()` kullanıldı. Bu yöntem şifrelemeyi korur ama **kimliği doğrulamaz**.
Araya giren biri kendini broker olarak tanıtabilir ve ESP32 bunu fark etmez. Buna
**ortadaki adam (man-in-the-middle)** saldırısı denir. Deneme için kabul edilebilir, üründe
kabul edilemez.

> Kök sertifikaların da son kullanma tarihi var. ISRG Root X1 2035'te sona eriyor. Gerçek
> ürünlerde sertifikayı uzaktan güncelleyebilmek (OTA) bu yüzden önemli.

---

## 9. İki görev, bir değişken: yarış durumu

### Durum
`esp-mqtt` kendi **FreeRTOS görevinde** çalışıyor, `loop()` ise başka bir görevde. İkisi
**aynı anda** çalışabilir:
```
MQTT görevi:  komut geldi → sonKomut'a "merhaba" yazıyor...
loop görevi:  ...tam o anda sonKomut'u okuyup ekrana basıyor
```
Yazma işlemi yarıdayken okuma yapılırsa ekranda yarım ya da bozuk bir metin çıkar. Buna
**yarış durumu (race condition)** denir. Nadiren olur, tekrarlaması zordur, bu yüzden bulması da
en zor hatalardandır.

### Çözüm: kritik bölge
```cpp
portENTER_CRITICAL(&komutKilidi);
memcpy(sonKomut, olay->data, n);     // bu arada kimse araya giremez
portEXIT_CRITICAL(&komutKilidi);
```
Okuyan taraf da aynı kilidi kullanır ve metnin bir kopyasını alıp kilidi hemen bırakır:
```cpp
portENTER_CRITICAL(&komutKilidi);
strcpy(komut, sonKomut);
portEXIT_CRITICAL(&komutKilidi);
display.printf("> %s", komut);       // uzun iş kilidin DIŞINDA
```
**Kural:** Kritik bölgeyi olabildiğince kısa tut. İçindeyken kesmeler (interrupt) bekler.

### `volatile` ne işe yarıyor?
```cpp
volatile bool mqttBagli = false;
```
Derleyiciye "bu değişken başka bir yerden değişebilir, her seferinde bellekten yeniden oku"
der. Tek bir `bool` için bu yeterli. Birden fazla byte'lık bir veri (metin gibi) için yetmez,
kilit gerekir.

---

## 10. Kendini test et

Cevaplar yukarıdaki bölümlerde.

1. I2C tarayıcı hiç cihaz bulamıyorsa ilk nereye bakarsın? Neden koda değil?
2. MPU6050'yi uyandırmadan `0x3B` register'ından okursan ne görürsün?
3. `ham[0] << 8 | ham[1]` ifadesini neden `(int16_t)` ile sarıyoruz?
4. `loop()` içine `delay(2000)` koyarsan web sayfası neden yavaşlar?
5. Web sunucusu yöntemiyle başka bir şehirden veriyi neden göremezsin? MQTT bunu nasıl çözüyor?
6. Retained mesaj ile Last Will birlikte hangi sorunu çözüyor?
7. `durum -2` hatasını aldığında sebebini bulmak için hangi adımları izlerdin?
8. `setInsecure()` şifrelemeyi kapatıyor mu? Neyi kapatıyor?
9. ESP32'ye neden sunucunun sertifikasını değil de kök sertifikayı veriyoruz?
10. İki görev aynı metin değişkenine erişiyorsa `volatile` neden yetmez?

---

## 11. Çalıştırma

Gereksinimler: VS Code, PlatformIO eklentisi, Wokwi eklentisi.

```
pio run                        # derle
F1 → Wokwi: Start Simulator    # simülasyonu başlat
```

| Ne | Nerede |
|---|---|
| Web sayfası | http://localhost:8180 |
| JSON verisi | http://localhost:8180/veri |
| Broker'ın canlı görünümü | https://public.cloud.shiftr.io |

**Komut göndermek için** herhangi bir MQTT istemcisi kullanılabilir (ör. HiveMQ Web Client):
host `public.cloud.shiftr.io`, port `443`, SSL açık, kullanıcı adı ve şifre `public` / `public`.
`yavuz-sensordenemesi/esp32/komut` konusuna mesaj gönder.

> **Uyarı:** Broker herkese açık. Konu adını bilen herkes veriyi görebilir ve komut gönderebilir.
> Deneme için sorun değil. Gerçek bir cihazda broker'ın kimlik doğrulaması olur ve her cihaz
> sadece kendi konularına erişebilir.

### Dosyalar

| Dosya | İçerik |
|---|---|
| `src/main.cpp` | Tüm firmware |
| `include/ca_sertifika.h` | ISRG Root X1 kök sertifikası |
| `diagram.json` | Wokwi devre şeması |
| `wokwi.toml` | Firmware yolu ve port yönlendirme |
| `platformio.ini` | Kart, framework, kütüphaneler |

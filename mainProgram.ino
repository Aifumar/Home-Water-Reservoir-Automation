/*
#####################################################
-------------------- KELOMPOK 6 ---------------------
-- IoT Sistem Monitoring Tandon Air dengan Sensor ---
- Debit Air dan Ultrasonik berbasis Server Antares --
------------ ELECTRICAL ENGINEERING 02 --------------
#####################################################

-------------------------------------
SPESIFIKASI TANDON YANG DIGUNAKAN

Merk : Penguin
Model : TB 110
Tinggi : 142 cm
Diameter : +/- 100 cm
Tinggi tampungan air : +/- 130 cm
Volume : 1050 liter
-------------------------------------

KONFIGURASI PIN
Trig = 0
Echo = 2
Relay = 14

SCK = D1
SDA = D2
*/

#define TRIGGER_PIN 0
#define ECHO_PIN 2
#define RELAY_PIN 14
#define SENSOR 12
#define GND 16
#define BUZZ 13

#define tinggi_tandon 130 // Tinggi tandon (cm)
#define volume_tandon 1050 // Volume tandon dalam liter
#define batas_bawah 20 // Batas ketinggian air untuk menyalakan pompa (cm)
#define batas_atas 120 // Batas ketinggian air untuk mematikan pompa (cm)
#define waktu_cek 30 // Lama waktu cek keadaan pompa

#define ACCESSKEY "058f2dff81fc07c6:bfa9004d3678320f" // Kunci Akses pada platform Antares
#define WIFISSID "Internet@wifi.id" // Nama WiFi
#define PASSWORD "asdfghjkl" // Kata sandi WiFi
String projectName = "coba_antares1"; // Nama proyek pada Antares
String deviceName = "ESP8266_1"; // Nama device pada Antares

#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <Wire.h>
#include <AntaresESPHTTP.h>

AntaresESPHTTP antares(ACCESSKEY);
Adafruit_SSD1306 display(128, 64, &Wire);

long currentMillis = 0;
long previousMillis = 0;
int interval = 1000;
boolean ledState = LOW, pompa, pompa_error = 0;
float calibrationFactor = 4.5;
volatile byte pulseCount;
byte pulse1Sec = 0;
float flowRate;
unsigned int flowMilliLitres;
unsigned long totalMilliLitres;

const unsigned char L_flow [] PROGMEM = {
	0x38, 0x00, 0x44, 0x10, 0x82, 0x10, 0x81, 0x20, 0x00, 0xc0, 0x38, 0x00, 0x44, 0x10, 0x82, 0x10, 
	0x81, 0x20, 0x00, 0xc0
};

const unsigned char L_waktu [] PROGMEM = {
	0x0c, 0x00, 0x33, 0x00, 0x40, 0x80, 0x42, 0x80, 0x84, 0x40, 0x88, 0x40, 0x40, 0x80, 0x40, 0x80, 
	0x33, 0x00, 0x0c, 0x00
};

const unsigned char L_tinggi [] PROGMEM = {
	0x10, 0x20, 0x10, 0x70, 0x38, 0xa8, 0x38, 0x20, 0x7c, 0x20, 0xfe, 0x20, 0xf6, 0x20, 0x6c, 0xa8, 
	0x7c, 0x70, 0x10, 0x20
};

int tinggi_air, volume, persentase, y, h, debit, hitung_pompa = 0, kirim = 0;
long durasi, jarak;

void IRAM_ATTR pulseCounter()
{
  pulseCount++;
}

void setup() {
  Serial.begin(115200);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  splashScreen();
  pinMode(SENSOR, INPUT_PULLUP);
  pinMode(GND, OUTPUT);
  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZ, OUTPUT);

  digitalWrite(GND, LOW);
  digitalWrite(BUZZ, HIGH);
  delay(2000);
  digitalWrite(BUZZ, LOW);
  delay(300);
  digitalWrite(BUZZ, HIGH);
  delay(200);
  digitalWrite(BUZZ, LOW);
  delay(100);
  digitalWrite(BUZZ, HIGH);
  delay(200);
  digitalWrite(BUZZ, LOW);

  pulseCount = 0;
  flowRate = 0.0;
  flowMilliLitres = 0;
  totalMilliLitres = 0;
  previousMillis = 0;
  delay(200);

  wifi();
  antares.setDebug(true);
  antares.wifiConnection(WIFISSID, PASSWORD);
  display.fillRoundRect(8, 45, 110, 5, 3, 1);
  display.display();
  attachInterrupt(digitalPinToInterrupt(SENSOR), pulseCounter, FALLING);
  delay(1000);
  digitalWrite(BUZZ, HIGH);
  delay(100);
  digitalWrite(BUZZ, LOW);
  display.clearDisplay();
}

void splashScreen() {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setCursor(34, 10);
    display.setFont(NULL);
    display.println("KELOMPOK 6");
    display.setCursor(13, 34);
    display.println("SISTEM MONITORING");
    display.setCursor(22, 45);
    display.println("TANDON AIR IoT");
    display.drawLine(15, 25, 113, 25, 1);
    display.drawRoundRect(0, 0, 128, 64, 3, 1);
    display.display();
}

void wifi(){
  display.clearDisplay();
  display.drawRoundRect(0, 0, 128, 64, 3, 1);
  display.setCursor(15, 13);
  display.println("Menghubungkan ke");
  display.setCursor(8, 25);
  display.println("Jaringan Internet..");
  display.drawRoundRect(8, 45, 110, 5, 3, 1);
  display.fillRoundRect(8, 45, 10, 5, 3, 1);
  display.display();
}

void baca_keadaan() {
  digitalWrite(TRIGGER_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIGGER_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGGER_PIN, LOW);

  durasi = pulseIn(ECHO_PIN, HIGH);
  jarak = (durasi / 2) / 29.1;
  tinggi_air = tinggi_tandon - jarak;
  volume = tinggi_air * (volume_tandon / tinggi_tandon);
  persentase = tinggi_air * 100 / tinggi_tandon;

  if (persentase <= 0){
    persentase = 0;
  } else if (persentase >= 100){
    persentase = 100;
  }

  if (volume <= 0){
    volume = 0;
  }

  y = 49 - (persentase / 4);

  if (y == 0) {
    y = 1;
  }
  
  h = persentase / 4;
  
  if (h == 0) {
    h = 1;
  }
}

void updateDisplay() {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setFont(NULL);
    display.setCursor(2, 2);
    display.println("MONITORING TANDON AIR");
    display.drawLine(5, 11, 123, 11, 1);
    display.drawRoundRect(2, 22, 24, 29, 3, 1);
    display.fillRoundRect(5, 19, 18, 3, 3, 1);
    display.setTextSize(1);
    display.fillRect(4, y, 20, h, 1);
    display.setCursor(3, 54);
    display.print(String(persentase)+"%");
    display.drawBitmap(36, 19, L_flow, 12, 10, WHITE);
    display.drawBitmap(37, 35, L_waktu, 10, 10, WHITE);
    display.drawBitmap(36, 51, L_tinggi, 13, 10, WHITE);
    display.setCursor(55, 20);
    display.print(String(debit)+" L/mnt");
    display.setCursor(55, 36);
    
    if (debit == 0){
      display.print("~ mnt");
    } else {
      display.print(String((volume_tandon-volume)/debit)+" mnt");
    }
    
    display.setCursor(55, 52);
    display.print(String(volume)+" Liter");

    if (pompa) {
      display.fillCircle(120, 38, 4, 1);
    } else {
      display.drawCircle(120, 38, 4, 1);
    }

    if (pompa_error) {
      display.setCursor(110, 35);
      display.print("!");
    }
    display.display();
}

void peringatan() {
  for (int n = 0; n < 4; n++) {
    digitalWrite(BUZZ, HIGH);
    delay(200);
    digitalWrite(BUZZ, LOW);
    delay(200);
  }
}

void antares_kirim() {
  if (kirim > 4) {
    kirim = 0;
    antares.add("volume", volume);
    antares.add("debit", debit);
    if (debit == 0){
      antares.add("timer","~ mnt");
    } else {
      antares.add("timer", (volume_tandon-volume)/debit);
    }
    antares.add("pompa", pompa);
    antares.add("persentase", persentase);
    antares.add("alert", pompa_error);
    antares.send(projectName, deviceName);
  } else {
    kirim++;
  } 
}

void loop() {
  currentMillis = millis();
  if (currentMillis - previousMillis > interval) {
    pulse1Sec = pulseCount;
    pulseCount = 0;
    flowRate = ((1000.0 / (millis() - previousMillis)) * pulse1Sec) / calibrationFactor;
    debit = flowRate;
    previousMillis = millis();
    flowMilliLitres = (flowRate / 60) * 1000;
    totalMilliLitres += flowMilliLitres;

    Serial.print("Flow rate: ");
    Serial.print(int(flowRate));
    Serial.println("L/min");
    baca_keadaan();
    Serial.print("Tinggi Air : ");
    Serial.print(tinggi_air);
    Serial.println(" cm");
    Serial.println("Volume Air : "+String(volume)+" Liter");
    Serial.println("Volume Air : "+String(persentase)+"%");

    if ((tinggi_air <= batas_bawah) && !pompa) {
      digitalWrite(RELAY_PIN, HIGH);
      Serial.println("Mengaktifkan Pompa");
      pompa = 1;
    } else if ((tinggi_air >= batas_atas) && pompa){
      digitalWrite(RELAY_PIN, LOW);
      Serial.println("Mematikan Pompa");
      pompa = 0;
    }

    if (pompa && hitung_pompa < waktu_cek && debit == 0) {
      hitung_pompa++;
    } else if (pompa && hitung_pompa < waktu_cek && debit != 0){
      hitung_pompa = 0;
    } else if (pompa && hitung_pompa >= waktu_cek && debit == 0){
      digitalWrite(RELAY_PIN, LOW);
      pompa_error = 1;
      peringatan();
      Serial.println("Pompa Error");
    }
    updateDisplay();
    antares_kirim();
  }
}

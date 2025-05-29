#include <Wire.h>
#include <ESP32Servo.h>
#include <RTClib.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <SD.h>

// OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// RTC
RTC_DS3231 rtc;

// Servo și buzzer
Servo servo;
#define SERVO_PIN 26
#define BUZZER_PIN 27

// Butoane
#define BUTTON_FEED 4
#define BUTTON_RESET 33
#define BUTTON_LOG 34 

// LED-uri
#define LED_GREEN 12
#define LED_YELLOW 13
#define LED_RED 14

// Senzor vibratii
#define VIBRATION_PIN 32

#define SD_CS 5

int feedCount = 0;
unsigned long lastFeedMillis = 0;
const unsigned long feedCooldown = 2000;
volatile bool feedInterrupt = false;
bool showResetMsg = false;
unsigned long resetMsgMillis = 0;

unsigned long lastVibrationMsgMillis = 0;
const unsigned long vibrationCooldown = 60000;
unsigned long vibrationIgnoreUntil = 0;


void IRAM_ATTR onFeedInterrupt() {
  feedInterrupt = true;
}

void updateLEDs() {
  digitalWrite(LED_GREEN, feedCount <= 3);
  digitalWrite(LED_YELLOW, feedCount >= 4 && feedCount <= 6);
  digitalWrite(LED_RED, feedCount > 6);
}

void displayTime(DateTime now) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  if (showResetMsg && millis() - resetMsgMillis < 10000) {
    display.setTextSize(1);
    String msg = "Contor resetat";

    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(msg, 0, 0, &x1, &y1, &w, &h);
    int x = (SCREEN_WIDTH - w) / 2;
    int y = (SCREEN_HEIGHT - h) / 2;

    display.setCursor(x, y);
    display.println(msg);
  } else {
    display.setTextSize(2);
    char timeStr[6];
    sprintf(timeStr, "%02d:%02d", now.hour(), now.minute());

    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(timeStr, 0, 0, &x1, &y1, &w, &h);
    int x_time = (SCREEN_WIDTH - w) / 2;
    display.setCursor(x_time, 0);
    display.println(timeStr);

    display.setTextSize(1);
    String title = "Chloe's Feeder";
    display.getTextBounds(title, 0, 0, &x1, &y1, &w, &h);
    int x_title = (SCREEN_WIDTH - w) / 2;
    display.setCursor(x_title, 24);  
    display.println(title);
  }

  display.display();
}

void logSimple(const String& text) {
  DateTime now = rtc.now();
  String logEntry = "[" + now.timestamp(DateTime::TIMESTAMP_FULL) + "] " + text;

  File f = SD.open("/log.txt", FILE_APPEND);
  if (f) {
    f.println(logEntry);
    f.close();
    Serial.println("SCRIS: " + logEntry);
  } else {
    Serial.println("⚠️ EROARE la deschiderea log.txt pentru scriere!");
    return;
  }

  // Verif daca scrie bine pe SD
  File fRead = SD.open("/log.txt");
  String lastLine = "";
  if (fRead) {
    while (fRead.available()) {
      lastLine = fRead.readStringUntil('\n');
    }
    fRead.close();
    lastLine.trim();

    if (lastLine == logEntry) {
      Serial.println("Confirmare: ultima linie coincide.");
    } else {
      Serial.println("EROARE: ultima linie NU coincide!");
      Serial.println("Ultima linie: " + lastLine);
    }
  } else {
    Serial.println("⚠️ EROARE la deschiderea log.txt pentru citire!");
  }
}

void printSDLog() {
  File f = SD.open("/log.txt");
  if (!f) {
    Serial.println("Nu pot deschide log.txt");
    return;
  }

  Serial.println("=== LOG DE PE CARD ===");
  while (f.available()) {
    Serial.write(f.read());
  }
  f.close();
  Serial.println("=== SFÂRȘIT LOG ===");
}

// 🔽 FUNCȚIE NOUĂ: afișează ultimul log
void showLastLog() {
  File f = SD.open("/log.txt");
  if (!f) {
    Serial.println("Nu pot deschide log.txt");
    return;
  }

  String lastLine = "";
  while (f.available()) {
    lastLine = f.readStringUntil('\n');
  }
  f.close();

  Serial.println("Ultimul log: " + lastLine);

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(lastLine, 0, 0, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH - w) / 2;
  int y = (SCREEN_HEIGHT - h) / 2;

  display.setCursor(x, y);
  display.println(lastLine);
  display.display();

  delay(3000); // Afișează 3 secunde
}

void feed(const String& source) {
  if (millis() - lastFeedMillis < feedCooldown) return;

  servo.detach();
  delay(50);
  servo.attach(SERVO_PIN, 500, 2400);
  servo.write(180);
  delay(500);
  servo.write(0);

  tone(BUZZER_PIN, 1000);
  delay(300);
  noTone(BUZZER_PIN);

  feedCount++;
  updateLEDs();
  logSimple("Hranire " + source);
  lastFeedMillis = millis();

  vibrationIgnoreUntil = millis() + 10000;
}

void resetFeed() {
  feedCount = 0;
  updateLEDs();
  logSimple("Reset contor");

  showResetMsg = true;
  resetMsgMillis = millis();
}

void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_FEED, INPUT_PULLUP);
  pinMode(BUTTON_RESET, INPUT_PULLUP);
  pinMode(BUTTON_LOG, INPUT); // GPIO34 NU are PULLUP intern
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(VIBRATION_PIN, INPUT);

  if (!rtc.begin()) Serial.println("Eroare RTC!");
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) Serial.println("Eroare OLED!");
  if (!SD.begin(SD_CS)) Serial.println("Eroare card SD!");
  else printSDLog();

  attachInterrupt(digitalPinToInterrupt(BUTTON_FEED), onFeedInterrupt, FALLING);

  servo.setPeriodHertz(50);
  servo.attach(SERVO_PIN, 500, 2400);
  servo.write(0);

  updateLEDs();
  showResetMsg = false;
}

void loop() {
  DateTime now = rtc.now();
  displayTime(now);

  // Hrănire automată
  if ((now.hour() == 8 || now.hour() == 13 || now.hour() == 18) &&
      now.minute() == 0 && now.second() == 0) {
    feed("automata");
    delay(1000);
  }

  // Hrănire manuală
  noInterrupts();
  bool shouldFeed = feedInterrupt;
  feedInterrupt = false;
  interrupts();

  if (shouldFeed) {
    feed("manuala");
  }

  // Reset contor
  if (digitalRead(BUTTON_RESET) == LOW) {
    resetFeed();
    delay(300);
  }

  // Afișare ultim log
  if (digitalRead(BUTTON_LOG) == HIGH) { // Butonul legat la 3.3V, rezistență pull-down externă!
    showLastLog();
    delay(300);
  }

  // Detectare vibrație
  if (millis() > vibrationIgnoreUntil && digitalRead(VIBRATION_PIN) == LOW) {
    if (millis() - lastVibrationMsgMillis > vibrationCooldown) {
      DateTime now = rtc.now();
      logSimple("Pisica a mancat");
      lastVibrationMsgMillis = millis();
    }
    delay(300);
  }

  delay(1000);
}

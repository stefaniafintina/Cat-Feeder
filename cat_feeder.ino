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
const int TIME_OFFSET = 3; // România: GMT+3

// Servo
Servo servo;
#define SERVO_PIN 26
#define BUZZER_PIN 27

// Butoane
#define BUTTON_FEED 4
#define BUTTON_RESET 5

// LED-uri
#define LED_GREEN 12
#define LED_YELLOW 13
#define LED_RED 14

// Senzor vibrații
#define VIBRATION_PIN 32

// SD Card
#define SD_CS 15

// Stare
int feedCount = 0;
unsigned long lastFeedMillis = 0;
const unsigned long feedCooldown = 2000;
volatile bool feedInterrupt = false;

// Convertire UTC → România (GMT+3)
DateTime getLocalTime() {
  return rtc.now() + TimeSpan(TIME_OFFSET * 3600);
}

void IRAM_ATTR onFeedInterrupt() {
  feedInterrupt = true;
}

void updateLEDs() {
  digitalWrite(LED_GREEN, feedCount <= 3);
  digitalWrite(LED_YELLOW, feedCount >= 4 && feedCount <= 5);
  digitalWrite(LED_RED, feedCount >= 6 && feedCount <= 7);
}

void displayLastFeedTime(DateTime utcNow) {
  DateTime now = utcNow + TimeSpan(TIME_OFFSET * 3600);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Ultima hranire:");
  if (now.hour() < 10) display.print("0");
  display.print(now.hour());
  display.print(":");
  if (now.minute() < 10) display.print("0");
  display.print(now.minute());
  display.display();
}

void displayCurrentTime(DateTime utcNow) {
  DateTime now = utcNow + TimeSpan(TIME_OFFSET * 3600);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 20);
  display.print("Timp curent: ");
  if (now.hour() < 10) display.print("0");
  display.print(now.hour());
  display.print(":");
  if (now.minute() < 10) display.print("0");
  display.print(now.minute());
  display.print(":");
  if (now.second() < 10) display.print("0");
  display.print(now.second());
  display.display();
}

void logEvent(const String& msg) {
  File f = SD.open("/log.txt", FILE_APPEND);
  if (f) {
    DateTime now = rtc.now();  // păstrăm UTC în log
    f.print("[");
    f.print(now.timestamp(DateTime::TIMESTAMP_FULL));
    f.print("] ");
    f.println(msg);
    f.close();
  }
}

void feed(const String& source) {
  if (millis() - lastFeedMillis < feedCooldown) return;

  servo.write(90);
  delay(500);
  servo.write(0);

  tone(BUZZER_PIN, 1000);
  delay(2000);
  noTone(BUZZER_PIN);

  feedCount++;
  updateLEDs();

  DateTime now = rtc.now();
  displayLastFeedTime(now);
  logEvent("Hranire " + source);

  lastFeedMillis = millis();
}

void resetFeed() {
  feedCount = 0;
  updateLEDs();
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("Contor resetat");
  display.display();
  logEvent("Reset contor");
}

void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_FEED, INPUT_PULLUP);
  pinMode(BUTTON_RESET, INPUT_PULLUP);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(VIBRATION_PIN, INPUT);

  attachInterrupt(digitalPinToInterrupt(BUTTON_FEED), onFeedInterrupt, FALLING);

  servo.setPeriodHertz(50);
  servo.attach(SERVO_PIN, 500, 2400);
  servo.write(0);

  if (!rtc.begin()) Serial.println("Eroare RTC!");
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) Serial.println("Eroare OLED!");
  if (!SD.begin(SD_CS)) Serial.println("Eroare card SD!");

  updateLEDs();
  displayLastFeedTime(rtc.now());
}

void loop() {
  DateTime now = getLocalTime();

  // Hrănire automată la orele 08:00, 14:00, 20:00
  if ((now.hour() == 8 || now.hour() == 14 || now.hour() == 20) &&
      now.minute() == 0 && now.second() == 0) {
    feed("automata");
    delay(1000);
  }

  if (feedInterrupt) {
    feedInterrupt = false;
    feed("manuala");
  }

  if (digitalRead(BUTTON_RESET) == LOW) {
    resetFeed();
    delay(300);
  }

  if (digitalRead(VIBRATION_PIN) == LOW) {
    logEvent("Vibratie detectata");
    delay(300);
  }

  // Afișare ceas real
  displayCurrentTime(rtc.now());
  delay(1000);
}

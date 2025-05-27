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


// LED-uri
#define LED_GREEN 12
#define LED_YELLOW 13
#define LED_RED 14

// Senzor vibrații
#define VIBRATION_PIN 32

// Card SD
#define SD_CS 5

// Stări
int feedCount = 0;
unsigned long lastFeedMillis = 0;
const unsigned long feedCooldown = 2000;
volatile bool feedInterrupt = false;
bool showResetMsg = false;
unsigned long resetMsgMillis = 0;

// ----- Funcții -----

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
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (showResetMsg && millis() - resetMsgMillis < 10000) {
    display.setCursor(0, 0);
    display.println("Contor resetat");
  } else {
    display.setCursor(0, 0);
    display.println("Ora curenta:");
    display.setCursor(0, 10);
    if (now.hour() < 10) display.print("0");
    display.print(now.hour()); display.print(":");
    if (now.minute() < 10) display.print("0");
    display.print(now.minute());
  }

  display.display();
}

void logEvent(const String& msg) {
  File f = SD.open("/log.txt", FILE_APPEND);
  if (f) {
    DateTime now = rtc.now();
    f.print("[");
    f.print(now.timestamp(DateTime::TIMESTAMP_FULL));
    f.print("] ");
    f.println(msg);
    f.close();
  }
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
  logEvent("Hranire " + source);
  lastFeedMillis = millis();
}


void resetFeed() {
  feedCount = 0;
  updateLEDs();
  logEvent("Reset contor");

  showResetMsg = true;
  resetMsgMillis = millis();
}

// void printSDLog() {
//   File f = SD.open("/log.txt");
//   if (!f) {
//     Serial.println("Nu pot deschide log.txt");
//     return;
//   }

//   Serial.println("== Conținutul log.txt ==");
//   while (f.available()) {
//     Serial.write(f.read());
//   }
//   f.close();
//   Serial.println("== Sfârșit log ==");
// }


// ----- Setup -----

void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_FEED, INPUT_PULLUP);
  pinMode(BUTTON_RESET, INPUT_PULLUP);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(VIBRATION_PIN, INPUT);

  if (!rtc.begin()) Serial.println("Eroare RTC!");
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) Serial.println("Eroare OLED!");
  if (!SD.begin(SD_CS)) Serial.println("Eroare card SD!");

  attachInterrupt(digitalPinToInterrupt(BUTTON_FEED), onFeedInterrupt, FALLING);

  servo.setPeriodHertz(50);
  servo.attach(SERVO_PIN, 500, 2400);  
  servo.write(0);

  updateLEDs();
  showResetMsg = false;
}

// ----- Loop -----

void loop() {
  DateTime now = rtc.now();
  displayTime(now);

  // Hrănire automată: 08:00, 13:00, 18:00
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

  // Detectare vibrație
  if (digitalRead(VIBRATION_PIN) == LOW) {
    logEvent("Pisica a mancat (vibratie)");
    delay(300);
  }

  static bool moved = false;
  if (!moved) {
    servo.write(180);
    delay(500);
    servo.write(0);
    moved = true;
  }


  delay(1000);
}

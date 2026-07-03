#include <ESP32Servo.h>  // <- ESP32-compatible Servo library
#include <WiFi.h>        //WiFi connector
#include <LittleFS.h>    // ESP32 Local Hosting
#include <Arduino.h>     //Core lib.
#include "DHT.h"
#include <Wire.h>  //LCD
#include <LiquidCrystal_I2C.h>
#include <ESPAsyncWebServer.h>  //web updating
#include <ArduinoJson.h>
#include "DFRobotDFPlayerMini.h"

//WiFi to be connected
//const char* ssid = "ZTE_2.4G_geYzgg";
//const char* password = "2XXFRejF";
//const char* ssid = "Dioneda";
//const char* password = "_Bambino12345";
//const char* ssid = "kamito";
//const char* password = "laptopUse";
const char* ssid = "JUSTIN";
const char* password = "HANNAHLEIJ";

#define FS LittleFS
#define DHTTYPE DHT22
#define DHTPIN 32
#define ECHO_PIN 23
#define TRIG_PIN 19

int LDRPIN = 35;
int SSPIN = 33;             //Sound Sensor
int CtouchSensorPin2 = 27;  //CTS 2
int CTouchSensorPin1 = 14;  //CTS 1
int CTouchSensorPin3 = 12;  //CTS 3
int LEDPIN3 = 25;           //LDRLED
int ServoPin = 18;          //servo
int ServoGreenLED = 4;      // Open servo
int ServoRedLED = 5;        // close servo
int TXDpin = 17;            //DF Robot DF Player mini
int RXDpin = 16;            //DF Robot DF Player mini
int SDApin = 21;            //LCD SDA
int SCLpin = 22;            //LCD SCL
int ExhaustFanPin = 26;     //DHT fan
int LEDPIN2 = 0;           // Sound sensor LED
int LEDPIN4 = 13;            //Living room lights
int BUZZPIN = 15;           //Active Buzzer 1
int BUZZPIN2 = 2;           //Active Buzzer 2

//Condition Variables
bool touching = false;            // Indoor Touch Only
bool oneTouching = false;         //track if only one is touched
bool successTriggered = false;    //track if 2 CTS touch for 3 sec
bool inDoorTouchLatched = false;  // prevents retrigger while held
bool showingGreeting = false;     //track if lcd shows greetings
bool objectWasClose = false;      //track if object is still within 10cm
bool servoActive = false;         // track if servo is rotated
bool servoLocked = false;
bool webGreetingState = false;  // persistent web greeting toggle

//DF Player parameters
bool SSLED = false;  //Sound Sensor LED
bool lastSSLED = false;
bool exhaustState = false;
bool lastExhaustState = false;
bool bedroomLightState = false;
bool lastBedroomLightState = false;

//Global Variables
int connectionFailureCount = 0;
int LDRvalue = 0;          // Light sensor
int intruderAttempts = 0;  // Counts failed attempts (single touch/early release)
float distance = -1;       // Ultrasonic distance
float tempValue = 0.0;
float humidValue = 0.0;
float heatIndex = 0.0;
float tempValueF = 0.0;
float humidValueF = 0.0;
float heatIndexF = 0.0;
float luxValue = 0.0;  // LDR → Lux
float soundDB = 0.0;   // Mic → dB

//for calculation
float Rfixed = 10000.0;  // 10k potentiometer
float LDR_gamma = 0.85;  // LDR gamma value
float LDR_R0 = 850000;   // LDR resistance at 10 lux
const float DIM_LUX = 100;


// Timers
unsigned long lastLCDNormalUpdate = 0;
unsigned long lastDHTread = 0;
unsigned long lastCTSread = 0;
unsigned long lastOneTouchRead = 0;
unsigned long lastTwoTouchRead = 0;
unsigned long greetingStartTime = 0;
unsigned long servoTriggerTime = 0;
unsigned long lastWiFiCheck = 0;
unsigned long webGreetingStart = 0;
unsigned long servoLockStartTime = 0;  // lock timer
long servoLockRemaining = 0;           // seconds remaining in locked mode

const unsigned long webGreetingDuration = 5000;    // 5 seconds
const unsigned long DHT_INTERVAL = 5000;           // 5 seconds
const unsigned long CTS_INTERVAL = 500;            // 0.5 senconds
const unsigned long T_INTERVAL = 3000;             // 3 seconds - touch interval
const unsigned long OT_INTERVAL = 1000;            // 1 second - indoor CTS
const unsigned long SERVO_DURATION = 10000;        // 10 seconds
const unsigned long WIFI_CHECK_INTERVAL = 10000;   // Check Wi-Fi status every 10 seconds
const unsigned long LCD_INTERVAL = 2000;           // 2 seconds
const unsigned long SERVO_LOCK_DURATION = 100000;  // 100 seconds

// Control Variables
bool manualControlLDR = false;
bool manualControlSound = false;
bool manualControlExhaust = false;
bool manualControlServo = false;
bool manualControlBedroomLight = false;
bool manualControlLivingLight = false;
bool manualControlKitchenFan = false;
bool manualControlKitchenLight = false;
bool manualControlGreeting = false;

LiquidCrystal_I2C lcd(0x27, 20, 4);
DHT dht(DHTPIN, DHTTYPE);
Servo myServo;
DFRobotDFPlayerMini dfPlayer;
HardwareSerial dfSerial(2);
AsyncWebServer server(80);

// LCD priority display viriables
enum LCDMode {
  LCD_NORMAL,
  LCD_GREETING,
  LCD_WEB_GREETING,
  LCD_INTRUDER
};
LCDMode lcdMode = LCD_NORMAL;
LCDMode lastLCDMode = LCD_NORMAL;
bool lcdNeedsRedraw = true;

struct GreetingState {
  bool active = false;
  unsigned long startTime = 0;
  enum Source { ULTRASONIC,
                WEB } source;
} greeting;

//Calculators
float calculateLux(int adcValue) {
  float voltage = (4095 - adcValue) * (3.3 / 4095.0);
  if (voltage <= 0.01) return 0;

  float Rldr = Rfixed * (3.3 / voltage - 1);
  float lux = pow(LDR_R0 / Rldr, 1.0 / LDR_gamma);

  if (lux < 0) lux = 0;
  return lux;
}
float calculateDB() {
  int samples = 1000;
  float vMax = 0;
  float vMin = 3.3;

  for (int i = 0; i < samples; i++) {
    float v = analogRead(SSPIN) * (3.3 / 4095.0);
    if (v > vMax) vMax = v;
    if (v < vMin) vMin = v;
    delayMicroseconds(300);
  }

  float vPP = vMax - vMin;
  float vRMS = vPP / (2.0 * sqrt(2));

  // Reference: 55 dB ≈ 50 mV
  float referenceDB = 55;
  float referenceV = 0.050;

  float dB = referenceDB + 20 * log10(vRMS / referenceV);

  if (isnan(dB) || dB < 0) dB = 0;

  return dB;
}

//WiFi auto connect once disconnected
void connectToWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Attempting to reconnect to WiFi...");

    // Explicitly disconnect to clear any bad state
    WiFi.disconnect();
    delay(100);

    WiFi.begin(ssid, password);

    // Give it a brief chance to connect
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10) {
      delay(500);
      Serial.print(".");
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nSuccessfully reconnected.");
      connectionFailureCount = 0;  // Reset counter on success
    } else {
      Serial.println("\nFailed to reconnect.");
      connectionFailureCount++;
    }
  }
}

// Buzzer methods
void beepOnce() {  //Beep the buzzer one time
  //digitalWrite(BUZZPIN, HIGH);
  tone(BUZZPIN, 20000, 150);
  tone(BUZZPIN2, 20000, 150);
}

void beepThreeTimes() {  //Beep the buzzer three times
  for (int i = 0; i < 3; i++) {
    tone(BUZZPIN, 1500, 150);
    tone(BUZZPIN2, 1500, 150);
    delay(300);
  }
}

// Open the door for 10 seconds
void openServo() {
  // Activate servo
  myServo.write(90);
  servoActive = true;
  servoTriggerTime = millis();
  updateServoLEDs();
}
void closeServo() {
  myServo.write(0);
  servoActive = false;
  updateServoLEDs();
}
void updateServoLEDs() {
  if (servoActive) {
    digitalWrite(ServoGreenLED, HIGH);
    digitalWrite(ServoRedLED, LOW);
  } else {
    digitalWrite(ServoGreenLED, LOW);
    digitalWrite(ServoRedLED, HIGH);
  }
}

//Play DF Player
void playAudio(int audioNumber) {
  if (!manualControlSound) return;
  dfPlayer.stop();
  dfPlayer.playMp3Folder(audioNumber);
}

void readDHT22() {
  //DHT22 Sensor Codes
  humidValue = dht.readHumidity();                                 //humidity percentage
  tempValue = dht.readTemperature();                               //celcius
  heatIndex = dht.computeHeatIndex(tempValue, humidValue, false);  //reads in celcius
  tempValueF  = dht.readTemperature(true);                            // temperature in Fahrenheit
  heatIndexF = dht.computeHeatIndex(tempValueF, humidValue, true);    // heat index in Fahrenheit

  if (isnan(humidValue) || isnan(tempValue) || isnan(tempValueF)) {  //Given from import libraries
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  if (!manualControlExhaust) {
    exhaustState = (tempValue >= 36);
    if (exhaustState && !lastExhaustState) {
      digitalWrite(ExhaustFanPin, HIGH);
      playAudio(6);  // ON sound
    } else if (!exhaustState && lastExhaustState) {
      digitalWrite(ExhaustFanPin, LOW);
      playAudio(7);  // OFF sound
    } else {
      digitalWrite(ExhaustFanPin, exhaustState ? HIGH : LOW);
    }
    lastExhaustState = exhaustState;
  }
}

void readLDR() {
  //Light Dependent Resistor Codes
  LDRvalue = analogRead(LDRPIN);
  luxValue = calculateLux(LDRvalue);

  if (!manualControlBedroomLight) {
    bedroomLightState = (luxValue < DIM_LUX);

    // ON edge
    if (bedroomLightState && !lastBedroomLightState) {
      digitalWrite(LEDPIN3, HIGH);
      playAudio(9);
    }
    // OFF edge
    else if (!bedroomLightState && lastBedroomLightState) {
      digitalWrite(LEDPIN3, LOW);
      playAudio(12);
    }
    // no change
    else {
      digitalWrite(LEDPIN3, bedroomLightState ? HIGH : LOW);
    }

    lastBedroomLightState = bedroomLightState;
  }
  /*
  Serial.print("ADC: "); Serial.print(LDRvalue);
  Serial.print(" | Lux: "); Serial.println(luxValue);
  */
}

void readSoundSensor() {
  //Sound Sensor Codes
  int SSvalue = analogRead(SSPIN);
  soundDB = calculateDB();

  if (soundDB >= 80) {
    SSLED = !SSLED;
  }
  if (SSLED && !lastSSLED) {
    if (!manualControlKitchenLight) {
      digitalWrite(LEDPIN2, HIGH);
      playAudio(9);
    }
    if (!manualControlLivingLight) {
      digitalWrite(LEDPIN4, HIGH);
      playAudio(9);
    }
  } else if (!SSLED && lastSSLED) {
    if (!manualControlKitchenLight) {
      digitalWrite(LEDPIN2, LOW);
      playAudio(12);
    }
    if (!manualControlLivingLight) {
      digitalWrite(LEDPIN4, LOW);
      playAudio(12);
    }
  }

  lastSSLED = SSLED;
  /*
  Serial.print(" | ssValue: ");
  Serial.print(SSvalue);
  Serial.print(" | Decibels: ");
  Serial.print(soundDB);
  */
}

void readCTS(unsigned long currentMS) {
  //Capacitive Touch Sensor Codes
  int detected1 = digitalRead(CTouchSensorPin1);
  int detected2 = digitalRead(CtouchSensorPin2);
  int detected3 = digitalRead(CTouchSensorPin3);
  //bool inDoorToched = (detected1 == 0 && detected2 == 0 && detected3 == 1);
  bool isTouched = (detected1 == 1 && detected2 == 1);
  bool oneTouched = (detected1 == 1 || detected2 == 1);
  bool inDoorTouch = (digitalRead(CTouchSensorPin3) == 1);  //use another variable since it is used in other logics

  // If inDoorTouch is touched
  if (inDoorTouch && !inDoorTouchLatched) {
    inDoorTouchLatched = true;

    if (!servoActive) {
      // SERVO IS CLOSED → touching inDoorTouch immediately opens it
      beepOnce();
      openServo();
      playAudio(10);
      intruderAttempts = 0;

      if (servoLocked) {
        servoLocked = false;
        intruderAttempts = 0;
        Serial.println("Lock has been overrided");
      }
    } else {
      // SERVO IS OPEN → holding inDoorTouch for 1 second closes it
      beepOnce();
      closeServo();
      playAudio(11);
    }
  }
  // Reset latch only when released
  if (!inDoorTouch) {
    inDoorTouchLatched = false;
  }

  if (servoLocked) {
    Serial.println("Try again later");
    return;
  }
  if (!manualControlServo) {
    if (isTouched) {  //Outside
      if (!touching) {
        touching = true;
        lastTwoTouchRead = currentMS;  // start counting
        successTriggered = false;
      }
      // Check if touch lasts 3 seconds
      if (!successTriggered && (currentMS - lastTwoTouchRead >= T_INTERVAL)) {
        beepOnce();
        openServo();
        playAudio(2);
        successTriggered = true;  // Prevent repeating
        intruderAttempts = 0;
      }
    } else {
      // Touch released early before success
      if (touching && !successTriggered) {
        beepThreeTimes();  // FAILURE = 3 beeps
        intruderAttempts++;

        if (intruderAttempts >= 3) {
          servoLocked = true;
          lcdNeedsRedraw = true;
          servoLockStartTime = currentMS;
          playAudio(5);
          closeServo();  // Ensure door is closed when locked
          Serial.println("!!! SERVO LOCK ACTIVATED (100 SECONDS) !!!");
        }
      }
      // Reset counting if finger is removed
      touching = false;
      successTriggered = false;
    }
    if (!isTouched && oneTouched) {
      // Only touched one CTS
      if (!oneTouching) {
        oneTouching = true;
        lastOneTouchRead = currentMS;  // start counting
      }

      if (currentMS - lastOneTouchRead >= T_INTERVAL) {
        beepThreeTimes();  // FAILURE = 3 beeps
        intruderAttempts++;

        if (intruderAttempts >= 3) {
          servoLocked = true;
          lcdNeedsRedraw = true;
          servoLockStartTime = currentMS;
          playAudio(5);
          closeServo();  // Ensure door is closed when locked
          Serial.println("!!! SERVO LOCK ACTIVATED (100 SECONDS) !!!");
        }
        oneTouching = false;  // reset timer
      }
    } else {
      oneTouching = false;  //reset timer
    }
  } else {
    Serial.println("Outside lock is turned off");
  }
}

void readUltrasonicSensor(unsigned long currentMS) {
  // Trigger ultrasonic pulse
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 60000);  // 60ms max
  if (duration > 0) {
    distance = duration * 0.0343 / 2;  // in cm
  }

  if (!manualControlGreeting) {  // Only in AUTO mode

    // Trigger greeting ONLY if object just entered and no greeting active
    if (distance > 0 && distance <= 10) {
      if (!objectWasClose && !greeting.active) {
        objectWasClose = true;   // Mark object detected
        greeting.active = true;  // Start greeting
        greeting.source = GreetingState::ULTRASONIC;
        greetingStartTime = currentMS;  // Store start time
        lcdNeedsRedraw = true;
      }
    } else {
      objectWasClose = false;  // reset for next detection
    }

    // Keep greeting active for full 5 seconds
    if (greeting.active && greeting.source == GreetingState::ULTRASONIC) {
      if (currentMS - greetingStartTime >= webGreetingDuration) {
        greeting.active = false;  // End greeting after 5s
        lcdNeedsRedraw = true;
      }
    }
  }
}

void drawNormalLCD() {
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Room1 Lights: ");
  lcd.print(digitalRead(LEDPIN3) ? "ON " : "OFF");

  lcd.setCursor(0, 1);
  lcd.print("Room2 Lights: ");
  lcd.print(digitalRead(LEDPIN2) ? "ON " : "OFF");

  lcd.setCursor(0, 2);
  lcd.print("H:");
  lcd.print(humidValue);
  lcd.print("% T:");
  lcd.print(tempValue);
  lcd.print((char)223);  // Prints the degree symbol (°)
  lcd.print("C");

  lcd.setCursor(0, 3);
  lcd.print("HI:");
  lcd.print(heatIndex);
  lcd.print((char)223);  // Prints the degree symbol (°)
  lcd.print("C");
}

void drawGreetingLCD() {
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("  WELCOME! THIS IS");
  lcd.setCursor(0, 1);
  lcd.print("     MICROHOME!");
  lcd.setCursor(0, 2);
  lcd.print("   LET'S GET THIS");
  lcd.setCursor(0, 3);
  lcd.print("   DONE TOGETHER.");
}

void drawIntruderLCD() {
  static long lastRemaining = -1;  // remember last value

  // Only redraw if remaining time changed
  if (servoLockRemaining != lastRemaining) {
    lastRemaining = servoLockRemaining;
    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("  !!! INTRUDER !!!");
    lcd.setCursor(0, 1);
    lcd.print(" TOO MANY ATTEMPTS");
    lcd.setCursor(0, 2);
    lcd.print("   DOOR LOCKED");
    lcd.setCursor(0, 3);
    lcd.print("  WAIT ");
    lcd.print(servoLockRemaining);
    lcd.print(" SECONDS   ");
  }
}

void updateLCD(unsigned long currentMS) {
  //LCD block of codes
  if (lcdMode == LCD_NORMAL && (millis() - lastLCDNormalUpdate >= LCD_INTERVAL)) {
    lcdNeedsRedraw = true;
    lastLCDNormalUpdate = millis();
  }

  if (servoLocked) {
    lcdMode = LCD_INTRUDER;
  } else if (webGreetingState) {
    lcdMode = LCD_WEB_GREETING;
  } else if (greeting.active) {
    lcdMode = LCD_GREETING;
  } else {
    lcdMode = LCD_NORMAL;
  }
  if (lcdMode != lastLCDMode || lcdNeedsRedraw) {
    lcdNeedsRedraw = false;

    switch (lcdMode) {
      case LCD_INTRUDER: drawIntruderLCD(); break;
      case LCD_WEB_GREETING: drawGreetingLCD(); break;
      case LCD_GREETING: drawGreetingLCD(); break;
      case LCD_NORMAL: drawNormalLCD(); break;
    }

    lastLCDMode = lcdMode;
  }

  if (lcdMode == LCD_INTRUDER) {
    lcdNeedsRedraw = true;
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(ExhaustFanPin, OUTPUT);
  pinMode(LEDPIN2, OUTPUT);
  pinMode(LEDPIN3, OUTPUT);
  pinMode(LEDPIN4, OUTPUT);
  pinMode(ServoGreenLED, OUTPUT);
  pinMode(ServoRedLED, OUTPUT);
  pinMode(BUZZPIN, OUTPUT);
  pinMode(BUZZPIN2, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LDRPIN, INPUT);
  pinMode(SSPIN, INPUT);
  pinMode(CTouchSensorPin1, INPUT);
  pinMode(CtouchSensorPin2, INPUT);
  pinMode(CTouchSensorPin3, INPUT);

  dht.begin();
  Wire.begin(SDApin, SCLpin);  // SDA = 21, SCL = 22 for ESP32 LCD
  Wire.setClock(100000);       // slow the initialization

  updateServoLEDs();
  myServo.attach(ServoPin);  //Pin 18
  myServo.write(0);          // start at 0°
  lcd.init();                // Initialize LCD
  lcd.backlight();           // Turn on backlight

  //Wifi set-up codes
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");
  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());

  // Start LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Failed!");
    return;
  }
  Serial.println("LittleFS mounted successfully!");

  // Call files
  server.onNotFound([](AsyncWebServerRequest* request) {
    String path = request->url();
    if (path == "/") path = "/login.html";

    if (LittleFS.exists(path)) {
      String contentType = "text/plain";
      if (path.endsWith(".html")) contentType = "text/html";
      else if (path.endsWith(".css")) contentType = "text/css";
      else if (path.endsWith(".js")) contentType = "application/javascript";
      request->send(LittleFS, path, contentType);
    } else {
      request->send(404, "text/plain", "File Not Found");
    }
  });

  // User and Password credentials
  server.on("/login", HTTP_POST, [](AsyncWebServerRequest* request){
  if (!request->hasParam("user", true) || !request->hasParam("pass", true)) {
    request->send(400, "text/plain", "Missing credentials");
    return;
  }

  String user = request->getParam("user", true)->value();
  String pass = request->getParam("pass", true)->value();

  if (user == "MicroHome" && pass == "Team2Dash") {
    request->send(200, "text/plain", "OK");
  } else {
    request->send(401, "text/plain", "FAIL");
  }
});

  //DF Player Codes
  dfSerial.begin(9600, SERIAL_8N1, RXDpin, TXDpin);  // RX = 17, TX = 16
  if (!dfPlayer.begin(dfSerial)) {
    Serial.println("DFPlayer Mini not detected!");
    while (true)
      ;
  }
  dfPlayer.volume(20);  // Range: 0–30
  Serial.println("DFPlayer ready");

  // Values to be sent
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest* request) {
    StaticJsonDocument<512> doc;

    doc["temperature"] = tempValue;
    doc["humidity"] = humidValue;
    doc["heatIndex"] = heatIndex;
    doc["temperatureF"] = tempValueF;
    doc["heatIndexF"] = heatIndexF;

    doc["distance"] = distance;
    doc["sounds"] = soundDB;
    doc["light"] = luxValue;
    doc["intruder"] = intruderAttempts;
    doc["lockRemaining"] = servoLockRemaining;

    doc["servoActive"] = servoActive;                              // Door Status
    doc["manualControlSound"] = manualControlSound;                // Audio Mode
    doc["manualControlExhaust"] = manualControlExhaust;            // Exhaust Mode
    doc["manualControlBedroomLight"] = manualControlBedroomLight;  // Bedroom Light Mode
    doc["manualControlLivingLight"] = manualControlLivingLight;    // Living Light Mode
    doc["manualControlKitchenLight"] = manualControlKitchenLight;  // Kitchen Light Mode
    doc["manualControlGreeting"] = manualControlGreeting;          // Greeting Mode

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  //DF Player
  server.on("/setAudio", HTTP_GET, [](AsyncWebServerRequest* request) {
    bool success = false;
    bool previousState = manualControlSound;

    if (request->hasParam("state")) {
      String state = request->getParam("state")->value();
      state.toUpperCase();

      if (state == "ON") {
        manualControlSound = true;
        if (!previousState) {
          playAudio(3);
          previousState = false;
        }
        success = true;
      } else if (state == "OFF") {
        if (previousState) {
          playAudio(4);
          previousState = true;
        }
        manualControlSound = false;
        success = true;
      }
    }

    if (success) {
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Error: Invalid state parameter. Use ON or OFF.");
    }
  });

  //Exhaust Fan
  server.on("/setExhaust", HTTP_GET, [](AsyncWebServerRequest* request) {
    bool success = false;
    bool previousState = digitalRead(ExhaustFanPin);

    if (request->hasParam("state")) {
      String state = request->getParam("state")->value();
      state.toUpperCase();

      if (state == "ON") {
        manualControlExhaust = true;
        if (!previousState) {
          digitalWrite(ExhaustFanPin, HIGH);
          playAudio(3);
        }
        success = true;
      } else if (state == "OFF") {
        manualControlExhaust = true;
        if (previousState) {
          digitalWrite(ExhaustFanPin, LOW);
          playAudio(4);
        }
        success = true;
      } else if (state == "AUTO") {
        manualControlExhaust = false;
        digitalWrite(ExhaustFanPin, (tempValue > 36) ? HIGH : LOW);
        success = true;
      }
    }

    if (success) {
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Error: Invalid state parameter. Use ON, OFF, or AUTO.");
    }
  });

  //LDR
  server.on("/setBedroomLight", HTTP_GET, [](AsyncWebServerRequest* request) {
    bool success = false;
    bool previousState = digitalRead(LEDPIN3);

    if (request->hasParam("state")) {
      String state = request->getParam("state")->value();
      state.toUpperCase();

      if (state == "ON") {
        manualControlBedroomLight = true;
        if (!previousState) {
          digitalWrite(LEDPIN3, HIGH);
          playAudio(3);
        }
        success = true;
      } else if (state == "OFF") {
        manualControlBedroomLight = true;
        if (previousState) {
          digitalWrite(LEDPIN3, LOW);
          playAudio(4);
        }
        success = true;
      } else if (state == "AUTO") {
        manualControlBedroomLight = false;
        success = true;
      }
    }
    if (success) request->send(200, "text/plain", "OK");
    else request->send(400, "text/plain", "Error: Invalid state.");
  });

  //SoundSensor
  server.on("/setLivingLight", HTTP_GET, [](AsyncWebServerRequest* request) {
    bool success = false;
    bool previousState = digitalRead(LEDPIN2);

    if (request->hasParam("state")) {
      String state = request->getParam("state")->value();
      state.toUpperCase();

      if (state == "ON") {
        manualControlLivingLight = true;
        if (!previousState) {
          digitalWrite(LEDPIN2, HIGH);
          playAudio(3);
        }
        success = true;
      } else if (state == "OFF") {
        manualControlLivingLight = true;
        if (previousState) {
          digitalWrite(LEDPIN2, LOW);
          playAudio(4);
        }
        success = true;
      } else if (state == "AUTO") {
        manualControlLivingLight = false;
        success = true;
      }
    }
    if (success) request->send(200, "text/plain", "OK");
    else request->send(400, "text/plain", "Error: Invalid state.");
  });

  //Kitchen Lights
  server.on("/setKitchenLight", HTTP_GET, [](AsyncWebServerRequest* request) {
    bool success = false;
    bool previousState = digitalRead(LEDPIN4);

    if (request->hasParam("state")) {
      String state = request->getParam("state")->value();
      state.toUpperCase();

      if (state == "ON") {
        manualControlKitchenLight = true;
        if (!previousState) {
          digitalWrite(LEDPIN4, HIGH);
          playAudio(3);
        }
        success = true;
      } else if (state == "OFF") {
        manualControlKitchenLight = true;
        if (previousState) {
          digitalWrite(LEDPIN4, LOW);
          playAudio(4);
        }
        success = true;
      } else if (state == "AUTO") {
        manualControlKitchenLight = false;
        success = true;
      }
    }
    if (success) request->send(200, "text/plain", "OK");
    else request->send(400, "text/plain", "Error: Invalid state.");
  });

  //servo
  server.on("/setDoorState", HTTP_GET, [](AsyncWebServerRequest* request) {
    bool success = false;

    if (request->hasParam("state")) {
      String state = request->getParam("state")->value();
      state.toUpperCase();

      if (state == "OPEN") {
        // Open the door only if not locked
        manualControlServo = true;
        if (!servoLocked && !servoActive) {
          openServo();  // Calls myServo.write(90) and sets servoActive = true
          beepOnce();
          success = true;
        } else {
          Serial.println("Door control denied: Servo is locked.");
          request->send(403, "text/plain", "Door is locked.");
          return;
        }
      } else if (state == "CLOSE") {
        closeServo();  // Calls myServo.write(0) and sets servoActive = false
        manualControlServo = true;
        success = true;
      } else if (state == "AUTO") {
        manualControlServo = false;
        success = true;
      }
    }

    if (success) {
      request->send(200, "text/plain", "OK");
    } else {
      request->send(400, "text/plain", "Error: Invalid state or command.");
    }
  });

  //Greeting Control
  server.on("/setGreeting", HTTP_GET, [](AsyncWebServerRequest* request) {
    bool success = false;

    if (request->hasParam("state")) {
      String state = request->getParam("state")->value();
      state.toUpperCase();

      if (state == "MANUAL") {
        manualControlGreeting = true;
        greeting.active = false;  // stop ultrasonic
        success = true;
      } else if (state == "AUTO") {
        manualControlGreeting = false;
        webGreetingState = false;
        success = true;
      } else if (state == "TRIGGER") {
        if (manualControlGreeting) {
          playAudio(1);
          webGreetingState = !webGreetingState;
          // stop ultrasonic greeting completely
          greeting.active = false;
          objectWasClose = false;
        }
        success = true;
      }
    }
    if (success) request->send(200, "text/plain", "OK");
    else request->send(400, "text/plain", "Error: Invalid state.");
  });

  server.begin();
  Serial.println("Web server started!");
}

void loop() {
  // put your main code here, to run repeatedly:
  unsigned long currentMS = millis();  //start counting

  if (servoActive && (currentMS - servoTriggerTime >= SERVO_DURATION)) {  //reads every 10 seconds
    closeServo();
    playAudio(11);
  }

  if (servoLocked) {
    long elapsed = (currentMS - servoLockStartTime) / 1000;
    long total = SERVO_LOCK_DURATION / 1000;
    servoLockRemaining = max(0L, total - elapsed);

    if (servoLockRemaining == 0) {
      servoLocked = false;
      intruderAttempts = 0;
      lcdNeedsRedraw = true;
      playAudio(1);
      Serial.println("Servo Lock Released");
    }
  } else {
    servoLockRemaining = 0;
  }

  // WiFI connection
  if (currentMS - lastWiFiCheck >= WIFI_CHECK_INTERVAL) {
    lastWiFiCheck = currentMS;

    if (WiFi.status() != WL_CONNECTED) {
      Serial.print("Wi-Fi status check failed. Failure Count: ");
      Serial.println(connectionFailureCount);

      connectToWiFi();  // Attempt reconnection

      // Failsafe: Reboot if too many consecutive failures
      if (connectionFailureCount >= 5) {  // 5 failures * 10 seconds = 50 seconds of failure
        Serial.println("Severe Wi-Fi failure. Initiating soft reboot...");
        ESP.restart();
      }
    } else {
      connectionFailureCount = 0;  // Reset on successful check
    }
  }

  if (currentMS - lastDHTread >= DHT_INTERVAL) {  //reads every 5 seconds
    lastDHTread = currentMS;
    readDHT22();
  }

  if (currentMS - lastCTSread >= CTS_INTERVAL) {  //read every 0.5 seconds
    lastCTSread = currentMS;
    readUltrasonicSensor(currentMS);
    readCTS(currentMS);
    readSoundSensor();
    readLDR();
  }
  updateLCD(currentMS);
}
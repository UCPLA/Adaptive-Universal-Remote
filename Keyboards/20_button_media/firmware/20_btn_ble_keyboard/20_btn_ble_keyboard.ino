/*****************************************************************************
   AUTHOR: Ruben Kackstaetter
   DATE: September 14, 2020
   
   DESCRIPTION: Keyboard firmware for SparkFun Thing Plus (ESP32 WROOM).
   This is configured as a simple BLE keyboard.

   Depends on:
   * https://github.com/nickgammon/Keypad_Matrix
   * https://github.com/T-vK/ESP32-BLE-Keyboard
 ****************************************************************************/

#include <Keypad_Matrix.h>
#include <BleKeyboard.h>

BleKeyboard Keyboard;

// Declare how many keys are on the keyboard
const byte ROWS = 5;
const byte COLS = 5;

// Define how the keypad is laid out
const char keys[ROWS][COLS] = {
  {'1','2','3','4','5'},
  {'a','b','c','d','e'},
  {'f','g','h','i','j'},
  {'k','l','m','n','o'},
  {'p','q','r','s','t'},
};

// These are the pin declarations for the Qwiic Keyboard Explorer
const byte rowPins[ROWS] = {19, 5, 4, 15, 32}; 
const byte colPins[COLS] = {16, 18, 14, 21, 17}; 

// use first channel of 16 channels (started from zero)
#define LEDC_CHANNEL_0     0

// use 13 bit precission for LEDC timer
#define LEDC_TIMER_13_BIT  13

// use 5000 Hz as a LEDC base frequency
#define LEDC_BASE_FREQ     5000

// fade LED PIN (replace with LED_BUILTIN constant for built-in LED)
const byte backlightPin = 26;

const byte hapticPin = 25;
const byte vbatSensePin = A2;

// Backlight brightness and 
// how much to increase the brightness with each fn button press
int brightness = 0;
int stepAmount = 255/3;

int vbatValue = 0;
int vbatLowValue = 3600;

// Create the Keypad object
Keypad_Matrix kpd = Keypad_Matrix( makeKeymap (keys), rowPins, colPins, ROWS, COLS );


void ledcAnalogWrite(uint8_t channel, uint32_t value, uint32_t valueMax = 255) {
  // calculate duty, 8191 from 2 ^ 13 - 1
  uint32_t duty = (8191 / valueMax) * min(value, valueMax);

  // write duty to LEDC
  ledcWrite(channel, duty);
}

// Increase Brightness by one step
void stepBacklightBrightness() {
  if (brightness < 255) {
    brightness += stepAmount;
  } else {
    brightness = 0;
  }
  Serial.print (F("Backlight: "));
  Serial.println (brightness);
  ledcAnalogWrite(LEDC_CHANNEL_0, brightness);
}

// Key Press Handler
void keyDown (const char which) {
  Keyboard.press(which);
  Serial.print (F("Key down: "));
  Serial.println (which);
  digitalWrite(hapticPin, HIGH);
}

// Key Release Handler
void keyUp (const char which) {
  Keyboard.release(which);
  Serial.print (F("Key up: "));
  Serial.println (which);
  digitalWrite(hapticPin, LOW);
  if (which == 'p') {
    stepBacklightBrightness();
  }
}

void checkBattery() {
  vbatValue = analogRead(vbatSensePin);
  Serial.print (F("vbat: "));
  Serial.println (vbatValue);
  if (vbatValue < vbatLowValue) {
    ledcAnalogWrite(LEDC_CHANNEL_0, 30);
  }
}

void setup() {
  Serial.begin(115200);
  
  kpd.begin();
  kpd.setKeyDownHandler(keyDown);
  kpd.setKeyUpHandler(keyUp);
  
  Keyboard.begin();
  
  pinMode(vbatSensePin, INPUT);
  
  // Setup timer and attach timer to a backlight pin
  ledcSetup(LEDC_CHANNEL_0, LEDC_BASE_FREQ, LEDC_TIMER_13_BIT);
  ledcAttachPin(backlightPin, LEDC_CHANNEL_0);
 
  pinMode(hapticPin, OUTPUT);
  digitalWrite(hapticPin, LOW);
}

void loop() {
  kpd.scan();
  checkBattery();
}

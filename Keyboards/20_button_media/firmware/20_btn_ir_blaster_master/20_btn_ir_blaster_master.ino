/*****************************************************************************
   AUTHOR: Ruben Kackstaetter
   DATE: September 14, 2020
   
   DESCRIPTION: Keyboard firmware for SparkFun Thing Plus (ESP32 WROOM).
   Developed to be paired to the Sparkfun WiFi IR Blaster over ESP-NOW, and
   Configured as a Roku TV remote.

    Depends on:
   * https://github.com/nickgammon/Keypad_Matrix
   * https://github.com/T-vK/ESP32-BLE-Keyboard
 ****************************************************************************/

#include "WiFi.h"
#include <esp_now.h>
#include <Keypad_Matrix.h>

//uint8_t broadcastAddress[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
uint8_t broadcastAddress[] = {0xCC,0x50,0xE3,0xC0,0xB6,0x56};

// Declare how many keys are on the keyboard
const byte ROWS = 5;
const byte COLS = 5;

enum key_cmd {
  send_ir,
  learn_ir
};

// Structure to send cmd to IR blaster
typedef struct cmd_data {
  int key;
  int cmd;

} cmd_data;

// Create cmdData struct
cmd_data cmdData;

// Define how the keypad is laid out
const char keys[ROWS][COLS] = {
  { 1, 2, 3, 4, 5},
  { 6, 7, 8, 9,10},
  {11,12,13,14,15},
  {16,17,18,19,20},
  {21,22,23,24,25},
};

// Pins for the key matrix scanning
const byte ROW_PINS[ROWS] = {19, 5, 4, 15, 32}; 
const byte COL_PINS[COLS] = {16, 18, 14, 21, 17}; 

// Pins for other functions
const byte BACKLIGHT_PIN = 26;
const byte HAPTIC_PIN = 25;
const byte VBAT_SENSE_PIN = A2;

// Use first PWM channel of 16 channels
const uint8_t BACKLIGHT_CH = 0;
// Use 13 bit precission for LEDC timer
const uint8_t LEDC_TIMER_13_BIT = 13;
// Use 5000 Hz as a LEDC base frequency
const uint32_t LEDC_BASE_FREQ = 5000;
// Set Max Value for PWM Channel
const uint32_t BACKLIGHT_MAX_VAL = 255;

const uint32_t MAX_RETRY = 24;

// Backlight brightness and 
// how much to increase the brightness with each fn button press
int brightness = 0;
int stepAmount = BACKLIGHT_MAX_VAL/3;

int vbatValue = 0;
int vbatLowValue = 3600;

int retryCnt = 0;

Keypad_Matrix kpd = Keypad_Matrix( makeKeymap (keys), ROW_PINS, COL_PINS, ROWS, COLS );

// LED Analog Write function to set brightness
void setBacklight(uint32_t value) {
  // calculate duty, 8191 from 2 ^ 13 - 1
  uint32_t duty = (8191 / BACKLIGHT_MAX_VAL) * min(value, BACKLIGHT_MAX_VAL);
  ledcWrite(BACKLIGHT_CH, duty);
}

// Increase Brightness by one step
void stepBacklightBrightness() {
  if (brightness < BACKLIGHT_MAX_VAL) {
    brightness += stepAmount;
  } else {
    brightness = 0;
  }
  Serial.print (F("Backlight: "));
  Serial.println (brightness);
  setBacklight(brightness);
}

void hapticVibrate() {
  digitalWrite(HAPTIC_PIN, HIGH);
  delay(200);
  digitalWrite(HAPTIC_PIN, LOW);
}

// Key Press Handler
void keyDown (const char which) {
  if (brightness == BACKLIGHT_MAX_VAL) { 
    hapticVibrate();
  }
  cmdData.key = (int)which;
  if (which == 21) {
    cmdData.cmd = learn_ir;
  } else {
    cmdData.cmd = send_ir;
  }
  Serial.print (F("Key down: "));
  Serial.println (cmdData.key);
}

// Key Release Handler
void keyUp (const char which) {
  Serial.print (F("Key up: "));
  Serial.println (which);
  if (which == 21 && cmdData.key == 21) {
    stepBacklightBrightness();
  } else {
    sendCmd(cmdData);
  }
}

void checkBattery() {
  vbatValue = analogRead(VBAT_SENSE_PIN);
  if (vbatValue < vbatLowValue) {
    Serial.print (F("vbat: "));
    Serial.println (vbatValue);
    setBacklight(30);
  }
}

void sendCmd(cmd_data cmdData) {     
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &cmdData, sizeof(cmd_data));
     
  if (result == ESP_OK) {
    Serial.println(F("Sent with success"));
  } else {
    Serial.println(F("Error sending the data"));
  }
}

// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print(F("\nLast Packet:\t"));
  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println(F("Delivery Success"));
  } else {
    Serial.println(F("Delivery Failed, Retrying last command"));
    if (retryCnt < MAX_RETRY) {
      sendCmd(cmdData);
      retryCnt += 1;
    } else {
      retryCnt = 0;
    }
  }
}

void initEsp32() {
  WiFi.mode(WIFI_STA);
 
  Serial.println();
  Serial.println(WiFi.macAddress());
 
  if (esp_now_init() != ESP_OK) {
    Serial.println(F("Error initializing ESP-NOW"));
    return;
  }

  // Once ESPNow successfully init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);
 
  // register peer
  esp_now_peer_info_t peerInfo;
   
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
         
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
}

void setup() {
  Serial.begin(115200);

  initEsp32();
  
  kpd.begin();
  kpd.setKeyDownHandler(keyDown);
  kpd.setKeyUpHandler(keyUp);
  
  pinMode(VBAT_SENSE_PIN, INPUT);
  
  // Setup timer and attach timer to a backlight pin
  ledcSetup(BACKLIGHT_CH, LEDC_BASE_FREQ, LEDC_TIMER_13_BIT);
  ledcAttachPin(BACKLIGHT_PIN, BACKLIGHT_CH);
 
  pinMode(HAPTIC_PIN, OUTPUT);
  digitalWrite(HAPTIC_PIN, LOW);
}

void loop() {
  kpd.scan();
  checkBattery();
}

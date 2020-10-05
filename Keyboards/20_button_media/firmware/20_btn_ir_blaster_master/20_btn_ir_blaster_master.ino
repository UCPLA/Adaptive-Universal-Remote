/*****************************************************************************
   AUTHOR: Ruben Kackstaetter
   DATE: September 14, 2020
   
   DESCRIPTION: Keyboard firmware for SparkFun Thing Plus (ESP32 WROOM).
   Developed to be paired to the Sparkfun WiFi IR Blaster over ESP-NOW, and
   Configured as a Roku TV remote.

    Depends on:
   * https://github.com/nickgammon/Keypad_Matrix
 ****************************************************************************/

#include "WiFi.h"
#include <EEPROM.h>
#include <esp_now.h>
#include <driver/rtc_io.h>
#include <Keypad_Matrix.h>

enum key_cmd {
  send_ir,
  learn_ir
};

// Structure to send cmd to IR blaster
typedef struct cmd_data {
  uint8_t key;
  uint8_t cmd;

} cmd_data;

// Declare cmdData
cmd_data cmdData;

RTC_DATA_ATTR uint32_t bootCount = 0;

typedef struct eeprom_data {
  uint8_t broadcastAddress[6];
  int analogOffset;
} eeprom_data;

// Declare EEPROM data
eeprom_data edata;

const uint16_t EEPROM_DATA_ADDR = 0;

uint16_t vbatMaxVal = 0;
uint16_t vbatMinVal = 5000;

const uint8_t DEFAULT_SLEEP_TIMEOUT_S = 10;
const uint8_t LOWBAT_SLEEP_TIMEOUT_S = 10;

// GPIO for the key matrix scanning
const byte ROWS = 5;
const byte ROW_PINS[ROWS] = {19, 5, 4, 15, 32}; 

const byte COLS = 5;
const byte COL_PINS[COLS] = {16, 18, 14, 21, 17}; 

// Define how the keypad is laid out
const char KEY_LAYOUT[ROWS][COLS] = {
  { 0, 1, 2, 3, 4},
  { 5, 6, 7, 8, 9},
  {10,11,12,13,14},
  {15,16,17,18,19},
  {20,21,22,23,24},
};

const char FN_KEY_VAL = 20;

// TODO: Redesign hardware to use RTC pins for all rows and columns.
// RTC GPIO for wake up, We can only use GPIO 4, 14, 15, 32 from key matrix.
// Rows as Wake Interrupt Triggers
const uint32_t WAKE_PINS_BITMASK = 0x100008010; // GPIO: 4, 15, 32
// Cols as Outputs.
const byte RTC_COLS = 1; // Ideally we would just us COLS, but not all cols are RTC
const gpio_num_t RTC_COL_PINS[RTC_COLS] = {GPIO_NUM_14};

// Pins for other functions
const byte BACKLIGHT_PIN = 26;
const byte HAPTIC_PIN = 25;
const byte VBAT_SENSE_PIN = A2;

// Use first PWM channel of 16 channels
const uint8_t BACKLIGHT_CH = 0;
// Use 13 bit precision for LEDC timer
const uint8_t LEDC_TIMER_13_BIT = 13;
// Use 5000 Hz as a LEDC base frequency
const uint32_t LEDC_BASE_FREQ = 5000;
// Set Max Value for PWM Channel
const uint32_t BACKLIGHT_MAX_VAL = 255;
// Backlight Brightness and step amount with each fn key press
const uint8_t BACKLIGHT_BRIGHTNESS_STEP = BACKLIGHT_MAX_VAL/5;
RTC_DATA_ATTR int backlightBrightness = BACKLIGHT_BRIGHTNESS_STEP;
// Backlight Pattern Variables
unsigned long backlightMilOld = 0;
uint8_t backlightPatCnt = 0;
uint16_t backlightPatDurMs = 0;
uint8_t backlightPatTimes = 0;

// These values are use to determine when to consider the battery at certain states.
// Since this can fluctuate between board builds, these values will need to be
// determined for each board.
const uint16_t VBAT_LOW = 0; // 3670
const uint16_t VBAT_CRITICAL = 0; // 3640
bool lowBattery = false;

// Esp now tx retry
const uint32_t MAX_TX_RETRY = 24;
int txRetryCount = 0;

Keypad_Matrix kpd = Keypad_Matrix( makeKeymap (KEY_LAYOUT), ROW_PINS, COL_PINS, ROWS, COLS );

hw_timer_t *sleepTimer = NULL;

// LED Analog Write function to set backlight brightness
void setBacklight(uint32_t value) {
  // calculate duty, 8191 from 2 ^ 13 - 1
  uint32_t duty = (8191 / BACKLIGHT_MAX_VAL) * min(value, BACKLIGHT_MAX_VAL);
  ledcWrite(BACKLIGHT_CH, duty);
}

// Increase Brightness by one step
void stepBacklightBrightness() {
  if (backlightBrightness < BACKLIGHT_MAX_VAL && !lowBattery) {
    backlightBrightness += BACKLIGHT_BRIGHTNESS_STEP;
  } else {
    backlightBrightness = BACKLIGHT_BRIGHTNESS_STEP;
  }

  if (lowBattery) {
    backlightLowBattery();
  }
  
  Serial.print(F("Backlight: "));
  Serial.println(backlightBrightness);
  setBacklight(backlightBrightness);
}

void hapticVibrate() {
  digitalWrite(HAPTIC_PIN, HIGH);
  delay(200);
  digitalWrite(HAPTIC_PIN, LOW);
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// Use only when vBat is at 3600 mV
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void calcSaveAnalogOffset() {
  uint32_t vbatValue = 0;
  const uint8_t samples = 30;
  
  for (uint8_t i = 0; i < samples; i++) {
   vbatValue += analogRead(VBAT_SENSE_PIN);
  }

  int vbatAveVal = vbatValue / samples;

  edata.analogOffset = (3600 - vbatAveVal);
  EEPROM.put(EEPROM_DATA_ADDR, edata);
  EEPROM.commit();
  Serial.print(F("Analog Offset: "));
  Serial.println(edata.analogOffset);
  backlightSetPattern(100, 11);
}

void saveBrodcastAddr(uint8_t macAddr[]) {
  // TODO: Call from from serial command.
  memcpy(edata.broadcastAddress, macAddr, 6);
  printBrodcastAddress();
  EEPROM.put(EEPROM_DATA_ADDR, edata);
  EEPROM.commit();
  backlightSetPattern(100, 11);
}

// Key Press Handler
void keyDown (const char which) {

  // Reset sleep timer on any key press
  timerWrite(sleepTimer, 0);
  
  if (backlightBrightness == BACKLIGHT_MAX_VAL) { 
    hapticVibrate();
  }
  
  cmdData.key = (int)which;

  if (which == FN_KEY_VAL) {
    cmdData.cmd = learn_ir;
  } else {
    sendCmd(cmdData);
    if(cmdData.cmd == learn_ir) {
      backlightLearnModeSent();
    }
  }    
  
  Serial.print (F("Key down: "));
  Serial.println (cmdData.key);
}

// Key Release Handler
void keyUp (const char which) {
  Serial.print (F("Key up: "));
  Serial.println ((int)which);
  
  if (which == FN_KEY_VAL && cmdData.key == FN_KEY_VAL) {
    stepBacklightBrightness();
//    uint8_t macAddr[] = {};
//    saveBrodcastAddr(uint8_t macAddr[]);
  }
  
  cmdData.cmd = send_ir;
}

void sendCmd(cmd_data cmdData) {     
  esp_err_t result = esp_now_send(edata.broadcastAddress, (uint8_t *) &cmdData, sizeof(cmd_data));
     
  if (result == ESP_OK) {
    Serial.println(F("Sent with success"));
  } else {
    Serial.println(F("Error sending the data"));
  }
  Serial.print(F("key: "));
  Serial.print(cmdData.key);
  Serial.print(F(", cmd: "));
  Serial.println(cmdData.cmd);
}

/* ESPNOW Callback after data is sent */
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print(F("\nLast Packet:\t"));
  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println(F("Delivery Success"));
  } else {
    Serial.println(F("Delivery Failed, Retrying last command"));
    if (txRetryCount < MAX_TX_RETRY) {
      sendCmd(cmdData);
      txRetryCount += 1;
    } else {
      txRetryCount = 0;
    }
  }
}

/* Print what triggered ESP32 wakeup from sleep */
void print_wakeup_trigger(){
  switch(esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup triggered by EXT0 (RTC_IO)"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup triggered by EXT1 (RTC_CNTL)"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup triggerd by Timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup Triggerd by Touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup Triggerd by ULP Program"); break;
    default : Serial.printf("Wakeup Triggered by Unknown: %d\n",esp_sleep_get_wakeup_cause()); break;
  }
}

/* Configure RTC GPIO to use the keyboard matrix as an external trigger */
void initRtcGpio() {
  // Configure the wake up source as external triggers.
  esp_sleep_enable_ext1_wakeup(WAKE_PINS_BITMASK,ESP_EXT1_WAKEUP_ANY_HIGH);

  // Set all keyboard matrix columns to be on during sleep.
  for (uint8_t i = 0; i < RTC_COLS; i++) {
    rtc_gpio_init(RTC_COL_PINS[i]);
    rtc_gpio_set_direction(RTC_COL_PINS[i], RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_set_level(RTC_COL_PINS[i], HIGH);
    rtc_gpio_hold_en(RTC_COL_PINS[i]);
  }
}

/* Remove GPIO holds for all keyboard matrix columns */
void clearRtcGpioHolds() {
  for (uint8_t i = 0; i < RTC_COLS; i++) {
    rtc_gpio_hold_dis(RTC_COL_PINS[i]);
  }
}

void IRAM_ATTR startDeepSleep() {
  initRtcGpio();
  Serial.println("Entering Deep Sleep");
  delay(1000);
  esp_deep_sleep_start();
}

// Non repeating flash pattern for backlight
// First entry always turns the backlight on
void updateBacklight () {
  if(backlightPatCnt < backlightPatTimes) {
    if((backlightMilOld + backlightPatDurMs) < millis()) {
      backlightMilOld = millis();
      if(backlightPatCnt % 2 == 0) {
          setBacklight(backlightBrightness);
      } else {
          setBacklight(0);
      }
      backlightPatCnt++;
    }
  }
}

void backlightSetPattern(uint16_t duration, uint8_t times) {
  backlightPatCnt = 0;
  backlightMilOld = 0;
  backlightPatDurMs = duration;
  backlightPatTimes = times;
}

void backlightOn() {
  backlightSetPattern(0, 0);
}

void backlightLowBattery() {
  backlightSetPattern(100, 7);
}

void backlightLearnModeSent() {
  backlightSetPattern(100, 5);
}

void checkBattery() {
  uint32_t vbatValue = 0;
  const uint8_t samples = 30;
  
  for (uint8_t i = 0; i < samples; i++) {
   vbatValue += analogRead(VBAT_SENSE_PIN);
  }

  uint16_t vbatAveVal = vbatValue / samples;
  vbatAveVal += edata.analogOffset;

  Serial.print (F("Battery Level: "));
  Serial.println (vbatAveVal);
  
  if (vbatAveVal < VBAT_CRITICAL) {
    Serial.println (F("Battery CRITICAL!"));
    startDeepSleep();
  } else  if (vbatAveVal < VBAT_LOW) {
    if (!lowBattery) {
      Serial.println (F("Battery Low!"));
      lowBattery = true;
      backlightLowBattery();
    }
  } else {
    if (lowBattery) {
      Serial.println (F("Battery No longer Low"));
      lowBattery = false;
    }
  }
}

void printBrodcastAddress() {
  Serial.print(F("Paired to MAC: "));
  for (uint8_t i = 0; i < 6; i++) {
    Serial.print(edata.broadcastAddress[i],HEX);
    if (i < 5) {
      Serial.print(":");
    }
  }
  Serial.println();
}

void startSleepTimer() {
  sleepTimer = timerBegin(0, 80, true); // timer 0, div 80
  timerAttachInterrupt(sleepTimer, &startDeepSleep, true);
  if (lowBattery) {
    timerAlarmWrite(sleepTimer, LOWBAT_SLEEP_TIMEOUT_S * 1000000, false);
  } else {
    timerAlarmWrite(sleepTimer, DEFAULT_SLEEP_TIMEOUT_S * 1000000, false);
  }
  timerAlarmEnable(sleepTimer);
}

void initEsp32Now() {
  WiFi.mode(WIFI_STA);
 
  Serial.println();
  Serial.print(F("MAC Address: "));
  Serial.println(WiFi.macAddress());
 
  if (esp_now_init() != ESP_OK) {
    Serial.println(F("Error initializing ESP-NOW"));
    return;
  }

  // Register for Send CB to get the status of TX data
  esp_now_register_send_cb(OnDataSent);
 
  // register peer
  esp_now_peer_info_t peerInfo;
   
  memcpy(peerInfo.peer_addr, edata.broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
         
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
}

void setup() {
  Serial.begin(115200);

  EEPROM.begin(sizeof(edata));
  EEPROM.get(EEPROM_DATA_ADDR, edata);

  // There is some timing issue I don't understand here.
  // initEsp32Now() must be called as soon as possible to avoid
  // "ESPNOW: Peer interface is invalid" error.
  initEsp32Now();

  print_wakeup_trigger();

  pinMode(VBAT_SENSE_PIN, INPUT);
 
  pinMode(HAPTIC_PIN, OUTPUT);
  digitalWrite(HAPTIC_PIN, LOW);

  // Setup PWM timer and attach to backlight pin
  ledcSetup(BACKLIGHT_CH, LEDC_BASE_FREQ, LEDC_TIMER_13_BIT);
  ledcAttachPin(BACKLIGHT_PIN, BACKLIGHT_CH);
  setBacklight(backlightBrightness);

  Serial.print(F("Analog Offset: "));
  Serial.println(edata.analogOffset);
  
  checkBattery();
  
  ++bootCount;
  Serial.print(F("Boot number: "));
  Serial.println(bootCount);

  printBrodcastAddress();

  clearRtcGpioHolds();
  
  kpd.begin();
  kpd.setKeyDownHandler(keyDown);
  kpd.setKeyUpHandler(keyUp);
  
  cmdData.cmd = send_ir;

  startSleepTimer();  
}

void loop() {
  kpd.scan();
  updateBacklight();
}

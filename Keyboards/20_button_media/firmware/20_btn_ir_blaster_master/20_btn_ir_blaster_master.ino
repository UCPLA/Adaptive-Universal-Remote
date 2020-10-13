/*****************************************************************************

AUTHOR: Ruben Kackstaetter
DATE: September 14, 2020
   
DESCRIPTION:
   Keyboard firmware for the Adaptive Universal remote powered with the 
   SparkFun Thing Plus (ESP32 WROOM). Designed to be paired to the Sparkfun
   WiFi IR Blaster over ESP-NOW, and Configured as a Roku TV remote.

LIBRARIES:
 * https://github.com/espressif/arduino-esp32
 * https://github.com/nickgammon/Keypad_Matrix

*****************************************************************************/

/*****************************************************************************
                                   INCLUDES
*****************************************************************************/

#include "WiFi.h"
#include <EEPROM.h>
#include <esp_now.h>
#include <driver/rtc_io.h>
#include <Keypad_Matrix.h>

/*****************************************************************************
                             CONSTANTS - HARDWARE
*****************************************************************************/

// IR Blaster MAC address used once to set EEPROM data
// No changes will be made when address is all 0xFF or
// this address matches what is in EEPROM
const uint8_t IR_BLASTER_MAC[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

const uint16_t EEPROM_DATA_ADDR = 0;

const byte VBAT_SENSE_PIN = A2;
const byte INTERNAL_BTN_PIN = 0;
const byte HAPTIC_PIN = 25;

// GPIO for the key matrix scanning
const byte ROWS = 5;
const byte ROW_PINS[ROWS] = {19, 5, 4, 15, 32};

const byte COLS = 5;
const byte COL_PINS[COLS] = {16, 18, 14, 21, 17};

// TODO: Redesign hardware to use RTC pins for all rows and columns.
// RTC GPIO for wake up, We can only use GPIO 4, 14, 15, 32 from key matrix.
// Ideally we would just use COLS from earlier, but not all cols are RTC

// Cols as Outputs for waking from deep-sleep.
const byte RTC_COLS = 1;
const gpio_num_t RTC_COL_PINS[RTC_COLS] = {GPIO_NUM_14};

// Rows as Wake Interrupt Triggers
const gpio_num_t WAKE_PIN = GPIO_NUM_4;

// Pin used to control backlight
const byte BACKLIGHT_PIN = 26;
// Use first PWM channel of 16 channels
const uint8_t BACKLIGHT_CH = 0;
// Use 13 bit precision for LEDC timer
const uint8_t LEDC_TIMER_13_BIT = 13;
// Use 5000 Hz as a LEDC base frequency
const uint32_t LEDC_BASE_FREQ = 5000;
// Maximum duty cycle from bit precision
const uint32_t MAX_DUTY_CYCLE = (2^LEDC_TIMER_13_BIT) - 1;


/*****************************************************************************
                             CONSTANTS - SETTINGS
*****************************************************************************/

const uint8_t DEFAULT_SLEEP_TIMEOUT_S = 60;
const uint8_t LOWBAT_SLEEP_TIMEOUT_S = 10;

const uint16_t DO_CALIBRATION_HOLD_MS = 2000;
const uint16_t FACTORY_RESET_HOLD_MS = 8000;
const uint8_t HAPTIC_DUR_MS = 200;

// These values are use to determine when to consider the battery low
// While the system is calibrated to detect a 3600mV at 3600 analog read
// the exact voltage that will trigger these states will vary.
const uint16_t VBAT_LOW = 3680;
const uint16_t VBAT_CRITICAL = 3630;

// Define how the keypad is laid out
const char KEY_LAYOUT[ROWS][COLS] = {
  { 0, 1, 2, 3, 4},
  { 5, 6, 7, 8, 9},
  {10,11,12,13,14},
  {15,16,17,18,19},
  {20,21,22,23,24},
};

const char FN_KEY = 20;
const char PWR_KEY = 5;

// Set max value for PWM channel and number of steps
const uint32_t BACKLIGHT_MAX_VAL = 255;
const uint8_t BACKLIGHT_MAX_STEPS = 5;

// Backlight brightness and step amount with each key press
const uint8_t BACKLIGHT_STEP = BACKLIGHT_MAX_VAL / BACKLIGHT_MAX_STEPS;

// The number of times to retry sending a command.
const uint32_t MAX_TX_RETRY = 24;

/*****************************************************************************
                               GLOBAL VARIABLES
*****************************************************************************/

enum key_cmd {
  send_ir,
  learn_ir,
  f_reset
};

// Structure to send cmd to IR blaster
typedef struct cmd_data {
  uint8_t key;
  uint8_t cmd;

} cmd_data;

// Declare g_cmd_data
cmd_data g_cmd_data;

RTC_DATA_ATTR uint32_t bootCount = 0;

typedef struct eeprom_data {
  uint8_t broadcast_addr[6];
  int analogOffset;
} eeprom_data;

// Declare EEPROM data
eeprom_data g_edata;

bool g_rst_key_down = false;
unsigned long g_rst_key_last_ms = 0;

bool g_int_btn_down = false;
unsigned long g_int_btn_last_ms = 0;

unsigned long g_haptic_stop_ms = 0;

RTC_DATA_ATTR int g_backlight_brightness = BACKLIGHT_STEP;

// Backlight Pattern Variables
unsigned long g_backlight_pat_last_ms = 0;
uint8_t g_backlight_pat_cnt = 0;
uint16_t g_backlight_pat_dur_ms = 0;
uint8_t g_backlight_pat_cnt_target = 0;

bool g_low_battery = false;

int g_tx_retry_cnt = 0;

Keypad_Matrix kpd = Keypad_Matrix(makeKeymap(KEY_LAYOUT),
                                  ROW_PINS,
                                  COL_PINS,
                                  ROWS,
                                  COLS);

hw_timer_t *g_sleep_timer = NULL;

/*****************************************************************************
                                  FUNCTIONS
*****************************************************************************/


/*****************************************************************************

FUNCTION NAME: set_backlight

PARAMATERS:
   value - the brightness value to set the backlight to

DESCRIPTION:
   Sets the backlight brightness by setting the PWM duty associated with the
   backlight control pin.

*****************************************************************************/
void set_backlight(uint32_t value) {
  uint32_t duty = ((MAX_DUTY_CYCLE / BACKLIGHT_MAX_VAL) *
                   min(value, BACKLIGHT_MAX_VAL));
  ledcWrite(BACKLIGHT_CH, duty);
}

/*****************************************************************************

FUNCTION NAME: step_backlight_brightness

DESCRIPTION:
   Increases g_backlight_brightness by one step only if battery is not low,
   then sets the brightness level.

*****************************************************************************/
void step_backlight_brightness() {
  if (g_backlight_brightness < BACKLIGHT_MAX_VAL && !g_low_battery) {
    g_backlight_brightness += BACKLIGHT_STEP;
  } else {
    g_backlight_brightness = BACKLIGHT_STEP;
  }

  if (g_low_battery) {
    backlight_low_battery();
  }
  
  Serial.print(F("Backlight: "));
  Serial.println(g_backlight_brightness);
  set_backlight(g_backlight_brightness);
}

/*****************************************************************************

FUNCTION NAME: backlight_update

DESCRIPTION:
   Non-blocking function that toggles the backlight state (on/off) based on
   a set number of times to display a blink pattern to the user. Backlight
   always starts in the on state.
   Must be called at regular intervals, to be able to update the backlight
   state at the appropriate times.

*****************************************************************************/
void backlight_update () {
  if (g_backlight_pat_cnt < g_backlight_pat_cnt_target) {
    if ((g_backlight_pat_last_ms + g_backlight_pat_dur_ms) < millis()) {
      g_backlight_pat_last_ms = millis();
      if (g_backlight_pat_cnt % 2 == 0) {
          set_backlight(g_backlight_brightness);
      } else {
          set_backlight(0);
      }
      g_backlight_pat_cnt++;
    }
  }
}

/*****************************************************************************

FUNCTION NAME: backlight_set_blink_pattern

PARAMATERS:
   duration_ms - the length of time between toggles in milliseconds
   toggle_cnt - the number of times to toggle the backlight between on and off

DESCRIPTION:
   Sets a uniform non repeating blink pattern that is implemented by the
   backlight_update function.

*****************************************************************************/
void backlight_set_blink_pattern(uint16_t duration_ms, uint8_t toggle_cnt) {
  g_backlight_pat_cnt = 0;
  g_backlight_pat_last_ms = 0;
  g_backlight_pat_dur_ms = duration_ms;
  g_backlight_pat_cnt_target = toggle_cnt;
}

/*****************************************************************************

FUNCTION NAME: backlight_low_battery

DESCRIPTION:
   Low battery backlight blink pattern

*****************************************************************************/
void backlight_low_battery() {
  backlight_set_blink_pattern(100, 7);
}

/*****************************************************************************

FUNCTION NAME: backlight_learn_mode

DESCRIPTION:
   Lean mode backlight blink pattern

*****************************************************************************/
void backlight_learn_mode() {
  backlight_set_blink_pattern(100, 5);
}

/*****************************************************************************

FUNCTION NAME: backlight_data_saved

DESCRIPTION:
   Data save to EEPROM backlight blink pattern

*****************************************************************************/
void backlight_data_saved() {
  backlight_set_blink_pattern(100, 11);
}

/*****************************************************************************

FUNCTION NAME: haptic_vibrate

DESCRIPTION:
   Activates the haptic vibrator motor and stores the time the vibrate motor
   should stop at. Relies on the haptic_update function to stop the haptic
   feedback at the right time.

*****************************************************************************/
void haptic_vibrate() {
  g_haptic_stop_ms = millis() + HAPTIC_DUR_MS;
  digitalWrite(HAPTIC_PIN, HIGH);
}

/*****************************************************************************

FUNCTION NAME: haptic_update

DESCRIPTION:
   Non-blocking function that stops the haptic vibrate after a preset time.
   Must be called at regular intervals, to be able to update stop the vibrator
   at the appropriate times.

*****************************************************************************/
void haptic_update() {
  if (millis() >= g_haptic_stop_ms) {
    digitalWrite(HAPTIC_PIN, LOW);
  }
}

/*****************************************************************************

FUNCTION NAME: calc_save_analog_offset

DESCRIPTION:
   Calculates an offset from the the current vbat analog reading to 3600, then
   saves this value to EEPROM. 

   THIS FUNCTION MUST ONLY BE CALLED WHEN THE VBAT PIN IS AT 3600 mV.

   Since the analog input reading for the battery voltage varies
   between each device. An offset is calculated to ensure that a critical
   battery voltage will recognized at an analog reading of 3600.

*****************************************************************************/
void calc_save_analog_offset() {
  uint32_t vbatValue = 0;
  const uint8_t samples = 30;
  
  for (uint8_t i = 0; i < samples; i++) {
   vbatValue += analogRead(VBAT_SENSE_PIN);
  }

  int vbatAveVal = vbatValue / samples;

  g_edata.analogOffset = (3600 - vbatAveVal);
  EEPROM.put(EEPROM_DATA_ADDR, g_edata);
  EEPROM.commit();
  Serial.print(F("Analog Offset: "));
  Serial.println(g_edata.analogOffset);
  backlight_data_saved();
}

/*****************************************************************************

FUNCTION NAME: set_brodcast_addr

DESCRIPTION:
   Sets the IR_BLASTER_MAC constant as the ESP-NOW broadcast address in EEPROM
   only when it is not the NULL_ADDR and is different than what is already
   saved in EEPROM.

*****************************************************************************/
void set_brodcast_addr() {
  const uint8_t NULL_ADDR[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
  bool isNullAddr = (memcmp(NULL_ADDR,
                            IR_BLASTER_MAC,
                            sizeof(IR_BLASTER_MAC)) == 0);

  if (!isNullAddr) {
    bool isNewAddr = (memcmp(g_edata.broadcast_addr,
                             IR_BLASTER_MAC,
                             sizeof(IR_BLASTER_MAC)) != 0);
    if (isNewAddr) {    
      memcpy(g_edata.broadcast_addr, IR_BLASTER_MAC, 6);
      print_broadcast_addr();
      EEPROM.put(EEPROM_DATA_ADDR, g_edata);
      EEPROM.commit();
      backlight_data_saved();
    }
  }
}

/*****************************************************************************

FUNCTION NAME: keyDown

PARAMATERS:
   which - the key that triggered the call of this function 

DESCRIPTION:
   They keyboard_matrix handler for any time a key is pressed down. 

*****************************************************************************/
void keyDown (const char which) {

  // Reset sleep timer on any key press
  timerWrite(g_sleep_timer, 0);
  
  if (g_backlight_brightness == BACKLIGHT_MAX_VAL) { 
    haptic_vibrate();
  }
  
  g_cmd_data.key = (int)which;

  if (which == FN_KEY) {
    g_cmd_data.cmd = learn_ir;
  }

  if (which == PWR_KEY) {
    g_rst_key_down = true;
    g_rst_key_last_ms = millis();
  }
  
  Serial.print (F("Key down: "));
  Serial.println (g_cmd_data.key);
}

/*****************************************************************************

FUNCTION NAME: keyDown

PARAMATERS:
   which - the key that triggered the call of this function

DESCRIPTION:
   They keyboard_matrix handler any time a key is released. 

*****************************************************************************/
void keyUp (const char which) {
  Serial.print (F("Key up: "));
  Serial.println ((int)which);
  
  if (which == FN_KEY || g_cmd_data.cmd == f_reset) {
    g_cmd_data.cmd = send_ir;
    if (g_cmd_data.key == FN_KEY) {
      step_backlight_brightness();
    }
  } else {
    send_cmd();
    if(g_cmd_data.cmd == learn_ir) {
      backlight_learn_mode();
    }
  }

  if (which == PWR_KEY) {
    g_rst_key_down = false;
  }
  
}

/*****************************************************************************

FUNCTION NAME: check_long_key_press

DESCRIPTION:
   Non-blocking function used to perform an action after a key has been held
   down for a certain amount of time.

   We use it here to do a factory reset if the power button is held down long
   enough (duration defined by setting constant FACTORY_RESET_HOLD_MS) 

*****************************************************************************/
void check_long_key_press() {
  unsigned long now = millis();
  if (g_rst_key_down && (now - g_rst_key_last_ms) > FACTORY_RESET_HOLD_MS) {
    g_cmd_data.cmd = f_reset;
    send_cmd();
    g_rst_key_down = false;
    backlight_data_saved();
    Serial.println(F("Sending Factory Reset"));
  }
}

/*****************************************************************************

FUNCTION NAME: send_cmd

DESCRIPTION:
   Transmit the current cmd_data values to the target using ESP-NOW

*****************************************************************************/
void send_cmd() {     
  esp_err_t result = esp_now_send(g_edata.broadcast_addr,
                                  (uint8_t *) &g_cmd_data,
                                  sizeof(cmd_data));
     
  if (result != ESP_OK) {
    Serial.println(F("Error sending the data"));
  }
  
  Serial.print(F("key: "));
  Serial.print(g_cmd_data.key);
  Serial.print(F(", cmd: "));
  Serial.println(g_cmd_data.cmd);
}

/*****************************************************************************

FUNCTION NAME: on_data_sent

PARAMATERS:
   mac_addr - MAC address of target device
   status - Status of data sent

DESCRIPTION:
   ESP-NOW Callback routine that is triggered after sending data.
   If data was not successfully transmitted, retry a fixed number of times
   before giving up.

*****************************************************************************/
void on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print(F("\nLast Packet:\t"));
  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println(F("Delivery Success"));
  } else {
    Serial.println(F("Delivery Failed, Retrying last command"));
    if (g_tx_retry_cnt < MAX_TX_RETRY) {
      send_cmd();
      g_tx_retry_cnt += 1;
    } else {
      g_tx_retry_cnt = 0;
    }
  }
}

/*****************************************************************************

FUNCTION NAME: print_wakeup_trigger

DESCRIPTION:
   Print what triggered the device to wake from sleep. Used to for debugging.

*****************************************************************************/
void print_wakeup_trigger() {
  switch(esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_EXT0:
        Serial.println(F("Wakeup triggered by EXT0 (RTC_IO)"));
      break;
    case ESP_SLEEP_WAKEUP_EXT1:
        Serial.println(F("Wakeup triggered by EXT1 (RTC_CNTL)"));
        Serial.print(F("GPIO that triggered the wake up: GPIO "));
        Serial.println((log(esp_sleep_get_ext1_wakeup_status()))/log(2), 0);
      break;
    case ESP_SLEEP_WAKEUP_TIMER:
        Serial.println(F("Wakeup triggered by Timer"));
      break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
        Serial.println(F("Wakeup triggered by Touchpad"));
      break;
    case ESP_SLEEP_WAKEUP_ULP:
        Serial.println(F("Wakeup triggered by ULP Program"));
      break;
    default:
        Serial.print(F("Wakeup Triggered by Unknown: "));
        Serial.println(esp_sleep_get_wakeup_cause());
      break;
  }
}

/*****************************************************************************

FUNCTION NAME: init_rtc_gpio

DESCRIPTION:
   Configure RTC GPIO to use the keyboard matrix as an external trigger to 
   wake the device from sleep.

*****************************************************************************/
void init_rtc_gpio() {
  // Configure the wake up source as external triggers.
  esp_sleep_enable_ext0_wakeup(WAKE_PIN, HIGH);
  // Ensure wake pin is pulled down to prevent false triggers.
  pinMode(WAKE_PIN, INPUT_PULLDOWN);
  
  // Set all keyboard matrix columns to be on during sleep.
  for (uint8_t i = 0; i < RTC_COLS; i++) {
    rtc_gpio_init(RTC_COL_PINS[i]);
    rtc_gpio_set_direction(RTC_COL_PINS[i], RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_set_level(RTC_COL_PINS[i], HIGH);
    rtc_gpio_hold_en(RTC_COL_PINS[i]);
  }
}

/*****************************************************************************

FUNCTION NAME: clear_rtc_gpio_holds

DESCRIPTION:
   Removes GPIO holds for all keyboard matrix columns that were set by
   init_rtc_gpio.

*****************************************************************************/
void clear_rtc_gpio_holds() {
  for (uint8_t i = 0; i < RTC_COLS; i++) {
    rtc_gpio_hold_dis(RTC_COL_PINS[i]);
  }
}

/*****************************************************************************

FUNCTION NAME: start_deep_sleep

DESCRIPTION:
   This function can be called from a timer. Causes the device to enter
   deep-sleep to conserve battery.

*****************************************************************************/
void IRAM_ATTR start_deep_sleep() {
  init_rtc_gpio();
  Serial.println("Entering Deep Sleep");
  delay(1000);
  esp_deep_sleep_start();
}

/*****************************************************************************

FUNCTION NAME: check_battery

DESCRIPTION:
   Determines the battery status based on the voltage level. This is a very
   basic way to detect a low battery.

*****************************************************************************/
void check_battery() {
  uint32_t vbatValue = 0;
  const uint8_t samples = 30;
  
  for (uint8_t i = 0; i < samples; i++) {
   vbatValue += analogRead(VBAT_SENSE_PIN);
  }

  uint16_t vbatAveVal = vbatValue / samples;
  vbatAveVal += g_edata.analogOffset;

  Serial.print (F("Battery Level: "));
  Serial.println (vbatAveVal);
  
  if (vbatAveVal < VBAT_CRITICAL) {
    Serial.println (F("Battery CRITICAL!"));
    check_internal_button();
    if (g_int_btn_down) {
      Serial.println (F("Skipping deep-sleep for calibration"));
      backlight_learn_mode();
      g_low_battery = true;
    } else {
      start_deep_sleep();
    }
  } else  if (vbatAveVal < VBAT_LOW) {
    if (!g_low_battery) {
      Serial.println (F("Battery Low!"));
      g_low_battery = true;
      backlight_low_battery();
    }
  } else {
    if (g_low_battery) {
      Serial.println (F("Battery No longer Low"));
      g_low_battery = false;
    }
  }
}

/*****************************************************************************

FUNCTION NAME: print_broadcast_addr

DESCRIPTION:
   Prints the broadcast address that is stored in EEPROM.

*****************************************************************************/
void print_broadcast_addr() {
  Serial.print(F("Paired to MAC: "));
  for (uint8_t i = 0; i < 6; i++) {
    Serial.print(g_edata.broadcast_addr[i],HEX);
    if (i < 5) {
      Serial.print(":");
    }
  }
  Serial.println();
}

/*****************************************************************************

FUNCTION NAME: check_internal_button

DESCRIPTION:
   Checks the status of the internal button and triggers the
   calc_save_analog_offset if held down for a fixed amount of time.

*****************************************************************************/
void check_internal_button() {
  unsigned long now = millis();
  bool btnIsDown = (digitalRead(INTERNAL_BTN_PIN) == LOW);
  
  if (g_int_btn_down != btnIsDown) {
      g_int_btn_down = btnIsDown;
      if (g_int_btn_down) {
        g_int_btn_last_ms = now;
      }
  }
  
  if (g_int_btn_down && (now - g_int_btn_last_ms) > DO_CALIBRATION_HOLD_MS) {
    calc_save_analog_offset();
    g_int_btn_down = false;
  }
}

/*****************************************************************************

FUNCTION NAME: start_sleep_timer

DESCRIPTION:
   Begins an interrupt watch-dog timer to trigger deep sleep after a set
   amount of time. A different timeout duration is used for when running on a
   low battery.

*****************************************************************************/
void start_sleep_timer() {
  g_sleep_timer = timerBegin(0, 80, true);
  timerAttachInterrupt(g_sleep_timer, &start_deep_sleep, true);
  if (g_low_battery) {
    timerAlarmWrite(g_sleep_timer, LOWBAT_SLEEP_TIMEOUT_S * 1000000, false);
  } else {
    timerAlarmWrite(g_sleep_timer, DEFAULT_SLEEP_TIMEOUT_S * 1000000, false);
  }
  timerAlarmEnable(g_sleep_timer);
}

/*****************************************************************************

FUNCTION NAME: init_esp_now

DESCRIPTION:
   Initializes the system to use ESP-NOW in a master slave configuration with
   this device as the master. Register a single device by the MAC address
   saved in EEPROM as its target.

*****************************************************************************/
void init_esp_now() {
  WiFi.mode(WIFI_STA);
 
  Serial.println();
  Serial.print(F("MAC Address: "));
  Serial.println(WiFi.macAddress());
 
  if (esp_now_init() != ESP_OK) {
    Serial.println(F("Error initializing ESP-NOW"));
    return;
  }

  // Register for send CB to get the status of TX data
  esp_now_register_send_cb(on_data_sent);
 
  // register peer
  esp_now_peer_info_t peerInfo;

  set_brodcast_addr();
  memcpy(peerInfo.peer_addr, g_edata.broadcast_addr, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
         
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
}

/*****************************************************************************

FUNCTION NAME: setup

DESCRIPTION:
   System setup return called at boot. Triggered at initial power on and
   after waking from sleep.

*****************************************************************************/
void setup() {
  Serial.begin(115200);

  EEPROM.begin(sizeof(g_edata));
  EEPROM.get(EEPROM_DATA_ADDR, g_edata);

  // There is some timing issue I don't understand here.
  // init_esp_now() must be called as soon as possible to avoid
  // "ESPNOW: Peer interface is invalid" error.
  init_esp_now();

  print_wakeup_trigger();

  pinMode(VBAT_SENSE_PIN, INPUT);
  pinMode(INTERNAL_BTN_PIN, INPUT_PULLUP);
 
  pinMode(HAPTIC_PIN, OUTPUT);
  digitalWrite(HAPTIC_PIN, LOW);

  // Setup PWM timer and attach to backlight pin
  ledcSetup(BACKLIGHT_CH, LEDC_BASE_FREQ, LEDC_TIMER_13_BIT);
  ledcAttachPin(BACKLIGHT_PIN, BACKLIGHT_CH);

  Serial.print(F("Analog Offset: "));
  Serial.println(g_edata.analogOffset);
  
  check_battery();
  
  ++bootCount;
  Serial.print(F("Boot number: "));
  Serial.println(bootCount);

  print_broadcast_addr();

  clear_rtc_gpio_holds();
  
  kpd.begin();
  kpd.setKeyDownHandler(keyDown);
  kpd.setKeyUpHandler(keyUp);
  
  g_cmd_data.cmd = send_ir;

  set_backlight(g_backlight_brightness);
  start_sleep_timer();
}

/*****************************************************************************

FUNCTION NAME: loop

DESCRIPTION:
   The main function called as a forever loop during system runtime.

*****************************************************************************/
void loop() {
  kpd.scan();
  check_long_key_press();
  backlight_update();
  haptic_update();
  check_internal_button();
}

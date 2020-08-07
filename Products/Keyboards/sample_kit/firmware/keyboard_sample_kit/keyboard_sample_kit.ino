/*****************************************************************************
   AUTHOR: Ruben Kackstaetter
   DATE: July 31, 2020
   DESCRIPTION: Keyboard firmware for Sparkfun pro micro based off of the code
   https://learn.sparkfun.com/tutorials/cherry-mx-switch-breakout-hookup-guide
   This custom keyboard is intended to be used with the Adaptive Universal
   Remote demo software.
 ****************************************************************************/

#include "Keyboard.h"

// Config Variables
#define NUM_VM_PINS 2
#define NUM_BTN_COLS 6
#define NUM_BTN_ROWS 3

// Software Debounce
// Number of high or low senses that trigger a press or release
#define MAX_DEBOUNCE 6
#define MAX_CAPT_DEBOUNCE 220

#define VIBERATE_DURRATION_MS 100

// Hardware Setup
static const uint8_t vibeMtrPins[NUM_VM_PINS] = {5, 6};
static const uint8_t btnRowPins[NUM_BTN_ROWS] = {15, 14, 16};
static const uint8_t btnColPins[NUM_BTN_COLS] = {2, 3, 4, 7, 8, 9};
static const uint8_t capTouchPin = 20;

// The key assigned to each button
static const char btnKeyCmds[NUM_BTN_ROWS][NUM_BTN_COLS] = {
  "qwert2",
  "asdf-1",
  "zxcvb0"
};
static const char capTouchCmd = btnKeyCmds[0][3];

// Global Variables
static int8_t debounce_count[NUM_BTN_ROWS][NUM_BTN_COLS];
static int16_t capt_debounce_cnt;

void setup() {

  Serial.begin(9600);
  setupSwitchPins();
  setupVibeMotorPins();

  pinMode(capTouchPin, INPUT);

  // Initialize Globals
  capt_debounce_cnt = 0;
  for (uint8_t i = 0; i < NUM_BTN_ROWS; i++) {
    for (uint8_t j = 0; j < NUM_BTN_COLS; j++) {
      debounce_count[i][j] = 0;
    }
  }
}

void loop() {
  scan_switch_matrix();
  scan_cap_touch();
}

static void send_key_down(char key_cmd) {
  Serial.print("Key(s) Down: ");
  switch (key_cmd) {
    // Browser back 
    case '0':
      Serial.println("Alt + Left_arrow");
      Keyboard.press(KEY_LEFT_ALT);
      Keyboard.press(KEY_LEFT_ARROW);
      break;
    // Browser Home
    case '1':
      Serial.println("Alt + Home");
      Keyboard.press(KEY_LEFT_ALT);
      Keyboard.press(KEY_HOME);
      break;
    // Browser forward
    case '2':
      Serial.println("Alt + Right_arrow");
      Keyboard.press(KEY_LEFT_ALT);
      Keyboard.press(KEY_RIGHT_ARROW);
      break;
    // Press key_cmd
    default:
      Serial.println(key_cmd);
      Keyboard.press(key_cmd);
      vibrate_on();
  }
}

static void send_key_up(char key_cmd) {
  Serial.print("Key(s) Up: ");
  switch (key_cmd) {
    // Browser back 
    case '0':
      Serial.println("Alt + Left_arrow");
      Keyboard.release(KEY_LEFT_ARROW);
      Keyboard.release(KEY_LEFT_ALT);
      break;
    // Browser Home
    case '1':
      Serial.println("Alt + Home");
      Keyboard.release(KEY_HOME);
      Keyboard.release(KEY_LEFT_ALT);
      break;
    // Browser forward
    case '2':
      Serial.println("Alt + Right_arrow");
      Keyboard.release(KEY_RIGHT_ARROW);
      Keyboard.release(KEY_LEFT_ALT);
      break;
    // Press key_cmd
    default:
      Serial.println(key_cmd);
      Keyboard.release(key_cmd);
      vibrate_off();
  }
}

static void scan_cap_touch() {
  if (digitalRead(capTouchPin) == HIGH) {
    if (capt_debounce_cnt < MAX_CAPT_DEBOUNCE) {
      capt_debounce_cnt++;
      if (capt_debounce_cnt == MAX_CAPT_DEBOUNCE) {
        send_key_down(capTouchCmd);
      }
    }
  } else {
    if (capt_debounce_cnt > 0) {
      capt_debounce_cnt = 0;
      send_key_up(capTouchCmd);
    }
  }
}

static void scan_switch_matrix() {
  static uint8_t currentRow = 0;
  uint8_t i;

  digitalWrite(btnRowPins[currentRow], LOW);

  for (i = 0; i < NUM_BTN_COLS; i++) {
    if (digitalRead(btnColPins[i]) == LOW) {
      if (debounce_count[currentRow][i] < MAX_DEBOUNCE) {
        debounce_count[currentRow][i]++;
        if (debounce_count[currentRow][i] == MAX_DEBOUNCE) {
          send_key_down(btnKeyCmds[currentRow][i]);
        }
      }
    } else {
      if (debounce_count[currentRow][i] > 0) {
        debounce_count[currentRow][i]--;
        if (debounce_count[currentRow][i] == 0 ) {
          send_key_up(btnKeyCmds[currentRow][i]);
        }
      }
    }
  }
  
  // Once done scanning, de-select the current row
  digitalWrite(btnRowPins[currentRow], HIGH);
  // Set the next row to be scanned
  currentRow++;
  if (currentRow >= NUM_BTN_ROWS) {
    currentRow = 0;
  }
}

static void setupVibeMotorPins() {
  for (uint8_t i = 0; i < NUM_VM_PINS; i++)  {
    pinMode(vibeMtrPins[i], OUTPUT);
    digitalWrite(vibeMtrPins[i], LOW);
  }
}

static void vibrate_off() {
  for (uint8_t i = 0; i < NUM_VM_PINS; i++)  {
    digitalWrite(vibeMtrPins[i], LOW);
  }
}

static void vibrate_on() {
  for (uint8_t i = 0; i < NUM_VM_PINS; i++)  {
    digitalWrite(vibeMtrPins[i], HIGH);
  }
  delay(VIBERATE_DURRATION_MS);
}

static void setupSwitchPins() {
  uint8_t i;

  // Button drive rows - written LOW when active, HIGH otherwise
  for (i = 0; i < NUM_BTN_ROWS; i++)  {
    pinMode(btnRowPins[i], OUTPUT);
    digitalWrite(btnRowPins[i], HIGH);
  }

  // Button select columns
  // Pulled high with internal resistor, LOW when active.
  for (i = 0; i < NUM_BTN_COLS; i++)  {
    pinMode(btnColPins[i], INPUT_PULLUP);
  }
}

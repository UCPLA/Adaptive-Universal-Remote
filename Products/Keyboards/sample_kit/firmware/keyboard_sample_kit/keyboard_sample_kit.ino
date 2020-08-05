/******************************************************************************
 * 
 *****************************************************************************/


#include "Keyboard.h"

// Config Variables //

#define NUM_VM_PINS 2
#define NUM_BTN_COLS 6
#define NUM_BTN_ROWS 3

// Software Debounce
// Number of high or low senses that trigger a press or release
#define MAX_DEBOUNCE 6

#define VIBERATE_DURRATION_MS 100

// Hardware Setup //

static const uint8_t vibeMtrPins[NUM_VM_PINS] = {5, 6};
static const uint8_t btnRowPins[NUM_BTN_ROWS] = {15, 14, 16};
static const uint8_t btnColPins[NUM_BTN_COLS] = {2, 3, 4, 7, 8, 9};

// The key assigned to each button
static const char btnKeyCmds[NUM_BTN_ROWS][NUM_BTN_COLS] = {
  "qwert1",
  "asdf-2",
  "zxcvb3"
};

// Global Variables //

static int8_t debounce_count[NUM_BTN_ROWS][NUM_BTN_COLS];
static int8_t key_is_down[NUM_BTN_ROWS][NUM_BTN_COLS];

void setup() {
  
  Serial.begin(9600);
  setupSwitchPins();
  setupVibeMotorPins();

  // Initialize arrays
  for (uint8_t i = 0; i < NUM_BTN_ROWS; i++) {
    for (uint8_t j = 0; j < NUM_BTN_COLS; j++) {
      debounce_count[i][j] = 0;
      key_is_down[i][j] = false;
    }
  }
}

void loop() {
  scan();
}

static void scan() {
  static uint8_t currentRow = 0;
  uint8_t i;

  digitalWrite(btnRowPins[currentRow], LOW);

  
  for ( i = 0; i < NUM_BTN_COLS; i++) {
    if (digitalRead(btnColPins[i]) == LOW) {
      if ( debounce_count[currentRow][i] < MAX_DEBOUNCE) {
        debounce_count[currentRow][i]++;
        if ( debounce_count[currentRow][i] == MAX_DEBOUNCE ) {
          Serial.print("Key Down: ");
          Serial.println(btnKeyCmds[currentRow][i]);
          Keyboard.press(btnKeyCmds[currentRow][i]);
          vibrate_on();
        }
      }
    } else {
      if ( debounce_count[currentRow][i] > 0) {
        debounce_count[currentRow][i]--;
        if ( debounce_count[currentRow][i] == 0 ) {
          Serial.print("Key Up: ");
          Serial.println(btnKeyCmds[currentRow][i]);
          Keyboard.release(btnKeyCmds[currentRow][i]);
          vibrate_off();
        }
      }
    }
  }
  // Once done scanning, de-select the switch rows
  digitalWrite(btnRowPins[currentRow], HIGH);
  
  // Increment currentRow, so next time we scan the next row
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

  // Buttn select columns. Pulled high through resistor. Will be LOW when active
  for (i = 0; i < NUM_BTN_COLS; i++)  {
    pinMode(btnColPins[i], INPUT_PULLUP);
  }
}

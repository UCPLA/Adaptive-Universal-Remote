/*****************************************************************************
   AUTHOR: Ruben Kackstaetter
   DATE: September 14, 2020
   
   DESCRIPTION: IR Blaster firmware for Sparkfun WiFi IR Blaster
   Developed to be paired and controlled by the 20 button IR blaster master
   using the SparkFun Thing Plus (ESP32 WROOM) Configured as a Roku TV remote

   Depends on:
   * https://github.com/esp8266/Arduino
   * https://github.com/crankyoldgit/IRremoteESP8266
   * https://github.com/jandelgado/jled
 ****************************************************************************/

#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <assert.h>
#include <espnow.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRrecv.h>
#include <IRac.h>
#include <IRtext.h>
#include <IRutils.h>
#include <jled.h>


#define ESP_OK 0

// Define Pins
const uint8_t kIrLedPin = 4;
const uint8_t kRecvPin = 13;

JLed statusLed = JLed(LED_BUILTIN).Off();

const uint32_t kBaudRate = 115200;

// As this program is a special purpose capture/decoder, let us use a larger
// than normal buffer so we can handle Air Conditioner remote codes.
const uint16_t kCaptureBufferSize = 1024;

// kTimeout is the Nr. of milli-Seconds of no-more-data before we consider a
// message ended.
// NOTE: Don't exceed kMaxTimeoutMs. Typically 130ms.
const uint8_t kTimeout = 15;

// Set the smallest sized "UNKNOWN" message packets we actually care about.
// NOTE: Set this value very high to effectively turn off UNKNOWN detection.
const uint16_t kMinUnknownSize = 999999;

const uint8_t NUM_IR_CODES = 25;

typedef struct eeprom_data {
  uint64_t irCodes[NUM_IR_CODES];
} eeprom_data;

// Declare EEPROM data
eeprom_data edata;

const uint16_t EEPROM_DATA_ADDR = 0;

const uint64_t ROKU_NEC_POWER     = 0x57E3E817;
const uint64_t ROKU_NEC_BACK      = 0x57E36699;
const uint64_t ROKU_NEC_HOME      = 0x57E3C03F;
const uint64_t ROKU_NEC_UP        = 0x57E39867;
const uint64_t ROKU_NEC_LEFT      = 0x57E37887;
const uint64_t ROKU_NEC_OK        = 0x57E354AB;
const uint64_t ROKU_NEC_RIGHT     = 0x57E3B44B;
const uint64_t ROKU_NEC_DOWN      = 0x57E3CC33;
const uint64_t ROKU_NEC_BACK20    = 0x57E31EE1;
const uint64_t ROKU_NEC_ASTERIX   = 0x57E38679;
const uint64_t ROKU_NEC_REWIND    = 0x57E32CD3;
const uint64_t ROKU_NEC_PLAY      = 0x57E332CD;
const uint64_t ROKU_NEC_FORWARD   = 0x57E3AA55;
const uint64_t ROKU_NEC_VOL_UP    = 0x57E3F00F;
const uint64_t ROKU_NEC_VOL_DOWN  = 0x57E308F7;
const uint64_t ROKU_NEC_MUTE      = 0x57E304FB;

const uint64_t irDefaults[] = {0,              0,                 0,               0,             0,
                               ROKU_NEC_POWER, ROKU_NEC_HOME,     ROKU_NEC_BACK,   ROKU_NEC_UP,   ROKU_NEC_HOME,
                               ROKU_NEC_UP,    ROKU_NEC_VOL_UP,   ROKU_NEC_LEFT,   ROKU_NEC_OK,   ROKU_NEC_RIGHT,
                               ROKU_NEC_DOWN,  ROKU_NEC_VOL_DOWN, ROKU_NEC_BACK20, ROKU_NEC_DOWN, ROKU_NEC_ASTERIX,
                               0,              ROKU_NEC_MUTE,     ROKU_NEC_REWIND, ROKU_NEC_PLAY, ROKU_NEC_FORWARD};

// The IR transmitter.
IRsend irsend(kIrLedPin);

// Use turn on the save buffer feature for m7ore complete capture coverage.
IRrecv irrecv(kRecvPin, kCaptureBufferSize, kTimeout, true);
decode_results results;  // Somewhere to store the results

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

// Create cmdData struct
cmd_data cmdData;

void ledStatusLearnStart() {
  statusLed = JLed(LED_BUILTIN).Blink(100,1200).Forever();
}
void ledStatusLearnEnd() {
  statusLed = JLed(LED_BUILTIN).Blink(100,100).Repeat(2);
}

void ledStatusFactoryReset() {
  statusLed = JLed(LED_BUILTIN).Blink(100,100).Repeat(5);
}

void ledStatusOff() {
  statusLed = JLed(LED_BUILTIN).Off();
}

void onReceiveData(uint8_t *mac, uint8_t *data, uint8_t len) {
 
  memcpy(&cmdData, data, sizeof(cmd_data));
  
  Serial.print (F("Received Key: "));
  Serial.print (cmdData.key);
  Serial.print (F(", Cmd: "));
  Serial.print (cmdData.cmd);

  Serial.print(F(" from MAC: "));
 
  for (int i = 0; i < 6; i++) { 
    Serial.printf("%02X", mac[i]);
    if (i < 5) {
      Serial.print(F(":"));
    } else {
      Serial.println();
    }
  }

  switch (cmdData.cmd) {
    case send_ir:
        irrecv.disableIRIn();
        irsend.sendNEC(edata.irCodes[cmdData.key]);
        Serial.print(F("Sending IR Code: 0x"));
        Serial.println(uint64ToString(edata.irCodes[cmdData.key], 16));
        ledStatusOff();
      break;
    case learn_ir:
        ledStatusLearnStart();
        irrecv.enableIRIn();
      break;
    case f_reset:
        irrecv.disableIRIn();
        factoryReset();
      break;
  }
}

void irlearn() {
  // Check if the IR code has been received.
  if (irrecv.decode(&results)) {
    // Check if we got an IR message that was to big for our capture buffer.
    if (results.overflow)
      Serial.printf(D_WARN_BUFFERFULL "\n", kCaptureBufferSize);

    if (!results.repeat) {
      // Display the basic output of what we found.
      Serial.print(resultToHumanReadableBasic(&results));
      
      // Display any extra A/C info if we have it.
      String description = IRAcUtils::resultAcToString(&results);
      if (description.length()) Serial.println(D_STR_MESGDESC ": " + description);
      yield();  // Feed the WDT as the text output can take a while to print.
  
      edata.irCodes[cmdData.key] = results.value;
      EEPROM.put(EEPROM_DATA_ADDR, edata);
      EEPROM.commit();
      irrecv.disableIRIn();
      ledStatusLearnEnd();
    }
  }
}

void factoryReset() {
  memcpy(edata.irCodes, irDefaults, sizeof(irDefaults));
  EEPROM.put(EEPROM_DATA_ADDR, edata);
  EEPROM.commit();
  Serial.println(F("IR Blaster Factory Reset"));
  ledStatusFactoryReset();
}

void setup() {
#if defined(ESP8266)
  Serial.begin(kBaudRate, SERIAL_8N1, SERIAL_TX_ONLY);
#else  // ESP8266
  Serial.begin(kBaudRate, SERIAL_8N1);
#endif  // ESP8266
  
  WiFi.mode(WIFI_STA);
 
  if (esp_now_init() != ESP_OK) {
    Serial.println(F("Error initializing ESP-NOW"));
    return;
  }

  Serial.println();
  Serial.print(F("MAC Address: "));
  Serial.println(WiFi.macAddress());

  EEPROM.begin(sizeof(edata));
  EEPROM.get(EEPROM_DATA_ADDR, edata);

  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(onReceiveData);

  // Perform a low level sanity checks that the compiler performs bit field
  // packing as we expect and Endianness is as we expect.
  assert(irutils::lowLevelSanityCheck() == 0);

#if DECODE_HASH
  // Ignore messages with less than minimum on or off pulses.
  irrecv.setUnknownThreshold(kMinUnknownSize);
#endif  // DECODE_HASH

  irsend.begin();
}
 
void loop() {
  statusLed.Update();
  irlearn();
}

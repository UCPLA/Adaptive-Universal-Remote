/*****************************************************************************
   AUTHOR: Ruben Kackstaetter
   DATE: September 14, 2020
   
   DESCRIPTION: IR Blaster firmware for Sparkfun WiFi IR Blaster
   Developed to be paired and controlled by the 20 button IR blaster master
   using the SparkFun Thing Plus (ESP32 WROOM) Configured as a Roku TV remote

   
 ****************************************************************************/

#include <ESP8266WiFi.h>
#include <espnow.h>
#include <IRremoteESP8266.h>
#include <singleLEDLibrary.h>
#include <IRsend.h>
//#include <IRrecv.h>
//#include <IRac.h>
//#include <IRtext.h>
//#include <IRutils.h>

#define ESP_OK 0

// Define Pins
const uint8_t kIrLedPin = 4;
const uint8_t kRecvPin = 13;

sllib statusLed(2);

// As this program is a special purpose capture/decoder, let us use a larger
// than normal buffer so we can handle Air Conditioner remote codes.
const uint16_t kCaptureBufferSize = 1024;

// kTimeout is the Nr. of milli-Seconds of no-more-data before we consider a
// message ended.
// This parameter is an interesting trade-off. The longer the timeout, the more
// complex a message it can capture. e.g. Some device protocols will send
// multiple message packets in quick succession, like Air Conditioner remotes.
// Air Coniditioner protocols often have a considerable gap (20-40+ms) between
// packets.
// The downside of a large timeout value is a lot of less complex protocols
// send multiple messages when the remote's button is held down. The gap between
// them is often also around 20+ms. This can result in the raw data be 2-3+
// times larger than needed as it has captured 2-3+ messages in a single
// capture. Setting a low timeout value can resolve this.
// So, choosing the best kTimeout value for your use particular case is
// quite nuanced. Good luck and happy hunting.
// NOTE: Don't exceed kMaxTimeoutMs. Typically 130ms.
const uint8_t kTimeout = 15;

// Set the smallest sized "UNKNOWN" message packets we actually care about.
// This value helps reduce the false-positive detection rate of IR background
// noise as real messages. The chances of background IR noise getting detected
// as a message increases with the length of the kTimeout value. (See above)
// The downside of setting this message too large is you can miss some valid
// short messages for protocols that this library doesn't yet decode.
//
// Set higher if you get lots of random short UNKNOWN messages when nothing
// should be sending a message.
// Set lower if you are sure your setup is working, but it doesn't see messages
// from your device. (e.g. Other IR remotes work.)
// NOTE: Set this value very high to effectively turn off UNKNOWN detection.
const uint16_t kMinUnknownSize = 60;

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

const uint64_t irCodes[26] = { 0,              0,                 0,               0,             0,
                               ROKU_NEC_POWER, ROKU_NEC_HOME,     ROKU_NEC_BACK,   ROKU_NEC_UP,   ROKU_NEC_HOME,
                               ROKU_NEC_UP,    ROKU_NEC_VOL_UP,   ROKU_NEC_LEFT,   ROKU_NEC_OK,   ROKU_NEC_RIGHT,
                               ROKU_NEC_DOWN,  ROKU_NEC_VOL_DOWN, ROKU_NEC_BACK20, ROKU_NEC_DOWN, ROKU_NEC_ASTERIX,
                               0,              ROKU_NEC_MUTE,     ROKU_NEC_REWIND, ROKU_NEC_PLAY, ROKU_NEC_FORWARD};

// The IR transmitter.
IRsend irsend(kIrLedPin);

// Use turn on the save buffer feature for more complete capture coverage.
//IRrecv irrecv(kRecvPin, kCaptureBufferSize, kTimeout, true);
//decode_results results;  // Somewhere to store the results

enum key_cmd {
  send_ir,
  learn_ir
};

// Structure to send cmd to IR blaster
typedef struct cmd_data {
  uint8_t key;
  uint8_t cmd;

} cmd_data;

// Create cmdData struct
cmd_data cmdData;

void ledStatusLearn() {
  int pattern[] = {100,100,100,1200};
  statusLed.setPatternSingle(pattern, 4);
}

void ledStatusOff() {
  statusLed.setOffSingle();
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
        irsend.sendNEC(irCodes[cmdData.key]);
        ledStatusOff();
      break;
    case learn_ir:
        ledStatusLearn();
//        irrecv.enableIRIn();  // Start the receiver
//        if (irrecv.decode(&results)) {
//          // Display a crude timestamp.
//          uint32_t now = millis();
//          Serial.printf(D_STR_TIMESTAMP " : %06u.%03u\n", now / 1000, now % 1000);
//          
//          // Check if we got an IR message that was to big for our capture buffer.
//          if (results.overflow)
//            Serial.printf(D_WARN_BUFFERFULL "\n", kCaptureBufferSize);
//          
//          // Display the library version the message was captured with.
//          Serial.println(D_STR_LIBRARY "   : v" _IRREMOTEESP8266_VERSION_ "\n");
//          
//          // Display the basic output of what we found.
//          Serial.print(resultToHumanReadableBasic(&results));
//    
//          // Display any extra A/C info if we have it.
//          String description = IRAcUtils::resultAcToString(&results);
//          if (description.length()) Serial.println(D_STR_MESGDESC ": " + description);
//          yield();  // Feed the WDT as the text output can take a while to print.
//    
//          // Output the results as source code
//          Serial.println(resultToSourceCode(&results));
//          Serial.println();    // Blank line between entries
//          yield();             // Feed the WDT (again)
//        }
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000); // Wait 1 sec for Serial Monitor
  
  WiFi.mode(WIFI_STA);
  Serial.println();
  Serial.print(F("MAC Address: "));
  Serial.println(WiFi.macAddress());
 
  if (esp_now_init() != ESP_OK) {
    Serial.println(F("Error initializing ESP-NOW"));
    return;
  }

  //esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(onReceiveData);

  //#if DECODE_HASH
  // Ignore messages with less than minimum on or off pulses.
  //irrecv.setUnknownThreshold(kMinUnknownSize);
  //#endif  // DECODE_HASH

  irsend.begin();
}
 
void loop() {
  statusLed.update();
}

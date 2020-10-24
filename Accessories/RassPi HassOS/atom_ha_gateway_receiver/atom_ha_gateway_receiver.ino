/**
 * This is a workaround for the Raspberry Pi/Home Assistant hub.  Upload this code
 * to any ESP32 based board and physically connect it to the USB port of the Pi.
 * Code receives a message from the gesture control remote and relays it to the Pi
 * through serial communication to avoid multiple interfering wireless communication protocols. 
 * 
 * Hackaday x UCPLA Dream Team, Kelvin Chow, October 6, 2020
 * https://hackaday.io/project/173454-2020-hdp-dream-team-ucpla
 */
 
#include <esp_now.h>
#include <WiFi.h>


// Variable to store if sending data was successful
String success;

//Structure example to send data
//Must match the receiver structure
typedef struct struct_message {
  int id;     //Board ID
  int state;
  float gX;   //IMU data
  float gY;
  float gZ;
  float aX;
  float aY;
  float aZ;
  float battery;
  String message;
} struct_message;


// Create a struct_message to hold incoming sensor readings
struct_message incomingReadings;


// Callback when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
  Serial.println(incomingReadings.message);  //Write new message to serial
  delay(100);
  Serial.println();  //Clear message

}
 
void setup() {
  // Init Serial Monitor
  Serial.begin(115200);
 
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register for a callback function that will be called when data is received
  esp_now_register_recv_cb(OnDataRecv);
}
 
void loop() {
}

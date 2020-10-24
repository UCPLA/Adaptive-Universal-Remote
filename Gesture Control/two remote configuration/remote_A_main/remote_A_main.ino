/**
 * Main gesture control remote firmware.  The basic function of this code is to send IMU data 
 * to the Atom matrix microcontroller.  Auxiliary functions include an input button for changing
 * states and receving commands from the Atom to vibrate a motor. Charge current is set at 630 ma fo 1000 mAh battery.  
 * Adjust accordingly for the battery capacity (or disable this code at line 123).
 * 
 * Install ESP32 board in IDE: https://randomnerdtutorials.com/installing-the-esp32-board-in-arduino-ide-windows-instructions/
 * Depends on M5StickC library: https://github.com/m5stack/M5StickC
 *    
 * Hackaday x UCPLA Dream Team, Kelvin Chow, October 3, 2020
 * https://hackaday.io/project/173454-2020-hdp-dream-team-ucpla  
 */

#include <esp_now.h>
#include <WiFi.h>
#include <M5StickC.h>

// Change these variables
uint8_t broadcastAddressAtom[] = {0x, 0x, 0x, 0x, 0x, 0x};  // Replace with your Atom Matrix MAC Address
// uint8_t broadcastAddressAtom[] = {0x50, 0x02, 0x91, 0x90, 0xAD, 0x60}; // Sample MAC address
const int numStates = 5;       // Set this variable to the number of device states.  
const int thisDeviceID = 2;    // DO NOT USE 0 or 1.  Use 2 for remote A, 3 for remote B,4 for remote C, etc.
const int period = 50;         // Set time delay in ms.  50 ms = 20 Hz sampling frequency.

const int btnPin = 26;    // Change to 37 for M5StickC out of the box.
const int motorPin = 25;
int joyState = 0; 
String devName;

float accX = 0;
float accY = 0;
float accZ = 0;
float gyroX = 0;
float gyroY = 0;
float gyroZ = 0;
float usbVoltage;
float batVoltage;

// Structure example to send data
// Must match the receiver structure
typedef struct struct_message {
  int id;
  int state;
  float gX;
  float gY;
  float gZ;
  float aX;
  float aY;
  float aZ;
  float battery;
  String message;
} struct_message;

// Create a struct_message called myData
struct_message myData;

// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if ((status == ESP_NOW_SEND_FAIL)){
    delay(50);
    if (usbVoltage < 2) {
      Serial.println("Didn't work, oh no, go to sleep, goodnight");
      vibrate(500);
      esp_deep_sleep_start();    
    }
  }
}

// callback when data is received
void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
  memcpy(&myData, incomingData, sizeof(myData));    
  //Read incoming data from receiver to set different vibration patterns
  if (myData.message == "SELECT") {
    vibrate(100);
    delay(50);
    vibrate(100);    
  }
  else if (myData.message == "BUZZ") {
    vibrate(100);
  }
}

// Code is run when the button on the remote is pressed
void sendJoystickState() {
  Serial.println(joyState);
  usbVoltage = M5.Axp.GetVBusVoltage();
  batVoltage = M5.Axp.GetBatVoltage();
  if (joyState == numStates) {
    myData.id = 0;
    myData.state = joyState;
    esp_err_t result = esp_now_send(broadcastAddressAtom, (uint8_t *) &myData, sizeof(myData));
    Serial.println("Going to sleep now");
    vibrate(500);   // long vibration indicates device going to sleep
    delay(1000);
    esp_deep_sleep_start();
  }
  else {
      for (int i=0; i<=joyState; i++) {
        vibrate(100);  
        delay(50);
    }
  }
  myData.id = 0;  // 0 is id for setting device state
  myData.message = devName;
  myData.state = joyState;
  myData.battery = batVoltage;
  esp_err_t result = esp_now_send(broadcastAddressAtom, (uint8_t *) &myData, sizeof(myData));
  delay(100);
}

void vibrate(int duration){   // Send user a single pulse. Duration in millesconds.
  digitalWrite(motorPin, HIGH);
  delay(duration);
  digitalWrite(motorPin, LOW);
}
 
void setup() {
  // Initialize serial monitor and M5 components
  Serial.begin(115200);
  M5.begin();
  M5.MPU6886.Init();
  M5.Axp.begin(true,true,false,false,false);  //Disable: Display backlight, Display control, RTC, DCDC1, DCDC3. *NEVER DISABLE DCDC1*
  //M5.Axp.SetChargeCurrent(CURRENT_630MA);   // Set charge current based on battery capacity. Max charge current should be 1C.  
  pinMode(btnPin,INPUT_PULLUP);
  pinMode(motorPin, OUTPUT);

  devName = "DEV" + String(thisDeviceID);
  Serial.println(devName);

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_26,0);   // Setup the big button (GPIO pin 26) to wake up from sleep. Change to 37 for M5StickC out of the box.

     
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    Serial.println("Go to sleep, goodnight");
    vibrate(500);
    esp_deep_sleep_start();  // Any failed connection puts the device to sleep
    return;
  }

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);
  
  // Register peer
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, broadcastAddressAtom, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    Serial.println("Go to sleep, goodnight");
    vibrate(500);
    esp_deep_sleep_start();  // Any failed connection puts the device to sleep
    return;
  }
  // Register for a callback function that will be called when data is received
  esp_now_register_recv_cb(OnDataRecv);
  sendJoystickState();  // On startup, device goes to state 0  
  //Serial.println(WiFi.macAddress());
}

void loop() {
  M5.MPU6886.getGyroData(&gyroX, &gyroY, &gyroZ);
  M5.MPU6886.getAccelData(&accX, &accY, &accZ);  
  // Set values to send              
  myData.aX = accX;
  myData.aY = accY;
  myData.aZ = accZ;
  myData.gX = gyroX;
  myData.gY = gyroY;
  myData.gZ = gyroZ;

  if (digitalRead(btnPin)==LOW){    // Check for button press to switch states
    joyState++;
    sendJoystickState();    
  }
  myData.id = thisDeviceID;
  usbVoltage = M5.Axp.GetVBusVoltage();
  esp_err_t result = esp_now_send(broadcastAddressAtom, (uint8_t *) &myData, sizeof(myData));

  M5.update();
  delay(period);  // Delay sets IMU sampling frequency.
}

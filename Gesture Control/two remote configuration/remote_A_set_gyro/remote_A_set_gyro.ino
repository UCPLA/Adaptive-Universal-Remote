/**
 * Before using gesture control remote, run this code before uploading the main gesture control firmware code.
 * Make sure Atom matrix receiver is running the main firmware and is plugged in.
 * Instructions: 
 *    1) Connect remote to computer and upload firmware
 *    2) After uploading, open serial monitor on the computer
 *    3) Press the large button on the gesture control remote and wait 6 seconds. DO NOT MOVE THE REMOTE DURING THIS TIME!
 *       During this time, the serial monitor should be displaying messages of "DELIVERY SUCCESS".  If you see "DELVIERY FAIL",
 *       please check if everything is setup correctly before retrying.
 *    4) Once calibration is done, a message on the serial monitor should appear saying, "CALIBRATION FINISHED!". Move on to uploading 
 *       the main gesture control firmware next. 
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
uint8_t broadcastAddress[] = {0x, 0x, 0x, 0x, 0x, 0x};  // Replace with your Atom Matrix MAC Address
//uint8_t broadcastAddress[] = {0x50, 0x02, 0x91, 0x90, 0x0E, 0xCC}; // Sample MAC address
const int thisDeviceID = 2;    // DO NOT USE 0 or 1.  Use 2 for remote A, 3 for remote B,4 for remote C, etc.

float accX = 0;
float accY = 0;
float accZ = 0;
float gyroX = 0;
float gyroY = 0;
float gyroZ = 0;

const int btnPin = 26;  // Change to 37 for M5StickC out of the box.
const int motorPin = 25;

int sendState=0;
long timer;
bool running=false;

// Structure to send data between ESP32 boards
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
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void vibrate(int duration){   // Send user a single pulse. Duration in millesconds.
  digitalWrite(motorPin, HIGH);
  delay(duration);
  digitalWrite(motorPin, LOW);
}
 
void setup() {
  //Initialize Serial monitor and M5 components
  Serial.begin(115200);
  M5.begin();
  M5.MPU6886.Init();
  M5.Axp.begin(true,true,false,false,true);
  pinMode(btnPin, INPUT_PULLUP);
  pinMode(motorPin, OUTPUT);

  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);
  
  // Register peer
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
    Serial.println("Waiting for button press...");
    while (digitalRead(btnPin) == HIGH){    // Stop code here until the button is pressed
    }
      Serial.println("Calibrating...");
      delay(1000);             // Wait for 1 second in case device is moving during button release
      timer = millis();
      myData.id = 1;            // id=1 tells Atom matrix to overwrite gyroscope offsets in flash memory.
      myData.message = "DEV" + String(thisDeviceID);
      while ((millis()-timer) < 5000) {    // Stream gyro data for 5 seconds.
        M5.MPU6886.getGyroData(&gyroX, &gyroY, &gyroZ);
        myData.state = 0;
        myData.gX = gyroX;
        myData.gY = gyroY;
        myData.gZ = gyroZ;
        esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
        delay(100);
      }
        myData.state = 1;   //If state = 1, end calibration routine
        myData.gX = gyroX;
        myData.gY = gyroY;
        myData.gZ = gyroZ;
        esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));      
        Serial.println("Calibration Finished!");  
        vibrate(100);
        delay(50);
        vibrate(100);
}
 
void loop() {             
}

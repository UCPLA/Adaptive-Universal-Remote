#include <esp_now.h>
#include <WiFi.h>
#include <M5StickC.h>


// REPLACE WITH YOUR RECEIVER MAC Address
uint8_t broadcastAddress[] = {0x, 0x, 0x, 0x, 0x, 0x};

float accX = 0;
float accY = 0;
float accZ = 0;
float gyroX = 0;
float gyroY = 0;
float gyroZ = 0;


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

} struct_message;

// Create a struct_message called myData
struct_message myData;

// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}
 
void setup() {
  // Init Serial Monitor
  Serial.begin(115200);
  
  M5.begin();
  M5.MPU6886.Init();
  // text print
  M5.Lcd.fillScreen(BLACK);
  M5.Axp.ScreenBreath(10);
  M5.Lcd.setCursor(22, 30);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setRotation(1);
  M5.Lcd.setTextSize(2);
  M5.Lcd.printf("JOYSTICK 1");
     
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
  
  myData.id = 0;
  myData.state=0;
}

void loop() {
  M5.MPU6886.getGyroData(&gyroX,&gyroY,&gyroZ);
  M5.MPU6886.getAccelData(&accX,&accY,&accZ);  
  // Set values to send              
  myData.aX = accX;
  myData.aY = accY;
  myData.aZ = accZ;
  myData.gX = gyroX;
  myData.gY = gyroY;
  myData.gZ = gyroZ;

  // Send message via ESP-NOW
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
   
  if (result == ESP_OK) {
    Serial.println("Sent with success");
    //Serial.println(accX);
  }
  else {
    Serial.println("Error sending the data");
  }
  delay(50);
}

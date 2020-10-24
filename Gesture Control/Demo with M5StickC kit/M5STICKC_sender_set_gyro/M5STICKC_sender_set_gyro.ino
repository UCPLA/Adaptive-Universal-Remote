#include <esp_now.h>
#include <WiFi.h>
#include <M5StickC.h>


// REPLACE WITH THE RECEIVER MAC Address
uint8_t broadcastAddress[] = {0x, 0x, 0x, 0x, 0x, 0x};

float accX = 0;
float accY = 0;
float accZ = 0;
float gyroX = 0;
float gyroY = 0;
float gyroZ = 0;

int sendState=0;
long timer;
bool running=false;

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
  M5.Lcd.setCursor(22, 30);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setRotation(1);
  M5.Lcd.setTextSize(2);
  M5.Lcd.printf("SET OFFSET?");

  pinMode(M5_BUTTON_HOME, INPUT);
  // Set values to send

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
    while (digitalRead(M5_BUTTON_HOME)==HIGH){
    }
      M5.Lcd.fillScreen(BLACK);
      delay(1000);
      timer=millis();
      myData.id=4;
      while ((millis()-timer)<5000){
        M5.MPU6886.getGyroData(&gyroX,&gyroY,&gyroZ);
        myData.state=0;
        myData.gX = gyroX;
        myData.gY = gyroY;
        myData.gZ = gyroZ;
        esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
        delay(100);
      }
        myData.state=1;   //If 1, finish calibration routine
        myData.gX = gyroX;
        myData.gY = gyroY;
        myData.gZ = gyroZ;
        esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));        
        M5.Lcd.printf("FINISHED!!!");
}
 
void loop() {             
}

#include <esp_now.h>
#include <WiFi.h>
#include <M5StickC.h>


// REPLACE WITH YOUR RECEIVER MAC Address
uint8_t broadcastAddress[] = {0xC8, 0x2B, 0x96, 0xB8, 0xB9, 0xC8};

float accX = 0;
float accY = 0;
float accZ = 0;
float gyroX = 0;
float gyroY = 0;
float gyroZ = 0;

int sendState=0;
int gameState=0;
String gameStatesText[4]={"Game:Maze", "Game:Pacman     & Snake", "Game:Space      Invaders", "Game:T-rex        Run"};
long timer;

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
  M5.Axp.ScreenBreath(10);
  draw_home_screen(gameStatesText[gameState]);

  pinMode(M5_BUTTON_HOME, INPUT);
  pinMode(M5_BUTTON_RST, INPUT);
  // Set values to send
  myData.id = 1;

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

    myData.id = 3;
    myData.state=0;            
    myData.aX = 0;
    myData.aY = 0;
    myData.aZ = 0;
    myData.gX = 0;
    myData.gY = 0;
    myData.gZ = 0;
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));

}
void draw_home_screen(String screenText){
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setRotation(0);
  M5.Lcd.drawTriangle(16, 157, 7, 149, 25, 149, WHITE);
  M5.Lcd.fillTriangle(16, 157, 7, 149, 25, 149, WHITE);
  M5.Lcd.drawTriangle(68, 74, 68, 94, 78, 84, WHITE);
  M5.Lcd.fillTriangle(68, 74, 68, 94, 78, 84,WHITE);
  M5.Lcd.setCursor(22, 55);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setRotation(1);
  M5.Lcd.setTextSize(2);
  M5.Lcd.print("CALIBRATE?");
  M5.Lcd.setCursor(5,17);
  M5.Lcd.print(screenText);
  

}
void loop() {             
  if (digitalRead(M5_BUTTON_HOME) == LOW) {
    myData.id=2;
    myData.state = sendState;
    Serial.println(sendState);
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
    sendState+=1;
    sendState = sendState%6;
    timer=millis();
    if (sendState!=1){
      M5.Lcd.fillScreen(GREEN);
      delay(2000);
      M5.Lcd.fillScreen(BLACK);
    }  
    else {
      M5.Lcd.fillScreen(BLACK);
      delay(1000);
    }
    M5.Lcd.setCursor(22, 30);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setRotation(1);
    M5.Lcd.setTextSize(2);
    if (sendState==0){
      //ALSO WRITE GAME MODE
      draw_home_screen(gameStatesText[gameState]);
    }
    else if (sendState==1){
      M5.Lcd.printf("HOLD NORMAL");     
    }
    else if (sendState==2){
      M5.Lcd.printf("HOLD LEFT");     
    }
    else if (sendState==3){
      M5.Lcd.printf("HOLD RIGHT");     
    }
    else if (sendState==4){
      M5.Lcd.printf("HOLD UP");     
    }
    else if (sendState==5){
      M5.Lcd.printf("HOLD DOWN");     
    }   
  }

  if((digitalRead(M5_BUTTON_RST) == LOW)&&(sendState==0)){
    gameState+=1;         
    gameState=gameState%4;
    myData.id=3;
    myData.state=gameState;
    Serial.println(gameState);
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
    draw_home_screen(gameStatesText[gameState]);
    delay(500);   
  }

  /*
  M5.MPU6886.getGyroData(&gyroX,&gyroY,&gyroZ);
  M5.MPU6886.getAccelData(&accX,&accY,&accZ);  
  // Set values to send  
  myData.id = 1;            
  myData.aX = accX;
  myData.aY = accY;
  myData.aZ = accZ;
  myData.gX = gyroX;
  myData.gY = gyroY;
  myData.gZ = gyroZ;
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
  */
  //delay(250);
  delay(100);
}

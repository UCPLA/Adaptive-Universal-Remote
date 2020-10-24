/**
 * Main atom matrix receiver firmware.  Receives IMU signals from gesture control remote and 
 * does simple signal processing to perform actions.  Also performs device calibration and 
 * displays device state and battery info on 5x5 LED matrix. This program works with 1 or 2 
 * gesture control remotes.  A button press on the gesture remote will change the 
 * variable, "deviceState", which can be used to run different routines.  Custom routines can be 
 * modified in the function, "triggertoAction()".
 *  Library dependencies: 
 *    M5Stack: https://github.com/m5stack/M5Stack
 *    M5Atom: https://github.com/m5stack/M5Atom
 *    FastLED: https://github.com/FastLED/FastLED
 *    BleCombo: https://github.com/blackketter/ESP32-BLE-Combo 
 *    
 *  Install ESP32 boards for IDE. Use board M5StickC and change partition scheme to minimal SPIFFS. 
 *    
 * Hackaday x UCPLA Dream Team, Kelvin Chow, October 22, 2020
 * https://hackaday.io/project/173454-2020-hdp-dream-team-ucpla   
 */

#include <M5Atom.h>
#include <esp_now.h>
#include <WiFi.h>
#include <BleCombo.h>
#include <EEPROM.h>

// CRGB colour codes for Atom Matrix Display
#define RED_DISPLAY 0x00F000    // Device off
#define GREEN_DISPLAY 0xF00000  // Nothing
#define BLUE_DISPLAY 0x0000F0   // Hold arrow keys (no vibration)
#define WHITE_DISPLAY 0xFFFFFF  // Home Assistant control / Roku
#define PURPLE_DISPLAY 0x00FFFF // Press and release arrow keys
#define YELLOW_DISPLAY 0xFFFF00 // Hold arrow keys (with vibration)
#define TEAL_DISPLAY 0xFF00FF   // Home Assistant/Roku secondary colour

// Variables to change for different configurations
const int numRemotes = 1; // number of gesture control remotes
const int numPosA = 5; // number of calibration positions for remote A
const int numAngles = 3; // number of angles (pitch, roll, yaw)
const int addressSize = 3; // for EEPROM, 3 addresses to store one float number
const int eepromSize = (numRemotes + numPosA) * numAngles * addressSize; // Number of addresses to read from memory
const int bluetoothResponseTime = 7;  //  7 x sending period (50 ms) = 350 ms delay before computer responds.  

// REPLACE WITH YOUR RECEIVER MAC Addresses
uint8_t broadcastAddressDevA[] = {0x, 0x, 0x, 0x, 0x, 0x};  //remote A MAC address
uint8_t broadcastAddressGateway[] = {0x, 0x, 0x, 0x, 0x, 0x};  //Atom Gateway MAC address
// Sample MAC addresses
// uint8_t broadcastAddressDevA[] = {0x50, 0x02, 0x91, 0x8C, 0xDC, 0xB8};  //remote A MAC address
// uint8_t broadcastAddressGateway[] = {0x50, 0x02, 0x91, 0x92, 0x31, 0x10};  //Atom Gateway MAC address

int serialRead = 0;  // Serial monitor logging. 0 = nothing, 1 = IMU signals, 2 = other stuff

// Gyroscope offsets that are either read from EEPROM or re-calibrated by user
float gyrXOffset[numRemotes];
float gyrYOffset[numRemotes];
float gyrZOffset[numRemotes];
float gyrSubtracted[numAngles];

// Variables for gyroscope calibration
float accumGyrX = 0;
float accumGyrY = 0;
float accumGyrZ = 0;
int countGyr = 0;
int gyrDevice;


// Variables for position re-calibration
int calibState = 0; // 0=nothing, 1=neutral, 2=left, 3=right, 4=up, 5=down
int calibNumber = 40; // Take average of 40 values for calibration, 2 seconds @ 20 Hz.
float calibVal[numPosA][numAngles];
int countTotal = 0;
int countCalib = 0;
bool calibrating = false;
float accumPitch = 0;
float accumRoll = 0;
float accumYaw = 0;
float newPitch;
float newRoll;
float newYaw;

// Angles
float pitch[numRemotes] = {0};
float roll[numRemotes] = {0};
float yaw[numRemotes] = {0};

// Variables to detect positions from angles
float err[numPosA];
int closestPos = 0;
int closestPosPrev = 0;
int keyCount = 0;


// Variables for Roku control
int subMenu = 0;  // 0=arrow keys, 10=submenu1, 20=submenu2(volume), 30 doesn't exist, 40=submenu3(play/rewind/FF)
int subMenuVal = 0; // 0=neutral, 1=left, 2 =right, 3=up, 4=down
int subMenuValTemp = 0;
int countBuzz = 0;

// Other variables
float batVoltage[numRemotes] = {0}; // On startup, don't show battery indicator to indicate no devices are connected
int deviceState = 0;   // 0=keyboard arrow keys w/o vibration, 1=keyboard arrow keys w/ vibration, 2=keyboard arrow keys hold down, 3=Roku Control, 4=Bluetooth mouse
bool deviceColour[numRemotes] = {false};
long timeStep[numRemotes];
long timeStart[numRemotes];

// Forward declerations
void drawLeft(int);
void drawRight(int);
void drawUp(int);
void drawDown();
void drawMiddle(int);
void drawHome(int);
void displayBattery(bool[numRemotes]);   

// Structure example to receive data
// Must match the sender structure
typedef struct struct_message {
  int id;     // Identify what to do with incoming data. 0 = IMU signals from device 1, 1 = IMU signals from device 2 (placeholder), 2 = change device state, 3 = set gyrscope offsets
  int state;  // Device stat
  float gX;   // IMU data
  float gY;
  float gZ;
  float aX;
  float aY;
  float aZ;
  float battery;  // Battery voltage of remote
  String message; // Messages to send to other devices
} struct_message;

// Create a struct_message called myData
struct_message myData;

void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
  memcpy(&myData, incomingData, sizeof(myData));    // Read data into myData variable
  if (myData.id == 2) {    // Process IMU signals from  gesture remote controllers
    countTotal += 1;      //Keep running total, not really necessary
    timeStep[0] = millis()-timeStart[0]; 
    gyrSubtracted[0] = myData.gX - gyrXOffset[0];
    gyrSubtracted[1] = myData.gY - gyrYOffset[0];
    gyrSubtracted[2] = myData.gZ - gyrZOffset[0];   
    imu_to_angle(myData.aX, myData.aY, myData.aZ, gyrSubtracted[0], gyrSubtracted[1], gyrSubtracted[2], &pitch[0], &roll[0], &yaw[0], timeStep[0]); // update angle values
    if (serialRead == 1){
      Serial.printf("%d, %d, %d, %4.2f, %4.2f, %4.2f, %1.3f, %1.3f, %1.3f, %3.1f, %3.1f, %3.1f \n",countTotal, timeStep[0], myData.id, gyrSubtracted[0], gyrSubtracted[1], gyrSubtracted[2], myData.aX, myData.aY, myData.aZ, pitch[0], roll[0], yaw[0]);
    }
    if ((calibState != 0) && (countCalib != 0)) {    // Check if we are in calibration mode
      // Keep running total
      countCalib -= 1;
      accumPitch += pitch[0];
      accumRoll += roll[0]; 
      accumYaw += yaw[0]; 
      if (serialRead == 2){
        Serial.printf("Count Calib: %d \n", countCalib);
      } 
      if (countCalib == 0) {
        // Calculate new totals and store new values to memory     
        newPitch = accumPitch/calibNumber;
        newYaw = accumYaw/calibNumber;
        newRoll = accumRoll/calibNumber;
        calibVal[calibState-1][0] = newPitch;
        calibVal[calibState-1][1] = newRoll;
        calibVal[calibState-1][2] = newYaw;
        EEPROM_writeFloat(((calibState-1) + (numRemotes*numAngles))*addressSize, newPitch);
        EEPROM_writeFloat(((calibState-1) + numPosA + (numRemotes*numAngles))*addressSize, newRoll);
        EEPROM_writeFloat(((calibState-1)+2*numPosA + (numRemotes*numAngles))*addressSize, newYaw);
        if (serialRead == 2) {
          Serial.printf("Remote ID: %d  Calibration Position: %d  Saved to Addresses: %d, %d, %d \n", myData.id, calibState-1, ((calibState-1) + (numRemotes*numAngles))*addressSize, ((calibState-1) + numPosA + (numRemotes*numAngles))*addressSize, ((calibState-1)+2*numPosA + (numRemotes*numAngles))*addressSize);
          Serial.printf("NEW PITCH: %3.1f  NEW ROLL: %3.1f  NEW YAW: %3.1f \n", newPitch, newRoll, newYaw);
        }
        // Reset values to zero
        accumPitch = 0;
        accumRoll = 0;   
        accumYaw = 0;
        // LED screen stuff
        switch (calibState) {
          case 0:
            break;
          case 1:
            drawLeft(1);
            break;
          case 2:
            drawRight(1);
            break;
          case 3:
            drawUp(1);
            break;          
          case 4:
            drawDown();
            break;                                                                                 
          case 5: // Reset
            drawHome(deviceState);
            calibState=0;
            calibrating=false;
            break;
          default:
            break;
        }
      }
    }
    triggerToAction(); // Mouse and keyboard actions
    timeStart[myData.id-2] = millis(); // Reset timer after every IMU-to-angle calculation
  }
  else if (myData.id == 0) {    // Change device state when buttons are pressed, update LED screen
      subMenu = 0;
      deviceState = myData.state;
      if (deviceState == 5) {  // device is off, battery indicator turns red.
        deviceColour[0] = false;
      }
      else {
        deviceColour[0] = true;
      }
      batVoltage[0] = myData.battery;
      drawHome(deviceState);
  }
  else if (myData.id == 1) {   //Set Gyro offsets
    if (myData.state == 0) {
      accumGyrX += myData.gX;
      accumGyrY += myData.gY;
      accumGyrZ += myData.gZ;
      countGyr += 1;
    }
    else {
      gyrXOffset[0] = accumGyrX/countGyr;
      gyrYOffset[0] = accumGyrY/countGyr;
      gyrZOffset[0] = accumGyrZ/countGyr;
      EEPROM_writeFloat(0, gyrXOffset[0]);
      EEPROM_writeFloat(3, gyrYOffset[0]);
      EEPROM_writeFloat(6, gyrZOffset[0]); 
      accumGyrX = 0;
      accumGyrY = 0;
      accumGyrZ = 0;
      countGyr = 0;     
    }
  }
}

// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (serialRead == 2) {
    Serial.print("\r\nLast Packet Send Status:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  }
}

void triggerToAction () { // Most important function for doing stuff including Bluetooth keyboard/mouse emulation and communicating with other ESP32 devices. Can be modified to do a bunch of other things.
  for (int i=0; i<numPosA; i++){ //0,1,2,3,4    //Calculate RMS error and find closest position
    err[i]=errorCalc(pitch[0], roll[0], yaw[0], calibVal[i][0], calibVal[i][1], calibVal[i][2]);
    if (err[i] < err[closestPos]){
      closestPos = i;
    }
  }
  if ((closestPos == closestPosPrev) && (closestPos != 0)){
    keyCount++;
  }
  else { //Change position or neutral position
    closestPosPrev = closestPos;
    keyCount=0;
  }
  if (deviceState != 3) {  // Games: deviceState=0 or 1 for maze, deviceState=2 for 2048
    if ((keyCount == 3) && (deviceState == 1) && (closestPos != 0)){   // Enable vibrations for maze game
      myData.message="BUZZ";
      esp_err_t result = esp_now_send(broadcastAddressDevA, (uint8_t *) &myData, sizeof(myData));
    }
    if (((deviceState != 2) && (keyCount%bluetoothResponseTime == 0)) || ((deviceState == 2) && (keyCount == bluetoothResponseTime))){
      switch (closestPos) {
        Keyboard.releaseAll();
        case 0:
          break;
        case 1:
          Keyboard.press(KEY_LEFT_ARROW);
          delay(100);
          Keyboard.releaseAll();            
          //Serial.println("PRESS LEFT");
          break;
        case 2:
          Keyboard.press(KEY_RIGHT_ARROW);
          delay(100);
          Keyboard.releaseAll();
          //Serial.println("PRESS RIGHT");
          break;
        case 3:
          Keyboard.press(KEY_UP_ARROW);
          delay(100);
          Keyboard.releaseAll();
          //Serial.println("PRESS UP");
          break;
        case 4:
          Keyboard.press(KEY_DOWN_ARROW);
          delay(100);
          Keyboard.releaseAll();
          //Serial.println("PRESS DOWN");
          break;
      }   
    }
  }
  else {  // Roku Control via gateway connected to Raspberry Pi/Home Assistant
    if ((keyCount == 10) || (keyCount == 40) || (keyCount == 80)) {
        myData.message = "BUZZ";
        esp_err_t result = esp_now_send(broadcastAddressDevA, (uint8_t *) &myData, sizeof(myData));  
        subMenuValTemp = closestPos;
        countBuzz += 1;    
    }
    else if ((countBuzz != 0) && (closestPos == 0)) { //Return back to neutral position and do stuff
      if (countBuzz == 1) {  // Single Click Functions
        subMenuVal = subMenu + subMenuValTemp;
        if (subMenuVal == 1) {
          rokuSend("LEFT");
          flashLeft();   
        }   
        else if (subMenuVal == 2) {
          rokuSend("RIGHT");
          flashRight();   
        }   
        else if (subMenuVal == 3) {
          rokuSend("UP");
          flashUp();   
        }
        else if (subMenuVal == 4) {
          rokuSend("DOWN");
          flashDown();   
        }
        else if (subMenuVal == 11) {
          rokuSend("HOME");
          flashLeft();   
        }
        else if (subMenuVal == 12) {
          rokuSend("BACK");
          flashRight();   
        }
        else if (subMenuVal == 13) {
          rokuSend("ASTERISK");
          flashUp();   
        }  
        else if (subMenuVal == 23) {
          rokuSend("VOLUP");
          flashUp();   
        }
        else if (subMenuVal == 24) {
          rokuSend("VOLDOWN");
          flashDown();   
        }
        else if (subMenuVal == 41) {
          rokuSend("RWD");
          flashLeft();   
        }
        else if (subMenuVal == 42) {
          rokuSend("FWD");
          flashRight();   
        }
        else if (subMenuVal == 43) {
          rokuSend("PLAY");
          flashUp();   
        } 
        else {    // Invalid message, longer buzz
          myData.message = "INVALID";
          flashSelect(false);
          esp_err_t result = esp_now_send(broadcastAddressDevA, (uint8_t *) &myData, sizeof(myData)); 
        }                                                             
      }
      else if (countBuzz == 2) {  // Double buzz functions occur between 2-4 seconds
        if (subMenuValTemp == 1) {  // Left sub menu 
          subMenu = 10;
          flashSub1();
          myData.message = "SUB1";
          esp_err_t result = esp_now_send(broadcastAddressDevA, (uint8_t *) &myData, sizeof(myData));  
        }
        else if (subMenuValTemp == 2) {  // Right sub menu (volume) 
          subMenu = 20;
          flashSub2();
          myData.message = "SUB2";
          esp_err_t result = esp_now_send(broadcastAddressDevA, (uint8_t *) &myData, sizeof(myData));  
        }
        else if (subMenuValTemp == 3) {  // UP
          rokuSend("SELECT");
          flashSelect(true);          
        }
        else if (subMenuValTemp == 4) {  // Down sub menu (press/play/rewind/FF) 
          subMenu = 40;
          flashSub4();
          myData.message = "SUB4";
          esp_err_t result = esp_now_send(broadcastAddressDevA, (uint8_t *) &myData, sizeof(myData));  
        } 
        else {    // Invalid message, longer buzz
          myData.message = "INVALID";
          flashSelect(false);
          esp_err_t result = esp_now_send(broadcastAddressDevA, (uint8_t *) &myData, sizeof(myData)); 
        }        
      }
      else if (countBuzz == 3) {  // Triple Buzz function occurs between 4-6 seconds. Only one function to reset
        if (subMenuValTemp == 4) {  // Reset menu level back to main
          subMenu = 0;
          drawHome(deviceState);
          myData.message = "SUB0";
          esp_err_t result = esp_now_send(broadcastAddressDevA, (uint8_t *) &myData, sizeof(myData));  
        }
      }
      else {    // Invalid message, longer buzz
        myData.message = "INVALID";
        flashSelect(false);
        esp_err_t result = esp_now_send(broadcastAddressDevA, (uint8_t *) &myData, sizeof(myData)); 
      }
      countBuzz = 0;
    }
    else if ((keyCount > 120) && (keyCount%7 == 0)) {  // "Rapid scroll" functions. occurs after 6 seconds for select keys.
      subMenuVal = subMenu + closestPos;
      if (subMenuVal == 1) {
        rokuSend("LEFT");
        flashLeft();   
      }   
      else if (subMenuVal == 2) {
        rokuSend("RIGHT");
        flashRight();   
      }   
      else if (subMenuVal == 3) {
        rokuSend("UP");
        flashUp();   
      }
      else if (subMenuVal == 4) {
        rokuSend("DOWN");
        flashDown();   
      }
      else if (subMenuVal == 23) {
        rokuSend("VOLUP");
        flashUp();   
      }
      else if (subMenuVal == 24) {
        rokuSend("VOLDOWN");
        flashDown();   
      }
      countBuzz = 0;      
    }
  }
}

void rokuSend (String msg) {  //void rokuSend (String msg, uint8_t ky) {
  myData.message = msg;
  esp_now_send(broadcastAddressDevA, (uint8_t *) &myData, sizeof(myData));       // Optional feedback message for gesture control remote
  //cmdData.key = ky;
  //esp_now_send(broadcastAddressIR, (uint8_t *) &cmdData, sizeof(cmdData));     // For IR blaster         
  esp_now_send(broadcastAddressGateway, (uint8_t *) &myData, sizeof(myData));     // For atom gateway for home assistant           
}

void imu_to_angle(float aX, float aY, float aZ, float gX, float gY, float gZ, float *pitch, float *roll, float *yaw, long dt)   //Update pitch, roll, yaw angles
{
  // These aren't technically pitch, roll, and yaw.  Yaw can't be calculated with a 6 DOF IMU, but having these 3 angles are helpful for distingusihing tilt orientation
  // The pitch angle is the only correct angle. The other two angles are using the same formula with swapped axes.
  // Complementary filter used for converting IMU signals to angles (More info: https://www.pieter-jan.com/node/11)
  // Hackaday log details: https://hackaday.io/project/173454-2020-hdp-dream-team-ucpla/log/181717-imu-joysticks-v2-calibration-routines-and-flag-semaphore
  float w = 0;
  float accP = atan2(aX, sqrt(aY*aY + aZ*aZ)) * 180/M_PI;
  float accR = atan2(aY, sqrt(aX*aX + aZ*aZ)) * 180/M_PI;
  float accY = atan2(aZ, sqrt(aX*aX + aY*aY)) * 180/M_PI;
  float accMag = abs(aX) + abs(aY) + abs(aZ);
  if ((accMag < 2) && (accMag > 0.5)){
    w = 0.15;     //This weight factor adjusts between the acceleration and gyroscope influence.  Higher the weight, the more accelerometer plays a role. Set w between 0 and 1, but usually this is a low number under 0.1.  
  }
  *pitch = (-gY*dt/1000 + *pitch)*(1-w) + accP*w;
  *roll = (gX*dt/1000 + *roll)*(1-w) + accR*w;
  *yaw = (gZ*dt/1000 + *yaw)*(1-w) + accY*w;
}

void EEPROM_writeFloat(int startAddress, float number){   // Convert float to 3 integer numbers to store in memory.  There might be a better way to do this. These EEPROM functions for type int.
  float dec = number - int(number);
  if (number >= 0){  // Store positive or negative for first address
    EEPROM.write(startAddress,2);
  }
  else {
    EEPROM.write(startAddress,0);
  }
  EEPROM.write(startAddress+1, abs(int(number))); // Store number before decimal point in second address
  EEPROM.write(startAddress+2, abs(int(dec*100))); // Store number after decimal point in third address
  EEPROM.commit();
}

float EEPROM_readFloat(int startAddress){   //Read one float number from 3 stored integer values from memory
  int sign=EEPROM.read(startAddress);
  int num1=EEPROM.read(startAddress+1);
  float num2=float(EEPROM.read(startAddress+2));
  return (num1 + num2/100)*(sign-1);
}

float errorCalc(float pitch, float roll, float yaw, float pitchCal, float rollCal, float yawCal){   //RMS error calculation
  return sqrt(pow((pitch - pitchCal), 2) + pow((roll - rollCal), 2)+pow((yaw - yawCal), 2)) / 2;
}

void drawHome(int colourState) {  // for each deviceState, display a different colour on the LED screen.
  CRGB colour;
  switch (colourState) {
    case 0:
      colour = BLUE_DISPLAY;
      break;
    case 1:
      colour = YELLOW_DISPLAY;
      break;
    case 2:
      colour = PURPLE_DISPLAY;
      break;
    case 3:
      colour = WHITE_DISPLAY;
      break;
    case 4: 
      colour = GREEN_DISPLAY;
      break;
    case 5:
      colour = RED_DISPLAY;
      break;     
  }  
  // Clear current display and update
   M5.dis.clear();  
       for (int i=1;i<=3;i++){
         for (int j=1;j<=3;j++){
           M5.dis.drawpix(i,j, colour);
         }
       }
  if (subMenu == 10) {
    flashSub1();
  }
  else if (subMenu == 20) {
    flashSub2();
  }
  else if (subMenu == 40) {
    flashSub4();
  }
   displayBattery(deviceColour);
}

void displayBattery(bool colBool[numRemotes]) {   // Update battery display anytime device changes states.  Battery is estimated from voltage level.
  if (serialRead == 2){
    Serial.printf("Battery 1: %1.3f  Battery 2: %1.3f  Time: %d \n", batVoltage[0], batVoltage[1], millis()/1000);
  }
  int batLevel[numRemotes];
  for (int z=0; z<numRemotes; z++) {
    if (batVoltage[z] < 3) {  // Display nothing on startup
      batLevel[z] = -1;
    }
    else if (batVoltage[z] < 3.5){
      batLevel[z] = 0;
    }
    else if (batVoltage[z] < 3.575){
      batLevel[z] = 1;
    }
    else if (batVoltage[z] < 3.675){
      batLevel[z] = 2;
    }
    else if (batVoltage[z] < 3.85) {
      batLevel[z] = 3;
    }
    else {
      batLevel[z] = 4;
    }
    CRGB colour;
    if (colBool[z]) {
      colour  = GREEN_DISPLAY;
    }
    else {
      colour = RED_DISPLAY;
    }
    for (int i=0; i<=batLevel[z]; i++){
      M5.dis.drawpix(i,z*4, colour);    //Remote A (DEV2) battery indicator is at top and Remote B (DEV3) indicator is at bottom of scren
    }
  } 
}

// Draw screens (arrows) for position calbiration
void drawMiddle(int col) {
  CRGB colour;
  if (col == 1) {
    colour = GREEN_DISPLAY;
  }
  else {
    colour = PURPLE_DISPLAY;
  }
  M5.dis.clear(); 
  M5.dis.drawpix(1,2, colour);
  M5.dis.drawpix(2,1, colour);
  M5.dis.drawpix(3,2, colour);
  M5.dis.drawpix(2,3, colour);  
}
void drawLeft(int col) {
  CRGB colour;
  if (col == 1) {
    colour = GREEN_DISPLAY;
  }
  else {
    colour = PURPLE_DISPLAY;
  }  
  M5.dis.clear(); 
  M5.dis.drawpix(1,1, colour);
  M5.dis.drawpix(0,2, colour);
  M5.dis.drawpix(1,3, colour);
}
void drawRight(int col) {
  CRGB colour;
  if (col == 1) {
    colour = GREEN_DISPLAY;
  }
  else {
    colour = PURPLE_DISPLAY;
  }  
  M5.dis.clear(); 
  M5.dis.drawpix(3,1, colour);
  M5.dis.drawpix(4,2, colour);
  M5.dis.drawpix(3,3, colour);
}
void drawUp(int col) {
  CRGB colour;
  if (col == 1) {
    colour = GREEN_DISPLAY;
  }
  else {
    colour = PURPLE_DISPLAY;
  }  
  M5.dis.clear(); 
  M5.dis.drawpix(1,1, colour);
  M5.dis.drawpix(2,0, colour);
  M5.dis.drawpix(3,1, colour);
}
void drawDown() {
  M5.dis.clear(); 
  M5.dis.drawpix(3,3, GREEN_DISPLAY);
  M5.dis.drawpix(2,4, GREEN_DISPLAY);
  M5.dis.drawpix(1,3, GREEN_DISPLAY);
}

void flashSub1() {
  for (int j=1;j<=3;j++){
    M5.dis.drawpix(1,j, TEAL_DISPLAY);
  }
}

void flashSub2() {
  for (int j=1;j<=3;j++){
    M5.dis.drawpix(3,j, TEAL_DISPLAY);
  }
}

void flashSub4() {
  for (int i=1;i<=3;i++){
    M5.dis.drawpix(i,3, TEAL_DISPLAY);
  }
}

void flashSelect(bool green) {
  if (green) {
    drawHome(4);
  }
  else {
    drawHome(5);
  }
  delay(500);
  drawHome(deviceState);
}

void flashLeft() {
  M5.dis.drawpix(1,2,GREEN_DISPLAY);
  delay(500);
  drawHome(deviceState);
}

void flashRight() {
  M5.dis.drawpix(3,2,GREEN_DISPLAY);
  delay(500);
  drawHome(deviceState);
}

void flashUp() {
  M5.dis.drawpix(2,1,GREEN_DISPLAY);
  delay(500);
  drawHome(deviceState);
}

void flashDown() {
  M5.dis.drawpix(2,3,GREEN_DISPLAY);
  delay(500);
  drawHome(deviceState);
}


void setup() {
// Initilize stuff
  M5.begin(true, false, true);  // From M5 docs
  Serial.begin(115200);
  EEPROM.begin(eepromSize);
  Keyboard.begin();
  Mouse.begin();
  
// ESP-NOW setup
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
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  // register peers  
  memcpy(peerInfo.peer_addr, broadcastAddressGateway, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
  memcpy(peerInfo.peer_addr, broadcastAddressDevA, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }    
  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info
  esp_now_register_recv_cb(OnDataRecv); 
            
//Read stored values
  for (int i=0; i<numPosA; i++){ //0,1,2,3,4
    for (int j=0; j<numAngles; j++){ //0,1,2
      calibVal[i][j]=EEPROM_readFloat((i+(j*numPosA) + (numRemotes*numAngles))*addressSize); 
      if (serialRead == 2) { 
        Serial.printf("Address: %d  Calibration Value: %2.2f \n", (i+(j*numPosA) + (numRemotes*numAngles))*addressSize, calibVal[i][j]);
      }
    }
  }
// Read gyro offsets from memory.  For the first remote, gyroscope offset values are stored from address 0 to 8. Second remote is 9-17 and so on. 
  for (int i=0; i<numRemotes; i++) {
    gyrXOffset[i]=EEPROM_readFloat(0 + 9*i);
    gyrYOffset[i]=EEPROM_readFloat(3 + 9*i);
    gyrZOffset[i]=EEPROM_readFloat(6 + 9*i);
  }
  if (serialRead == 2) { 
    Serial.printf("gyrXOffA: %2.2f  gyrYOffA: %2.2f  gyrZOffA: %2.2f\n", gyrXOffset[0], gyrYOffset[0], gyrZOffset[0]);
    Serial.printf("gyrXOffB: %2.2f  gyrYOffB: %2.2f  gyrZOffB: %2.2f\n", gyrXOffset[1], gyrYOffset[1], gyrZOffset[1]);
  }
  if (serialRead == 1){
    Serial.println("#, time(ms), device ID, gX (deg/s), gY (deg/s), gZ (deg/s), aX (g), aY (g), aZ (g), pitch (deg), roll (deg), yaw (deg)");
  }
  timeStart[0] = millis();
  drawHome(5);  // Display red screen to start
}

void loop()
{  
  if (M5.Btn.wasPressed()) {  // Start calibration mode if the Atom matrix button is pressed.  Keep pressing button to calibrate different positions.
      if (!calibrating){ 
        drawMiddle(1);
        calibrating = true;
      }
      else {
        calibState++;
        countCalib = calibNumber;   // Start countdown counter
        M5.dis.clear();
      }
      delay(100);
  }
  M5.update();
}

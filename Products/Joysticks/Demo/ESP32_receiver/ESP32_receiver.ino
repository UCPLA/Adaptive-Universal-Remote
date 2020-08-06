#include <esp_now.h>
#include <WiFi.h>
#include <BleCombo.h>
#include <EEPROM.h>

#define EEPROM_SIZE 54

float gyrXOffset;
float gyrYOffset;
float gyrZOffset;

float accumGyrX=0;
float accumGyrY=0;
float accumGyrZ=0;
int countGyr=0;

int gameState=0;   //0 = arrow keys hold, 1=arrow keys once, 2=side keys + spacebar, 3 =mouse hold
int calibState=0; //0=no calibration, 1=neutral, 2=left, 3=right, 4=up, 5=down
int calibNumber=40; //Take 40 values for calibration
const int numRows=5;  //Store 5 calibration values
const int numCols=3;  //Store 3 angles: pitch, roll, yaw
const int sizeAddress=3;  //use 3 bytes to store a float number in memory
float calibVal[numRows][numCols];
float accumPitch=0;
float accumRoll=0;
float accumYaw=0;
float newPitch;
float newRoll;
float newYaw;

float pitch=0;
float roll=0;
float yaw=0;
float err[numRows]={10000,1000,1000,1000,1000};  //Not sure if initialization is necessary.
int closestPos=0;
int closestPosPrev=0;
int keyCount=0;

int countTotal=0;
int countCalib=0;
long timeStep;
long timeStart;


// Structure example to receive data
// Must match the sender structure
typedef struct struct_message {
  int id;     //Board ID
  int state;  //Calibration state
  float gX;   //IMU data
  float gY;
  float gZ;
  float aX;
  float aY;
  float aZ;
} struct_message;

// Create a struct_message called myData
struct_message myData;

struct_message board1;    //IMU data from first board
struct_message board2;    //IMU data from second board
struct_message board3;    //Pseudo third board for sending calibration data from second board
struct_message board4;    //Pseudo fourth board for changing game mode from second board
struct_message board5;    //Pseudo fifth board for setting gyro offset value from first board
struct_message boardsStruct[5] = {board1, board2, board3, board4, board5};

// callback function that will be executed when data is received
void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
  memcpy(&myData, incomingData, sizeof(myData));    //Read data into myData variable
  
  boardsStruct[myData.id].state=myData.state;

  if (myData.id==0){    //Only process IMU signals from first board
    boardsStruct[myData.id].aX = myData.aX;
    boardsStruct[myData.id].aY = myData.aY;
    boardsStruct[myData.id].aZ = myData.aZ;
    boardsStruct[myData.id].gX = myData.gX-gyrXOffset;
    boardsStruct[myData.id].gY = myData.gY-gyrYOffset;
    boardsStruct[myData.id].gZ = myData.gZ-gyrZOffset;
    countTotal+=1;      //Keep running total, not really necessary
    timeStep=millis()-timeStart;
    imu_to_angle(boardsStruct[myData.id].aX, boardsStruct[myData.id].aY, boardsStruct[myData.id].aZ,boardsStruct[myData.id].gX, boardsStruct[myData.id].gY, boardsStruct[myData.id].gZ, &pitch, &roll, &yaw, timeStep);
    Serial.printf("%d, %d, %4.2f, %4.2f, %4.2f, %1.3f, %1.3f, %1.3f, %3.1f, %3.1f, %3.1f \n",countTotal, timeStep, boardsStruct[0].gX, boardsStruct[0].gY, boardsStruct[0].gZ, boardsStruct[0].aX, boardsStruct[0].aY, boardsStruct[0].aZ, pitch, roll, yaw);
    //Serial.println(countCalib);
    if (calibState !=0 && countCalib>0){    //Check if we are trying to calibrate IMUs
      countCalib-=1;
      accumPitch+=pitch;
      accumRoll+=roll; 
      accumYaw+=yaw;    
      if (countCalib==0){
        newPitch=accumPitch/calibNumber;
        newYaw=accumYaw/calibNumber;
        newRoll=accumRoll/calibNumber;
        calibVal[calibState-1][0]=newPitch;
        calibVal[calibState-1][1]=newRoll;
        calibVal[calibState-1][2]=newYaw;
        EEPROM_writeFloat((calibState-1)*sizeAddress,newPitch);
        //Serial.println((calibState-1)*sizeAddress);
        EEPROM_writeFloat((calibState-1+numRows)*sizeAddress,newRoll);
        //Serial.println((calibState-1+numRows)*sizeAddress);
        EEPROM_writeFloat((calibState-1+2*numRows)*sizeAddress,newYaw);
        //Serial.println((calibState-1+2*numRows)*sizeAddress);
        //Serial.printf("CALIIBRATION PITCH: %3.1f  CALIBRATION ROLL: %3.1f \n", newPitch, newRoll);
        accumPitch=0;
        accumRoll=0;   
        accumYaw=0;     
      }
    }
  }
  else if (myData.id==2) {    //On button click, trigger calibration mode
    //Calibration routine
      calibState=int(boardsStruct[myData.id].state);
      countCalib=calibNumber;  //Take average of 40 values
      //Serial.println(calibState);
  }
  else if (myData.id==3) {    //Change game mode
      gameState=int(boardsStruct[myData.id].state);
  }
  else if (myData.id==4){   //Set Gyro offsets
    if (myData.state==0){
      accumGyrX += myData.gX;
      accumGyrY += myData.gY;
      accumGyrZ += myData.gZ;
      countGyr +=1 ;
    }
    else {
      gyrXOffset = accumGyrX/countGyr;
      gyrYOffset = accumGyrY/countGyr;
      gyrZOffset = accumGyrZ/countGyr;
      EEPROM_writeFloat(45,gyrXOffset);
      EEPROM_writeFloat(48,gyrYOffset);
      EEPROM_writeFloat(51,gyrZOffset);      
    }
  }
  timeStart=millis();
}

void imu_to_angle(float aX, float aY, float aZ, float gX, float gY, float gZ, float *pitch, float *roll, float *yaw, long dt)   //Update pitch, roll, yaw angles
{
  //These aren't technically pitch, roll, and yaw.  Yaw can't be calculated with 6 DOF IMUs but not having this "yaw" angle makes it more difficult to distinguish positions
  float w=0;
  float accP=atan2(aX,sqrt(aY*aY+aZ*aZ))*180/M_PI;
  //float accR=atan2(aY,aZ)*180/M_PI;
  float accR=atan2(aY,sqrt(aX*aX+aZ*aZ))*180/M_PI;
  float accY=atan2(aZ,sqrt(aX*aX+aY*aY))*180/M_PI;
  float accMag=abs(aX)+abs(aY)+abs(aZ);
  if (accMag<2 && accMag>0.5){
    w=0.15;     //This weight factor adjusts between the acceleration and gyroscope influence.  Higher the weight, the more accelerometer plays a role. From 0-1, but usually this is a low number under 0.1.  
  }
  *pitch = (-gY*dt/1000 + *pitch)*(1-w) + accP*w;
  *roll = (gX*dt/1000 + *roll)*(1-w) + accR*w;
  *yaw = (gZ*dt/1000 + *yaw)*(1-w) + accY*w;
}

void EEPROM_writeFloat(int startAddress, float number){   //Convert float to 3 integer numbers to store in memory.  There might be a better way to do this.
  float dec=number-int(number);
  if (number>=0){
    EEPROM.write(startAddress,2);
  }
  else {
    EEPROM.write(startAddress,0);
  }
  EEPROM.write(startAddress+1,abs(int(number))); 
  EEPROM.write(startAddress+2,abs(int(dec*100)));
  EEPROM.commit();
}

float EEPROM_readFloat(int startAddress){   //Read a float from 3 stored integer values from memory
  int sign=EEPROM.read(startAddress);
  int num1=EEPROM.read(startAddress+1);
  float num2=float(EEPROM.read(startAddress+2));
  return (num1+num2/100)*(sign-1);
}

float errorCalc(float pitch, float roll, float yaw, float pitchCal, float rollCal, float yawCal){   //RMS error calculation
  return sqrt(pow((pitch-pitchCal),2)+pow((roll-rollCal),2)+pow((yaw-yawCal),2))/2;
}

 
void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);

  Keyboard.begin();
  // Mouse.begin();
// Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  /*
  //Write gyroscope offsets to memory
  EEPROM_writeFloat(45,14.57);
  EEPROM_writeFloat(48,-12.42);
  EEPROM_writeFloat(51,3.19);
  */
  
  Serial.println("#, time(ms),gX (deg/s), gY (deg/s), gZ (deg/s), aX (g), aY (g), aZ (g), pitch (deg), roll (deg), yaw (deg)");
  timeStart=millis();
  //Read stored values
  for (int i=0; i<numRows; i++){ //0,1,2,3,4
    for (int j=0; j<numCols; j++){ //0,1,2
      calibVal[i][j]=EEPROM_readFloat((i+(j*numRows))*3);  
      //Serial.println((i+(j*numRows))*3);  
      //Serial.println(calibVal[i][j]);
    }
  }
  gyrXOffset=EEPROM_readFloat(45);
  gyrYOffset=EEPROM_readFloat(48);
  gyrZOffset=EEPROM_readFloat(51);

  //Serial.println(gyrXOffset);
  //Serial.println(gyrYOffset);
  //Serial.println(gyrZOffset);
  
  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info
  esp_now_register_recv_cb(OnDataRecv);
}
 
void loop() {
  //Serial.println(abs(pitch)+abs(roll)+abs(yaw));
    if (abs(pitch)+abs(roll)+abs(yaw)!=0){
      for (int i=0; i<numRows; i++){ //0,1,2,3,4    //Calculate RMS error and find closest position
          err[i]=errorCalc(pitch, roll, yaw, calibVal[i][0], calibVal[i][1], calibVal[i][2]);
          if (err[i]<err[closestPos]){
            closestPos = i;
          }
      }    
      
      if (closestPos==closestPosPrev){
        Keyboard.releaseAll();
        //Mouse.release();
        keyCount+=1;
      }
      else{
        closestPosPrev=closestPos;
        keyCount=0;
      }
  
      if (gameState==0){      //Maze Game
        if ((keyCount%3==0)  && (keyCount!=6)){
          switch (closestPos) {
            Keyboard.releaseAll();
            case 0:
              break;
            case 1:
              Keyboard.releaseAll();
              Keyboard.press(KEY_LEFT_ARROW);
              //Serial.println("PRESS LEFT");
              break;
            case 2:
              Keyboard.releaseAll();
              Keyboard.press(KEY_RIGHT_ARROW);
              //Serial.println("PRESS RIGHT");
              break;
            case 3:
              Keyboard.releaseAll();
              Keyboard.press(KEY_UP_ARROW);
              //Serial.println("PRESS UP");
              break;
            case 4:
              Keyboard.releaseAll();
              Keyboard.press(KEY_DOWN_ARROW);
              //Serial.println("PRESS DOWN");
              break;
          }   
        }
      }
      else if (gameState==1){     //Pacman/Snake
          if (keyCount==4){
          switch (closestPos) {
            Keyboard.releaseAll();
            case 0:
              break;
            case 1:
              Keyboard.releaseAll();
              Keyboard.press(KEY_LEFT_ARROW);
              //Serial.println("PRESS LEFT");
              break;
            case 2:
              Keyboard.releaseAll();
              Keyboard.press(KEY_RIGHT_ARROW);
              //Serial.println("PRESS RIGHT");
              break;
            case 3:
              Keyboard.releaseAll();
              Keyboard.press(KEY_UP_ARROW);
              //Serial.println("PRESS UP");
              break;
            case 4:
              Keyboard.releaseAll();
              Keyboard.press(KEY_DOWN_ARROW);
              //Serial.println("PRESS DOWN");
              break;
          }   
        }
      }
      else if (gameState==2){     //Space Invaders
         if ((keyCount%2==0)){
          switch (closestPos) {
            Keyboard.releaseAll();
            case 0:
              break;
            case 1:
              Keyboard.releaseAll();
              Keyboard.press(KEY_LEFT_ARROW);
              //Serial.println("PRESS LEFT");
              break;
            case 2:
              Keyboard.releaseAll();
              Keyboard.press(KEY_RIGHT_ARROW);
              //Serial.println("PRESS RIGHT");
              break;
            case 3:
              Keyboard.releaseAll();
              Keyboard.press(32);  
              //Serial.println("PRESS SPACEBAR");
              break;
            case 4:
              Keyboard.releaseAll();
              break;
          }   
        }
      }
      else {
          switch (closestPos) {
              case 0:
                Keyboard.releaseAll();
                break;
              case 1:
                Keyboard.press(32);  
                delay(10);
                Keyboard.releaseAll();
                //Serial.println("Hold Mouse click");
                break;
              case 2:
                Keyboard.releaseAll();
                //Serial.println("Release Mouse click");
                break;
              case 3:
                Keyboard.releaseAll();
                break;
              case 4:
                Keyboard.releaseAll();
                break;
          } 
      }
      delay(100);
    } 
}

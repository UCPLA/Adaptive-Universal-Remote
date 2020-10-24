/**
 * On every ESP32-based device, run this code to get the board's MAC address.
 * Install ESP32 board in IDE: https://randomnerdtutorials.com/installing-the-esp32-board-in-arduino-ide-windows-instructions/
 * 
 * Instructions:
 *    1) Upload this code, and open the serial monitor to get the board's MAC address.
 *       It should be in the format of xx:xx:xx:xx:xx:xx
 *    2) Save this address somewhere as it is important for ESP-NOW communication between boards.
 *       This address will be needed in other firmware programs.
 * 
 * Hackaday x UCPLA Dream Team, Kelvin Chow, October 3, 2020
 * https://hackaday.io/project/173454-2020-hdp-dream-team-ucpla
 */

#include "WiFi.h"
 
void setup(){
  Serial.begin(115200);
  WiFi.mode(WIFI_MODE_STA);
  Serial.println(WiFi.macAddress());
}
 
void loop(){
    Serial.println(WiFi.macAddress());
    delay(5000);
}

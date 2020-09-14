# Firmware installation instructions

1. Install [ESP32 Library](https://randomnerdtutorials.com/installing-the-esp32-board-in-arduino-ide-windows-instructions/) in Arduino IDE

2. Install [Bluetooth keyboard and mouse library](https://github.com/blackketter/ESP32-BLE-Combo) in Arduino IDE

3. Upload _ESP32\_get\_receiver\_mac\_address.ino_ to the Atom. 
	* Board: ESP32 dev
	* Upload speed: 115200

4. After uploading, open the serial monitor, reset the Atom, and copy the MAC address from the serial monitor, which will be need to copy to other sketches.

5. Upload _ESP32\_receiver.ino_ to the Atom.
	* Before uploading, change partition scheme to `minimal spiffs (1.9 MB App with OTA...)`

6. Make sure the Atom is left on (it doesn't need to be plugged into a computer).

7. Open _M5StickC\_set\_gyro.ino_ sketch and set the MAC address (beginning of the sketch) as saved in step 3.

8. Upload _M5StickC\_set\_gyro.ino_ sketch to one of the M5StickCs (Joystick 1)

9. Make sure the M5StickC is on a flat level surface and press the big M5 button. Do not touch the joystick until it says it's finished (5 seconds). 
	* This stores the gyroscope offsets into flash memory of the Atom.

10. Open _M5StickC\_sender\_joystick.ino_ sketch and set the MAC address (beginning of the sketch) as saved in step 3.

11. Now upload _M5StickC\_sender\_joystick.ino_ sketch to the same M5StickC used in step 8. 

12. Open _M5StickC\_sender\_calibration.ino_ sketch and set the MAC address (beginning of the sketch) as saved in step 3.

13. Upload _M5StickC\_sender\_calibration.ino_ to another M5StickC.

Now you can continue with the examples in the readme.md
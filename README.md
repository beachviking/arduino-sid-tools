# arduino-sid-tools
Various SID tools for Arduino(mainly ESP32 and STM32), based off the original ReSID implementation by Dag Lem.

## Installation in Arduino
You can download the library as a zip and select Include Library -> zip library in the Arduino IDE. Or you can git clone this project into the Arduino libraries folder e.g. with

```
cd  ~/Documents/Arduino/libraries
git clone https://github.com/beachviking/arduino-sid-tools.git
```

I recommend to use git because you can easily update to the latest version just by executing the ```git pull``` command in the project folder.

## Dependencies
The library and the examples rely on the following libraries to work properly:
  - https://github.com/pschatzmann/arduino-audio-tools
  - https://github.com/pschatzmann/arduino-audiokit
  - https://github.com/adafruit/Adafruit-GFX-Library 
  - https://github.com/adafruit/Adafruit_SSD1306
  - https://github.com/greiman/SdFat


As hardware, various confugurations have been used:
- AI ESP32 Audio Kit V2.2 from AliExpress, which uses I2S to deliver audio signals.
- STM32 Nucleo G0B1RE with a custom external low pass filter, driven by a PWM signal.

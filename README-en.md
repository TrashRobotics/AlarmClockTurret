<h1 align="center">
  <a href="https://youtu.be/pdIlyJ9fpZc"><img src="https://github.com/TrashRobotics/AlarmClockTurret/blob/main/img/alarm_clock_turret.jpg" alt="Turret alarm clock" width="800"></a>
  <br>
   Automatic alarm clock turret 
  <br>
</h1>

<p align="center">
  <a href="https://github.com/TrashRobotics/AlarmClockTurret/blob/main/README.md">Русский(Russian)</a> •
  <a href="https://github.com/TrashRobotics/AlarmClockTurret/blob/main/README-en.md">English</a> 
</p>

# Description
Automatic water turret alarm clock

# Main parts
* esp8266 NodeMCU v3;
* 2 x 18650 Batteries and battery compartment for them;
* SSD1306 128x64 I2C display;
* PCA9685 module;
* DS1302 real time clock;
* LM2596 step down dc/dc converter;
* 2 x MG995 Servos;
* 2 x HW-517 or other transistor switch;
* water pump;
* speaker;
* silicone tube D=10mm, d=7mm;
* 2 x plywood 150x150x6 mm for the base; (+ second_link_base.stl can also be cut from plywood)
* toggle switch;
* HC-SR501 motion sensor;
* breadboard (optional);
* [links, body, etc.] (https://www.thingiverse.com/thing:5167743)

### Fasteners
* Self-tapping screw DIN7982 3.5x9.5	x2;
* Self-tapping screw DIN7981 2.2x9.5	x20;
* Self-tapping screw DIN7981 2.9x9.5	x14;
* Screw DIN7985 M3x25		x1;
* Screw DIN7985 M3x16		x6;
* Screw DIN7985 M3x12		x21;
* Screw DIN965  M6x60		x4;
* Nut DIN934 M3x0.5		    x28;
* Nut DIN934 M6x5			x4;

# Wiring diagram
![Wiring diagram](https://github.com/TrashRobotics/AlarmClockTurret/blob/main/img/schematic.png)

# Sketch
Sketch for NodeMCU: **alarm_clock_turret/alarm_clock_turret.ino**
In the same folder there is a resource file (images, html pages, etc.): **sources.h** and
robot voice files in Russian **sounds_ru.h** and English **sounds_en.h** languages.
For more details watch the video.


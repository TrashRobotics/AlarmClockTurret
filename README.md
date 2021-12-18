<h1 align="center">
  <a href="https://youtu.be/pdIlyJ9fpZc"><img src="https://github.com/TrashRobotics/AlarmClockTurret/blob/main/img/alarm_clock_turret.jpg" alt="Турель-будильник" width="800"></a>
  <br>
    Автоматическая турель-будильник
  <br>
</h1>

<p align="center">
  <a href="https://github.com/TrashRobotics/AlarmClockTurret/blob/main/README.md">Русский</a> •
  <a href="https://github.com/TrashRobotics/AlarmClockTurret/blob/main/README-en.md">English(Английский)</a> 
</p>

# Описание проекта
Автоматическая водная турель-будильник

# Основные детали
* esp8266 NodeMCU v3;
* 2 x 18650 Аккумуляторы и батарейный отсек для них
* SSD1306 128x64 I2C дисплей;
* PCA9685 модуль;
* DS1302 часы реального времени;
* LM2596 понижающий dc/dc преобразователь;
* 2 x MG995 сервопривода;
* 2 x HW-517 или другой транзисторный ключ;
* водяной насос;
* динамик;
* силиконовая трубка D=10мм, d=7мм;
* 2 x фанерки 150x150x6 мм для базы; (+ базу второго звена тоже можно вырезать из фанеры)
* тумблер;
* HC-SR501 датчик движения;
* макетная плата (по желанию);
* [звенья, корпус и т.д.](https://www.thingiverse.com/thing:5167743)

### Крепеж
* Саморез DIN7982 3.5x9.5	x2;
* Саморез DIN7981 2.2x9.5	x20;
* Саморез DIN7981 2.9x9.5	x14;
* Винт DIN7985 M3x25		x1;
* Винт DIN7985 M3x16		x6;
* Винт DIN7985 M3x12		x21;
* Винт DIN965  M6x60		x4;
* Гайка DIN934 M3x0.5		x28;
* Гайка DIN934 M6x5			x4;

# Схема подключения
![Схема подключения](https://github.com/TrashRobotics/AlarmClockTurret/blob/main/img/schematic.png)

# Прошивка
Прошивка для NodeMCU: **alarm_clock_turret/alarm_clock_turret.ino**
В той же папке лежит файл ресурсов (изображения, html страницы и т.д.): **sources.h** и
файлы озвучки робота на русском **sounds_ru.h** и английском **sounds_en.h** языке.
Другие подробности смотрите в видео.


#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include "ArduinoJson.h"
#include "SSD1306Wire.h" 
#include <EEPROM.h>
#include <DS1302.h>
#include <Adafruit_PWMServoDriver.h>
#include "Wire.h"
#include "String.h" 
#include "sources.h"
#include "sounds_en.h"

#define PCA9685_ADRESS  0x40  // адреса I2C устройств 
#define SCREEN_ADDRESS  0x3C  // 

#define EEPROM_RW_ADDR  0x00  // адресс чтения-записи в eeprom
#define EEPROM_INIT_KEY 0x05  // ключ проверки - проводилась ли первоначальная инициализация eeprom

#define SDA_PIN D1  // пины шины I2C
#define SCL_PIN D2  //
#define VOLTAGE_PIN A0  // пин для считывания напряжения с li-ion 
#define CE_RST_PIN  D5  // пины RTC DS1302
#define IO_DAT_PIN  D6  //
#define SCL_CLK_PIN D7  //
#define SPEAKER_PIN D4  //  динамик 
#define HC_SR501_PIN  D8  // датчик движния

#define VOLTAGE_REF 3.3f    // питающее esp напряжение 
#define VOLTAGE_GAIN 3.00f  // коэффициент передачи делителя напряжения 9V <-> 3V
#define VOLTAGE_OFFSET 60   // 60  // ошибка измерения аналогового выхода 
#define MIN_SUPPLY_VOLTAGE 6.5f // минимальное напряжение АКБ
#define LOW_VOLTAGE_WARNING_MSG_PERIOD 15000 // в миллисекундах

#define UDP_PORT 5000

#define SERVO_FREQ 50

// номера пинов и граничные значения поворота сервопривода в мкс
#define FIRST_LINK_SERVO_PCA_PIN  0
#define SECOND_LINK_SERVO_PCA_PIN 1
#define PUMP_PCA_PIN              15

#define FIRST_LINK_SERVO_PULSE_BASE 1500  // базовое (начальное) положение сервопривода // в мкс
#define FIRST_LINK_SERVO_PULSE_MIN  1100  
#define FIRST_LINK_SERVO_PULSE_MAX  1900

#define SECOND_LINK_SERVO_PULSE_BASE 1500  // в мкс
#define SECOND_LINK_SERVO_PULSE_MIN  1250  
#define SECOND_LINK_SERVO_PULSE_MAX  1700

// скорость поворота сервоприводов
#define FIRST_LINK_SERVO_DELAY  8  // задержка для плавного движения сервоприводов
#define SECOND_LINK_SERVO_DELAY 12  // задержка для плавного движения сервоприводов
// мощность насоса
#define PUMP_POWER  3500

// экземпляры используемых классов
ESP8266WebServer webServer(80);   // web server
WiFiUDP udp;                      // udp reciever
IPAddress ip(192,168,1,1);        // AP parameter
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);

StaticJsonDocument<400> jsondoc;

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(PCA9685_ADRESS);
SSD1306Wire display(SCREEN_ADDRESS, SDA_PIN, SCL_PIN);
DS1302 rtc(CE_RST_PIN, IO_DAT_PIN, SCL_CLK_PIN);

// данные точки доступа
const char *ssid = "clockTurret";
const char *password = "123456789";

Time defaultTime(2022, 1, 1, 0, 0, 0, Time::kSaturday);  // дата по умолчанию (изменить можно будет только время)

// сохраняемые в eeprom данные
struct StoredData{  
  uint8_t key;  // проверка на первую инициализацию: если key != EEPROM_INIT_KEY, то данные еще не были записаны
  uint8_t alarmHour;  // время будильника
  uint8_t alarmMin;   //
};

// состояния конечного автомата
enum FsmStates
{
  INITIALIZATION, // инициализация системы
  LOW_VOLTAGE,  // прерывает инициализацию и работу, уходит в полуспящий режим
  CLOCK, // режим работы как часы
  ALARM, // будильник
  ERROR // если появилась какая-то ошибка
};
 
// глобальные переменные
FsmStates state = FsmStates::INITIALIZATION;
uint32_t lowVoltageTimer = 0; // таймер показа сообщения о низком заряде
uint32_t checkAlarmTimer = 0; // таймер проверки совпадения текущего времени и будильника
uint8_t alarmHour = 25;  // время будильника (без установки пользователем)
uint8_t alarmMin = 60;   //

// переменные для ручного управления
int8_t xControlDirection = 0;   
int8_t yControlDirection = 0;
bool shotControl = false;

// предъобявление некоторых функций с параметрами по умолчанию из-за костылей arduino IDE
void smoothProgressBar(uint8_t progressStart, uint8_t progressEnd, String text, uint8_t dt=30);


void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  
  // настройки динамика и ШИМа
  pinMode(SPEAKER_PIN, OUTPUT); 
  analogWriteRange(255);
  analogWriteFreq(60000);
  
  // настройка датчика hc-sr501
  pinMode(HC_SR501_PIN, INPUT);  
}


void loop() {
  switch(state){
      case FsmStates::INITIALIZATION:{
        // инициализация дисплея
        display.init();
        display.flipScreenVertically();
        display.setFont(ArialMT_Plain_10);
        display.clear();
        display.drawXbm(32, 0, LOGO_WIDTH, LOGO_HEIGHT, logoBits); // рисуем лого
        display.display();
        delay(2000);       
  
        // рисуем полосу загрузки, которые также используются как delay
        smoothProgressBar(0, 10, "Check voltage...");
        smoothProgressBar(11, 20, "Voltage: " + String(getVoltage()) + " V");
        
        if(getVoltage() < MIN_SUPPLY_VOLTAGE){
          state = FsmStates::LOW_VOLTAGE;
          progressBar(21, "Voltage is low");
          delay(500);
          break; // останавливаем инициализацию
        }
        
        smoothProgressBar(22, 27, "Voltage is acceptable");     
        smoothProgressBar(28, 29, "Servo initialize...");
        
        pwm.begin();
        pwm.setOscillatorFrequency(27000000);
        pwm.setPWMFreq(SERVO_FREQ);  

        smoothProgressBar(30, 32, "Servo initialize...");
        pwm.writeMicroseconds(FIRST_LINK_SERVO_PCA_PIN, FIRST_LINK_SERVO_PULSE_BASE);
        smoothProgressBar(33, 43, "Servo initialize...");
        delay(200);
        pwm.writeMicroseconds(SECOND_LINK_SERVO_PCA_PIN, SECOND_LINK_SERVO_PULSE_BASE);
        smoothProgressBar(44, 55, "Servo initialize...");
        delay(200);
        
        smoothProgressBar(56, 60, "EEPROM initialize...");
        
        EEPROM.begin(512);
        StoredData stval; // создаем структуру, читаемую из eeprom
        EEPROM.get(EEPROM_RW_ADDR, stval);
        
        smoothProgressBar(61, 65, "EEPROM check first initialization...");
        
        if(stval.key != EEPROM_INIT_KEY){   // если запись в eeprom никогда не производилась
          stval.key = EEPROM_INIT_KEY;
          stval.alarmHour = alarmHour;
          stval.alarmMin = alarmMin;
          EEPROM.put(EEPROM_RW_ADDR, stval);  // записываем значения по умолчанию
          EEPROM.commit();
          smoothProgressBar(66, 72, "EEPROM first initialize...");
        }
        else{ // если запись уже производилась, то загружаем данные из eeprom
          alarmHour = stval.alarmHour;
          alarmMin = stval.alarmMin;
          smoothProgressBar(66, 72, "EEPROM reading data...");
        }
        
        smoothProgressBar(73, 80, "WIFI AP initialize...");
        //настраиваем точку доступа
        WiFi.mode(WIFI_AP);   
        WiFi.softAPConfig(ip, gateway, subnet);
        bool result = WiFi.softAP(ssid, password);
        if (!result){
          state = FsmStates::ERROR;
          progressBar(81, "WiFi softAP failed");
          delay(500);
          break;
        }

        udp.begin(UDP_PORT);

        smoothProgressBar(82, 90, "Web server initialize...");
        // подключаем обработчики GET запросов
        webServer.on("/", handleControl);           // корневая директория - страница управления
        webServer.on("/clock", handleClock);     // страница с будильником
        webServer.on("/about", handleAbout);        // страница about
        webServer.on("/general.css", handleCss);    // отдельно сервится таблица стилей
        webServer.on("/getData", handleGetData);    // обработчик запроса на получение данных html страницей
        webServer.on("/setTime", handleSetTime);    // обработчик запроса на установку времени
        webServer.on("/setAlarm", handleSetAlarm);  // обработчик запроса на установку будильника
        webServer.on("/action", handleAction);      // обработчик запросов ручного управления
        webServer.onNotFound(handleNotFound);       
        webServer.begin();

        smoothProgressBar(91, 99, "Web server initialize...");
        
        state = FsmStates::CLOCK; //  переключаемся в рабочее состояние
        progressBar(100, "Loading is complete");
        delay(500);
        break;
      }
      
      case FsmStates::LOW_VOLTAGE:{ // переодически зажигает экран на 3 секунды, говоря, что АКБ сел
        if ((millis() > lowVoltageTimer + LOW_VOLTAGE_WARNING_MSG_PERIOD) || (lowVoltageTimer == 0)){
          xControlDirection = 0;   
          yControlDirection = 0;
          shotControl = false;
          delay(500);
          shot(false);
          
          displayLowVoltage();
          delay(3000);
          display.clear();
          display.display();
          lowVoltageTimer = millis();
        }
        break;
      }  
      
      case FsmStates::CLOCK:{ 
        if (!checkVoltage()){   // проверяется раз в секунду, т.к., если часто дергать аналоговый выход, падает вайфай -__- 
          //
          state = FsmStates::LOW_VOLTAGE;
          delay(100);
          break;
        }
        
        if (millis() > checkAlarmTimer + 5000) {   // проверяем время раз в 5 секунд и рисуем часы
          Time t = rtc.time();  // получаем время
          if((t.hr == alarmHour) && (t.min == alarmMin))  // время будильника
          {
            state = FsmStates::ALARM;
            break;
          }
          displayClock(t.hr, t.min);
          checkAlarmTimer = millis();
        }

        checkUdp();       // проверка udp канала на наличие сигналов управления
        remoteControl();  // дистанционное управление (через web или через udp) 
        break;
      }  

      case FsmStates::ALARM:{   // блокирующее на несколько минут
        if (!checkVoltage()){   
          state = FsmStates::LOW_VOLTAGE;
          delay(100);
          break;
        }
        
        state = FsmStates::CLOCK; // следующее состояние - часы
        
        xControlDirection = 0;   
        yControlDirection = 0;
        shotControl = false;
        delay(500);
        shot(false);
        
        displayFace(DisplayFaces::HAPPY);
        for(uint8_t i = 0; i < 4; i++)  say(alarm_data, ALARM_LENGTH);

        displayFace(DisplayFaces::NORMAL);
        for(uint8_t i = 0; i < 3; i++)  say(alarm_data, ALARM_LENGTH);
        
        displayFace(DisplayFaces::DISAPPOINTED);
        for(uint8_t i = 0; i < 2; i++)  say(alarm_data, ALARM_LENGTH);
        
        delay(1000);

        displayFace(DisplayFaces::BENDER);
        delay(200);
        say(wakeup_data, WAKEUP_LENGTH);
        delay(1000);
        displayFace(DisplayFaces::HAPPY);

        if(!findBody()) break;  // если не нашли человека по датчику HC-SR501, то заканчиваем будить

        say(foundyou_data, FOUNDYOU_LENGTH);  // говорим, что нашли человека
        delay(500);

        shot(true);
        delay(3000);
        shot(false);
        delay(1000);
        break;
      }    
      
      case FsmStates::ERROR:{
        break;
      }    
      
      default: {
        break;
      }
  }
  webServer.handleClient();
}


void handleControl() {
 webServer.send_P(200, "text/html", controlHtml); // отправляем веб страницу
}


void handleClock() {
 webServer.send_P(200, "text/html", clockHtml); // отправляем веб страницу
}


void handleAbout() {
 webServer.send_P(200, "text/html", aboutHtml); // отправляем веб страницу
}


void handleCss() {
 webServer.send_P(200, "text/css", generalCss); // отправляем талицу стилей
}


void handleNotFound(){
  webServer.send(404, "text/plain", "404: Not found");
}


void handleAction(){
  String direction = webServer.arg("direction");  // получаем значение направления перемещения (была нажата или отдата кнопка движения)
  String value = webServer.arg("value");  // значение была ли нажата (value == 1) или отжата (value == 0) кнопка
  if (value.toInt() == 1){
    if (direction.equals("forward")) {yControlDirection = 1;} 
    else if (direction.equals("backward")) {yControlDirection = -1;} 
    else if (direction.equals("left")) {xControlDirection = -1;}
    else if (direction.equals("right")) {xControlDirection = 1;}
    else if (direction.equals("central")) {shotControl = true;}
  } 
  else{
    if (direction.equals("forward") || direction.equals("backward")) {yControlDirection = 0;} 
    else if (direction.equals("left") || direction.equals("right")) {xControlDirection = 0;}
    else if (direction.equals("central")) {shotControl = false;}
  }
  webServer.send(200, "text/plane", "");
}


void handleGetData() {
  char tempBuffer[8];
  
  Time now = rtc.time();
  getTimeString(tempBuffer, now.hr, now.min);
  jsondoc["currentTime"] = String(tempBuffer);  
  jsondoc["currentAlarmTime"] = String(alarmHour) + ":" + alarmMin;                 
  String output;
  serializeJson(jsondoc, output);      // создаем json-строку
  webServer.send(200, "application/json", output);   // отправляем ее на страницу
}


void handleSetTime() 
{
  String timeToSet = webServer.arg("time");   
  uint8_t _hour = timeToSet.substring(0,2).toInt();  // переводим его в целочисленный вид
  uint8_t _minutes = timeToSet.substring(3,5).toInt();
  
  rtc.writeProtect(false);
  rtc.halt(false);
  
  Time now = rtc.time();
  now.hr = _hour;
  now.min = _minutes;
  rtc.time(now);
  
  rtc.writeProtect(true);
  rtc.halt(true);
  
  webServer.send(200, "text/plane", "");
}


void handleSetAlarm() 
{
  String timeToAlarmSet = webServer.arg("time");      // получаем время будильника
  alarmHour = timeToAlarmSet.substring(0,2).toInt();  // переводим его в целочисленный вид
  alarmMin = timeToAlarmSet.substring(3,5).toInt();
  
  StoredData stval; // создаем структуру, записываемую в eeprom
  stval.key = EEPROM_INIT_KEY;
  stval.alarmHour = alarmHour;  
  stval.alarmMin = alarmMin;
  EEPROM.put(EEPROM_RW_ADDR, stval);  // записываем будильник
  EEPROM.commit();
  webServer.send(200, "text/plane", "");
}


void checkUdp()
{
  static uint8_t udpRecBuffer[255];
  uint16_t packetSize = udp.parsePacket();
  if (packetSize) {
    uint16_t len = udp.read(udpRecBuffer, 255);
    if (len > 0)  udpRecBuffer[len] = '\0';
    
    DeserializationError err = deserializeJson(jsondoc, udpRecBuffer);
    if (err != DeserializationError::Ok) return;

    if (jsondoc["x_dir"].is<int>() && jsondoc["y_dir"].is<int>() && jsondoc["shot"].is<int>())   // x_dir and y_dir is {-1, 0, 1}; shot is {0, 1} 
    {
      int8_t x = (int8_t)jsondoc["x_dir"];
      int8_t y = (int8_t)jsondoc["y_dir"];
      int8_t shot = (int8_t)jsondoc["shot"];
      
      if (((x == -1) || (x == 0) || (x == 1)) &&
          ((y == -1) || (y == 0) || (y == 1)) &&
          ((shot == 0) || (shot == 1)))
      {
        xControlDirection = x;
        yControlDirection = y;
        shotControl = (bool)shot;        
      }
    }
  }
}


void remoteControl()
{
  moveFirstLinkServo(xControlDirection, FIRST_LINK_SERVO_DELAY);
  moveSecondLinkServo(yControlDirection, SECOND_LINK_SERVO_DELAY);
  shot(shotControl);
}


void getTimeString(char* timeStr, uint8_t hour, uint8_t minutes)
{
  sprintf(timeStr, "%02d:%02d", hour, minutes);
}


float getVoltage(){
  return (float)(constrain((int16_t)analogRead(VOLTAGE_PIN) - VOLTAGE_OFFSET, 0, 1024)) * VOLTAGE_REF * VOLTAGE_GAIN / 1024.f;
}


bool checkVoltage()   // проверка напряжения раз в несколько секунд в неблокирующем режиме, иначе глохнет точка доступа
{
  static uint32_t checkTimer = millis();
  static bool result = true;  
  if (millis() > checkTimer + 1500) {   // проверяет напряжение раз в секунду
    result = getVoltage() > MIN_SUPPLY_VOLTAGE;
    checkTimer = millis();
  }
  return result;
}


void moveFirstLinkServo(int8_t dir, uint8_t speedDelay)
{
  static uint16_t firstLinkPulseAngle = FIRST_LINK_SERVO_PULSE_BASE;   // переменная, хранящая текущее значение поворота сервы
  static int8_t step = -1;  // 1 мкс
  
  if (dir != 0)
  {
    firstLinkPulseAngle += dir * step;  
    firstLinkPulseAngle = constrain(firstLinkPulseAngle, FIRST_LINK_SERVO_PULSE_MIN, FIRST_LINK_SERVO_PULSE_MAX);
    pwm.writeMicroseconds(FIRST_LINK_SERVO_PCA_PIN, firstLinkPulseAngle);
    // yield();   // используется в delay()
    delay(speedDelay);
  }
}


void moveSecondLinkServo(int8_t dir, uint8_t speedDelay)
{
  static uint16_t secondLinkPulseAngle = SECOND_LINK_SERVO_PULSE_BASE;   // переменная, хранящая текущее значение поворота сервы
  static int8_t step = 1;  // 1 мкс
  
  if (dir != 0)
  {
    secondLinkPulseAngle += dir * step;  
    secondLinkPulseAngle = constrain(secondLinkPulseAngle, SECOND_LINK_SERVO_PULSE_MIN, SECOND_LINK_SERVO_PULSE_MAX);
    pwm.writeMicroseconds(SECOND_LINK_SERVO_PCA_PIN, secondLinkPulseAngle);
    // yield();   // используется в delay()
    delay(speedDelay);
  }
}


void shot(uint8_t isShot)
{
  static uint32_t checkIsShotTimer = 0;
  if (millis() > checkIsShotTimer + 100) {   // проверяет раз в 0.1 сек
    if (isShot) pwm.setPWM(PUMP_PCA_PIN, 0, PUMP_POWER);  // включаем насос
    else  pwm.setPWM(PUMP_PCA_PIN, 0, 4096);  // выключаем насос
    checkIsShotTimer = millis();
  }
}

bool checkHcsr501() // проверяет датчик раз в некоторое время, чтоб не повесить wifi
{
  static uint32_t checkTimer = millis();
  static bool result = false;  
  if (millis() > checkTimer + 100) {   // проверяет напряжение раз в 0.1 сек
    result = (bool)digitalRead(HC_SR501_PIN);
    checkTimer = millis();
  }
  return result;
}


bool findBody() // блокирующее // ищет человека по датчику hc-dr501 перебором положений сервоприводов. Если найдет, то вернет true, если нет, то false
{
  static uint16_t firstLinkPulseAngle = FIRST_LINK_SERVO_PULSE_BASE;    // текущая позиция первого звена
  static int8_t firstLinkDirection = 1;                                 // направление движения первого звена
  const int8_t firstLinkStep = 1;                                       // шаг движения
  static uint16_t secondLinkPulseAngle = SECOND_LINK_SERVO_PULSE_BASE;  // текущая позиция второго звена
  static int8_t secondLinkDirection = 1;                                // направление движения второго звена
  const int8_t secondLinkStep = 30;                                     // шаг движения
  
  // базовая позиция
  pwm.writeMicroseconds(FIRST_LINK_SERVO_PCA_PIN, FIRST_LINK_SERVO_PULSE_BASE);
  delay(500);
  pwm.writeMicroseconds(SECOND_LINK_SERVO_PCA_PIN, SECOND_LINK_SERVO_PULSE_BASE);
  delay(2500);  // задержка на восстановление датчика hc-sr501

  while (!checkHcsr501())
  { 
    if ((SECOND_LINK_SERVO_PULSE_MIN >= secondLinkPulseAngle) || (secondLinkPulseAngle >= SECOND_LINK_SERVO_PULSE_MAX))   // если уже прошли одно направление по второму звену, возвращаемся в центр и начинаем в другое
    {
      secondLinkDirection *= -1;  // меняем направление
      
      firstLinkPulseAngle = FIRST_LINK_SERVO_PULSE_BASE;    
      secondLinkPulseAngle = SECOND_LINK_SERVO_PULSE_BASE;  
      pwm.writeMicroseconds(FIRST_LINK_SERVO_PCA_PIN, firstLinkPulseAngle);
      delay(500);
      pwm.writeMicroseconds(SECOND_LINK_SERVO_PCA_PIN, secondLinkPulseAngle);
      delay(2500);  // задержка на восстановление датчика hc-sr501
    }
    
    do {
      if (checkHcsr501()) 
      {
        firstLinkPulseAngle = FIRST_LINK_SERVO_PULSE_BASE;    
        secondLinkPulseAngle = SECOND_LINK_SERVO_PULSE_BASE;  
        return true;  // если нашли человека, то заканчиваем
      }
      
      pwm.writeMicroseconds(FIRST_LINK_SERVO_PCA_PIN, firstLinkPulseAngle);
      firstLinkPulseAngle = constrain(firstLinkPulseAngle + firstLinkDirection * firstLinkStep, FIRST_LINK_SERVO_PULSE_MIN, FIRST_LINK_SERVO_PULSE_MAX);
      delay(FIRST_LINK_SERVO_DELAY);
    } while(!((FIRST_LINK_SERVO_PULSE_MIN >= firstLinkPulseAngle) || (firstLinkPulseAngle >= FIRST_LINK_SERVO_PULSE_MAX)));
    
    firstLinkDirection *= -1;   // меняем направление

    secondLinkPulseAngle = constrain(secondLinkPulseAngle + secondLinkDirection * secondLinkStep, SECOND_LINK_SERVO_PULSE_MIN, SECOND_LINK_SERVO_PULSE_MAX);
    pwm.writeMicroseconds(SECOND_LINK_SERVO_PCA_PIN, secondLinkPulseAngle);
    delay(SECOND_LINK_SERVO_DELAY);
  }
  
  firstLinkPulseAngle = FIRST_LINK_SERVO_PULSE_BASE;    
  secondLinkPulseAngle = SECOND_LINK_SERVO_PULSE_BASE;  
  return false;
}


void progressBar(uint8_t progress, String text){
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 10, "Loading: " + String(progress) + "%");
  display.drawProgressBar(3, 30, 125, 10, progress);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(5, 50, text);
  display.display();
}


void smoothProgressBar(uint8_t progressStart, uint8_t progressEnd, String text, uint8_t dt){
  for(uint8_t i = progressStart; i <= progressEnd; i++){  // плавная прорисовка
    progressBar(i, text);
    delay(dt);
  }
}

void displayLowVoltage(){
  display.clear();
  display.drawXbm(36, 20, BATTERY_WIDTH, BATTERY_HEIGHT, batteryBits); // рисуем 
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 0, "LOW VOLTAGE: " + String(getVoltage())+ " V");
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 50, "power on and reboot");
  display.display();
}


void displayClock(uint8_t hour, uint8_t minutes){
  static char tempBuffer[8];
  getTimeString(tempBuffer, hour, minutes);
  display.clear();
  display.setFont(ArialMT_Plain_24);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 20, String(tempBuffer));
  display.display();
}


void displayFace(DisplayFaces face){
  display.clear();
  switch(face){
    case HAPPY:{
      display.drawXbm(24, 2, FACE_WIDTH, FACE_HEIGHT, faceHappyBits);  
      break;
    }
    case NORMAL:{
      display.drawXbm(24, 2, FACE_WIDTH, FACE_HEIGHT, faceNormalBits); 
      break;
    }
    case CONFUSED:{
      display.drawXbm(24, 2, FACE_WIDTH, FACE_HEIGHT, faceConfusedBits); 
      break;
    }
    case DISAPPOINTED:{
      display.drawXbm(24, 2, FACE_WIDTH, FACE_HEIGHT, faceDisappointedBits); 
      break;
    }
    case BENDER:{
      display.drawXbm(0, 0, BENDER_WIDTH, BENDER_HEIGHT, benderBits); 
      break;
    }
    default: {
      break;
    }
  }
  display.display();
}


void say(const uint8_t* sound, uint32_t soundSize)  // блокирующее
{
  analogWrite(SPEAKER_PIN, 0);
  for(uint32_t count = 0; count <= soundSize; count++)
  {
    analogWrite(SPEAKER_PIN, pgm_read_byte(&sound[count]));
    yield();
    delayMicroseconds(30);   
  }
  analogWrite(SPEAKER_PIN, 0);
}

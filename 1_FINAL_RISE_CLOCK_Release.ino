/* 
Световой будильник 
*/
//**********************************************************************************************************************************
#define NTP_OFFSET   3*60*60        // В секундах часовой пояс МСК +3
#define NTP_INTERVAL 60 * 1000      // В миллисекундах
#define NTP_ADDRESS  "ntp3.ntp-servers.net"

#define DAWN_TIME 20     // продолжительность рассвета (в минутах) 
#define ALARM_TIMEOUT 5*60  // таймаут на автоотключение будильника, секунды  
//#define ALARM_BLINK 0     // 1 - мигать лампой при будильнике, 0 - не мигать

#define DAWN_MIN 0    // начальная яркость лампы (0 - 255) (для сетевых матриц начало света примерно с 50)
#define DAWN_MAX 180  // максимальная яркость лампы (0 - 255)

#define MAX_BRIGHT 140  // яркость дисплея дневная (0 - 255)
#define MID_BRIGHT 70   // яркость дисплея средняя (0 - 255)
#define MIN_BRIGHT 2   // яркость дисплея ночная (0 - 255)
#define NIGHT_START 23  // час перехода на ночную подсветку (MIN_BRIGHT)
#define NIGHT_END 7     // час перехода на среднюю подсветку 
#define LED_BRIGHT 1   // яркость светодиода индикатора (0 - 255)

#define EEPROM_SIZE 3

// ************ ПИНЫ ******************************************************************************************************
#define CLKe 25  // энкодер S1
#define DTe 33   // энкодер S2
#define SWe 32   // энкодер кнопка Key

// #define DIM_PIN 3     // мосфет / DIM(PWM) пин диммера  вместо них ргб:

#define R_PIN 19
#define G_PIN 18
#define B_PIN 5

#define LED_PIN 2          // светодиод индикатор HA ESP
#define BRIGHTNESS_PIN 13  // яркость дисплея

// ***************** ОБЪЕКТЫ И ПЕРЕМЕННЫЕ *********************************************************************************
#include <Wire.h>               // i2c для экрана
#include <LiquidCrystal_I2C.h>  // экран
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Указываем I2C адрес


#include <NTPClient.h>  //время
#include <TimeLib.h>
#include <WiFiUdp.h>  //время по вайфаю

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
const char* host = "RiseClock";
// Указываем идентификатор и пароль от своей WiFi-сети

const char* ssid = "5G Tower";
const char* password = "fKBxxemH";


#include "EEPROM.h"

#include "GyverTimer.h"
GTimer_ms timeoutTimer(15000);
GTimer_ms dutyTimer((long)DAWN_TIME * 60 * 1000 / (DAWN_MAX - DAWN_MIN));
GTimer_ms alarmTimeout((long)ALARM_TIMEOUT * 1000);

#include <EncButton.h>    // энкодер
EncButton<EB_TICK, DTe, CLKe, SWe> enc;  // энкодер с кнопкой <A, B, KEY>

boolean alarmFlag;  //minuteFlag deleted
int8_t hrs = 21, mins = 55, secs;
int8_t alm_hrs, alm_mins;
int8_t dwn_hrs, dwn_mins;
byte mode;

boolean dawn_start = false;
volatile boolean LampF = false; //--------------------true--============================================================================--=-=-=-=-=-=-=-=-=-=-=-=-=-=--=
volatile boolean alarmRun = false;
volatile int duty;

byte customCharP[]  = {  B11111,  B10001,  B10001,  B10001,  B10001,  B10001,  B10001,  B00000};
byte customCharCH[] = {  B10001,  B10001,  B10001,  B10001,  B01111,  B00001,  B00001,  B00000};
byte customCharB[]  = {  B11111,  B10000,  B10000,  B11110,  B10001,  B10001,  B11110,  B00000};


/*Задание по дням недели Вс Пн  Вт  Ср  Чт   Пт  Cб
  int8_t  alm_hrsW[] = { 9,  9,  8,  6,  8,  9,  9};
  int8_t alm_minsW[] = {40, 40, 30, 45, 30, 40, 40};
*/
// ***************** wifi *************************************************************************
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);

// use first 3 channels of 16 channels (started from zero)
#define LEDC_CHANNEL_0_R    5
#define LEDC_CHANNEL_1_G    6
#define LEDC_CHANNEL_2_B    7
#define LEDC_CHANNEL_3_Br   8
#define LEDC_CHANNEL_4_LED  9
// use bit precission for LEDC timer
#define LEDC_TIMER  8
// use Hz as a LEDC base frequency
#define LEDC_BASE_FREQ     10000

// void ledcAnalogWrite(uint8_t channel, uint32_t value, uint32_t valueMax = 1023) {
//   // calculate duty
//   uint32_t duty = (LEDC_BASE_FREQ / valueMax) * min(value, valueMax);

//   // write duty to LEDC
//   ledcWrite(channel, duty);
// }



WebServer server(80);
/*
 * Login page
 */
const char* loginIndex = 
 "<form name='loginForm'>"
    "<table width='20%' bgcolor='A09F9F' align='center'>"
        "<tr>"
            "<td colspan=2>"
                "<center><font size=4><b>ESP32 Login Page</b></font></center>"
                "<br>"
            "</td>"
            "<br>"
            "<br>"
        "</tr>"
        "<td>Username:</td>"
        "<td><input type='text' size=25 name='userid'><br></td>"
        "</tr>"
        "<br>"
        "<br>"
        "<tr>"
            "<td>Password:</td>"
            "<td><input type='Password' size=25 name='pwd'><br></td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
            "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
        "</tr>"
    "</table>"
"</form>"
"<script>"
    "function check(form)"
    "{"
    "if(form.userid.value=='admin' && form.pwd.value=='admin')"
    "{"
    "window.open('/serverIndex')"
    "}"
    "else"
    "{"
    " alert('Error Password or Username')/*displays error message*/"
    "}"
    "}"
"</script>";
/*
 * Server Index Page
 */
const char* serverIndex = 
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
   "<input type='file' name='update'>"
        "<input type='submit' value='Update'>"
    "</form>"
 "<div id='prg'>progress: 0%</div>"
 "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')"
 "},"
 "error: function (a, b, c) {"
 "}"
 "});"
 "});"
 "</script>";
/*
 * setup function
 */


















//______________________________________________________________Сетап_______________________________________________________
void setup() {
  //Serial.begin(115200);
  lcd.init();       // Инициализация дисплея
  lcd.backlight();  // Подключение подсветки
  lcd.setCursor(0, 0);  // Установка курсора в начало первой строки

  // Начинаем подключение к сети
  WiFi.begin(ssid, password);

  // Проверяем статус. Если нет соединения, то выводим сообщение о подключении
  while (WiFi.status() != WL_CONNECTED) {
    lcd.setCursor(0, 0);
    lcd.print("Connect WiFi...");
  }

  /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) { //http://esp32.local
    // Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }


  lcd.init();           // Инициализация дисплея
  lcd.setCursor(0, 1);  // Установка курсора в начало первой строки
  lcd.print("WiFi connected");
  delay(1000);
  lcd.setCursor(0, 1);
  lcd.println("                 ");
  lcd.createChar(1, customCharP);
  lcd.createChar(2, customCharCH);
  lcd.createChar(3, customCharB);

  enc.setEncType(EB_HALFSTEP);  // тип энкодера: EB_FULLSTEP (0) по умолч., EB_HALFSTEP (1) если энкодер делает один поворот за два щелчка


  // //analogWriteFrequency(50000);
  // pinMode(R_PIN, OUTPUT);
  // pinMode(G_PIN, OUTPUT);
  // pinMode(B_PIN, OUTPUT);

  EEPROM.begin(EEPROM_SIZE);  //Init EEPROM

  while (year() < 2021)
{
  timeClient.update();
  String formattedTime = timeClient.getFormattedTime();
  long unsigned int unixtime = timeClient.getEpochTime();  // синхронизируем системное время
  setTime(unixtime);
  secs = second();
  mins = minute();
  hrs  = hour();
  int8_t numweek = ymdToWeekNumber(year(),month(),day());
}
  timeClient.update();  //контрольный, да хоть и выше есть
  String formattedTime = timeClient.getFormattedTime();
  long unsigned int unixtime = timeClient.getEpochTime();  // синхронизируем системное время
  setTime(unixtime);
  secs = second();
  mins = minute();
  hrs  = hour();
  int8_t numweek = ymdToWeekNumber(year(),month(),day());
  alarmFlag = EEPROM.read(2);

  alm_hrs = constrain(alm_hrs, 0, 23);  //ОГРАНИЧЕНИЕ
  alm_mins = constrain(alm_mins, 0, 59);

  alarmFlag = constrain(alarmFlag, 0, 1);



  if      (weekday() == 7) {alm_hrs = 9; alm_mins = 30;}
  else if (weekday() == 1) {alm_hrs = 9; alm_mins = 30;}
  else if (weekday() == 2) {alm_hrs = 9; alm_mins = 30;}
  else if (weekday() == 3) {alm_hrs = 9; alm_mins = 30;}
//  if((numweek % 2) == 0)   {alm_hrs = 8; alm_mins = 30;}  //четная неделя(зн)
//     else                  {alm_hrs = 6; alm_mins = 30; } //нечетная неделя(ЧС)
//    }
  else if (weekday() == 4) {alm_hrs = 9; alm_mins = 30;}
//  if((numweek % 2) == 0)   {alm_hrs = 6; alm_mins = 30;}  //четная неделя(зн)
//      else {                alm_hrs = 8; alm_mins = 30; } //нечетная неделя(ЧС)
//    }
  else if (weekday() == 5) {alm_hrs = 9; alm_mins = 30;}
  else if (weekday() == 6) {alm_hrs = 9; alm_mins = 30;}
  EEPROM.write(0, alm_hrs);
  EEPROM.write(1, alm_mins);

  calculateDawn();  // расчёт времени рассвета



    /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) { //http://esp32.local
    //Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
 //Serial.println("mDNS responder started");
  /*return index page which is stored in serverIndex */
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });
  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      //Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        //Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  server.begin();

  ledcSetup(LEDC_CHANNEL_0_R, LEDC_BASE_FREQ, LEDC_TIMER);
  ledcAttachPin(R_PIN, LEDC_CHANNEL_0_R);
  ledcSetup(LEDC_CHANNEL_1_G, LEDC_BASE_FREQ, LEDC_TIMER);
  ledcAttachPin(G_PIN, LEDC_CHANNEL_1_G);
  ledcSetup(LEDC_CHANNEL_2_B, LEDC_BASE_FREQ, LEDC_TIMER);
  ledcAttachPin(B_PIN, LEDC_CHANNEL_2_B);

  ledcSetup(LEDC_CHANNEL_3_Br, LEDC_BASE_FREQ, LEDC_TIMER);
  ledcAttachPin(BRIGHTNESS_PIN, LEDC_CHANNEL_3_Br);
  ledcSetup(LEDC_CHANNEL_4_LED, LEDC_BASE_FREQ, LEDC_TIMER);
  ledcAttachPin(LED_PIN, LEDC_CHANNEL_4_LED);

  ledcWrite(LEDC_CHANNEL_3_Br, 255-MAX_BRIGHT);

}
//----------------------------------------------сетап--------------------------------------------------------------------

//______ЛУП___________ЛУП________ЛУП______________ЛУП_____________ЛУП__________________ЛУП_______________ЛУП_____________
void loop() {
  encoderTick();  // отработка энкодера
  server.handleClient();

  if (secs == 0)
  {
    timeClient.update();
    long unsigned int unixtime = timeClient.getEpochTime();  // синхронизируем системное время
    setTime(unixtime);
  }
  else long unsigned int unixtime = timeClient.getEpochTime();  // синхронизируем системное время
  String formattedTime = timeClient.getFormattedTime();
  
  secs = second();
  mins = minute();
  hrs  = hour();
  int8_t numweek = ymdToWeekNumber(year(),month(),day());

  // выводим время
  lcd.setCursor(0, 0);
  if      (weekday() == 1) {lcd.print("BC");}
  else if (weekday() == 2) {lcd.print(char(1)); lcd.print("H");}
  else if (weekday() == 3) {lcd.print("BT");}
  else if (weekday() == 4) {lcd.print("CP");}
  else if (weekday() == 5) {lcd.print(char(2)); lcd.print("T");}
  else if (weekday() == 6) {lcd.print(char(1)); lcd.print("T");}
  else if (weekday() == 7) {lcd.print("C"); lcd.print(char(3));}
 
if (hrs == 19 && mins == 30) {
  if      (weekday() == 7) {alm_hrs = 9; alm_mins = 30;}
  else if (weekday() == 1) {alm_hrs = 9; alm_mins = 30;}
  else if (weekday() == 2) {alm_hrs = 9; alm_mins = 30;}
  else if (weekday() == 3) {alm_hrs = 9; alm_mins = 30;}
//  if((numweek % 2) == 0)   {alm_hrs = 8; alm_mins = 30;}  //четная неделя(зн)
//     else                  {alm_hrs = 6; alm_mins = 30; } //нечетная неделя(ЧС)
//    }
  else if (weekday() == 4) {alm_hrs = 9; alm_mins = 30;}
//  if((numweek % 2) == 0)   {alm_hrs = 6; alm_mins = 30;}  //четная неделя(зн)
//      else {                alm_hrs = 8; alm_mins = 30; } //нечетная неделя(ЧС)
//    }
  else if (weekday() == 5) {alm_hrs = 9; alm_mins = 30;}
  else if (weekday() == 6) {alm_hrs = 9; alm_mins = 30;}
  EEPROM.write(0, alm_hrs);
  EEPROM.write(1, alm_mins);
  calculateDawn();        // расчёт времени рассвета
  }

  lcd.setCursor(4, 0);
  lcd.print(formattedTime);  //время

  if (day() < 10)   {
    lcd.setCursor(14, 0);
    lcd.print("0");
    lcd.setCursor(15, 0);
    lcd.print(day());
  }
  else {
      lcd.setCursor(14, 0);
      lcd.print(day());
  }

  if (month() < 10)   {
    lcd.setCursor(14, 1);
    lcd.print("0");
    lcd.setCursor(15, 1);
    lcd.print(month());
  }
  else {
      lcd.setCursor(14, 1);
      lcd.print(month());
  }


  clockTick();    // считаем время
  alarmTick();    // обработка будильника
  settings();     // настройки


  if (dawn_start || alarmRun) colorWheel();  // если рассвет или уже будильник управление лампой (dutytick)
  if (!dawn_start && !alarmRun && !LampF) setHSV(0,0,0);  // если нерассвет или небудильник управление лампой (dutytick)
  
  if (enc.hasClicks(2)) LampF = !LampF; 
  if (LampF)  setHSV(20,222,255);
  //setHSV(26,222,190);        -------------------------------------------------------------------------------------///////////////////////
  
  
  if (alm_hrs <= NIGHT_END) 
  {
    if ((hrs >= NIGHT_START && hrs <= 23) || (hrs >= 0 && hrs < alm_hrs) || (hrs >= 0 && hrs == alm_hrs && mins < alm_mins))   ledcWrite(LEDC_CHANNEL_3_Br, MIN_BRIGHT); 
    else   ledcWrite(LEDC_CHANNEL_3_Br, MAX_BRIGHT);
  }

  if (alm_hrs > NIGHT_END)
  {
    if ( (hrs >= NIGHT_START && hrs <= 23) || (hrs >= 0 && hrs <= NIGHT_END) )   ledcWrite(LEDC_CHANNEL_3_Br, MIN_BRIGHT);   
    else if ( (hrs >= NIGHT_END && hrs <= alm_hrs) || (hrs >= 0 && hrs == alm_hrs && mins < alm_mins) )  ledcWrite(LEDC_CHANNEL_3_Br, MID_BRIGHT);
    else ledcWrite(LEDC_CHANNEL_3_Br, MAX_BRIGHT);
  }


  // if (minuteFlag && mode == 0 && !alarmRun) {  // если новая минута и стоит режим часов и не орёт будильник
  //   minuteFlag = false;
  // }
  EEPROM.commit();
}
//______ЛУП___________ЛУП________ЛУП______________ЛУП_____________ЛУП__________________ЛУП_______________ЛУП_____________













void colorWheel() {  //счетчик для установления нужного цвета и яркости
  if (duty <= 160) {
    setHSV(10 + duty / 10, 255 - (duty / 6), duty/3);
  } else if (duty > 160) {  //макс наш свет
    setHSV(26, 222, (-127*duty*duty/800 + 2433*duty/40-5615));
  }
}


void encoderTick() {
  enc.tick();   // работаем с энкодером
      if (alarmFlag) {
        lcd.setCursor(10, 1);
        lcd.print("on ");
        ledcWrite(LEDC_CHANNEL_4_LED, LED_BRIGHT);
      } else {
        lcd.setCursor(10, 1);
        lcd.print("off");
        ledcWrite(LEDC_CHANNEL_4_LED, 0);
      }
  // *********** КЛИК ПО ЭНКОДЕРУ **********
  if (enc.click()) {        // клик по энкодеру
     //Serial.println("click"); //+++++++++++++++++++
    //minuteFlag = true;        // вывести минуты при следующем входе в режим 0
    mode++;                   // сменить режим
    if (mode > 1) {           // выход с режима установки будильника и часов
      mode = 0;
    }
    calculateDawn();        // расчёт времени рассвета
    EEPROM.write(0, alm_hrs);
    EEPROM.write(1, alm_mins);
    timeoutTimer.reset();               // сбросить таймаут
    if (mode == 0)  {lcd.setCursor(8, 1); lcd.print(" "); } 
  }

  // *********** УДЕРЖАНИЕ ЭНКОДЕРА **********
  if (enc.held()) {       // кнопка удержана
    //minuteFlag = true;        // вывести минуты при следующем входе в режим 0
    //Serial.println("hold");   // будет постоянно возвращать true после удержания+++++++++++++++++
    if (dawn_start) {         // если удержана во время рассвета или будильника
      dawn_start = false;     // прекратить рассвет
      alarmRun = false;          // и будильник
      duty = 0;
      colorWheel();
      return;
    }
    if (mode == 0 && !dawn_start) {   // кнопка удержана в режиме часов и сейчас не рассвет
      alarmFlag = !alarmFlag;     // переключаем будильник

      EEPROM.write(2, alarmFlag);
      //delay(1000);
    } //else if (mode == 1) {   // кнопка удержана в режиме настройки будильника
      //mode = 0;}               // сменить режим    }
    timeoutTimer.reset();     // сбросить таймаут
  }
}
//_________________________________________________________________________________________________________________________________________
void calculateDawn() {
  // расчёт времени рассвета
  if (alm_mins > DAWN_TIME) {                // если минут во времени будильника больше продолжительности рассвета
    dwn_hrs = alm_hrs;                       // час рассвета равен часу будильника
    dwn_mins = alm_mins - DAWN_TIME;         // минуты рассвета = минуты будильника - продолж. рассвета
  } else {                                   // если минут во времени будильника меньше продолжительности рассвета
    dwn_hrs = alm_hrs - 1;                   // значит рассвет будет часом раньше
    if (dwn_hrs < 0) dwn_hrs = 23;           // защита от совсем поехавших
    dwn_mins = 60 - (DAWN_TIME - alm_mins);  // находим минуту рассвета в новом часе
  }
}

void clockTick() {
  if (dwn_hrs == hrs && dwn_mins == mins && alarmFlag && !dawn_start) {
    duty = DAWN_MIN;
    dawn_start = true;
  }
  if (alm_hrs == hrs && alm_mins == mins && alarmFlag && dawn_start && !alarmRun) {
    alarmRun = true;
    alarmTimeout.reset();
  }
}

void alarmTick() {
  if (dawn_start && alarmFlag) {
    if (dutyTimer.isReady()) {    // поднимаем яркость по таймеру
      duty++;
      if (duty > DAWN_MAX) duty = DAWN_MAX;
    }
  }
  if (alarmRun) {                    // настало время будильника
    if (alarmTimeout.isReady()) { // таймаут будильника
      dawn_start = false;         // прекратить рассвет
      alarmRun = false;              // и будильник
      duty = 0;
      setHSV(0,0,0);
    }
  }
}

void settings() {
  enc.tick();
  // *********** РЕЖИМ УСТАНОВКИ БУДИЛЬНИКА **********
  if (mode == 1) {
      lcd.setCursor(8, 1);
      lcd.print("S"); 
    if (timeoutTimer.isReady()) mode = 0;   // если сработал таймаут, вернёмся в режим 0

    if (enc.right()) {
      //Serial.println("right");   // поворот направо++++++++++++++++++++++++++++++++
      alm_mins++;
      if (alm_mins > 59) {
        alm_mins = 0;
        alm_hrs++;
        if (alm_hrs > 23) alm_hrs = 0;
      }

    }
    if (enc.left()) {
      //Serial.println("left");     // поворот налево+++++++++++++++++++++++++++++++++
      alm_mins--;
      if (alm_mins < 0) {
        alm_mins = 59;
        alm_hrs--;
        if (alm_hrs < 0) alm_hrs = 23;
      }
    }
    if (enc.rightH()) {
     // Serial.println("rightH"); // нажатый поворот направо++++++++++++++++++++++++++++
      alm_hrs++;
      if (alm_hrs > 23) alm_hrs = 0;
    }
    if (enc.leftH()) {
     // Serial.println("leftH");   // нажатый поворот налево++++++++++++++++++++++++++
      alm_hrs--;
      if (alm_hrs < 0) alm_hrs = 23;
      
    }
    if (enc.turn()) {     // вывести свежие изменения при повороте
      timeoutTimer.reset();               // сбросить таймаут
    }
  }
  if (mode == 0)  {lcd.setCursor(8, 1); lcd.print(" "); } //убираем S

    lcd.setCursor(3, 1);
    lcd.print(":"); 

    if (alm_hrs < 10)   {
    lcd.setCursor(1, 1);
    lcd.print("0");
    lcd.setCursor(2, 1);
    lcd.print(alm_hrs);
  }
    else {
      lcd.setCursor(1, 1);
      lcd.print(alm_hrs);
    }
    if (alm_mins < 10)   {
    lcd.setCursor(4, 1);
    lcd.print("0");
    lcd.setCursor(5, 1);
    lcd.print(alm_mins);
  }
    else {
    lcd.setCursor(4, 1);
    lcd.print(alm_mins);
  }
}

// включить цвет НА ПИНАХ в HSV, принимает 0-255 по всем параметрам_________________________________________________________________
void setHSV(uint8_t h, uint8_t s, uint8_t v) {
  float r, g, b;
  byte _r, _g, _b;

  float H = (float)h / 255;
  float S = (float)s / 255;
  float V = (float)v / 255;

  int i = int(H * 6);
  float f = H * 6 - i;
  float p = V * (1 - S);
  float q = V * (1 - f * S);
  float t = V * (1 - (1 - f) * S);

  switch (i % 6) {
    case 0: r = V, g = t, b = p; break;
    case 1: r = q, g = V, b = p; break;
    case 2: r = p, g = V, b = t; break;
    case 3: r = p, g = q, b = V; break;
    case 4: r = t, g = p, b = V; break;
    case 5: r = V, g = p, b = q; break;
  }
  _r = r * 255;
  _g = g * 255;
  _b = b * 255;

  ledcWrite(LEDC_CHANNEL_0_R, 255 - _r);
  ledcWrite(LEDC_CHANNEL_1_G, 255 - _g);
  ledcWrite(LEDC_CHANNEL_2_B, 255 - _b);
}

int ymdToWeekNumber (int y, int m, int d) {
  // reject out-of-range input
  if ((y < 1965)||(y > 2099)) return 0;
  if ((m < 1)||(m > 12)) return 0;
  if ((d < 1)||(d > 31)) return 0;
  // compute correction for year   If Jan. 1 falls on: Mo Tu We Th Fr Sa Su
  // then the correction is:  0 +1 +2 +3 -3 -2 -1
  int corr = ((((y - 1965) * 5) / 4) % 7) - 3;
  // compute day of the year (in range 1-366)
  int doy = d;
  if (m > 1) doy += 31;
  if (m > 2) doy += (((y%4)==0) ? 29 : 28);
  if (m > 3) doy += 31;
  if (m > 4) doy += 30;
  if (m > 5) doy += 31;
  if (m > 6) doy += 30;
  if (m > 7) doy += 31;
  if (m > 8) doy += 31;
  if (m > 9) doy += 30;
  if (m > 10) doy += 31;
  if (m > 11) doy += 30;
  // compute corrected day number
  int cdn = corr + doy;
  // check for boundary conditions if our calculation would give us "week 53",
  // we need to find out whether week 53 really exists, or whether it is week 1 of the following year
  if (cdn > 364) {
    // check for year beginning on Thurs.
    if (corr==3) return 53;
    // check for leap year beginning on Wed.
    if (((y%4)==0) && (corr==2)) return 53;
    // otherwise, there is no week 53
    return 1;
  }
  // if our calculation would give us "week 0", then go to the previous year
  // and find out whether we are in week 52 or week 53
  if (cdn < 1) {
    // first, compute correction for the previous year
    corr = ((((y - 1966) * 5) / 4) % 7) - 3;
    // then, compute day of year with respect to that same previous year
    doy = d + (((y%4)==1)?366:365);
    // finally, re-compute the corrected day number
    cdn = corr + doy;
  }
  // compute number of weeks, rounding up to nearest whole week
  return ((cdn + 6) / 7);
}
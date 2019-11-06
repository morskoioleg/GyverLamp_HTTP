/*
  Скетч к проекту "Многофункциональный RGB светильник"
  Страница проекта (схемы, описания): https://alexgyver.ru/GyverLamp/
  Исходники на GitHub: https://github.com/AlexGyver/GyverLamp/
  Нравится, как написан код? Поддержи автора! https://alexgyver.ru/support_alex/
  Автор: AlexGyver, AlexGyver Technologies, 2019
  https://AlexGyver.ru/
*/

/*
  Версия 1.6:
  TODO
*/

// Ссылка для менеджера плат:
// http://arduino.esp8266.com/stable/package_esp8266com_index.json

// Для WEMOS выбираем плату LOLIN(WEMOS) D1 R2 & mini		
// Для NodeMCU выбираем NodeMCU 1.0 (ESP-12E Module)

// ============= НАСТРОЙКИ =============
// -------- КНОПКА -------		
#define USE_BUTTON 1    // 1 - использовать кнопку, 0 - нет
// -------- ВРЕМЯ -------
#define GMT 3              // смещение (москва 3)
#define NTP_ADDRESS  "europe.pool.ntp.org"    // сервер времени

// -------- РАССВЕТ -------
#define DAWN_BRIGHT 200       // макс. яркость рассвета
#define DAWN_TIMEOUT 1        // сколько рассвет светит после времени будильника, минут

// ---------- МАТРИЦА ---------
#define BRIGHTNESS 40         // стандартная маскимальная яркость (0-255)
#define CURRENT_LIMIT 2000    // лимит по току в миллиамперах, автоматически управляет яркостью (пожалей свой блок питания!) 0 - выключить лимит

#define WIDTH 16              // ширина матрицы
#define HEIGHT 16             // высота матрицы

#define COLOR_ORDER GRB       // порядок цветов на ленте. Если цвет отображается некорректно - меняйте. Начать можно с RGB

#define MATRIX_TYPE 0         // тип матрицы: 0 - зигзаг, 1 - параллельная
#define CONNECTION_ANGLE 2    // угол подключения: 0 - левый нижний, 1 - левый верхний, 2 - правый верхний, 3 - правый нижний
#define STRIP_DIRECTION 2     // направление ленты из угла: 0 - вправо, 1 - вверх, 2 - влево, 3 - вниз
// при неправильной настройке матрицы вы получите предупреждение "Wrong matrix parameters! Set to default"
// шпаргалка по настройке матрицы здесь! https://alexgyver.ru/matrix_guide/

// --------- ESP --------
#define ESP_MODE 1
// 0 - точка доступа
// 1 - локальный
byte IP_AP[] = {192, 168, 4, 66};   // статический IP точки доступа (менять только последнюю цифру)

// ----- AP (точка доступа) -------
#define AP_SSID "GyverLamp"
#define AP_PASS "12345678"
#define AP_PORT 8888

// -------- Менеджер WiFi ---------
#define AC_SSID "AutoConnectAP"
#define AC_PASS "12345678"

// ============= ДЛЯ РАЗРАБОТЧИКОВ =============
#define LED_PIN 2             // пин ленты
#define BTN_PIN 4
#define MODE_AMOUNT 19

#define NUM_LEDS WIDTH * HEIGHT
#define SEGMENTS 1            // диодов в одном "пикселе" (для создания матрицы из кусков ленты)
// ---------------- БИБЛИОТЕКИ -----------------
#define FASTLED_INTERRUPT_RETRY_COUNT 0
#define FASTLED_ALLOW_INTERRUPTS 0
#define FASTLED_ESP8266_RAW_PIN_ORDER
#define NTP_INTERVAL 60 * 1000    // обновление (1 минута)

#include "timer2Minim.h"
#include <FastLED.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>
#include <WiFiUdp.h>
#include <WiFiClient.h>
#include <EEPROM.h>
#include <NTPClient.h>
#include <GyverButton.h>
#include "fonts.h"


// ------------------- ТИПЫ --------------------
MDNSResponder mdns;
ESP8266WebServer httpServer(80);
String webPage = "";
CRGB leds[NUM_LEDS];
WiFiServer server(80);
WiFiUDP Udp;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, GMT * 3600, NTP_INTERVAL);
timerMinim timeTimer(1000);
GButton touch(BTN_PIN, LOW_PULL, NORM_OPEN);

// ----------------- ПЕРЕМЕННЫЕ ------------------
const char* autoConnectSSID = AC_SSID;
const char* autoConnectPass = AC_PASS;
const char AP_NameChar[] = AP_SSID;
const char WiFiPassword[] = AP_PASS;
unsigned int localPort = AP_PORT;
char packetBuffer[UDP_TX_PACKET_MAX_SIZE + 1]; //buffer to hold incoming packet
String inputBuffer;
static const byte maxDim = max(WIDTH, HEIGHT);
struct {
  byte brightness = 50;
  byte speed = 30;
  byte scale = 40;
} modes[MODE_AMOUNT];

struct {
  boolean state = false;
  int time = 0;
} alarm[7];

byte dawnOffsets[] = {5, 10, 15, 20, 25, 30, 40, 50, 60};
byte dawnMode;
boolean dawnFlag = false;
long thisTime;
boolean manualOff = false;

int8_t currentMode = 0;
boolean loadingFlag = true;
boolean ONflag = true;
uint32_t eepromTimer;
boolean settChanged = false;
// Конфетти, Огонь, Радуга верт., Радуга гориз., Смена цвета,
// Безумие 3D, Облака 3D, Лава 3D, Плазма 3D, Радуга 3D,
// Павлин 3D, Зебра 3D, Лес 3D, Океан 3D,

//ПОД ВОПРОСОМ НАДО ЛИ ЭТО
//TEXT
byte modeCode;    // 0 бегущая, 1 часы, 2 игры, 3 нойс маднесс и далее, 21 гифка или картинка,
boolean fullTextFlag = false;
#define D_TEXT_SPEED 500      // скорость бегущего текста по умолчанию (мс)
timerMinim scrollTimer(D_TEXT_SPEED);
//END ПОД ВОПРОСОМ НАДО ЛИ ЭТО
unsigned char matrixValue[8][16];
String lampIP = "";		
byte hrs, mins, secs;		
byte days;

void setup() {
  ESP.wdtDisable();
  //ESP.wdtEnable(WDTO_8S);

  // ЛЕНТА
  FastLED.addLeds<WS2812B, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)/*.setCorrection( TypicalLEDStrip )*/;
  FastLED.setBrightness(BRIGHTNESS);
  if (CURRENT_LIMIT > 0) FastLED.setMaxPowerInVoltsAndMilliamps(5, CURRENT_LIMIT);
  FastLED.clear();
  FastLED.show();

  touch.setStepTimeout(100);
  touch.setClickTimeout(500);

  Serial.begin(115200);
  Serial.println();
  delay(1000);

  // WI-FI
  if (ESP_MODE == 0) {    // режим точки доступа
    WiFi.softAPConfig(IPAddress(IP_AP[0], IP_AP[1], IP_AP[2], IP_AP[3]),
                      IPAddress(192, 168, 4, 1),
                      IPAddress(255, 255, 255, 0));

    WiFi.softAP(AP_NameChar, WiFiPassword);
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("Access point Mode");
    Serial.print("AP IP address: ");
    Serial.println(myIP);

    server.begin();
  } else {                // подключаемся к роутеру
    Serial.print("WiFi manager");
    WiFiManager wifiManager;
    //wifiManager.setDebugOutput(false);
    
#if (USE_BUTTON == 1)		
    if (digitalRead(BTN_PIN)) wifiManager.resetSettings();		
#endif

    wifiManager.autoConnect(autoConnectSSID, autoConnectPass);
    Serial.print("Connected! IP address: ");
    Serial.println(WiFi.localIP());
    lampIP = WiFi.localIP().toString();
  }
  Serial.printf("UDP server on port %d\n", localPort);
  Udp.begin(localPort);

  if (mdns.begin("esp8266", WiFi.localIP())) {
    Serial.println("MDNS responder started");
  }


  webPage += "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, shrink-to-fit=no\"><link rel=\"stylesheet\" href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.3.1/css/bootstrap.min.css\" integrity=\"sha384-ggOyR0iXCbMQv3Xipma34MD+dH/1fQ784/j6cY/iJTQUOhcWr7x9JvoRxT2MZw1T\" crossorigin=\"anonymous\"><link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/bootstrap-slider/10.6.2/css/bootstrap-slider.min.css\"><title>Lamp</title></head><body><h1>LAMP</h1><div class=\"container-fluid\"><div class=\"btn-toolbar justify-content-between\" role=\"toolbar\" aria-label=\"Toolbar with button groups\"><div class=\"btn-group mr-1\" role=\"group\" aria-label=\"Button group with nested dropdown\"> <button type=\"button\" class=\"btn btn-lg btn-outline-secondary\"><</button><div class=\"btn-group\" role=\"group\"> <button style=\"width:200px\" id=\"btnGroupDrop\" type=\"button\" class=\"btn btn-lg btn-outline-secondary dropdown-toggle\" data-toggle=\"dropdown\" aria-haspopup=\"true\" aria-expanded=\"false\"> Эффекты </button><div class=\"dropdown-menu\" aria-labelledby=\"btnGroupDrop1\"> <a class=\"dropdown-item js-mode-button\" data-mode=\"0\" href=\"#\">Конфетти</a> <a class=\"dropdown-item js-mode-button\" data-mode=\"1\" href=\"#\">Огонь</a> <a class=\"dropdown-item js-mode-button\" data-mode=\"2\" href=\"#\">Радуга(верт)</a> <a class=\"dropdown-item js-mode-button\" data-mode=\"3\" href=\"#\">Радуга(гор)</a> <a class=\"dropdown-item js-mode-button\" data-mode=\"4\" href=\"#\">Смена цвета</a> <a class=\"dropdown-item js-mode-button\" data-mode=\"5\" href=\"#\">Безумие</a> <a class=\"dropdown-item js-mode-button\" data-mode=\"6\" href=\"#\">Облака</a> <a class=\"dropdown-item js-mode-button\" data-mode=\"7\" href=\"#\">Лава</a> <a class=\"dropdown-item js-mode-button\" data-mode=\"8\" href=\"#\">Плазма</a> <a class=\"dropdown-item js-mode-button\" data-mode=\"9\" href=\"#\">Радуга</a> <a class=\"dropdown-item js-mode-button\" data-mode=\"10\" href=\"#\">Павлин</a> <a class=\"dropdown-item js-mode-button\" data-mode=\"11\" href=\"#\">Зебра</a> <a class=\"dropdown-item js-mode-button\" data-mode=\"12\" href=\"#\">Лес</a> <a class=\"dropdown-item js-mode-button\" data-mode=\"13\" href=\"#\">Океан</a> <a class=\"dropdown-item js-mode-button\" data-mode=\"14\" href=\"#\">Цвет</a> <a class=\"dropdown-item js-mode-button\" data-mode=\"15\" href=\"#\">Снег</a> <a class=\"dropdown-item js-mode-button\" data-mode=\"16\" href=\"#\">Матрица</a> <a class=\"dropdown-item js-mode-button\" data-mode=\"17\" href=\"#\">Светлячки</a> <a class=\"dropdown-item js-mode-button\" data-mode=\"18\" href=\"#\">Сервис</a></div></div> <button type=\"button\" class=\"btn btn-lg btn-outline-secondary\">></button></div></div><div style=\"margin-top: 20px;\"> <input type=\"range\" orient=\"vertical\" id=\"brightness\" name=\"brightness\" min=\"1\" max=\"255\" style=\"width: 100%;\"> <label for=\"brightness\">Яркость</label></div><div style=\"margin-top: 20px;\"> <input type=\"range\" id=\"speed\" name=\"speed\" min=\"1\" max=\"255\" style=\"width: 100%;\"> <label for=\"speed\">Скорость</label></div><div style=\"margin-top: 20px;\"> <input type=\"range\" id=\"scale\" name=\"scale\" min=\"1\" max=\"100\" style=\"width: 100%;\"> <label for=\"scale\">Масштаб</label></div><div style=\"margin-top: 40px;\"> <button type=\"button\" class=\"btn btn-success btn-lg\" id=\"js-on-button\">ON</button> <button type=\"button\" class=\"btn btn-danger btn-lg\" id=\"js-off-button\">OFF</button></div><div id=\"output\"></div> <script src=\"http://code.jquery.com/jquery-3.4.1.min.js\" integrity=\"sha256-CSXorXvZcTkaix6Yvo6HppcZGetbYMGWSFlBw8HfCJo=\" crossorigin=\"anonymous\"></script> <script src=\"https://cdnjs.cloudflare.com/ajax/libs/popper.js/1.14.7/umd/popper.min.js\" integrity=\"sha384-UO2eT0CpHqdSJQ6hJty5KVphtPhzWj9WO1clHTMGa3JDZwrnQq4sF86dIHNDz0W1\" crossorigin=\"anonymous\"></script> <script src=\"https://stackpath.bootstrapcdn.com/bootstrap/4.3.1/js/bootstrap.min.js\" integrity=\"sha384-JjSmVgyd0p3pXB1rRibZUAYoIIy6OrQ6VrjIEaFf/nJGzIxFDsf4x0xIM+B07jRM\" crossorigin=\"anonymous\"></script> <script>let scale_time=0;let brightness_time=0;let speed_time=0;$(\"#scale\").on(\"input\",function(){clearTimeout(scale_time);scale=this.value;scale_time=setTimeout(function(){sendCmdJQuery('SCA'+scale);},100);});$(\"#speed\").on(\"input\",function(){clearTimeout(speed_time);speed=this.value;speed_time=setTimeout(function(){sendCmdJQuery('SPD'+speed);},100);});$(\"#brightness\").on(\"input\",function(){clearTimeout(brightness_time);brightness=this.value;brightness_time=setTimeout(function(){sendCmdJQuery('BRI'+brightness);},100);});$(\"#js-on-button\").on(\"click\",function(){sendCmdJQuery('P_ON');setOn(true);});$(\"#js-off-button\").on(\"click\",function(){sendCmdJQuery('P_OFF');setOn(false);});$(\".js-mode-button\").on(\"click\",function(){sendCmdJQuery('EFF'+$(this).attr('data-mode'));setCurrentModeName($(this).text());get_status(false);});function setOn(lampEnabled) {$(\"#js-on-button\").removeClass('active').removeClass('disabled');$(\"#js-off-button\").removeClass('disabled').removeClass('active');if(lampEnabled) {$(\"#js-on-button\").addClass('disabled');$(\"#js-off-button\").addClass('active');} else {$(\"#js-on-button\").addClass('active');$(\"#js-off-button\").addClass('disabled');}} function setCurrentModeName(name) {$('#btnGroupDrop').text(name);} function toJsonContainer(json){setCurrentModeName($(\"a[data-mode=\"+json.status.mode+\"]\").text());$(\"#brightness\").val(json.status.brightness);$(\"#speed\").val(json.status.speed);$(\"#scale\").val(json.status.scale);setOn(json.status.onFlag);} function sendCmdJQuery(command_string){$.get('/specificArgs',{command:command_string});} function get_status(setTimer){$.ajax({type:\"GET\",url:\"/specificArgs\",data:\"command=STATUS\",dataType:\"jsonp\",jsonpCallback:\"toJsonContainer\",timeout:5000,complete:function(response){if(setTimer) setTimeout(function(){get_status(true);},5000);}});} $(function(){get_status(true);});</script> </body></html>";
  
  httpServer.on("/", [](){
    httpServer.send(200, "text/html", webPage);
  });

  httpServer.on("/specificArgs", handleSpecificArg);   //Associate the handler function to the path

  httpServer.begin();

  scrollTimer.setInterval(D_TEXT_SPEED); //ПОД ВОПРОСОМ НАДО ЛИ ЭТО
  
  // EEPROM
  EEPROM.begin(202);
  delay(50);
  if (EEPROM.read(198) != 20) {   // первый запуск
    EEPROM.write(198, 20);
    EEPROM.commit();

    for (byte i = 0; i < MODE_AMOUNT; i++) {
      EEPROM.put(3 * i + 40, modes[i]);
      EEPROM.commit();
    }
    for (byte i = 0; i < 7; i++) {
      EEPROM.write(5 * i, alarm[i].state);   // рассвет
      eeWriteInt(5 * i + 1, alarm[i].time);
      EEPROM.commit();
    }
    EEPROM.write(199, 0);   // рассвет
    EEPROM.write(200, 0);   // режим
    EEPROM.commit();
  }
  for (byte i = 0; i < MODE_AMOUNT; i++) {
    EEPROM.get(3 * i + 40, modes[i]);
  }
  for (byte i = 0; i < 7; i++) {
    alarm[i].state = EEPROM.read(5 * i);
    alarm[i].time = eeGetInt(5 * i + 1);
  }
  dawnMode = EEPROM.read(199);
  currentMode = (int8_t)EEPROM.read(200);

  // отправляем настройки
  sendCurrent();
  char reply[inputBuffer.length() + 1];
  inputBuffer.toCharArray(reply, inputBuffer.length() + 1);
  Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
  Udp.write(reply);
  Udp.endPacket();
  timeClient.begin();
  memset(matrixValue, 0, sizeof(matrixValue));

  randomSeed(micros());
  
  // получаем время
  byte count = 0;
  while (count < 5) {
    if (timeClient.update()) {
      hrs = timeClient.getHours();
      mins = timeClient.getMinutes();
      secs = timeClient.getSeconds();
      days = timeClient.getDay();
      break;
    }
    count++;
    delay(500);
  } 
}

void handleSpecificArg() { 
  String message = "";
  inputBuffer = "";
  if (httpServer.arg("command")== ""){     //Parameter not found
    message = "Command Argument not found";
  }else{     //Parameter found
    message = "Command Argument = ";
    inputBuffer = httpServer.arg("command");
    message += inputBuffer;     //Gets the value of the query parameter
  }

    Serial.println("inputBuffer:");
    Serial.println(inputBuffer);
     //TODO: CHange to switch
    if (inputBuffer.startsWith("DEB")) {  //not tested
      inputBuffer = "OK " + timeClient.getFormattedTime();
    } else if (inputBuffer.startsWith("GET")) {//not used
      message = "CURR";
      message += " ";
      message += String(currentMode);
      message += " ";
      message += String(modes[currentMode].brightness);
      message += " ";
      message += String(modes[currentMode].speed);
      message += " ";
      message += String(modes[currentMode].scale);
      message += " ";
      message += String(ONflag);
    } else if (inputBuffer.startsWith("STATUS")) { //{"status":{"mode":0,"brightness":4,"speed":75,"scale":5,"onFlag":1}}
      message = "toJsonContainer({\"status\":{\"mode\":";
      message += String(currentMode);
      message += ",\"brightness\":";
      message += String(modes[currentMode].brightness);
      message += ",\"speed\":";
      message += String(modes[currentMode].speed);
      message += ",\"scale\":";
      message += String(modes[currentMode].scale);
      message += ",\"onFlag\":";
      message += String(ONflag);
      message += "}})";      
      httpServer.send(200, "text/json", message);          //Returns the HTTP response      
    }  else if (inputBuffer.startsWith("EFF")) {
      saveEEPROM();
      currentMode = (byte)inputBuffer.substring(3).toInt();
      loadingFlag = true;
      FastLED.clear();
      delay(1);
      sendCurrent();
      FastLED.setBrightness(modes[currentMode].brightness);
    } else if (inputBuffer.startsWith("BRI")) {
    Serial.println( inputBuffer.substring(3).toInt());      
      modes[currentMode].brightness = inputBuffer.substring(3).toInt();
      FastLED.setBrightness(modes[currentMode].brightness);
      settChanged = true;
      eepromTimer = millis();
    } else if (inputBuffer.startsWith("SPD")) {
    Serial.println( inputBuffer.substring(3).toInt());      
      modes[currentMode].speed = inputBuffer.substring(3).toInt();
      loadingFlag = true;
      settChanged = true;
      eepromTimer = millis();
    } else if (inputBuffer.startsWith("SCA")) {
      modes[currentMode].scale = inputBuffer.substring(3).toInt();
      loadingFlag = true;
      settChanged = true;
      eepromTimer = millis();
    } else if (inputBuffer.startsWith("P_ON")) {
      ONflag = true;
      changePower();
      sendCurrent();
    } else if (inputBuffer.startsWith("P_OFF")) {
      ONflag = false;
      changePower();
      sendCurrent();
    } else if (inputBuffer.startsWith("P_SWITCH")) {//not tested
      ONflag = !ONflag;
      changePower();
      sendCurrent();      
    } else if (inputBuffer.startsWith("ALM_SET")) { //not tested
      byte alarmNum = (char)inputBuffer[7] - '0';
      alarmNum -= 1;
      if (inputBuffer.indexOf("ON") != -1) {
        alarm[alarmNum].state = true;
        inputBuffer = "alm #" + String(alarmNum + 1) + " ON";
      } else if (inputBuffer.indexOf("OFF") != -1) {
        alarm[alarmNum].state = false;
        inputBuffer = "alm #" + String(alarmNum + 1) + " OFF";
      } else {
        int almTime = inputBuffer.substring(8).toInt();
        alarm[alarmNum].time = almTime;
        byte hour = floor(almTime / 60);
        byte minute = almTime - hour * 60;
        inputBuffer = "alm #" + String(alarmNum + 1) +
                      " " + String(hour) +
                      ":" + String(minute);
      }
      saveAlarm(alarmNum);
    } else if (inputBuffer.startsWith("ALM_GET")) {  //TODO: not tested
      sendAlarms();
    } else if (inputBuffer.startsWith("DAWN")) {  //TODO: not tested
      dawnMode = inputBuffer.substring(4).toInt() - 1;
      saveDawnMmode();
    }

  httpServer.send(200, "text/plain", message);          //Returns the HTTP response
  
}


void loop() {
  httpServer.handleClient();
  //parseUDP();
  effectsTick();
  eepromTick();
  timeTick();
#if (USE_BUTTON == 1)  
  buttonTick();
#endif
  ESP.wdtFeed();   // пнуть собаку
  yield();  // ещё раз пнуть собаку
}

void eeWriteInt(int pos, int val) {
  byte* p = (byte*) &val;
  EEPROM.write(pos, *p);
  EEPROM.write(pos + 1, *(p + 1));
  EEPROM.write(pos + 2, *(p + 2));
  EEPROM.write(pos + 3, *(p + 3));
  EEPROM.commit();
}

int eeGetInt(int pos) {
  int val;
  byte* p = (byte*) &val;
  *p        = EEPROM.read(pos);
  *(p + 1)  = EEPROM.read(pos + 1);
  *(p + 2)  = EEPROM.read(pos + 2);
  *(p + 3)  = EEPROM.read(pos + 3);
  return val;
}

void saveSettings()
{
  loadingFlag = true;
  settChanged = true;
  eepromTimer = millis();  
}

#if (USE_WEBUI == 1)  
void parseHTTP() { 
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
  byte thisDay = days;
  if (thisDay == 0) thisDay = 7;  // воскресенье это 0
  thisDay--;
  thisTime = hrs * 60 + mins;
      
      message = "OK " + timeClient.getFormattedTime();
         message +=("thisDay");
 message += (thisDay);
   message +=("alarm[thisDay].state");
 message +=(alarm[thisDay].state);
   message +=("thisTime");
 message +=(thisTime);
   message +=("alarm[thisDay].time");
 message +=(alarm[thisDay].time);
   message +=("dawnOffsets[dawnMode]");
 message +=(dawnOffsets[dawnMode]);
   message +=("DAWN_TIMEOUT");
 message +=(DAWN_TIMEOUT);
    } else if (inputBuffer.startsWith("STATUS")) { //{"status":{"mode":0,"brightness":4,"speed":75,"scale":5,"onFlag":1}}
      byte thisDay = days;
      if (thisDay == 0) thisDay = 7;  // воскресенье это 0
      thisDay--;      
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
      message += ",\"currentTime\":\"";
      message += String(timeClient.getFormattedTime());
      message += "\",\"thisDay\":";
      message += String(thisDay); 
      message += ",\"alarmStateThisDay\":";
      message += String(alarm[thisDay].state);
      message +=",\"dawnOffset\":";
      message +=(dawnOffsets[dawnMode]);
      message +=",\"dawnMode\":";
      message +=(dawnMode);      
      message +=",\"dawnTimeout\":";
      message +=(DAWN_TIMEOUT);    
      message += ",\"alarms\":[";
        for (byte i = 0; i < 7; i++) {
          message +="{\"on\":";
          message +=String(alarm[i].state);          
          message +=",\"time\":";
          message += String(alarm[i].time);
          message +=",\"day\":";
          message += String(i);          
          message += "},";
        }
      message += "]";  

        
      message += "}})";      
      httpServer.send(200, "text/json", message);          //Returns the HTTP response      
    }  else if (inputBuffer.startsWith("EFF")) {
      saveEEPROM();
      currentMode = (byte)inputBuffer.substring(3).toInt();
      loadingFlag = true;
      FastLED.clear();
      delay(1);
      FastLED.setBrightness(modes[currentMode].brightness);
    } else if (inputBuffer.startsWith("BRI")) {
    Serial.println( inputBuffer.substring(3).toInt());      
      modes[currentMode].brightness = inputBuffer.substring(3).toInt();
      FastLED.setBrightness(modes[currentMode].brightness);
      saveSettings();
    } else if (inputBuffer.startsWith("SPD")) {
    Serial.println( inputBuffer.substring(3).toInt());      
      modes[currentMode].speed = inputBuffer.substring(3).toInt();
      saveSettings();
    } else if (inputBuffer.startsWith("SCA")) {
      modes[currentMode].scale = inputBuffer.substring(3).toInt();
      saveSettings();
    } else if (inputBuffer.startsWith("P_ON")) {
      ONflag = true;
      changePower();
    } else if (inputBuffer.startsWith("P_OFF")) {
      ONflag = false;
      changePower();
    } else if (inputBuffer.startsWith("P_SWITCH")) {
      ONflag = !ONflag;
      changePower();
    } else if (inputBuffer.startsWith("ALM_SET")) { //not tested
      byte alarmNum = (char)inputBuffer[7] - '0';
      //byte alarmNum = (byte)inputBuffer.substring(7).toInt();
      alarmNum -= 1;
      Serial.println(inputBuffer);
      //Serial.println(inputBuffer.substring(5));
      //Serial.println(inputBuffer.substring(6));
      //Serial.println(inputBuffer.substring(7));
      Serial.println(inputBuffer.substring(8));
      //Serial.println(inputBuffer.substring(9));
      //Serial.println(inputBuffer.substring(10));
      if (inputBuffer.indexOf("ON") != -1) {
        alarm[alarmNum].state = true;
        message = "alm #" + String(alarmNum + 1) + " ON";
      } else if (inputBuffer.indexOf("OFF") != -1) {
        alarm[alarmNum].state = false;
        message = "alm #" + String(alarmNum + 1) + " OFF";
      } else {
        int almTime = inputBuffer.substring(8).toInt();
        Serial.println(almTime);
        alarm[alarmNum].time = almTime;
        byte hour = floor(almTime / 60);
        Serial.println(String(hour));
        byte minute = almTime - hour * 60;
        Serial.println(String(minute));
        message = "alm #" + String(alarmNum + 1) +
                      " " + String(hour) +
                      ":" + String(minute);
      }
      saveAlarm(alarmNum);
    } else if (inputBuffer.startsWith("ALM_GET")) {  //TODO: not tested
        message = "ALMS ";
        for (byte i = 0; i < 7; i++) {
          message += String(alarm[i].state);
          message += " ";
        }
        for (byte i = 0; i < 7; i++) {
          message += String(alarm[i].time);
          message += " ";
        }
        message += (dawnMode + 1);
    } else if (inputBuffer.startsWith("DAWN")) {  //TODO: not tested
      Serial.println(inputBuffer.substring(4));
      dawnMode = inputBuffer.substring(4).toInt() - 1;
      saveDawnMmode();
    }

  httpServer.send(200, "text/plain", message);          //Returns the HTTP response
  
}
#endif
#if (USE_UDP == 1)
void parseUDP() {
  int packetSize = Udp.parsePacket();
  if (packetSize) {
    int n = Udp.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE);
    packetBuffer[n] = 0;
    inputBuffer = packetBuffer;

    if (inputBuffer.startsWith("DEB")) {
      inputBuffer = "OK " + timeClient.getFormattedTime();
    } else if (inputBuffer.startsWith("GET")) {
      sendCurrent();
    } else if (inputBuffer.startsWith("EFF")) {
      saveEEPROM();
      currentMode = (byte)inputBuffer.substring(3).toInt();
      loadingFlag = true;
      FastLED.clear();
      delay(1);
      sendCurrent();
      FastLED.setBrightness(modes[currentMode].brightness);
    } else if (inputBuffer.startsWith("BRI")) {
      modes[currentMode].brightness = inputBuffer.substring(3).toInt();
      FastLED.setBrightness(modes[currentMode].brightness);
      settChanged = true;
      eepromTimer = millis();
    } else if (inputBuffer.startsWith("SPD")) {
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
    } else if (inputBuffer.startsWith("ALM_SET")) {
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
    } else if (inputBuffer.startsWith("ALM_GET")) {
      sendAlarms();
    } else if (inputBuffer.startsWith("DAWN")) {
      dawnMode = inputBuffer.substring(4).toInt() - 1;
      saveDawnMmode();
    }

    char reply[inputBuffer.length() + 1];
    inputBuffer.toCharArray(reply, inputBuffer.length() + 1);
    Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
    Udp.write(reply);
    Udp.endPacket();
  }
}
#endif

void sendCurrent() {
  inputBuffer = "CURR";
  inputBuffer += " ";
  inputBuffer += String(currentMode);
  inputBuffer += " ";
  inputBuffer += String(modes[currentMode].brightness);
  inputBuffer += " ";
  inputBuffer += String(modes[currentMode].speed);
  inputBuffer += " ";
  inputBuffer += String(modes[currentMode].scale);
  inputBuffer += " ";
  inputBuffer += String(ONflag);
}

void sendAlarms() {
  inputBuffer = "ALMS ";
  for (byte i = 0; i < 7; i++) {
    inputBuffer += String(alarm[i].state);
    inputBuffer += " ";
  }
  for (byte i = 0; i < 7; i++) {
    inputBuffer += String(alarm[i].time);
    inputBuffer += " ";
  }
  inputBuffer += (dawnMode + 1);
}

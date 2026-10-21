
// https://github.com/Links2004/arduinoWebSockets
// I had to put the .zip directly in Arduino/libraries

// To upload the data (https://github.com/earlephilhower/arduino-littlefs-upload):
//  Ctrl+Shift+P => Upload LittleFS to Pico/ESP8266/ESP32
// The uploaded data is in the data/ folder
// If nothing is found => restart the Arduino IDE

/**First use:
 * - Set the file system size: Tools->Flash size
 * - Format using the formatFS program
 * - Upload the data/ folder : Ctrl+shif+P => Upload LittleFS to Pico/ESP8266/ESP32
 * - Finally, upload the program
 */

/**
  Connection: Wi-Fi access point (password below)
  The IP address is probably 192.168.42.1
  (otherwise, run 'ip -a' on the computer and use address 1 on the same subnet: xx.yy.zz.1) 
 */

#include "ota.h"
#include "wifiLink.h"
#include "pwmLed.h"
#include "sequencer.h"
#include "calibration.h"

// Define the access point SSID and password
const char *ssid = "PicoW_SolarPanel";
const char *password = "LeSole1lVirtu3lEstTop!"; // At least 8 characters

wifiLink wl;
Sequencer sequencer;
Calibration calibration;

volatile bool periodicLuxRead = false;

void wifiLinkUserEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
  if (type == WStype_TEXT)
  {
    String command = (char *)payload;
    if (command == "LED_TOGGLE")
    {
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    } else if(command == "TOGGLE_CALIBRATION")
    {
      if(calibration.isRunning())
      {
        calibration.stop();
        wl.sendTXT("CAL_STOPPED");
      } else { //start calibration (and stop seq if exists.)
        sequencer.stop();
        wl.sendTXT("SEQ_STOPPED");
        calibration.start();
        wl.sendTXT("CAL_STARTED");
      }
    } else if (command.startsWith("PWM:"))
    {
      int pwmValue = command.substring(4).toInt();
      constrain(pwmValue, 0, 4095);
      pwmLedSet(pwmValue);
    }
    else if (command.startsWith("FAN"))
    {
      int id = command.substring(3).toInt();
      int pwmValue = command.substring(5).toInt();
      constrain(pwmValue, 0, 511);
      constrain(id, 0,1);
      fanSet(id,pwmValue);
      // wl.sendLog("set fan "+String(id)+" to "+String(pwmValue));

    }
    else if (command.startsWith("LUX_READ_START"))
    {
      periodicLuxRead = true;
    }
    else if (command.startsWith("LUX_READ_STOP"))
    {
      periodicLuxRead = false;
    }

    else if (command.startsWith("GET_DURATION:"))
    {
      const String filename = command.substring(13);
      const uint32_t duration = Sequencer::getDuration(filename);
      wl.sendTXT("DURATION:" + filename + ":" + String(duration));
    } else if(command.startsWith("START_SEQ:"))
    {
      calibration.stop();
      wl.sendTXT("CAL_STOPPED");
      String filename = command.substring(10);
      sequencer.setFile(filename); //may stop current running seq.
      sequencer.start();
      wl.sendTXT("SEQ_STARTED:" + filename + ":");

    } else if(command.startsWith("STOP_SEQ"))
    {
      sequencer.stop();
      wl.sendTXT("SEQ_STOPPED");
    }
    else
    {
      wl.sendLog("unknown:" + command);
    }
  }
}

void wifiLinkUserUploadEvent(uint8_t num, String filename)
{
  // if the file is a sequence (.seq), verify it.
  if (filename.endsWith(".seq"))
  {
    sequencer.setFile(filename);
    if (!sequencer.verifyFile())
    {
      sendLog("Sequence file verification failed! => deleting file.");
      wl.deleteFile(num, filename);
    }
    else
    {
      sendLog("Sequence file verified successfully!");
    }
  }
}

void setup()
{
  // LED…
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(115200);
  int nbTry = 0;
  while (!Serial && nbTry < 50)
  {
    delay(100);
    digitalWrite(LED_BUILTIN, nbTry & 1);
    nbTry++;
  }
  Serial.println("serial ok.");

  // Wifi link (Access point, LittleFS, http server on port 80, websocket on port 81)
  wl.setup(ssid, password);
  wl.onEvent(wifiLinkUserEvent);
  wl.onUploadEvent(wifiLinkUserUploadEvent);

  // start OTA (over the air) update capability.
  setupOTA();
  Serial.println("OTA ok.");

  pwmLedSetup();
  pinMode(16, OUTPUT);

  // fanSet(0,255); //50% (max is 511)
  // fanSet(1,255);

  if(calibration.setup() != 0) 
  {
    Serial.println("Cannot init Adafruit VEML7700 lux sensor!");
  }
}

void sendLog(String str) { wl.sendLog(str); }

void loop()
{
  static uint32_t deadlineLuxRead = 200;
  static int deadline = 50;
  wl.loop();
  loopOTA();
  if (millis() > deadline)
  {
    deadline += 50;
    digitalWrite(16, !digitalRead(16));
  }
  if(sequencer.isRunning())
  {
    // sendLog("*");
    int32_t ret = sequencer.loop();
    if(ret >= 100) {wl.sendTXT("SEQ_STOPPED");}
    else if(ret >= 0) wl.sendTXT("SEQ_PROGRESS:" + String(ret));
  } 
  else if(calibration.isRunning())
  {
    // sendLog("#");
    unsigned int id = calibration.loop();
    if(id == 0) {wl.sendTXT("CAL_STOPPED");}
    else {
      wl.sendTXT("CAL_PROGRESS:" + String((id*1000)/4096)+String(":")+String(calibration.getLastValue()));
    }
  }
  if(periodicLuxRead)
  {
    if(millis() > deadlineLuxRead)
    {
      wl.sendTXT("LUX:"+String(calibration.testSensor()));
      deadlineLuxRead = millis()+200; //not precise, but we don't mind here
    }
  }
  //Serial.println(calibration.testSensor());
  delay(1);
}

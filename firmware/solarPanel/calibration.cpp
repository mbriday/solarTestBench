#include "calibration.h"
#include <Arduino.h>
#include <LittleFS.h>

#include "Adafruit_VEML7700.h"
#include "pwmLed.h"

Adafruit_VEML7700 veml = Adafruit_VEML7700();

Calibration::Calibration()
{
  m_running = false;
  m_init = false;
}

int Calibration::setup()
{
  m_running = false;
  if (!veml.begin()) {
    return -1;
  }
  m_init = true;
  return 0;
}

void Calibration::start()
{
  if(!m_init) //not yet initialized.
  {
    if(!setup()) return;
  }
  m_nextId = 0;
  m_running = true;
}

unsigned int Calibration::loop()
{
  if(m_running)
  {
    if(m_nextId>4095U) {m_nextId = 0; return 0;} //should not happen
    //sensor
    pwmLedSet(m_nextId);
    delay(10);
    const float lux = veml.readLux(VEML_LUX_AUTO);
    m_data[m_nextId] = lux;
    m_nextId++;
    //end?
    if(m_nextId == 4096)
    {
      //save
      File f = LittleFS.open("/calibration.bin", "w");
      if(!f) {return -1;}
      f.write((const char *)m_data, 4096*sizeof(float));
      f.close();
      //end
      stop();
      return 0;
    }
    return m_nextId;
  }
  return 0;
}

void Calibration::stop()
{
  m_running = false;
}

float Calibration::getLastValue()
{
  float result = 0.0;
  if(m_running)
  {
    if((m_nextId>0) && (m_nextId<4096)) // >0 to use m_nextId-1!
    {
      result = m_data[m_nextId-1];
    } 
  }
  return result;
}

float Calibration::testSensor()
{
  return veml.readLux(VEML_LUX_AUTO);
}

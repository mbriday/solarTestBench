
#include "sequencer.h"
#include "pwmLed.h"

// external function to provide..
void sendLog(String str);

Sequencer::Sequencer()
    : m_filename(""), m_sequenceLoaded(false), m_sequenceRunning(false), m_nextTimestamp(0), m_nextPWM(0) {}

bool Sequencer::setFile(String filename)
{
  stop(); // stop any running sequence
  this->m_filename = filename;
  return true;
}

uint16_t Sequencer::computeChecksum(const uint8_t *data, size_t length)
{
  uint16_t sum = 0;
  for (size_t i = 0; i < length; i++)
  {
    sum += data[i];
  }
  return sum;
}

uint32_t Sequencer::computeCRC32(uint32_t crc, uint8_t data)
{
  crc ^= data;
  for (int j = 0; j < 8; j++)
  {
    if (crc & 1)
      crc = (crc >> 1) ^ 0xEDB88320;
    else
      crc >>= 1;
  }

  return crc;
}

bool Sequencer::verifyFile()
{
  if (!m_filename)
    return false;
  m_sequenceFile = LittleFS.open(m_filename, "r");
  if (!m_sequenceFile)
  {
    sendLog("Failed to open sequence file!");
    return false;
  }

  SequenceHeader header;
  m_sequenceFile.read((uint8_t *)&header, sizeof(header));
  if (header.magic != HEADER_MAGIC || header.version != HEADER_VERSION)
  {
    sendLog("Invalid header!");
    m_sequenceFile.close();
    return false;
  }
  sendLog("nb entries!" + String(header.num_entries));
  char description[128];
  m_sequenceFile.read((uint8_t *)description, 128);
  uint16_t storedChecksum;
  m_sequenceFile.read((uint8_t *)&storedChecksum, sizeof(storedChecksum));

  uint8_t *headerData = (uint8_t *)&header;
  uint16_t computedChecksum = computeChecksum(headerData, sizeof(header)) + computeChecksum((uint8_t *)description, 128);
  if (computedChecksum != storedChecksum)
  {
    sendLog("Header checksum mismatch!");
    m_sequenceFile.close();
    return false;
  }

  // CRC32 check for data
  m_sequenceFile.seek(sizeof(SequenceHeader) + 128 + sizeof(uint16_t), SeekSet);
  uint32_t crc = 0xFFFFFFFF;
  const int nbBytes = sizeof(SequenceEntry) * header.num_entries;
  for (int i = 0; i < nbBytes; i++)
  {
    if (m_sequenceFile.available())
    {
      uint8_t byte = m_sequenceFile.read();
      crc = computeCRC32(crc, byte);
    }
  }
  crc ^= 0xFFFFFFFF;

  uint32_t storedCRC;
  m_sequenceFile.seek(m_sequenceFile.size() - sizeof(uint32_t), SeekSet);
  m_sequenceFile.read((uint8_t *)&storedCRC, sizeof(uint32_t));

  if (crc != storedCRC)
  {
    sendLog("Data CRC32 mismatch!");
    m_sequenceFile.close();
    return false;
  }

  m_sequenceFile.close();
  return true;
}

bool Sequencer::load()
{
  if (!m_filename)
    return false;
  m_sequenceFile = LittleFS.open(m_filename, "r");
  if (!m_sequenceFile)
  {
    sendLog("Failed to open sequence file!");
    return false;
  }
  m_sequenceFile.read((uint8_t *)&m_header, sizeof(SequenceHeader));
  //first, get duration
  //size - CRC32 - last entry
  m_sequenceFile.seek(m_sequenceFile.size() - sizeof(uint32_t) - sizeof(SequenceEntry), SeekSet);
  SequenceEntry lastEntry;
  m_sequenceFile.read((uint8_t *)&lastEntry, sizeof(SequenceEntry));
  m_duration = lastEntry.ts * m_header.sampling_duration;
  // then, set at the beginning of entries:
  // set at beginning of entries
  m_sequenceFile.seek(sizeof(SequenceHeader) + 128 + sizeof(uint16_t), SeekSet);
  m_sequenceLoaded = true;
  m_sequenceRunning = false;
  return true;
}

uint32_t Sequencer::getDuration(String filename)
{
  File f = LittleFS.open(filename, "r");
  if(!f) {
    sendLog("Failed to open sequence file!" + filename);
    return 0;
  }
  SequenceHeader header;
  f.read((uint8_t *)&header, sizeof(SequenceHeader));
  //size - CRC32 - last entry
  f.seek(f.size() - sizeof(uint32_t) - sizeof(SequenceEntry), SeekSet);
  SequenceEntry lastEntry;
  f.read((uint8_t *)&lastEntry, sizeof(SequenceEntry));
  f.close();

  return lastEntry.ts * header.sampling_duration;
}

uint32_t Sequencer::getPercentageDone()
{
  const uint32_t done = millis()-m_startOffset;
  uint32_t percentage;
  if(m_duration > 0) percentage = (100*done)/m_duration;
  else percentage = 0;
  return percentage; 
}


int Sequencer::update()
{
  int result = 0;
  // check if the sequence is loaded and running
  // and if the file is still available
  if (!m_sequenceLoaded || !m_sequenceRunning || !m_sequenceFile.available())
    return 3;

  // is it the deadline?
  uint32_t currentMillis = millis();
  if (currentMillis < m_nextTimestamp)
    return 0;
  // yes, update PWM..
  pwmLedSet(m_nextPWM);

  // and read the next entry
  SequenceEntry entry;
  m_sequenceFile.read((uint8_t *)&entry, sizeof(entry));
  m_nextTimestamp = entry.ts * m_header.sampling_duration + m_startOffset;
  m_nextPWM = constrain(entry.value, 0, 4095);

  result = 1;
  if (!m_sequenceFile.available()) //end of seq?
  {
    m_sequenceFile.close();
    m_sequenceLoaded = false;
    m_sequenceRunning = false;
    result = 2;
  }
  return result;
}

void Sequencer::start()
{
  if (!m_sequenceLoaded)
  {
    if (!load())
      return;
  }
  m_startOffset = millis();
  //update will react now.
  m_nextTimestamp = 0;
  m_nextPWM = 0;
  // start the sequence
  m_sequenceRunning = true;
}


void Sequencer::stop()
{
  if (m_sequenceLoaded)
  {
    m_sequenceFile.close();
  }
  m_sequenceLoaded = false;
  m_sequenceRunning = false;
  pwmLedSet(0);
}

int32_t Sequencer::loop()
{
  static int deadlinePercentageDone = 0;
  int32_t ret = -1; 
  switch(update())
  {
    case 0: // no update
      if(millis() > deadlinePercentageDone)
      {
        deadlinePercentageDone = millis()+250;
        ret = getPercentageDone();
      }
      break;
    case 1: // update pwm
      if(millis() > deadlinePercentageDone)
      {
        deadlinePercentageDone = millis()+250;
        ret = getPercentageDone();
      }
      break;
    case 2: // end of sequence
      ret = 100;
      break;
    case 3: // error (no sequence to run)
      break;
  }
  return ret;
}

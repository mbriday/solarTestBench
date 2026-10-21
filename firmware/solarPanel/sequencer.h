#ifndef SEQUENCER_H
#define SEQUENCER_H

#include <Arduino.h>
#include <LittleFS.h>

/**
 * Sequencer for executing PWM updates based on a pre-defined binary sequence file stored in LittleFS.
 *
 * File format (Little Endian):
 * - Header:
 *   - 16-bit magic number (0xCAFE)
 *   - 16-bit version (0x01)
 *   - 32-bit sampling duration
 *   - 32-bit number of entries
 *   - Variable-length description (128 bytes max)
 *   - 16-bit header checksum
 * - Sequence Entries:
 *   - 32-bit timestamp
 *   - 16-bit PWM value
 * - Final 32-bit CRC32 checksum
 */

#define HEADER_MAGIC 0xCAFE
#define HEADER_VERSION 0x01

struct __attribute__((packed)) SequenceEntry {
    uint32_t ts;
    uint16_t value;
};

struct __attribute__((packed)) SequenceHeader {
    uint16_t magic;
    uint16_t version;
    uint32_t sampling_duration;
    uint32_t num_entries;
};

class Sequencer {
private:
    String m_filename;
    File m_sequenceFile;
    uint32_t m_startOffset;   //in ms
    uint32_t m_nextTimestamp;
    uint16_t m_nextPWM;
    uint32_t m_duration;       //in ms 
    bool m_sequenceLoaded;   // true if the sequence file is loaded
    bool m_sequenceRunning;  // true if the sequence is running
    SequenceHeader m_header;

    uint16_t computeChecksum(const uint8_t *data, size_t length);
    uint32_t computeCRC32(uint32_t crc, uint8_t data);
public:
    Sequencer();
    bool setFile(String filename);
    bool verifyFile();
    bool load();
    /** @return
     * 0 : no update (ok)
     * 1 : update pwm
     * 2 : end of sequence
     * 3 : error
     */
    int update();
    void start();
    void stop();
    bool isRunning() {return m_sequenceRunning;};
    static uint32_t getDuration(String filename);
    uint32_t getPercentageDone(); //between 0 and 100, during update.
    /** main loop of the sequence
     * @return * -1 if there is no modification of the pwm
     *         * the new percentage of the sequence done if a step is done (or 250ms as been elapsed)
     *         * 100 at the end of the sequence.
     */
    int32_t loop();
 };

#endif // SEQUENCER_H
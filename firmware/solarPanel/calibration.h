#ifndef __CALIBRATION_H__
#define __CALIBRATION_H__

class Calibration {
  bool m_running; //calibration is running!
  bool m_init;    //setup() has been called
  unsigned int m_nextId;
  float m_data[4096];

  public:
  Calibration();
  /**
   * @return *  0 in case of success;
   *         * -1 if the sensor is not well initialized
  */
  int setup();
  void start();
  /// @brief 1 calibration (out of 4096). 
  /// During the last step, the tabular is written back on the filesystem.
  /// @return the number of steps done. If 0, calibration is ended
  unsigned int loop();
  void stop();
  bool isRunning() {return m_running;};
  float getLastValue();
  float testSensor();
};

#endif //__CALIBRATION_H__
#include "pwmLed.h"

#define PWM1 0
#define PWM2 6
#define PWM3 9
#define PWM4 2
#define PWM5 10
#define PWM6 15
#define PWM7 20
#define PWM8 13

#define FAN1 7
#define FAN2 1

#include <Arduino.h>
#include "hardware/pwm.h"

const uint32_t PWM_pins[] = { PWM1, PWM2, PWM3, PWM4, PWM5, PWM6, PWM7, PWM8 };
static uint PWM_slice[8];
const uint16_t counter = 511; //range [0-511] => 512 values. freq: 133MHz/512 => ~259KHz

void pwmLedSetup()
{
  pwm_clkdiv_mode(PWM_DIV_FREE_RUNNING); //full speed => 133MHz
  for (int i=0; i < 8; i++) {
    gpio_set_function(PWM_pins[i], GPIO_FUNC_PWM);      //set alternative function PWM on pins
    PWM_slice[i] = pwm_gpio_to_slice_num(PWM_pins[i]);
    pwm_set_wrap(PWM_slice[i], counter);                //maximal value: counter => counter+1 steps
    pwm_set_gpio_level(PWM_pins[i], 0);                 //initial comparison value
    pwm_set_counter(PWM_slice[i], (i*64));              //initial timer value: We have 512 values => offset between each pwm: 64
  }
  //fan
  gpio_set_function(FAN1, GPIO_FUNC_PWM);               //set alternative function PWM on FAN pins
  gpio_set_function(FAN2, GPIO_FUNC_PWM);               //set alternative function PWM on FAN pins
  pwm_set_gpio_level(FAN1, 0);                          //initial comparison value
  pwm_set_gpio_level(FAN2, 0);                          //initial comparison value

  pwm_set_mask_enabled(0b11111111);                     //all pwm slices are running together
}

//delay time of TC4424A (Power MOSFET Driver) is 48ns Max
//@133MHz, it is done in 6.3 steps.
//We have to be sure that the minimal value is above 7 for each channel (or 0).

#define MINSTEPS 7

/* between 0 and 4095
 0    => no light
 4095 => full pwm on each channel
 3 cases:
  * above 8*MINSTEP, the duty is distributed equally between each PWM channel
  * below, the duty is divided between channels so that no channel can be below MINSTEP
  * below MINSTEP => 0
  ** code fully tested **
  0 =>   0-  0-  0-  0-  0-  0-  0-  0
  [..]
  6 =>   0-  0-  0-  0-  0-  0-  0-  0
  7 =>   7-  0-  0-  0-  0-  0-  0-  0
  8 =>   8-  0-  0-  0-  0-  0-  0-  0
  9 =>   9-  0-  0-  0-  0-  0-  0-  0
 10 =>  10-  0-  0-  0-  0-  0-  0-  0
 11 =>  11-  0-  0-  0-  0-  0-  0-  0
 12 =>  12-  0-  0-  0-  0-  0-  0-  0
 13 =>  13-  0-  0-  0-  0-  0-  0-  0
 14 =>   7-  7-  0-  0-  0-  0-  0-  0
 15 =>   7-  8-  0-  0-  0-  0-  0-  0
 16 =>   7-  9-  0-  0-  0-  0-  0-  0
 17 =>   7- 10-  0-  0-  0-  0-  0-  0
 18 =>   7- 11-  0-  0-  0-  0-  0-  0
 19 =>   7- 12-  0-  0-  0-  0-  0-  0
 20 =>   7- 13-  0-  0-  0-  0-  0-  0
 [..]
 71 =>   9-  9-  9-  9-  9-  9-  9-  8
 72 =>   9-  9-  9-  9-  9-  9-  9-  9
 73 =>  10-  9-  9-  9-  9-  9-  9-  9
 74 =>  10- 10-  9-  9-  9-  9-  9-  9
 75 =>  10- 10- 10-  9-  9-  9-  9-  9
 76 =>  10- 10- 10- 10-  9-  9-  9-  9
 77 =>  10- 10- 10- 10- 10-  9-  9-  9
 78 =>  10- 10- 10- 10- 10- 10-  9-  9
 [..]
*/
void pwmLedSet(uint16_t duty)
{
  //saturate
  uint16_t setpoint = duty;
  if(duty > 4095) setpoint = 4095;
  if(duty > (MINSTEPS*8)) //nominal case, each channel above MINSTEPS
  {
    const int common = setpoint >> 3; //common part to each channel
    const uint8_t offset = setpoint & 7;
    for (int i=0; i < 8; i++) {
      if(i < offset) 
        pwm_set_gpio_level(PWM_pins[i], common+1);
      else
        pwm_set_gpio_level(PWM_pins[i], common);
    }
  } else { //special case. We may be below the minimal value (around 1% of the full range...)
    // example: 30, MINSTEPS = 7
    // we should get: 7 7 7 9 0 0 0 0
    int nbFull = duty/MINSTEPS;
    if(nbFull == 0) { // nothing between 0 and MINSTEPS
      for(int i=0;i<8;i++) pwm_set_gpio_level(PWM_pins[i], 0);
    } //at least one is at MINSTEPS: nbFull > 0
    else {
      for(int i=0;i<(nbFull-1);i++)
      {
        pwm_set_gpio_level(PWM_pins[i], MINSTEPS);
        duty -= MINSTEPS;
      }
      pwm_set_gpio_level(PWM_pins[nbFull-1], duty);
      for(int i= nbFull; i < 8; i++) {
        pwm_set_gpio_level(PWM_pins[i], 0);
      }
    }
  }
}

void fanSet(int id, int duty)
{
  //default FAN1
  int fanId = FAN1;
  if(id==1) fanId = FAN2;

  int value = duty;
  if(value > 511) value = 511;
  if(value < MINSTEPS) value = 0;
  pwm_set_gpio_level(fanId, value);
}

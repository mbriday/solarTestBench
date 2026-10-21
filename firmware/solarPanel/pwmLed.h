#ifndef __PWM_LED_H__
#define __PWM_LED_H__
#include <stdint.h>

/** init PWM system (both leds and PWM)
 */
void pwmLedSetup();

/** set light level on leds
 * 3 cases:
 * * above 8*MINSTEP, the duty is distributed equally between each PWM channel
 * * below, the duty is divided between channels so that no channel can be below MINSTEP
 * * below MINSTEP => 0
 *  \param duty duty cycle between 0 and 4095
 *     * 0    => no light
 *     * 4095 => full pwm on each channel
 **/
void pwmLedSet(uint16_t duty);

/** set fan duty cycle
 * \param id 0 or 1 is fan id
 * \param duty duty cycle between 0 and 511
 */
void fanSet(int id, int duty);


#endif

#ifndef __OTA_H__
#define __OTA_H__

/* basic wrapper of the OTA functions
 * doc here: https://arduino-pico.readthedocs.io/en/latest/ota.html
 * We use the automatic signature (Mac/Linux).
 *  
 * 1 - in the example folder, generate the 2 public/private keys
 * `openssl genrsa -out private.key 2048`
 * `openssl rsa -in private.key -outform PEM -pubout -out public.key`
 * In practice, we sign the hash code with the private key => signature.
 * And it works by itself, you'll get ‘Enabling binary signing’ at compile time :)
 *  
 * 2 - in the sketch, you need a 
 * - ArduinoOTA.begin(); in the setup()
 * ArduinoOTA.handle(); in the loop()
 * - possibly events on the progress of the update. 
 *  
 * 3 - in tool->Flash size, you need to define an FS zone: 1MB, 1MB for example. 
 * I needed to make an initial sketch that formats the FS.
 *  
 * 3 - the next time, you only choose the port->tool.
 *  
 */

void setupOTA();
void loopOTA();

#endif

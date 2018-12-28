// Hardware_pins.h

#ifndef _HARDWARE_PINS_h
#define _HARDWARE_PINS_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#define DHT_PIN         8

#define SIM_RESET_PIN   9
#define SIM_TX_PIN      10
#define SIM_RX_PIN      11

#define LED_STT         12

#define WATER_EMPTY     A0
#define WATER_FULL      A1

#define LED_ON();       digitalWrite(LED_STT, LOW);
#define LED_OFF();      digitalWrite(LED_STT, HIGH);

#define MAX_PHONE_TOTAL		5
#define MAX_PHONE_LENGTH	15

#define Sprint(x)	 {LED_ON(); Serial.print(x);   LED_OFF();}
#define Sprintln(x)	 {LED_ON(); Serial.println(x); LED_OFF();}

#endif


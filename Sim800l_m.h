#ifndef Sim800l_h
#define Sim800l_h
#include "SoftwareSerial.h"
#include "Arduino.h"
#include "Hardware_pins.h"


#define RX_PIN SIM_TX_PIN
#define TX_PIN SIM_RX_PIN	
#define RESET_PIN SIM_RESET_PIN   // pin to the reset pin sim800l

#define LED true // used for indicator led, in case that you don want set to false . 
#define LED_PIN LED_STT //pin to indicate states. 



void led_blink(unsigned long interval = 3000);

class Sim800l
{
private:
	int _timeout;
	String _buffer;
	String _readSerial(long t = 5000);


public:

	void begin();
	void reset();

	bool waitReady(unsigned long timeout = 5000);
	// Methods for calling || Funciones de llamadas. 
	bool answerCall();
	void callNumber(char* number);
	bool hangoffCall();
	uint8_t getCallStatus();
	//Methods for sms || Funciones de SMS.
	bool sendSms(const char* number, const char* text);
	String readSms(uint8_t index); //return all the content of sms
	String getNumberSms(uint8_t index); //return the number of the sms..   
	bool delAllSms();     // return :  OK or ERROR .. 

	void signalQuality();
	void setPhoneFunctionality();
	void activateBearerProfile();
	void deactivateBearerProfile();
	//get time with the variables by reference
	void RTCtime(int *day, int *month, int *year, int *hour, int *minute, int *second);
	String dateNet(); //return date,time, of the network
	bool updateRtc(int utc);  //Update the RTC Clock with de Time AND Date of red-.
};

#endif 
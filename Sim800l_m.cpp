#include "Arduino.h"
#include "Sim800l_m.h"
#include <SoftwareSerial.h>


SoftwareSerial SIM(RX_PIN, TX_PIN);

//String _buffer;

#define SIM_print(x){		\
	LED_ON();				\
	SIM.print(x);			\
	Serial.println();		\
	LED_OFF();				\
}

String SIM_readString() {
	String s = SIM.readString();
	Serial.println(s);
	return s;
}

void Sim800l::begin() {
	SIM.begin(9600);
#if (LED) 
	pinMode(OUTPUT, LED_PIN);
	LED_ON();
#endif
	_buffer.reserve(255); //reserve memory to prevent intern fragmention
}


//
//PRIVATE METHODS
//
String Sim800l::_readSerial(long t) {
	_timeout = 0;
	while (!SIM.available() && _timeout < t)
	{
		delay(13);
		_timeout++;
	}
	if (SIM.available()) {
		return SIM_readString();
	}
	return "";
}


//
//PUBLIC METHODS
//

void Sim800l::reset() {
#if (LED)
	digitalWrite(LED_PIN, 1);
#endif 
	digitalWrite(RESET_PIN, 1);
	delay(1000);
	digitalWrite(RESET_PIN, 0);
	delay(1000);
	// wait for the module response

	SIM_print(F("AT\r\n"));
	while (_readSerial().indexOf("OK") == -1) {
		SIM_print(F("AT\r\n"));
	}

	//wait for sms ready
	while (_readSerial().indexOf("SMS") == -1) {
	}
#if (LED)
	digitalWrite(LED_PIN, 0);
#endif 
}

bool Sim800l::waitReady(unsigned long timeout) {
	unsigned long t = millis();
	SIM_print(F("AT\r\n"));
	while (_readSerial(500).indexOf("OK") == -1) {
		SIM_print(F("AT\r\n"));
		if (millis() - t > timeout) {
			return false;
		}
	}
	return true;
}

void Sim800l::setPhoneFunctionality() {
	/*AT+CFUN=<fun>[,<rst>]
	Parameters
	<fun> 0 Minimum functionality
	1 Full functionality (Default)
	4 Disable phone both transmit and receive RF circuits.
	<rst> 1 Reset the MT before setting it to <fun> power level.
	*/
	SIM_print(F("AT+CFUN=1\r\n"));
}


void Sim800l::signalQuality() {
	/*Response
	+CSQ: <rssi>,<ber>Parameters
	<rssi>
	0 -115 dBm or less
	1 -111 dBm
	2...30 -110... -54 dBm
	31 -52 dBm or greater
	99 not known or not detectable
	<ber> (in percent):
	0...7 As RXQUAL values in the table in GSM 05.08 [20]
	subclause 7.2.4
	99 Not known or not detectable
	*/
	SIM_print(F("AT+CSQ\r\n"));
	Serial.println(_readSerial());
}


void Sim800l::activateBearerProfile() {
	SIM_print(F(" AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\" \r\n")); _buffer = _readSerial();  // set bearer parameter 
	SIM_print(F(" AT+SAPBR=3,1,\"APN\",\"internet\" \r\n")); _buffer = _readSerial(); // set apn  
	SIM_print(F(" AT+SAPBR=1,1 \r\n")); delay(1200); _buffer = _readSerial();// activate bearer context
	SIM_print(F(" AT+SAPBR=2,1\r\n ")); delay(3000); _buffer = _readSerial(); // get context ip address
}


void Sim800l::deactivateBearerProfile() {
	SIM_print(F("AT+SAPBR=0,1\r\n "));
	delay(1500);
}



bool Sim800l::answerCall() {
	SIM_print(F("ATA\r\n"));
	_buffer = _readSerial();
	//Response in case of data call, if successfully connected 
	if ((_buffer.indexOf("OK")) != -1) return true;
	else return false;
}


void  Sim800l::callNumber(char* number) {
	SIM_print(F("ATD"));
	SIM_print(number);
	SIM_print(F("\r\n"));
}



uint8_t Sim800l::getCallStatus() {
	/*
	  values of return:

	 0 Ready (MT allows commands from TA/TE)
	 2 Unknown (MT is not guaranteed to respond to tructions)
	 3 Ringing (MT is ready for commands from TA/TE, but the ringer is active)
	 4 Call in progress

	*/
	SIM_print(F("AT+CPAS\r\n"));
	_buffer = _readSerial();
	return _buffer.substring(_buffer.indexOf("+CPAS: ") + 7, _buffer.indexOf("+CPAS: ") + 9).toInt();
}



bool Sim800l::hangoffCall() {
	SIM_print(F("ATH\r\n"));
	_buffer = _readSerial();
	if ((_buffer.indexOf("OK")) != -1) return true;
	else return false;
}



bool Sim800l::sendSms(const char* number, const char* text) {

	SIM_print(F("AT+CMGF=1\r")); //set sms to text mode  
	_buffer = _readSerial(2000);
	SIM_print(F("AT+CMGS=\""));  // command to send sms
	SIM_print(number);
	SIM_print(F("\"\r"));
	_buffer = _readSerial(2000);
	SIM_print(text);
	SIM_print("\r");
	//change delay 100 to readserial	
	_buffer = _readSerial();
	SIM_print((char)26);
	_buffer = _readSerial();
	//expect CMGS:xxx   , where xxx is a number,for the sending sms.
	if (((_buffer.indexOf("CMGS")) != -1)) {
		return true;
	}
	else {
		return false;
	}
}


String Sim800l::getNumberSms(uint8_t index) {
	_buffer = readSms(index);
	Serial.println(_buffer.length());
	if (_buffer.length() > 10) //avoid empty sms
	{
		uint8_t _idx1 = _buffer.indexOf("+CMGR:");
		_idx1 = _buffer.indexOf("\",\"", _idx1 + 1);
		return _buffer.substring(_idx1 + 3, _buffer.indexOf("\",\"", _idx1 + 4));
	}
	else {
		return "";
	}
}



String Sim800l::readSms(uint8_t index) {
	SIM_print(F("AT+CMGF=1\r"));
	if ((_readSerial().indexOf("ER")) == -1) {
		SIM_print(F("AT+CMGR="));
		SIM_print(index);
		SIM_print("\r");
		_buffer = _readSerial();
		if (_buffer.indexOf("CMGR:") != -1) {
			return _buffer;
		}
		else return "";
	}
	else
		return "";
}


bool Sim800l::delAllSms() {
	SIM_print(F("at+cmgda=\"del all\"\n\r"));
	_buffer = _readSerial();
	if (_buffer.indexOf("OK") != -1) { return true; }
	else { return false; }

}


void Sim800l::RTCtime(int *day, int *month, int *year, int *hour, int *minute, int *second) {
	SIM_print(F("at+cclk?\r\n"));
	// if respond with ERROR try one more time. 
	_buffer = _readSerial();
	if ((_buffer.indexOf("ERR")) != -1) {
		delay(50);
		SIM_print(F("at+cclk?\r\n"));
	}
	if ((_buffer.indexOf("ERR")) == -1) {
		_buffer = _buffer.substring(_buffer.indexOf("\"") + 1, _buffer.lastIndexOf("\"") - 1);
		*year = _buffer.substring(0, 2).toInt();
		*month = _buffer.substring(3, 5).toInt();
		*day = _buffer.substring(6, 8).toInt();
		*hour = _buffer.substring(9, 11).toInt();
		*minute = _buffer.substring(12, 14).toInt();
		*second = _buffer.substring(15, 17).toInt();
	}
}

//Get the time  of the base of GSM
String Sim800l::dateNet() {
	SIM_print(F("AT+CIPGSMLOC=2,1\r\n "));
	_buffer = _readSerial();

	if (_buffer.indexOf("OK") != -1) {
		return _buffer.substring(_buffer.indexOf(":") + 2, (_buffer.indexOf("OK") - 4));
	}
	else
		return "0";
}

// Update the RTC of the module with the date of GSM. 
bool Sim800l::updateRtc(int utc) {

	activateBearerProfile();
	_buffer = dateNet();
	deactivateBearerProfile();

	_buffer = _buffer.substring(_buffer.indexOf(",") + 1, _buffer.length());
	String dt = _buffer.substring(0, _buffer.indexOf(","));
	String tm = _buffer.substring(_buffer.indexOf(",") + 1, _buffer.length());

	int hour = tm.substring(0, 2).toInt();
	int day = dt.substring(8, 10).toInt();

	hour = hour + utc;

	String tmp_hour;
	String tmp_day;
	//TODO : fix if the day is 0, this occur when day is 1 then decrement to 1, 
	//       will need to check the last month what is the last day .  
	if (hour < 0) {
		hour += 24;
		day -= 1;
	}
	if (hour < 10) {

		tmp_hour = "0" + String(hour);
	}
	else {
		tmp_hour = String(hour);
	}
	if (day < 10) {
		tmp_day = "0" + String(day);
	}
	else {
		tmp_day = String(day);
	}
	//for debugging
  //Serial.println("at+cclk=\""+dt.substring(2,4)+"/"+dt.substring(5,7)+"/"+tmp_day+","+tmp_hour+":"+tm.substring(3,5)+":"+tm.substring(6,8)+"-03\"\r\n");
	SIM_print("at+cclk=\"" + dt.substring(2, 4) + "/" + dt.substring(5, 7) + "/" + tmp_day + "," + tmp_hour + ":" + tm.substring(3, 5) + ":" + tm.substring(6, 8) + "-03\"\r\n");
	if ((_readSerial().indexOf("ER")) != -1) { return false; }
	else return true;


}

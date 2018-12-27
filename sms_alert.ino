/*
 *    PINOUT:
 *        _____________________________
 *       |  ARDUINO UNO >>>   SIM800L  |
 *        -----------------------------
 *             GND      >>>   GND
 *        RX   10       >>>   TX
 *        TX   11       >>>   RX
 *       RESET  9       >>>   RST
 *
 *		 DHT_PIN		8
 *		 WATER_EMPTY	5
 *		 WATER_FULL		6
 *
*/

//https://www.facebook.com/hoanglong171
//{"cmd":"setphone","phone":["0387845097","+84387845097","387845097","84387845097","01687845097"]}
//{"cmd":"config","t0":20.1,"t1":35,"h0":50,"h1":99}

#include <EEPROM.h>
#include "ArduinoJson.h"
#include "DHT.h"
#include "Button.h"
#include "Sim800l_m.h"
#include "SoftwareSerial.h"

#define VERSION "0.2.1"


#define DHT_PIN			8
#define WATER_EMPTY		A3
#define WATER_FULL		A4


#define MAX_PHONE_TOTAL		5
#define MAX_PHONE_LENGTH	15
char phone[MAX_PHONE_TOTAL][MAX_PHONE_LENGTH];

struct config {
	float temp_min;
	float temp_max;
	float humi_min;
	float humi_max;
} config;

Sim800l sim;
String res_phone; //save sdt gửi đến -> hẹn gửi đi

DHT dht(DHT_PIN, DHT11);
bool isDhtReady = false;
float temp;
float humi;


#define PULLUP true        //To keep things simple, we use the Arduino's internal pullup resistor.
#define INVERT true        //Since the pullup resistor will keep the pin high unless the
//switch is closed, this is negative logic, i.e. a high state
//means the button is NOT pressed. (Assuming a normally open switch.)
#define DEBOUNCE_MS 500     //A debounce time of 20 milliseconds usually works well for tactile button switches.

Button WaterEmpty(WATER_EMPTY, PULLUP, INVERT, DEBOUNCE_MS);
Button WaterFull(WATER_FULL, PULLUP, INVERT, DEBOUNCE_MS);

int freeRam() {
	extern int __heap_start, *__brkval;
	int v;
	return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}
#define ram(x) Serial.println(String(F("RAM ")) + String(F(x)) + " " + String(freeRam()));

void printConfig() {
	Serial.print(F("t0\t"));
	Serial.println(config.temp_min);
	Serial.print(F("t1\t"));
	Serial.println(config.temp_max);
	Serial.print(F("h0\t"));
	Serial.println(config.humi_min);
	Serial.print(F("h1\t"));
	Serial.println(config.humi_max);
}
void printPhones() {
	Serial.println(F("Idx\tLen\tPhone"));
	for (int i = 0; i < 5; i++)
	{
		String p = getPhone(i);
		Serial.print(i);
		Serial.print("\t");
		Serial.print(p.length());
		Serial.print("\t");
		Serial.print(p.c_str());
		Serial.println();
	}
}
void saveConfig() {
	EEPROM.put(100, config);
	Serial.println(F("\r\nSave configs"));
	printConfig();
}
void loadConfig() {
	EEPROM.get(100, config);
	Serial.println(F("\r\nLoad configs"));
	printConfig();
}
void savePhones() {
	EEPROM.put(0, phone);
	Serial.println(F("\r\nSave phone list"));
	printPhones();
}

void loadPhones() {
	EEPROM.get(0, phone);
	Serial.println(F("\r\nLoad phone list"));
	printPhones();
}


String getPhone(int index) {
	if (index < 0 || index > MAX_PHONE_TOTAL) {
		return;
	}
	String p;
	for (int i = 0; i < MAX_PHONE_LENGTH; i++)
	{
		char c = phone[index][i];
		if (isAlphaNumeric(c)) {
			p += c;
		}
	}
	p.trim();
	if (isPhoneNumber(p)) {
		p = formatStandardPhoneNumber(p);
	}
	return p;
}

bool isPhoneNumber(const String& number) {
	if (number.length() <= 0) {
		return false;
	}
	for (int i = 0; i < number.length(); i++) {
		char c = number.charAt(i);
		if (!isdigit(c) && (c != '+')) {
			return false;
		}
	}
	return true;
}
String formatStandardPhoneNumber(const String& number) {
	String n = number;
	if (n.length() == 0) {
		return "";
	}
	if (n.charAt(0) == '+') {
		return n;
	}
	if (n.startsWith(String(F("84")))) {
		n = "+" + n;
	}
	else if (n[0] == '0') {
		n = String(F("+84")) + n.substring(1);
	}
	else {
		n = String(F("+84")) + n;
	}
	return n;
}

String getConfigs() {
	String c = String(F("Configs"));
	c += String(F("\r\nt0=")) + String(config.temp_min);
	c += String(F("\r\nt1=")) + String(config.temp_max);
	c += String(F("\r\nh0=")) + String(config.humi_min);
	c += String(F("\r\nh1=")) + String(config.humi_max);
	return c;
}
String getPhoneList() {
	String p = String(F("Phones:\r\n"));
	for (int i = 0; i < MAX_PHONE_TOTAL; i++)
	{
		String n = getPhone(i);
		if (isPhoneNumber(n)) {
			p += n + String(F(";\r\n"));
		}
	}
	return p;
}

void command_execute(const String& cmd) {
	ram("exec");
	Serial.print(F("\r\nCommand ["));
	Serial.print(cmd.length());
	Serial.println(F("]"));
	Serial.println(cmd);
	Serial.println(F("--------"));

	if (cmd.startsWith(String(F("/config")))) {
		Serial.println(F("\r\nConfigs"));
		printConfig();

		if (isPhoneNumber(res_phone)) {
			ram("/config");
			if (sendSms(res_phone, getConfigs())) {
				sim.delAllSms();
			}
		}
		return;
	}
	else if (cmd.startsWith(String(F("/phone")))) {
		Serial.println(F("\r\nPhones"));
		printPhones();

		if (isPhoneNumber(res_phone)) {
			ram("/phone");
			if (sendSms(res_phone, getPhoneList())) {
				sim.delAllSms();
			}
		}
		return;
	}

	StaticJsonBuffer<170> buffer;
	JsonObject& command = buffer.parseObject(cmd);
	ram("json");
	if (!command.success()) {
		String err = F("Json Syntax Wrong");
		Serial.println(err);
		if (isPhoneNumber(res_phone)) {
			sendSms(res_phone, err);
			sim.delAllSms();
		}
		return;
	}
	String c = command[String(F("cmd"))].as<String>();
	if (c == String(F("setphone"))) {
		JsonArray& phoneArray = command[String(F("phone"))].asArray();
		if (!phoneArray.success()) {
			Serial.println(F("Phone Array Wrong"));
		}
		//clear phone[][]
		for (int r = 0; r < MAX_PHONE_TOTAL; r++) {
			for (int c = 0; c < MAX_PHONE_LENGTH; c++) {
				phone[r][c] = 0;
			}
		}

		int size = phoneArray.size();
		if (size > 5) {
			size = 5;
		}
		for (int i = 0; i < size; i++) {
			String p = phoneArray.get<String>(i);
			p.trim();
			if (p.length() > MAX_PHONE_LENGTH) {
				p = p.substring(0, MAX_PHONE_LENGTH);
			}
			for (int j = 0; j < p.length(); j++) {
				phone[i][j] = p.charAt(j);
			}
			if (p.length() < MAX_PHONE_LENGTH) {
				for (int d = p.length(); d < MAX_PHONE_LENGTH; d++) {
					phone[i][d] = 0;
				}
			}
		}

		savePhones();

		if (isPhoneNumber(res_phone)) {
			ram("res phone");
			if (sendSms(res_phone, getPhoneList())) {
				sim.delAllSms();
			}
		}
	}

	else if (c == String(F("config"))) {
		config.temp_min = command[String(F("t0"))].as<float>();
		config.temp_max = command[String(F("t1"))].as<float>();
		config.humi_min = command[String(F("h0"))].as<float>();
		config.humi_max = command[String(F("h1"))].as<float>();

		saveConfig();

		if (isPhoneNumber(res_phone)) {
			ram("res config");
			if (sendSms(res_phone, getConfigs())) {
				sim.delAllSms();
			}
		}
	}
}

bool sendSms(const String& number, const String&  text) {
	String n = formatStandardPhoneNumber(number);
	if (!isPhoneNumber(n)) {
		return false;
	}
	Serial.print(F("\r\nSMS << "));
	Serial.println(n);
	Serial.println(text);
	if (isPhoneNumber(n)) {
		ram("send sms");
		bool ret = sim.sendSms(n.c_str(), text.c_str());
		//bool ret = true;
		if (ret) {
			Serial.println(F("Send SMS success"));
			return true;
		}
		else {
			Serial.println(F("Send SMS failed"));
			return false;
		}
	}
	else {
		Serial.println(F("Number invalid"));
		return false;
	}
}

void readDHT(unsigned long interval = 3000) {
	static unsigned long t = millis();
	if (millis() - t > interval) {
		t = millis();
		float h = dht.readHumidity();
		float t = dht.readTemperature();

		if (isnan(h) || isnan(t)) {
			isDhtReady = false;
			Serial.println(F("Failed to read from DHT sensor!"));
			return;
		}

		isDhtReady = true;
		temp = t;
		humi = h;

		Serial.print(F("\r\nTemp = "));
		Serial.print(temp);
		Serial.print(F("\r\nHumi = "));
		Serial.println(humi);
	}
}
void alarmTemp() {
	static bool isAllowAlarmTemp = false;
	if (!isDhtReady) {
		return;
	}
	if (!isAllowAlarmTemp) {
		if (config.temp_min <= temp && temp <= config.temp_max) {
			isAllowAlarmTemp = true;
			Serial.print(F("Temp alarm reset "));
			Serial.println(temp);
		}
		return;
	}
	if (temp < config.temp_min) {
		isAllowAlarmTemp = false;
		String a = String(F("Canh bao nhiet do qua thap: ")) + String(temp);
		for (int i = 0; i < MAX_PHONE_TOTAL; i++) {
			sendSms(getPhone(i), a);
		}
	}
	if (temp > config.temp_max) {
		isAllowAlarmTemp = false;
		String a = String(F("Canh bao nhiet do qua cao: ")) + String(temp);
		for (int i = 0; i < MAX_PHONE_TOTAL; i++) {
			sendSms(getPhone(i), a);
		}
	}
}
void alarmHumi() {
	static bool isAllowAlarmHumi = false;
	if (!isDhtReady) {
		return;
	}
	if (!isAllowAlarmHumi) {
		if (config.humi_min <= humi && humi <= config.humi_max) {
			isAllowAlarmHumi = true;
			Serial.print(F("Humi alarm reset "));
			Serial.println(humi);
		}
		return;
	}
	if (humi < config.humi_min) {
		isAllowAlarmHumi = false;
		String a = String(F("Canh bao do am qua thap: ")) + String(humi);
		for (int i = 0; i < MAX_PHONE_TOTAL; i++) {
			sendSms(getPhone(i), a);
		}
	}
	if (humi > config.humi_max) {
		isAllowAlarmHumi = false;
		String a = String(F("Canh bao do am qua cao: ")) + String(humi);
		for (int i = 0; i < MAX_PHONE_TOTAL; i++) {
			sendSms(getPhone(i), a);
		}
	}
}
void alarmWaterEmpty() {
	static bool isAllowAlarmWaterEmpty = false;

	if (!isAllowAlarmWaterEmpty) {
		if (WaterEmpty.wasReleased()) {
			isAllowAlarmWaterEmpty = true;
		}
		return;
	}
	if (WaterEmpty.wasPressed() && WaterFull.isPressed()) {
		isAllowAlarmWaterEmpty = false;
		String a = F("Canh bao muc nuoc qua thap.");
		for (int i = 0; i < MAX_PHONE_TOTAL; i++) {
			sendSms(getPhone(i), a);
		}
	}
}
void alarmWaterFull() {
	static bool isAllowAlarmWaterFull = false;

	if (!isAllowAlarmWaterFull) {
		if (WaterFull.wasPressed()) {
			isAllowAlarmWaterFull = true;
		}
		return;
	}
	if (WaterFull.wasReleased() && WaterEmpty.isReleased()) {
		isAllowAlarmWaterFull = false;
		String a = F("Canh bao muc nuoc qua cao.");
		for (int i = 0; i < MAX_PHONE_TOTAL; i++) {
			sendSms(getPhone(i), a);
		}
	}
}

void readSensors(unsigned long interval) {
	readDHT(interval);

	WaterEmpty.read();
	WaterFull.read();
}
void alarm() {
	alarmTemp();
	alarmHumi();
	alarmWaterEmpty();
	alarmWaterFull();
}

void sms_handle(unsigned long timeout = 10000) {
	static unsigned long t = millis();
	if (millis() - t < timeout) {
		return;
	}
	t = millis();
	String msg = sim.readSms(1);
	msg.toLowerCase();
	msg.trim();
	ram("recv sms");
	int phoneRes = msg.indexOf(String(F("\"+")));
	if (phoneRes >= 0) {
		res_phone = msg.substring(phoneRes + 1);
		res_phone = res_phone.substring(0, res_phone.indexOf(String(F("\""))));
		Serial.println(res_phone);
	}

	int idx = msg.indexOf(String(F("\"\r\n")));
	if (idx >= 0) {
		msg = msg.substring(idx + 3, msg.length() - 4);
		msg.trim();
		Serial.print(F("SMS>> "));

		ram("before exec sms");
		command_execute(msg);

		Serial.println();
	}
	res_phone = "";
}

void command_handle() {
	if (Serial.available()) {
		Serial.println();
		String s = Serial.readString();
		s.trim();
		ram("before exec serial");
		command_execute(s);
	}

	sms_handle();
}

void setup() {
	Serial.begin(9600);
	Serial.setTimeout(200);
	delay(50);
	Serial.println(String(F("Version: ")) + String(F(VERSION)));

	ram("1");
	sim.begin();
	dht.begin();

	ram("2");
	loadConfig();
	loadPhones();

	ram("3");
	Serial.println();
	if (sim.waitReady()) {
		Serial.println(F("SIM ready"));
	}
	else {
		Serial.println(F("SIM unavailable"));
	}
	ram("run");
}

void loop() {
	readSensors(10000);
	alarm();
	checkram();
	command_handle();
}
void checkram() {
	static unsigned long t = millis();
	if (millis() - t > 10000) {
		t = millis();
		ram("");
	}
}


//void setup() {
//	Serial.begin(9600);
//	delay(1000);
//	ram();
//}
//void loop() {
//	delay(1);
//}
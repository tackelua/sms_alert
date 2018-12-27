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
//{"cmd":"setphone","phone":["0123456789","+84231564","84906161266","2222224","568498"]}
//{"cmd":"config","t0":20.1,"t1":35,"h0":50,"h1":99}

#include <EEPROM.h>
#include "ArduinoJson.h"
#include "DHT.h"
#include "Button.h"
#include "Sim800l_m.h"
#include "SoftwareSerial.h"

#define VERSION "0.2"


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
String sms;
String res_phone; //save sdt gửi đến -> hẹn gửi đi
String res_text;  //text hẹn gửi đi
DHT dht(DHT_PIN, DHT11);
bool isDhtReady = false;
float temp;
float humi;


#define PULLUP true        //To keep things simple, we use the Arduino's internal pullup resistor.
#define INVERT true        //Since the pullup resistor will keep the pin high unless the
//switch is closed, this is negative logic, i.e. a high state
//means the button is NOT pressed. (Assuming a normally open switch.)
#define DEBOUNCE_MS 20     //A debounce time of 20 milliseconds usually works well for tactile button switches.

Button WaterEmpty(WATER_EMPTY, PULLUP, INVERT, DEBOUNCE_MS);
Button WaterFull(WATER_FULL, PULLUP, INVERT, DEBOUNCE_MS);

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

bool isPhoneNumber(String number) {
	number.trim();
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
String formatStandardPhoneNumber(String number) {
	number.trim();
	String n = number;
	if (n.length() == 0) {
		return "";
	}
	if (n.charAt(0) == '+') {
		return n;
	}
	if (n.startsWith("84")) {
		n = "+" + n;
	}
	else if (n[0] == '0') {
		n = "+84" + n.substring(1);
	}
	else {
		n = "+84" + n;
	}
	return n;
}

String getConfigs() {
	String c = "Configs";
	c += "\r\nt0=" + String(config.temp_min);
	c += "\r\nt1=" + String(config.temp_max);
	c += "\r\nh0=" + String(config.humi_min);
	c += "\r\nh1=" + String(config.humi_max);
	return c;
}
String getPhoneList() {
	String p = "Phones:\r\n";
	for (int i = 0; i < MAX_PHONE_TOTAL; i++)
	{
		String n = getPhone(i);
		if (isPhoneNumber(n)) {
			p += n + ";\r\n";
		}
	}
	return p;
}

void command_execute(String cmd) {
	cmd.toLowerCase();
	cmd.trim();
	Serial.print(F("\r\nCommand ["));
	Serial.print(cmd.length());
	Serial.println(F("]"));
	Serial.println(cmd);
	Serial.println(F("--------"));
	if (cmd.startsWith("/info")) {
		Serial.println(F("\r\nConfigs"));
		printConfig();
		Serial.println(F("\r\nPhones"));
		printPhones();


		if (isPhoneNumber(res_phone)) {
			res_text = getConfigs() + "\r\n\r\n" + getPhoneList();
		}
		return;
	}

	int f = cmd.lastIndexOf("}");
	if (f >= 0) {
		cmd = cmd.substring(0, f + 1);
		cmd.trim();
		Serial.println(cmd);
	}
	StaticJsonBuffer<200> buffer;
	JsonObject& command = buffer.parseObject(cmd);
	if (!command.success()) {
		String err = "Json Syntax Wrong";
		Serial.println(err);
		if (isPhoneNumber(res_phone)) {
			sendSms(res_phone, err);
			res_phone = "";
		}
		return;
	}
	String c = command["cmd"].as<String>();
	if (c == "setphone") {
		JsonArray& phoneArray = command["phone"].asArray();
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
			res_text = getPhoneList();
		}
	}

	else if (c == "config") {
		config.temp_min = command["t0"].as<float>();
		config.temp_max = command["t1"].as<float>();
		config.humi_min = command["h0"].as<float>();
		config.humi_max = command["h1"].as<float>();
		saveConfig();

		if (isPhoneNumber(res_phone)) {
			res_text = getConfigs();
		}
	}
}

bool sendSms(String number, String text) {
	String n = formatStandardPhoneNumber(number);
	if (!isPhoneNumber(n)) {
		return false;
	}
	text += "\r\n--\r\nfb.com/tackelua";
	Serial.print(F("\r\nSMS << "));
	Serial.println(n);
	Serial.println(text);
	if (isPhoneNumber(n)) {
		//bool ret = sim.sendSms((char*)n.c_str(), (char*)text.c_str());
		bool ret = true;
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

void sms_handle(unsigned long timeout = 10000) {
	static unsigned long t = millis();
	if (millis() - t < timeout) {
		return;
	}
	t = millis();
	String msg = sim.readSms(1);
	msg.toLowerCase();
	msg.trim();
	int phoneRes = msg.indexOf("\"+");
	if (phoneRes >= 0) {
		res_phone = msg.substring(phoneRes + 1);
		res_phone = res_phone.substring(0, res_phone.indexOf("\""));
		Serial.println(res_phone);
	}

	int idx = msg.indexOf("\"\r\n");
	if (idx >= 0) {
		msg = msg.substring(idx + 3, msg.length() - 4);
		msg.trim();
		Serial.print(F("SMS>> "));
		sms = msg;
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
		}
		return;
	}
	if (temp < config.temp_min) {
		isAllowAlarmTemp = false;
		String a = "Canh bao nhiet do qua thap: " + String(temp);
		for (int i = 0; i < MAX_PHONE_TOTAL; i++) {
			sendSms(getPhone(i), a);
		}
	}
	if (temp > config.temp_max) {
		isAllowAlarmTemp = false;
		String a = "Canh bao nhiet do qua cao: " + String(temp);
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
		}
		return;
	}
	if (humi < config.humi_min) {
		isAllowAlarmHumi = false;
		String a = "Canh bao do am qua thap: " + String(humi);
		for (int i = 0; i < MAX_PHONE_TOTAL; i++) {
			sendSms(getPhone(i), a);
		}
	}
	if (humi > config.humi_max) {
		isAllowAlarmHumi = false;
		String a = "Canh bao do am qua cao: " + String(humi);
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
		String a = "Canh bao muc nuoc qua thap.";
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
		String a = "Canh bao muc nuoc qua cao.";
		for (int i = 0; i < MAX_PHONE_TOTAL; i++) {
			sendSms(getPhone(i), a);
		}
	}
}

void readSensors() {
	readDHT();

	static unsigned long t = millis();
	if (millis() - t > 300) {
		t = millis();
		WaterEmpty.read();
		WaterFull.read();
	}
}
void alarm() {
	alarmTemp();
	alarmHumi();
	alarmWaterEmpty();
	alarmWaterFull();
}

void command_handle() {
	if (Serial.available()) {
		String s = Serial.readString();
		s.trim();
		command_execute(s);
	}

	sms_handle();
	if (sms != "") {
		command_execute(sms);
		sms = "";

		if (res_phone != "") {
			sendSms(res_phone, res_text);
			res_phone = "";
		}

		Serial.println();
		sim.delAllSms();
	}
}

void setup() {
	Serial.begin(9600);
	Serial.setTimeout(100);
	delay(50);
	sms.reserve(200);
	res_phone.reserve(15);
	res_text.reserve(200);
	Serial.println("Version: " VERSION);

	sim.begin();
	dht.begin();

	//bool result = sim.sendSms("+84396141801", "the text go here");

	loadConfig();
	loadPhones();

	Serial.println();
	if (sim.waitReady()) {
		Serial.println(F("SIM ready"));
	}
	else {
		Serial.println(F("SIM unavailable"));
	}

}

void loop() {
	readSensors();
	alarm();
	command_handle();
}

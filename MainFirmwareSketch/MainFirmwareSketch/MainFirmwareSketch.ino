/*
 Name:		MainFirmwareSketch.ino
 Created:	3/21/2017 11:30:40 AM
 Author:	Maxim
*/
﻿#include <ESP8266WiFi.h>
#include <WiFiClient.h> 
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include "user_interface.h"
#include "ets_sys.h"
#include "osapi.h"
#include <ESP8266WiFiMulti.h>
//Подключение mqtt-библиотеки
#include <MQTTClient.h>
//Connect Wifi module
ESP8266WiFiMulti WiFiMulti;
WiFiClientSecure net; // sec
ESP8266WebServer server(80);

//Переменные для формирования стартовой страницы 
String netList;
String content;

//Переменная для отражения текущих параметрах сети
IPAddress myIP;

//Параметры подключения к IBM Bluemix
MQTTClient client;
#define ORG "kwxqcy" // имя организации
#define DEVICE_TYPE "YL38" // тип устройства
#define DEVICE_NAME = ""; // тип устройства

//Параметры подключения данных датчика
int sensorPin = A0;
int sensorValue = 0;
//Состояние устройста(0-Конфигурирование, 1- Работа
int DEV_MODE;

#define TOKEN "12345678" // - задаешь в IOT хабе
char mqttserver[] = ORG ".messaging.internetofthings.ibmcloud.com"; // подключаемся к Bluemix
char topic[] = "iot-2/evt/status/fmt/json";

char authMethod[] = "use-token-auth";
char token[] = TOKEN;

String clientID;
String deviceID;

char  cID[100];
boolean isBluemixConnected = false;
//----------------------------------------------

void setup()
{

	Serial.begin(115200);
	DEV_MODE = 1;
	//Вывод в консоль общей информации об устройстве----------
	Serial.printf("DEV_MODE = %d\n", DEV_MODE);
	Serial.printf("ESP8266 Chip id = %08X\n", ESP.getChipId());
	Serial.printf("ESP8266 FlashChipId = %08X\n", ESP.getFlashChipId());
	Serial.printf("ESP8266 ResetInfo = ");
	Serial.println(ESP.getResetInfo());
	Serial.printf("SP8266 VCC = ");
	Serial.println(ESP.getVcc());
	Serial.printf("ESP8266 SDK Version = ");
	Serial.println(ESP.getSdkVersion());
	Serial.printf("ESP8266 CoreVersion = ");
	Serial.println(ESP.getCoreVersion());
	Serial.printf("ESP8266 BootVersion = ");
	Serial.println(ESP.getBootVersion());

	Serial.print("Configuring access point...");
	WiFi.softAP(get_WIFI_STA_SSID(), get_WIFI_STA_PWD());
	WiFi.mode(WIFI_AP_STA);

	myIP = WiFi.softAPIP();
	Serial.print("AP IP address: ");
	Serial.println(myIP);
	server.on("/", handleRoot);
	server.on("/connect", connectToWifi);
	server.on("/scanwifi", scanNearWifiNetworks);
	server.on("/testInternet", testInternetConnection);

	server.begin();
	Serial.println("HTTP server started");

	connectSensor();

	if (DEV_MODE = 1) {
		connectToBluemix();
	}

	delay(1000);
	Serial.println();

}

void loop()
{
	server.handleClient();

	if (DEV_MODE = 1) {
		sendSensorDataToBluemix();
	}




}
//Функция формирования сообщения в формате JSON для отправки к IBM Bluemix
String buildMqttMessage(float param1, int32_t param2, String DeviceID)
{
	String pl = "{ \"d\" : {\"deviceid\":\"";
	pl += DeviceID;
	pl += "\",\"param1\":\"";
	pl += param1;
	pl += "\",\"param2\":\"";
	pl += param2;
	pl += "\"}}";
	return pl;
}
void messageReceived(String topic, String payload, char * bytes, unsigned int length) {
	Serial.print("incoming: ");
	Serial.print(topic);
	Serial.print(" - ");
	Serial.print(payload);
	Serial.println();
}
//Функция для отправки данных на IBM Bluemix
void sendSensorDataToBluemix() {
	if (client.connected()) {

		deviceID = "ESP8266-";
		deviceID += ESP.getChipId();

		Serial.print("Attempting send message to IBM Bluemix to: ");
		Serial.print(deviceID);
		Serial.print("\n");

		sensorValue = analogRead(sensorPin);
		String payload = buildMqttMessage(sensorValue, 0, deviceID);
		Serial.println((char*)payload.c_str());
		Serial.println("\n");
		client.publish(topic, (char*)payload.c_str());

	}
	else {
		Serial.print("failed, rc=");
		Serial.println(" try again in 5 seconds\n");
		connectToBluemix();
	}

	delay(3000);
}
//Функция проверяющая доступность сервиса IBM с подключенным внешнем wifi
void testInternetConnection() {
	const char* host = "kwxqcy.messaging.internetofthings.ibmcloud.com";

	Serial.print("connecting to ");
	Serial.println(host);

	// Use WiFiClient class to create TCP connections
	WiFiClient client;
	const int httpPort = 80;
	if (!client.connect(host, httpPort)) {
		Serial.println("connection failed");
		server.send(200, "text/html", "<h3>Связь с сервером IBM Отсутствует....</h3><br/><a href='/'>Вернуться назад</a>");
		return;
	}

	// We now create a URI for the request
	//String url = "/stan";
	String url = "/";


	Serial.print("Requesting URL: ");
	Serial.println(url);

	// This will send the request to the server
	client.print(String("GET ") + url + " HTTP/1.1\r\n" +
		"Host: " + host + "\r\n" +
		"Connection: close\r\n\r\n");
	delay(10);

	// Read all the lines of the reply from server and print them to Serial
	Serial.println("Respond:");
	while (client.available()) {
		String line = client.readStringUntil('\r');
		Serial.print(line);
		server.send(200, "text/html", "<h3>Связь с сервером IBM подтверждена!</h3><br/><a href='/'>Вернуться назад</a>");
	}
	Serial.println();
	Serial.println("closing connection");
}

void scanNearWifiNetworks() {
	netList = "";
	scanWifiNetworks();
	handleRoot();
}

//Connection to WIFI
void connectToWifi() {
	Serial.println("Starting to connect wifi...");
	String stip = server.arg("wifi");
	String stpsw = server.arg("password");

	char wifi[50];
	char pwd[50];
	stip.toCharArray(wifi, 50);
	stpsw.toCharArray(pwd, 50);

	Serial.print("New wifi= ");
	Serial.println(wifi);
	Serial.print("New pwd= ");
	Serial.println(pwd);

	WiFiMulti.addAP(wifi, pwd);

	Serial.print("checking wifi...");
	while (WiFiMulti.run() != WL_CONNECTED) {
		Serial.print(".");
		delay(500);
	}
	Serial.println("Local WiFi connected");
	Serial.println("IP address: ");
	Serial.println(WiFi.localIP());

	content = "<!DOCTYPE HTML>\r\n<html>";
	content += "<p>Connected to Newtwork <br> </p>";
	content += stip;
	content += ":";
	content += stpsw;
	content += "</br>";
	content += "<h1>Device is reconnected to: ";
	content += ipToString(WiFi.localIP());
	content += "</h1>";
	content += "<a href='/'>Вернуться к выбору сети</a>";
	content += "</html>";

	server.send(200, "text/html", content);
}
//Подключение специализированного датчика(sensor)
void connectSensor() {

	Serial.println("Any sensor is not pluged, check wiring!");

}
//Функция подключения к сервису IBM Bluemix
void connectToBluemix() {

	clientID = "d:" ORG ":" DEVICE_TYPE ":" "ESP8266-";
	clientID += ESP.getChipId();
	clientID.toCharArray(cID, 50);
	client.begin(mqttserver, 8883, net);
	Serial.println("\nConnecting to IBM Bluemix Client=");
	Serial.println(clientID);
	Serial.println("\n");

	while (!client.connect(cID, authMethod, token)) {
		Serial.print("+");
		delay(1000);
	}
	Serial.println("\nConnected to IBM Bluemix!");
	isBluemixConnected = true;
}

void handleRoot() {

	content = "<html><head><style>table, th, td{ border: 1px solid black; width: 500px;}</style></head><body bgcolor='#B0C4DE'>";
	content += "<h1>IJ Temperature Device:(MODE=";
	content += EEPROM.read(0);
	content += ")</h1>";

	content += "</br>";
	content += "<h3>";
	content += "ESP8266 Chip id =";
	content += ESP.getChipId();
	content += "</br>";
	content += "ESP8266 FlashChipId = ";
	content += ESP.getFlashChipId();
	content += "</br>";
	content += "Device Name= ESP8266-";
	content += ESP.getChipId();
	content += "</br>";
	content += "External IP address= ";
	content += ipToString(WiFi.localIP());
	content += "</br>";
	content += "Соединение с Bluemix= ";
	content += isBluemixConnected;
	content += "</h3>";
	content += "Влажность= ";
	content += analogRead(sensorPin);
	content += "</h3>";
	content += "</br>";



	content += "</br>";
	content += "<a href='/reboot'>Reboot Device</a>";
	content += "</br>";
	content += "<a href='/scanwifi'>Сканировать доступные WIFI-сети</a>";
	content += "</br>";
	content += "<a href='/testInternet'>Проверить доступность сервера IBM</a>";
	content += "</br>";
	content += "<a href='/testmqtt'>Отправить тестовое MQTT-сообщение</a>";
	content += "</br>";
	content += "<table>";
	content += "<tr><th>Network</th><th>Password</th><th>Action</th></tr>";
	content += netList;
	content += "</table>";

	content += "</body></html>";
	server.send(200, "text/html", content);
}

void scanWifiNetworks() {

	Serial.println(myIP);

	Serial.print("Scan start ... ");
	int n = WiFi.scanNetworks();
	Serial.print(n);
	Serial.println(" network(s) found");

	for (int i = 0; i < n; i++)
	{
		//Serial.println(WiFi.SSID(i));

		netList += "<tr>";
		netList += "<td>";
		netList += WiFi.SSID(i).c_str();
		netList += "</td>";


		netList += "<form action = '/connect'>";
		netList += "<input type = 'hidden' name = 'wifi' value='";
		netList += WiFi.SSID(i).c_str();
		netList += "'>";

		netList += "<td>";
		netList += "<input type = 'text' name = 'password'>";
		netList += "</td>";

		netList += "<td>";
		netList += "<input type = 'submit' value = 'Подключиться'>";
		netList += "</td>";

		netList += "</form>";

		netList += "</tr>";

	}

	Serial.println();
}

String ipToString(IPAddress ip) {
	String s = "";
	for (int i = 0; i<4; i++)
		s += i ? "." + String(ip[i]) : String(ip[i]);
	return s;
}

char* get_WIFI_STA_SSID() {

	char chipId[11] = "";
	sprintf(chipId, "%d", ESP.getChipId());

	char * str = "ESP8266-WIFI-";
	strcat(str, chipId);
	return str;
}

char * get_WIFI_STA_PWD() {
	return "12345678";
}
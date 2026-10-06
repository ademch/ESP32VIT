#include <WiFi.h>

#include <Wire.h>
#include "SPIFFS.h"
#include "pinsConfig.h"
#include "DS3231.h"
#include "ADS1115.h"

// https://github.com/espressif/arduino-esp32/blob/master/libraries/SD/examples/SD_Test/SD_Test.ino
#include <SD.h>
#include <SPI.h>

const char* strBuildTimestamp = __TIMESTAMP__;

// Enter your WiFi credentials
const char *ssid        = "mzorova";
const char *password    =
#include "password.d"
;

const char *ap_ssid     = "ESP32VIT_AP";
const char *ap_password = "12345678";



// Callback when station connects
void onClientConnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
	Serial.println(">> New client connected!");
	Serial.print("MAC: ");
	for (int i = 0; i < 6; i++) {
		Serial.printf("%02X", info.wifi_ap_staconnected.mac[i]);
		if (i < 5) Serial.print(":");
	}
	Serial.println();
}

// Callback when station disconnects
void onClientDisconnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
	Serial.println(">> Client disconnected!");
	Serial.print("MAC: ");
	for (int i = 0; i < 6; i++) {
		Serial.printf("%02X", info.wifi_ap_stadisconnected.mac[i]);
		if (i < 5) Serial.print(":");
	}
	Serial.println();
}

bool i2cDevicePresent(uint8_t address)
{
	Wire.beginTransmission(address);
	return Wire.endTransmission() == 0;
}

DS3231 rtc;
ADS1115 adc;


void setup()
{
	delay(2000);

	// Start UART
	Serial.begin(115200);
  
	// Enable diagnostic output for WiFi libraries
	Serial.setDebugOutput(true);

	// configure LED pins
	digitalWrite(LED_AMBER_GPIO_NUM, LOW);
	pinMode(LED_AMBER_GPIO_NUM, OUTPUT);

	digitalWrite(LED_GREEN_GPIO_NUM, LOW);
	pinMode(LED_GREEN_GPIO_NUM, OUTPUT);

	// configure RELAY CONTROL pins
	digitalWrite(REL1_GPIO_NUM, LOW);
	pinMode(REL1_GPIO_NUM, OUTPUT);

	digitalWrite(REL2_GPIO_NUM, LOW);
	pinMode(REL2_GPIO_NUM, OUTPUT);

	digitalWrite(REL3_GPIO_NUM, LOW);
	pinMode(REL3_GPIO_NUM, OUTPUT);

	digitalWrite(REL4_GPIO_NUM, LOW);
	pinMode(REL4_GPIO_NUM, OUTPUT);

	// Initialize I2C with custom pins and frequency (default 100kHz)
	Wire.begin(I2C_SDA_GPIO_NUM, I2C_SCL_GPIO_NUM, 100000);	// SDA, SCL, frequency in Hz

	Serial.print("Connecting to DS3231 RTC...");

		if (!i2cDevicePresent(I2C_RTC_ADDRESS))
		{
			digitalWrite(LED_AMBER_GPIO_NUM, HIGH);
			Serial.println("fail");
		}
		else
			rtc.bOnline = true;

		if (!rtc.oscillatorCheck())
			Serial.println("RTC oscillator was stopped, check battery");
		else
			rtc.bTimeIsGood = true;

	Serial.println("success");

	Serial.print("Connecting to ADS1115...");

		if (!i2cDevicePresent(I2C_ADS_ADDRESS))
		{
			digitalWrite(LED_AMBER_GPIO_NUM, HIGH);
			Serial.println("fail");
			return;
		}

		adc.begin();

	Serial.println("success");

	Serial.print("Initializing SD card...");

		if (!SD.begin(SS, SPI, 1000000))	// CS_PIN, HARDWARE INTERFACE, SPEED
		{
			digitalWrite(LED_AMBER_GPIO_NUM, HIGH);
			Serial.println("fail");
			return;
		}

	Serial.println("success");

	Serial.printf("Total space: %llu MB\n", SD.totalBytes() / (1024UL * 1024UL));
	Serial.printf("Used space: %llu MB\n",  SD.usedBytes()  / (1024UL * 1024UL));

	if (true)
	{
		WiFi.softAP(ap_ssid, ap_password);
		Serial.print("WiFi AP started: ");
		Serial.println(ap_ssid);
		Serial.print("IP address: ");
		Serial.println(WiFi.softAPIP());

		// Register callback for AP client connection/disconnection
		WiFi.onEvent(onClientConnected,    ARDUINO_EVENT_WIFI_AP_STACONNECTED);
		WiFi.onEvent(onClientDisconnected, ARDUINO_EVENT_WIFI_AP_STADISCONNECTED);
	}
	else // Station mode (connect to a router)
	{
		WiFi.begin(ssid, password);
		WiFi.setSleep(false);
				
		Serial.print("Connecting to WiFi access point ");
		Serial.print(ssid);
		while (WiFi.status() != WL_CONNECTED)
		{
			delay(250);			// wait for connection
			Serial.print(".");
		}
		Serial.println("success");
	}

	Serial.print("Mounting SPIFFS...");

		// Mount SPIFFS
		//if (!SPIFFS.begin(true)) { // true = format if failed
		//	Serial.println("Failed to mount SPIFFS");
		//	return;
		//}

	Serial.println("SPIFFS mounted successfully");

	// Get total and used bytes
	//Serial.printf("* SPIFFS partition: %u kbytes\n", SPIFFS.totalBytes() >> 10);
	//Serial.printf("* SPIFFS used: %u kbytes\n", SPIFFS.usedBytes() >> 10);


	Serial.println("Reading user calibration data from SPIFFS...");


	Serial.println("success");

  Serial.println("Launching http servers...");

	//startControlAndStreamServers();

  Serial.println("success");

  Serial.print("All ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");

  digitalWrite(LED_GREEN_GPIO_NUM, HIGH);
}

void loop()
{
  // Do nothing. Everything is done in another task by the web server

  //Serial.println(".");

  digitalWrite(REL1_GPIO_NUM, HIGH);
  delay(1000);
  digitalWrite(REL1_GPIO_NUM, LOW);

  digitalWrite(REL2_GPIO_NUM, HIGH);
  delay(1000);
  digitalWrite(REL2_GPIO_NUM, LOW);

  digitalWrite(REL3_GPIO_NUM, HIGH);
  delay(1000);
  digitalWrite(REL3_GPIO_NUM, LOW);

  digitalWrite(REL4_GPIO_NUM, HIGH);
  delay(1000);
  digitalWrite(REL4_GPIO_NUM, LOW);

  float fVoltage = adc.readVoltageDifferential(ADS1115_AIN0, ADS1115_AIN1);
  if (isnan(fVoltage))
  {
	  Serial.println("Read failed");
  }
  Serial.printf("Voltage: %f\n", fVoltage);

}

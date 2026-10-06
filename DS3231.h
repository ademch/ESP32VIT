#ifndef DS3231_H
#define DS3231_H

#include <Arduino.h>
#include <time.h>
#include <Wire.h>
#include <string>

#define I2C_RTC_ADDRESS 0x68


class DS3231
{
public:

	// Constructors
	DS3231();

	TwoWire & _Wire;

	bool bOnline;
	bool bTimeIsGood;

	std::string GetTime();
	bool        SetTime(const char* date_time);

	float getTemperature();

	// Oscillator functions

	void enableOscillator(bool bOn, bool battery, uint8_t frequency);

	// Turns the 32kHz output pin on (true); or off (false).
	void enable32kHzOutput(bool bOn);

	// The OSF is cleared by function setSecond()
	bool oscillatorCheck();

private:
	uint8_t dow_to_ds3231(uint8_t d) { return d == 0 ? 7 : d; }
	uint8_t ds3231_to_dow(uint8_t d) { return d == 7 ? 0 : d; }

	uint8_t dayOfTheWeek(uint16_t y, uint8_t m, uint8_t d);

	uint8_t readReg(uint8_t reg);
	void    writeReg(uint8_t control, uint8_t reg);

};

#endif
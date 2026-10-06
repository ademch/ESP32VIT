/*
	https://github.com/NorthernWidget/DS3231

*/

#include "DS3231.h"

// These included for the DateTime class inclusion; will try to find a way to not need them in the future...
#if defined(__AVR__)
	#include <avr/pgmspace.h>
#elif defined(ESP8266)
	#include <pgmspace.h>
#endif

#include <Arduino.h>

#define REG_TIME_SECONDS   0x00
#define REG_CONTROL		   0x0E
#define REG_CONTROL_STATUS 0x0F

#define BIT_OSF			   0b10000000
#define BIT_BBSQW		   0b01000000
#define BIT_EN32KHZ		   0b00001000



uint8_t dec2bcd(uint8_t val)
{
	return (val/10*16) + (val%10);
}

uint8_t bcd2dec(uint8_t val)
{
	// left digit 4bits + right digit 4bits
	return (val/16*10)  + (val%16);
}


// Constructor
DS3231::DS3231() : _Wire(Wire)
{
	bOnline     = false;
	bTimeIsGood = false;
}

static const uint8_t daysInMonth[]  = { 31,28,31,30,31,30,31,31,30,31,30,31 };
static const char g_strMonthNames[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
static const char g_strDOWNames[]   = "MonTueWedThuFriSatSun";


// A year is a leap year if it is divisible by 4, except century years must be divisible by 400
bool isLeapYear(const uint16_t y)
{
	// check if divisible by 4
	if (y & 3)
		return false;

	// divisible, perform additional checks
	return (y % 100 || y % 400 == 0);
}


// number of days since 2000/01/01, valid for 2001..2199
uint32_t date2days(uint16_t y, uint8_t m, uint8_t d)
{
	if (y >= 2000) y -= 2000;

	uint32_t days = d;

	for (uint8_t i = 1; i < m; ++i)
		days += daysInMonth[i - 1];
	
	if (m > 2 && isLeapYear(y))
		++days;
	
	return days + 365UL * y + (y + 3)/4 - y/100 - 1;
}


// 7 = Sunday
// 1 = Monday
// 2 = Tuesday
// 3 = Wednesday
// 4 = Thursday
// 5 = Friday
// 6 = Saturday
uint8_t DS3231::dayOfTheWeek(uint16_t y, uint8_t m, uint8_t d)
{
	uint32_t day = date2days(y, m, d);
	day = (day + 6) % 7;

	return (d == 0) ? 7 : day;
}

// Format is "dd Mmm yyyy hh:mm:ss" (same as __DATE__ and __TIME__)
bool DS3231::SetTime(const char* date_time)
{
	char strMon[4] = {'0','0','0','0'};

	uint16_t year;
	uint8_t  month, day, hour, min, sec;
	
	if (sscanf(date_time, "%hhu %c%c%c %hu %hhu:%hhu:%hhu",
		                  &day, &strMon[0],&strMon[1],&strMon[2], &year, &hour, &min, &sec) != 8)
		return false;

	char* p = strstr(g_strMonthNames, strMon);
	if (!p)
		return false;

	month = (p - g_strMonthNames) / 3 + 1;

	if (year < 2000 || year > 2199 || day < 1 || day > 31)
		return false;

	if (day > daysInMonth[month - 1])
	{
		if (!(month == 2 && day == 29 && isLeapYear(year)))
			return false;
	}

	if (hour > 23 || min > 59 || sec > 59)
		return false;

	_Wire.beginTransmission(I2C_RTC_ADDRESS);
		_Wire.write(REG_TIME_SECONDS);
		_Wire.write(dec2bcd(sec));
		_Wire.write(dec2bcd(min));
		_Wire.write(dec2bcd(hour));
		_Wire.write(dec2bcd(dow_to_ds3231(dayOfTheWeek(year, month, day))));
		_Wire.write(dec2bcd(day));
		_Wire.write(dec2bcd(month) | ((year >= 2100) ? 0x80 : 0));
		_Wire.write(dec2bcd(year % 100));
	_Wire.endTransmission();

	// Clear OSF flag
	uint8_t contrByte = readReg(REG_CONTROL_STATUS);
	writeReg((contrByte & ~BIT_OSF), REG_CONTROL_STATUS);

	return true;
}


// Format is "dd Mmm yyyy Dow hh:mm:ss"
std::string DS3231::GetTime()
{
	// Set address pointer to 00h
	_Wire.beginTransmission(I2C_RTC_ADDRESS);
		_Wire.write(REG_TIME_SECONDS);
	_Wire.endTransmission();

	_Wire.requestFrom(I2C_RTC_ADDRESS, 7);

	uint16_t year;
	uint8_t  month, day, hour, min, sec, dow;

	// seconds: 00h
	sec = bcd2dec(_Wire.read());
	// minutes: 01h
	min = bcd2dec(_Wire.read());
	// hours  : 02h
	hour = _Wire.read();
	if (hour & 0b01000000)
		// 12h mode
		hour = bcd2dec(hour & 0b00011111) + 12;
	else
		// 24h mode
		hour = bcd2dec(hour & 0b00111111);

	// day of week: 03h
	dow = ds3231_to_dow(_Wire.read());

	// date: 04h
	day   = bcd2dec(_Wire.read());
	// month and century flag: 05h
	month = bcd2dec(_Wire.read());
	// year: 06h
	year  = bcd2dec(_Wire.read()) + 2000;

	if (month >= 80)
	{
		month -= 80;
		year  += 100;
	}

	// "dd Mmm yyyy Dow hh:mm:ss"
	char strOutBuf[128];
	snprintf(strOutBuf, sizeof(strOutBuf), "%02hhu %.3s %u %.3s %02hhu:%02hhu:%02hhu",
		                                    day, g_strMonthNames + month*3, year, g_strDOWNames + dow*3, hour, min, sec);

	return strOutBuf;
}



uint8_t DS3231::readReg(uint8_t reg)
{
	_Wire.beginTransmission(I2C_RTC_ADDRESS);
		_Wire.write(reg);
	_Wire.endTransmission();

	_Wire.requestFrom(I2C_RTC_ADDRESS, 1);
	return _Wire.read();
}

void DS3231::writeReg(uint8_t control, uint8_t reg)
{
	_Wire.beginTransmission(I2C_RTC_ADDRESS);
		_Wire.write(reg);
		_Wire.write(control);
	_Wire.endTransmission();
}

// Checks the internal thermometer on the DS3231 and returns
// the temperature as a floating-point value
float DS3231::getTemperature()
{
	int8_t tMSB, tLSB;

	// temp registers (11h-12h) get updated automatically every 64s
	_Wire.beginTransmission(I2C_RTC_ADDRESS);
		_Wire.write(0x11);
	_Wire.endTransmission();

	_Wire.requestFrom(I2C_RTC_ADDRESS, 2);

		tMSB = _Wire.read(); // 2's complement int portion
		tLSB = _Wire.read(); // fraction portion in 0.25 C

	return tMSB + (tLSB >> 6)*0.25f;
}


// turns oscillator on or off. True is on, false is off.
// If battery is true, turns on even for battery-only operation,
// otherwise turns off if Vcc is off.
// frequency must be 0, 1, 2, or 3.
// 0 = 1 Hz
// 1 = 1.024 kHz
// 2 = 4.096 kHz
// 3 = 8.192 kHz (Default if frequency byte is out of range)
void DS3231::enableOscillator(bool bOn, bool battery, uint8_t frequency)
{
	if (frequency > 3) frequency = 3;

	// read control byte in, but zero out current state of RS2 and RS1
	uint8_t contrByte = readReg(REG_CONTROL) & 0b11100111;
	if (battery)
	{
		// turn on BBSQW flag
		contrByte = contrByte | BIT_BBSQW;
	} else
	{
		// turn off BBSQW flag
		contrByte = contrByte & ~BIT_BBSQW;
	}
	
	if (bOn)
	{
		// set ~EOSC to 0 and INTCN to zero
		contrByte = contrByte & 0b01111011;
	} else
	{
		// set ~EOSC to 1, leave INTCN as is
		contrByte = contrByte | 0b10000000;
	}
	
	// shift frequency into bits 3 and 4 and set.
	frequency = frequency << 3;
	contrByte = contrByte | frequency;

	// write the control bits
	writeReg(contrByte, REG_CONTROL);
}


void DS3231::enable32kHzOutput(bool bOn)
{
	// turn 32kHz pin on or off
	uint8_t contrByte = readReg(REG_CONTROL_STATUS);
		if (bOn)
		{
			// turn on 32kHz pin
			contrByte = contrByte | BIT_EN32KHZ;
		} else
		{
			// turn off 32kHz pin
			contrByte = contrByte & ~BIT_EN32KHZ;
		}
	writeReg(contrByte, REG_CONTROL_STATUS);
}

// Returns false if the oscillator has been off for some reason.
// If this is the case, the time is probably not correct.
bool DS3231::oscillatorCheck()
{
	uint8_t contrByte = readReg(REG_CONTROL_STATUS);

	if (contrByte & BIT_OSF)
	{
		// Oscillator Stop Flag (OSF) is set, return false
		return false;
	}
	return true;
}


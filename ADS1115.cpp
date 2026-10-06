#include "ADS1115.h"

// https://github.com/TrifVlad15/ADS1115-Arduino-Library

/*
	#include <Wire.h>
	#include "ADS1115.h"

	ADS1115 adc(Wire, 0x48);

	void setup()
	{
		Serial.begin(115200);
		while (!Serial) {}

		Wire.begin();

		if (!adc.begin())
		{
			Serial.println("Nu gasesc ADS1115. Verifica conexiunile!");
			while (true) {}
		}

		adc.setGain(ADS1115::Gain::GAIN_FOUR);           // +/-1.024 V pentru senzori de 3.3 V
		adc.setDataRate(ADS1115::DataRate::SPS_475);
		adc.setMode(ADS1115::Mode::MODE_SINGLE_SHOT);
	}

	void loop()
	{
		float ch0 = adc.readVoltageSingleEnded(0);
		if (isnan(ch0))
		{
			Serial.println("Citire esuata");
			delay(1000);
			return;
		}

		int16_t diff = 0;
		if (!adc.readRawDifferential(0, 1, diff))
		{
			Serial.println("Differential fail");
		}

		Serial.print("CH0: ");
		Serial.print(ch0, 4);
		Serial.print(" V  |  DIFF0-1: ");
		Serial.print(adc.rawToVoltage(diff), 4);
		Serial.println(" V");

		delay(250);
	}

*/

ADS1115::ADS1115(TwoWire &wire, uint8_t address)
	   : _wire(wire),
		_address(address),
		_gain(Gain::GAIN_TWO),
		_dataRate(DataRate::SPS_128),
		_mode(Mode::MODE_SINGLE_SHOT),
		_compMode(ComparatorMode::COMPARATOR_TRADITIONAL),
		_compPolarity(ComparatorPolarity::ACTIVE_LOW),
		_compLatching(ComparatorLatching::COMPARATOR_NON_LATCHING),
		_compQueue(ComparatorQueue::ASSERT_DISABLED)
{}

bool ADS1115::begin()
{
	uint16_t value = 0;

	if (!readRegister(REG_CONFIG, value))
		return false;

	return writeRegister(REG_CONFIG, buildConfig(muxForSingleEnded(0)) | OS_SINGLE);
}

void ADS1115::setGain(Gain gain)
{
	_gain = gain;
}

void ADS1115::setDataRate(DataRate rate)
{
	_dataRate = rate;
}

void ADS1115::setMode(Mode mode)
{
	_mode = mode;
}

void ADS1115::setComparator(ComparatorMode mode, ComparatorPolarity polarity, ComparatorLatching latching, ComparatorQueue queue)
{
	_compMode     = mode;
	_compPolarity = polarity;
	_compLatching = latching;
	_compQueue    = queue;
}

bool ADS1115::setComparatorThresholds(int16_t low, int16_t high)
{
	return writeRegister(REG_LO_THRESH, static_cast<uint16_t>(low)) &&
		   writeRegister(REG_HI_THRESH, static_cast<uint16_t>(high));
}

bool ADS1115::readRawSingleEnded(uint8_t channel, int16_t &value)
{
	const uint16_t muxBits = muxForSingleEnded(channel);
	if (muxBits == 0) return false;

	const uint16_t config = buildConfig(muxBits) | OS_SINGLE;
	if (!writeRegister(REG_CONFIG, config))
		return false;

	const uint32_t waitUs = conversionDelayMicros();
	if (waitUs >= 1000UL)
	{
		delay(waitUs / 1000UL);
	}
	delayMicroseconds(static_cast<unsigned int>(waitUs % 1000UL));

	uint16_t raw = 0;
	if (!readRegister(REG_CONVERSION, raw))
		return false;

	value = static_cast<int16_t>(raw);
	return true;
}

float ADS1115::readVoltageSingleEnded(uint8_t channel)
{
	int16_t raw = 0;
	if (!readRawSingleEnded(channel, raw))
		return NAN;

	return rawToVoltage(raw);
}

bool ADS1115::readRawDifferential(uint8_t posChannel, uint8_t negChannel, int16_t &value)
{
	const uint16_t muxBits = muxForDifferential(posChannel, negChannel);
	if (muxBits == 0xFFFF)
		return false;

	const uint16_t config = buildConfig(muxBits) | OS_SINGLE;
	if (!writeRegister(REG_CONFIG, config))
		return false;

	const uint32_t waitUs = conversionDelayMicros();
	if (waitUs >= 1000UL)
	{
		delay(waitUs / 1000UL);
	}
	delayMicroseconds(static_cast<unsigned int>(waitUs % 1000UL));

	uint16_t raw = 0;
	if (!readRegister(REG_CONVERSION, raw))
		return false;

	value = static_cast<int16_t>(raw);
	return true;
}

float ADS1115::readVoltageDifferential(uint8_t posChannel, uint8_t negChannel)
{
	int16_t raw = 0;
	if (!readRawDifferential(posChannel, negChannel, raw))
		return NAN;

	return rawToVoltage(raw);
}

float ADS1115::rawToVoltage(int16_t raw) const
{
	// full scale range
	float fsr = 6.144f;
	switch (_gain)
	{
	case Gain::GAIN_TWO_THIRDS:
		fsr = 6.144f;
		break;
	case Gain::GAIN_ONE:
		fsr = 4.096f;
		break;
	case Gain::GAIN_TWO:
		fsr = 2.048f;
		break;
	case Gain::GAIN_FOUR:
		fsr = 1.024f;
		break;
	case Gain::GAIN_EIGHT:
		fsr = 0.512f;
		break;
	case Gain::GAIN_SIXTEEN:
		fsr = 0.256f;
		break;
	}
	return (fsr / 32768.0f) * static_cast<float>(raw);
}

bool ADS1115::readLastConfig(uint16_t &configValue)
{
	return readRegister(REG_CONFIG, configValue);
}

uint16_t ADS1115::buildConfig(uint16_t muxBits) const
{
	return muxBits |
		   static_cast<uint16_t>(_gain) |
		   static_cast<uint16_t>(_dataRate) |
		   static_cast<uint16_t>(_mode) |
		   static_cast<uint16_t>(_compMode) |
		   static_cast<uint16_t>(_compPolarity) |
		   static_cast<uint16_t>(_compLatching) |
		   static_cast<uint16_t>(_compQueue);
}

// Meassure against GND
uint16_t ADS1115::muxForSingleEnded(uint8_t channel) const
{
	switch (channel)
	{
	case ADS1115_AIN0:
		return 0x4000;

	case ADS1115_AIN1:
		return 0x5000;

	case ADS1115_AIN2:
		return 0x6000;

	case ADS1115_AIN3:
		return 0x7000;

	default:
		return 0;
	}
}

uint16_t ADS1115::muxForDifferential(uint8_t positive, uint8_t negative) const
{
	if (positive == ADS1115_AIN0 && negative == ADS1115_AIN1)
		return 0x0000;

	if (positive == ADS1115_AIN0 && negative == ADS1115_AIN3)
		return 0x1000;

	if (positive == ADS1115_AIN1 && negative == ADS1115_AIN3)
		return 0x2000;

	if (positive == ADS1115_AIN2 && negative == ADS1115_AIN3)
		return 0x3000;

	return 0xFFFF;
}

uint32_t ADS1115::conversionDelayMicros() const
{
	switch (_dataRate)
	{
	case DataRate::SPS_8:
		return 130000UL;
	case DataRate::SPS_16:
		return 70000UL;
	case DataRate::SPS_32:
		return 35000UL;
	case DataRate::SPS_64:
		return 18000UL;
	case DataRate::SPS_128:
		return 10000UL;
	case DataRate::SPS_250:
		return 5000UL;
	case DataRate::SPS_475:
		return 2500UL;
	case DataRate::SPS_860:
	default:
		return 1500UL;
	}
}

bool ADS1115::writeRegister(uint8_t reg, uint16_t value)
{
	_wire.beginTransmission(_address);
		_wire.write(reg);
		_wire.write(static_cast<uint8_t>(value >> 8));
		_wire.write(static_cast<uint8_t>(value & 0xFF));
	return _wire.endTransmission() == 0;
}

bool ADS1115::readRegister(uint8_t reg, uint16_t &value)
{
	_wire.beginTransmission(_address);
		_wire.write(reg);
	if (_wire.endTransmission(false) != 0)
		return false;

	if (_wire.requestFrom(_address, static_cast<uint8_t>(2)) != 2)
		return false;

		const uint8_t msb = _wire.read();
		const uint8_t lsb = _wire.read();
	value = static_cast<uint16_t>((msb << 8) | lsb);

	return true;
}
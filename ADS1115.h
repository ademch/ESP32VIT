#pragma once

#include <Arduino.h>
#include <Wire.h>

#define I2C_ADS_ADDRESS 0x48

enum ADS1115_INPUT
{
	ADS1115_AIN0 = 0,
	ADS1115_AIN1 = 1,
	ADS1115_AIN2 = 2,
	ADS1115_AIN3 = 3
};

class ADS1115
{
public:
	enum class Gain : uint16_t
	{
		GAIN_TWO_THIRDS = 0x0000,     // +/-6.144 V
		GAIN_ONE        = 0x0200,     // +/-4.096 V
		GAIN_TWO        = 0x0400,     // +/-2.048 V (IC default)
		GAIN_FOUR       = 0x0600,     // +/-1.024 V
		GAIN_EIGHT      = 0x0800,     // +/-0.512 V
		GAIN_SIXTEEN    = 0x0A00      // +/-0.256 V
	};

	enum class DataRate : uint16_t
	{
		SPS_8   = 0x0000,
		SPS_16  = 0x0020,
		SPS_32  = 0x0040,
		SPS_64  = 0x0060,
		SPS_128 = 0x0080,	// IC default
		SPS_250 = 0x00A0,
		SPS_475 = 0x00C0,
		SPS_860 = 0x00E0
	};

	enum class Mode : uint16_t {
		MODE_CONTINUOUS = 0x0000,
		MODE_SINGLE_SHOT = 0x0100
	};

	enum class ComparatorMode : uint16_t {
		COMPARATOR_TRADITIONAL = 0x0000,
		COMPARATOR_WINDOW = 0x0010
	};

	enum class ComparatorPolarity : uint16_t {
		ACTIVE_LOW = 0x0000,
		ACTIVE_HIGH = 0x0008
	};

	enum class ComparatorLatching : uint16_t {
		COMPARATOR_NON_LATCHING = 0x0000,
		COMPARATOR_LATCHING = 0x0004
	};

	enum class ComparatorQueue : uint16_t {
		ASSERT_AFTER_1  = 0x0000,
		ASSERT_AFTER_2  = 0x0001,
		ASSERT_AFTER_4  = 0x0002,
		ASSERT_DISABLED = 0x0003
	};

	explicit ADS1115(TwoWire &wire = Wire, uint8_t address = I2C_ADS_ADDRESS);

	bool begin();

	void setGain(Gain gain);
	void setDataRate(DataRate rate);
	void setMode(Mode mode);
	void setComparator(ComparatorMode mode, ComparatorPolarity polarity, ComparatorLatching latching, ComparatorQueue queue);
	bool setComparatorThresholds(int16_t low, int16_t high);

	Gain gain() const		  { return _gain; }
	DataRate dataRate() const { return _dataRate; }
	Mode mode() const		  { return _mode; }

	bool  readRawSingleEnded(uint8_t channel, int16_t &value);
	float readVoltageSingleEnded(uint8_t channel);

	bool  readRawDifferential(uint8_t positiveChannel, uint8_t negativeChannel, int16_t &value);
	float readVoltageDifferential(uint8_t posChannel, uint8_t negChannel);

	float rawToVoltage(int16_t raw) const;

	bool readLastConfig(uint16_t &configValue);

private:
	static constexpr uint8_t REG_CONVERSION = 0x00;
	static constexpr uint8_t REG_CONFIG		= 0x01;
	static constexpr uint8_t REG_LO_THRESH  = 0x02;
	static constexpr uint8_t REG_HI_THRESH  = 0x03;
	static constexpr uint16_t OS_SINGLE		= 0x8000;

	uint16_t buildConfig(uint16_t muxBits) const;
	uint16_t muxForSingleEnded(uint8_t channel) const;
	uint16_t muxForDifferential(uint8_t positive, uint8_t negative) const;
	uint32_t conversionDelayMicros() const;

	bool writeRegister(uint8_t reg, uint16_t value);
	bool readRegister(uint8_t reg, uint16_t &value);

	TwoWire &_wire;
	uint8_t _address;

	Gain			   _gain;
	DataRate		   _dataRate;
	Mode			   _mode;
	ComparatorMode	   _compMode;
	ComparatorPolarity _compPolarity;
	ComparatorLatching _compLatching;
	ComparatorQueue	   _compQueue;
};
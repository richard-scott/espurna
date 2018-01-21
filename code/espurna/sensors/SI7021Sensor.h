// -----------------------------------------------------------------------------
// SI7021 / HTU21D Sensor over I2C
// Copyright (C) 2017-2018 by Xose Pérez <xose dot perez at gmail dot com>
// -----------------------------------------------------------------------------

#if SENSOR_SUPPORT && SI7021_SUPPORT

#pragma once

#include "Arduino.h"
#include "I2CSensor.h"
#if I2C_USE_BRZO
#include <brzo_i2c.h>
#else
#include <Wire.h>
#endif

#define SI7021_SCL_FREQUENCY    200

#define SI7021_CHIP_SI7021      0x15
#define SI7021_CHIP_HTU21D      0x32

#define SI7021_CMD_TMP_HOLD     0xE3
#define SI7021_CMD_HUM_HOLD     0xE5
#define SI7021_CMD_TMP_NOHOLD   0xF3
#define SI7021_CMD_HUM_NOHOLD   0xF5

PROGMEM const char si7021_chip_si7021_name[] = "SI7021";
PROGMEM const char si7021_chip_htu21d_name[] = "HTU21D";

class SI7021Sensor : public I2CSensor {

    public:

        // ---------------------------------------------------------------------
        // Public
        // ---------------------------------------------------------------------

        SI7021Sensor(): I2CSensor() {
            _sensor_id = SENSOR_SI7021_ID;
        }

        // ---------------------------------------------------------------------
        // Sensor API
        // ---------------------------------------------------------------------

        // Initialization method, must be idempotent
        void begin() {

            if (!_dirty) return;
            _dirty = false;

            // I2C auto-discover
            unsigned char addresses[] = {0x40};
            _address = _begin_i2c(_address, sizeof(addresses), addresses);
            if (_address == 0) return;

            // Initialize sensor
            _init();

        }

        // Descriptive name of the sensor
        String description() {
            char name[10];
            strncpy_P(name,
                _chip == SI7021_CHIP_SI7021 ?
                    si7021_chip_si7021_name :
                    si7021_chip_htu21d_name,
                sizeof(name)
            );
            char buffer[25];
            snprintf(buffer, sizeof(buffer), "%s @ I2C (0x%02X)", name, _address);
            return String(buffer);
        }

        // Descriptive name of the slot # index
        String slot(unsigned char index) {
            return description();
        };

        // Type for slot # index
        unsigned char type(unsigned char index) {
            _error = SENSOR_ERROR_OK;
            if (index == 0) return MAGNITUDE_TEMPERATURE;
            if (index == 1) return MAGNITUDE_HUMIDITY;
            _error = SENSOR_ERROR_OUT_OF_RANGE;
            return MAGNITUDE_NONE;
        }

        // Pre-read hook (usually to populate registers with up-to-date data)
        void pre() {

            _error = SENSOR_ERROR_UNKNOWN_ID;
            if (_chip == 0) return;
            _error = SENSOR_ERROR_OK;

            double value;

            value = _read(SI7021_CMD_TMP_NOHOLD);
            if (_error != SENSOR_ERROR_OK) return;
            _temperature = (175.72 * value / 65536) - 46.85;

            value = _read(SI7021_CMD_HUM_NOHOLD);
            if (_error != SENSOR_ERROR_OK) return;
            value = (125.0 * value / 65536) - 6;
            _humidity = constrain(value, 0, 100);

        }

        // Current value for slot # index
        double value(unsigned char index) {
            _error = SENSOR_ERROR_OK;
            if (index == 0) return _temperature;
            if (index == 1) return _humidity;
            _error = SENSOR_ERROR_OUT_OF_RANGE;
            return 0;
        }

    protected:

        // ---------------------------------------------------------------------
        // Protected
        // ---------------------------------------------------------------------

        void _init() {

            // Check device
            #if I2C_USE_BRZO
                uint8_t buffer[2] = {0xFC, 0xC9};
                brzo_i2c_start_transaction(_address, SI7021_SCL_FREQUENCY);
                brzo_i2c_write(buffer, 2, false);
                brzo_i2c_read(buffer, 1, false);
                brzo_i2c_end_transaction();
                _chip = buffer[0];
            #else
                Wire.beginTransmission(_address);
                Wire.write(0xFC);
                Wire.write(0xC9);
                Wire.endTransmission();
                Wire.requestFrom(_address, (unsigned char) 1);
                _chip = Wire.read();
            #endif

            if ((_chip != SI7021_CHIP_SI7021) & (_chip != SI7021_CHIP_HTU21D)) {
                i2cReleaseLock(_address);
                _error = SENSOR_ERROR_UNKNOWN_ID;
                _count = 0;
            } else {
                _count = 2;
            }

        }

        unsigned int _read(uint8_t command) {

            unsigned char bytes = (command == 0xE0) ? 2 : 3;

            #if I2C_USE_BRZO
                uint8_t buffer[2] = {command, 0x00};
                brzo_i2c_start_transaction(_address, SI7021_SCL_FREQUENCY);
                brzo_i2c_write(buffer, 1, false);
            #else
                Wire.beginTransmission(_address);
                Wire.write(command);
                Wire.endTransmission();
            #endif

            // When not using clock stretching (*_NOHOLD commands) delay here
            // is needed to wait for the measurement.
            // According to datasheet the max. conversion time is ~22ms
            unsigned long start = millis();
            while (millis() - start < 50) delay(1);

            #if I2C_USE_BRZO
                brzo_i2c_read(buffer, 2, false);
                brzo_i2c_end_transaction();
                unsigned int msb = buffer[0];
                unsigned int lsb = buffer[1];
            #else
                Wire.requestFrom(_address, bytes);
                unsigned int msb = Wire.read();
                unsigned int lsb = Wire.read();
            #endif

            // Clear the last to bits of LSB to 00.
            // According to datasheet LSB of RH is always xxxxxx10
            lsb &= 0xFC;

            unsigned int value = (msb << 8) | lsb;

            _error = SENSOR_ERROR_OK;
            return value;

        }

        unsigned char _chip;
        double _temperature = 0;
        double _humidity = 0;

};

#endif // SENSOR_SUPPORT && SI7021_SUPPORT

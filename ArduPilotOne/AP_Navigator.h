/*
 * AP_Navigator.h
 * Copyright (C) James Goppert 2010 <james.goppert@gmail.com>
 *
 * This file is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef AP_Navigator_H
#define AP_Navigator_H

#include "AP_HardwareAbstractionLayer.h"
#include "AP_MavlinkCommand.h"
#include "constants.h"
#include "APO_Config.h"

namespace apo {

/// Navigator class
class AP_Navigator {
public:
	AP_Navigator(AP_HardwareAbstractionLayer * hal) :
		_hal(hal), _timeStamp(0), _roll(0), _rollRate(0), _pitch(0),
		_pitchRate(0), _yaw(0), _yawRate(0), _airSpeed(0), _groundSpeed(0),
		_vD(0),  _lat_degInt(0), _heading(0),
		_lon_degInt(0), _alt_intM(0)
	{
	}
	virtual void calibrate() = 0;
	virtual void updateFast(float dt) = 0;
	virtual void updateSlow(float dt) = 0;
    float getPD() const
    {
    	AP_MavlinkCommand home = AP_MavlinkCommand(0);
        return home.getPD(getAlt_intM());
    }

    float getPE() const
    {
    	AP_MavlinkCommand home = AP_MavlinkCommand(0);
        return home.getPE(getLat_degInt(),getLon_degInt());
    }

    float getPN() const
    {
    	AP_MavlinkCommand home = AP_MavlinkCommand(0);
        return home.getPN(getLat_degInt(),getLon_degInt());
    }

    void setPD(float _pD)
    {
        AP_MavlinkCommand home = AP_MavlinkCommand(0);
        setAlt(home.getAlt(_pD));
    }

    void setPE(float _pE)
    {
    	AP_MavlinkCommand home = AP_MavlinkCommand(0);
    	setLat(home.getLat(_pE));
    }

    void setPN(float _pN)
    {
    	AP_MavlinkCommand home = AP_MavlinkCommand(0);
    	setLon(home.getLon(_pN));
    }

    float getAirSpeed() const
    {
        return _airSpeed;
    }

    int32_t getAlt_intM() const
    {
        return _alt_intM;
    }

    float getAlt() const
    {
        return _alt_intM / scale_m;
    }

    void setAlt(float _alt)
    {
        this->_alt_intM = _alt * scale_m;
    }

    float getLat() const
    {
        return _lat_degInt * degInt2Rad;
    }

    void setLat(float _lat)
    {
        this->_lat_degInt = _lat * rad2DegInt;
    }

    float getLon() const
    {
        return _lon_degInt * degInt2Rad;
    }

    void setLon(float _lon)
    {
        this->_lon_degInt = _lon * rad2DegInt;
    }

    float getVD() const
    {
        return _vD;
    }

    float getHeading() const
    {
    	return _heading;
    }

    float getVE() const
    {
        return sin(getHeading())*getGroundSpeed();
    }

    float getGroundSpeed() const
    {
        return _groundSpeed;
    }

    int32_t getLat_degInt() const
    {
        return _lat_degInt;
    }

    int32_t getLon_degInt() const
    {
        return _lon_degInt;
    }

    float getVN() const
    {
        return cos(getHeading())*getGroundSpeed();
    }

    float getPitch() const
    {
        return _pitch;
    }

    float getPitchRate() const
    {
        return _pitchRate;
    }

    float getRoll() const
    {
        return _roll;
    }

    float getRollRate() const
    {
        return _rollRate;
    }

    float getYaw() const
    {
        return _yaw;
    }

    float getYawRate() const
    {
        return _yawRate;
    }

    void setAirSpeed(float _airSpeed)
    {
        this->_airSpeed = _airSpeed;
    }

    void setHeading(float _heading)
    {
    	this->_heading = _heading;
    }

    void setAlt_intM(int32_t _alt_intM)
    {
        this->_alt_intM = _alt_intM;
    }

    void setVD(float _vD)
    {
        this->_vD = _vD;
    }

    void setGroundSpeed(float _groundSpeed)
    {
        this->_groundSpeed = _groundSpeed;
    }

    void setLat_degInt(int32_t _lat_degInt)
    {
        this->_lat_degInt = _lat_degInt;
    }

    void setLon_degInt(int32_t _lon_degInt)
    {
        this->_lon_degInt = _lon_degInt;
    }

    void setPitch(float _pitch)
    {
        this->_pitch = _pitch;
    }

    void setPitchRate(float _pitchRate)
    {
        this->_pitchRate = _pitchRate;
    }

    void setRoll(float _roll)
    {
        this->_roll = _roll;
    }

    void setRollRate(float _rollRate)
    {
        this->_rollRate = _rollRate;
    }

    void setYaw(float _yaw)
    {
        this->_yaw = _yaw;
    }

    void setYawRate(float _yawRate)
    {
        this->_yawRate = _yawRate;
    }
    void setTimeStamp(int32_t _timeStamp)
    {
        this->_timeStamp = _timeStamp;
    }
    int32_t getTimeStamp() const
    {
        return _timeStamp;
    }

protected:
	AP_HardwareAbstractionLayer * _hal;
private:
	int32_t _timeStamp; // micros clock
	float _roll; // rad
	float _rollRate; //rad/s
	float _pitch; // rad
	float _pitchRate; // rad/s
	float _yaw; // rad
	float _yawRate; // rad/s
	float _airSpeed; // m/s
	float _groundSpeed; // m/s
	float _heading; // rad
	float _vD; // m/s
	int32_t _lat_degInt; // deg / 1e7
	int32_t _lon_degInt; // deg / 1e7
	int32_t _alt_intM; // meters / 1e3
};

class DcmNavigator: public AP_Navigator {
private:
	/**
	 * Sensors
	 */

	RangeFinder * _rangeFinderDown;
	AP_DCM * _dcm;
	IMU * _imu;
	uint16_t _imuOffsetAddress;

public:
	DcmNavigator(AP_HardwareAbstractionLayer * hal) :
		AP_Navigator(hal), _dcm(), _imuOffsetAddress(0) {

		// if orientation equal to front, store as front
		/**
		 * rangeFinder<direction> is assigned values based on orientation which
		 * is specified in ArduPilotOne.pde.
		 */
		for (int i = 0; i < _hal-> rangeFinders.getSize(); i++) {
			if (_hal->rangeFinders[i] == NULL)
				continue;
			if (_hal->rangeFinders[i]->orientation_x == 0
					&& _hal->rangeFinders[i]->orientation_y == 0
					&& _hal->rangeFinders[i]->orientation_z == 1)
				_rangeFinderDown = _hal->rangeFinders[i];
		}

		if (_hal->mode() == MODE_LIVE) {
			if (_hal->adc)
				_hal->imu = new AP_IMU_Oilpan(_hal->adc, _imuOffsetAddress);
			if (_hal->imu)
				_dcm = new AP_DCM(_hal->imu, _hal->gps, _hal->compass);
			if (_hal->compass) {
				_dcm->set_compass(_hal->compass);

			}
		}
		calibrate();
	}
	virtual void calibrate() {
		// TODO: handle cold restart
		if (_hal->imu) {
			/*
			 * Gyro has built in warm up cycle and should
			 * run first */
			_hal->imu->init_gyro();
			_hal->imu->init_accel();
		}
	}
	virtual void updateFast(float dt) {
		if (_hal->mode() != MODE_LIVE)
			return;

		setTimeStamp(micros()); // if running in live mode, record new time stamp


		//_hal->debug->println_P(PSTR("nav loop"));

		/**
		 * The altitued is read off the barometer by implementing the following formula:
		 * altitude (in m) = 44330*(1-(p/po)^(1/5.255)),
		 * where, po is pressure in Pa at sea level (101325 Pa).
		 * See http://www.sparkfun.com/tutorials/253 or type this formula
		 * in a search engine for more information.
		 * altInt contains the altitude in meters.
		 */
		if (_hal->baro) {

			if (_rangeFinderDown != NULL && _rangeFinderDown->distance <= 695)
				setAlt(_rangeFinderDown->distance);

			else {
				float tmp = (_hal->baro->Press/ 101325.0);
				tmp = pow(tmp, 0.190295);
				//setAlt(44330 * (1.0 - tmp)); //sets the altitude in meters XXX wrong, baro reads 0 press
				setAlt(0.0);
			}
		}

		// dcm class for attitude
		if (_dcm) {
			_dcm->update_DCM(dt);
			setRoll(_dcm->roll);
			setPitch(_dcm->pitch);
			setYaw(_dcm->yaw);
			setRollRate(_dcm->get_gyro().x);
			setPitchRate(_dcm->get_gyro().y);
			setYawRate(_dcm->get_gyro().z);



			// need to use lat/lon and convert
			AP_MavlinkCommand home(0);
			setPN((getLat() - home.getLat())/rEarth);
			setPE((getLon() - home.getLon())*cos(home.getLat())/rEarth);
			setPD(-(getAlt() - home.getAlt()));

			/*
			 * accel/gyro debug
			 */
			/*
			 Vector3f accel = _hal->imu->get_accel();
			 Vector3f gyro = _hal->imu->get_gyro();
			 Serial.printf_P(PSTR("accel: %f %f %f gyro: %f %f %f\n"),
			 accel.x,accel.y,accel.z,gyro.x,gyro.y,gyro.z);
			 */
		}
	}
	virtual void updateSlow(float dt) {
		if (_hal->mode() != MODE_LIVE)
			return;

		setTimeStamp(micros()); // if running in live mode, record new time stamp

		if (_hal->gps) {
			_hal->gps->update();
			updateGpsLight();
			if (_hal->gps->fix && _hal->gps->new_data) {
				setLat_degInt(_hal->gps->latitude);
				setLon_degInt(_hal->gps->longitude);
				setAlt_intM(_hal->gps->altitude*10); // gps in cm, intM in mm
				setGroundSpeed(_hal->gps->ground_speed/100.0); // gps is in cm/s
			}
		}

		if (_hal->compass) {
			_hal->compass->read();
			_hal->compass->calculate(getRoll(),getPitch());
			_hal->compass->null_offsets(_dcm->get_dcm_matrix());
		}
	}
	void updateGpsLight(void) {
		// GPS LED on if we have a fix or Blink GPS LED if we are receiving data
		// ---------------------------------------------------------------------
		static bool GPS_light = false;
		switch (_hal->gps->status()) {
		case (2):
			digitalWrite(C_LED_PIN, HIGH); //Turn LED C on when gps has valid fix.
			break;

		case (1):
			if (_hal->gps->valid_read == true) {
				GPS_light = !GPS_light; // Toggle light on and off to indicate gps messages being received, but no GPS fix lock
				if (GPS_light) {
					digitalWrite(C_LED_PIN, LOW);
				} else {
					digitalWrite(C_LED_PIN, HIGH);
				}
				_hal->gps->valid_read = false;
			}
			break;

		default:
			digitalWrite(C_LED_PIN, LOW);
			break;
		}
	}
};

} // namespace apo

#endif // AP_Navigator_H
// vim:ts=4:sw=4:expandtab

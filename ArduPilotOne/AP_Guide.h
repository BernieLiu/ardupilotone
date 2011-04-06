/*
 * AP_Guide.h
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

#ifndef AP_Guide_H
#define AP_Guide_H

#include "GCS_MAVLink.h"
#include "AP_Navigator.h"
#include <AP_Common.h>
#include <AP_Vector.h>

#include "AP_MavlinkCommand.h"

namespace apo {

/// Guide class
class AP_Guide {
public:

	/**
	 * This is the constructor, which requires a link to the navigator.
	 * @param navigator This is the navigator pointer.
	 */
	AP_Guide(AP_Navigator * navigator, AP_HardwareAbstractionLayer * hal) :
		_navigator(navigator), headingCommand(0), airSpeedCommand(0),
				groundSpeedCommand(0), altitudeCommand(0),
				pNCmd(0), pECmd(0), pDCmd(0), _hal(hal) {
	}

	virtual void update() = 0;
	virtual void nextCommand() = 0;
	float headingCommand;
	float airSpeedCommand;
	float groundSpeedCommand;
	float altitudeCommand;
	float pNCmd;
	float pECmd;
	float pDCmd;
	static float rEarth;
protected:
	uint8_t _cmdNum;
	uint8_t _cmdIndex;
	AP_Navigator * _navigator;
	AP_HardwareAbstractionLayer * _hal;
};

class MavlinkGuide: public AP_Guide {
public:
	MavlinkGuide(AP_Navigator * navigator, AP_HardwareAbstractionLayer * hal) :
		AP_Guide(navigator,hal),_rangeFinderFront(), _rangeFinderBack(),
		 _rangeFinderLeft(), _rangeFinderRight(),
		_prevCommand(0), _nextCommand(1) {
		for (int i = 0; i < _hal->rangeFinders.getSize(); i++) {
			RangeFinder * rF = _hal->rangeFinders[i];
			if (rF == NULL)
				continue;
			if (rF->orientation_x == 1 && rF->orientation_y == 0
					&& rF->orientation_z == 0)
				_rangeFinderFront = rF;
			else if (rF->orientation_x == -1 && rF->orientation_y == 0
					&& rF->orientation_z == 0)
				_rangeFinderBack = rF;
			else if (rF->orientation_x == 0 && rF->orientation_y == 1
					&& rF->orientation_z == 0)
				_rangeFinderRight = rF;
			else if (rF->orientation_x == 0 && rF->orientation_y == -1
					&& rF->orientation_z == 0)
				_rangeFinderLeft = rF;

		}
	}

	virtual void update() {
		_hal->debug->println_P(PSTR("guide loop"));

		// TODO, setting to a fixed value for testing with the car right now
		float temp = crossTrack()*-0.001; // crosstrack gain
		if (temp > M_PI/2) temp = M_PI/2;
		if (temp < -M_PI/2) temp = -M_PI/2;
		headingCommand = _prevCommand.bearingTo(_nextCommand) + temp;
		groundSpeedCommand = 3;

		pNCmd = 1;
		pECmd = 2;
		pDCmd = -3;


		// process mavlink commands
		//handleCommand();

		// obstacle avoidance overrides
		// stop if your going to drive into something in front of you
		if (_rangeFinderFront && _rangeFinderFront->distance < 30) {
			airSpeedCommand = 0;
			groundSpeedCommand = 0;
		}
		if (_rangeFinderBack && _rangeFinderBack->distance < 30) {
			airSpeedCommand = 0;
			groundSpeedCommand = 0;
		}

		if (_rangeFinderLeft && _rangeFinderLeft->distance < 30) {
			airSpeedCommand = 0;
			groundSpeedCommand = 0;
		}

		if (_rangeFinderRight && _rangeFinderRight->distance < 30) {
			airSpeedCommand = 0;
			groundSpeedCommand = 0;
		}
	}

	//calculates cross track of a current location
	float crossTrack() {
		float d = _prevCommand.distanceTo(_navigator->latInt,_navigator->lonInt);
		float bCurrent = _prevCommand.bearingTo(_navigator->latInt,_navigator->lonInt);
		float bNext = _prevCommand.bearingTo(_nextCommand);
		return asin(sin(d/rEarth) * sin(bCurrent - bNext)) * rEarth;
	}

	// calculates along  track distance of a current location
	float alongTrack() {
		float dXt = crossTrack();
		float d = _prevCommand.distanceTo(_navigator->latInt,_navigator->lonInt);
		return acos(cos(d / rEarth) / cos(dXt / rEarth)) * rEarth;
	}

	void nextCommand() {
		// if command index is exceeded, return home and repeat the mission
		_prevCommand = AP_MavlinkCommand(_cmdIndex);
		if (_cmdIndex++ > _cmdNum) _cmdIndex = 0;
		_nextCommand = AP_MavlinkCommand(_cmdIndex);
	}

	void handleCommand()
	{
		// TODO handle more commands
		switch(_nextCommand.getCommand()) {
		case MAV_CMD_NAV_WAYPOINT:
		{
			// if within radius, increment
			float d = _prevCommand.distanceTo(_navigator->latInt,_navigator->lonInt);
			if (d < _nextCommand.getRadius())
			{
				nextCommand();
			}
			break;
		}
		/*
		case MAV_CMD_CONDITION_CHANGE_ALT:
		case MAV_CMD_CONDITION_DELAY:
		case MAV_CMD_CONDITION_DISTANCE:
		case MAV_CMD_CONDITION_LAST:
		case MAV_CMD_CONDITION_YAW:
		case MAV_CMD_DO_CHANGE_SPEED:
		case MAV_CMD_DO_CONTROL_VIDEO:
		case MAV_CMD_DO_JUMP:
		case MAV_CMD_DO_LAST:
		case MAV_CMD_DO_LAST:
		case MAV_CMD_DO_REPEAT_RELAY:
		case MAV_CMD_DO_REPEAT_SERVO:
		case MAV_CMD_DO_SET_HOME:
		case MAV_CMD_DO_SET_MODE:
		case MAV_CMD_DO_SET_PARAMETER:
		case MAV_CMD_DO_SET_RELAY:
		case MAV_CMD_DO_SET_SERVO:
		case MAV_CMD_PREFLIGHT_CALIBRATION:
		case MAV_CMD_PREFLIGHT_STORAGE:
		case MAV_CMD_NAV_LAND:
		case MAV_CMD_NAV_LAST:
		case MAV_CMD_NAV_LOITER_TIME:
		case MAV_CMD_NAV_LOITER_TURNS:
		case MAV_CMD_NAV_LOITER_UNLIM:
		case MAV_CMD_NAV_ORIENTATION_TARGET:
		case MAV_CMD_NAV_PATHPLANNING:
		case MAV_CMD_NAV_RETURN_TO_LAUNCH:
		case MAV_CMD_NAV_TAKEOFF:
		*/
		default:
			// unhandled command, skip
			nextCommand();
			break;
		}
	}
private:
	RangeFinder * _rangeFinderFront;
	RangeFinder * _rangeFinderBack;
	RangeFinder * _rangeFinderLeft;
	RangeFinder * _rangeFinderRight;
	AP_MavlinkCommand _prevCommand;
	AP_MavlinkCommand _nextCommand;
};
float AP_Guide::rEarth = 6371000;

} // namespace apo

#endif // AP_Guide_H
// vim:ts=4:sw=4:expandtab

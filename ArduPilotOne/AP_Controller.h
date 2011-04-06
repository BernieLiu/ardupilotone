/*
 * AP_Controller.h
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

#ifndef AP_Controller_H
#define AP_Controller_H

#include "AP_Navigator.h"
#include "AP_Guide.h"
#include "AP_HardwareAbstractionLayer.h"

#include <AP_Common.h>
#include <AP_Vector.h>
#include <AP_Var.h>
#include <APM_RC.h>

namespace apo {

const float one = 1.0;
const float zero = 0.0;
const float negativeOne = -1.0;

/// Controller class
class AP_Controller {
public:
	AP_Controller(AP_Navigator * nav, AP_Guide * guide,
			AP_HardwareAbstractionLayer * hal) :
		_nav(nav), _guide(guide), _hal(hal) {
	}
	class Block {
	public:
		Block() :
			_input(), _output() {
		}
		virtual void update(const float & dt) = 0;
		virtual void connect(Block * block) {
			if (!block)
				return;
			for (int i = 0; i < block->getOutput().getSize(); i++)
				_input.push_back(block->getOutput()[i]);
		}
		const Vector<float *> & getOutput() const {
			return _output;
		}
		const float & input(int i) {
			return (*_input[i]);
		}
		float & output(int i) {
			return (*_output[i]);
		}
	protected:
		Vector<const float *> _input;
		Vector<float *> _output;
	};

	void addBlock(Block * block) {
		if (!block) {
			_hal->debug->println("Attemping to add a null block");
			return;
		}
		if (_blocks.getSize() > 0) {
			if (_blocks[_blocks.getSize() - 1] == NULL) {
				_hal->debug->println("Attempted to connect to null block");
				return;
			} else {
				block->connect(_blocks[_blocks.getSize() - 1]);
			}
		}
		_blocks.push_back(block);
	}
	virtual void update(const float & dt) {
		for (int i = 0; i < _blocks.getSize(); i++) {
			if (!_blocks[i])
				continue;
			_blocks[i]->update(dt);
		}
	}

protected:
	Vector<Block *> _blocks;
	AP_Navigator * _nav;
	AP_Guide * _guide;
	AP_HardwareAbstractionLayer * _hal;
};

/// Servo Block
class ToServo: public AP_Controller::Block {
public:
	ToServo(AP_RcChannelSimple * ch) :
		_ch(ch) {
	}
	virtual void update(const float & dt = 0) {
		//Serial.println("calling to servo update");
		Serial.println("input: ");
		Serial.println(input(0));
		if (_input.getSize() > 0) {
			_ch->setPosition(input(0));
		}
	}
protected:
	float _position;
	AP_RcChannelSimple * _ch;
};

/// SumGain
class SumGain: public AP_Controller::Block {
public:
	/// Constructor that allows 1-8 sum gain pairs, more
	/// can be added if necessary
	SumGain(const float * var1 = NULL, const float * gain1 = NULL,
			const float * var2 = NULL, const float * gain2 = NULL,
			const float * var3 = NULL, const float * gain3 = NULL,
			const float * var4 = NULL, const float * gain4 = NULL,
			const float * var5 = NULL, const float * gain5 = NULL,
			const float * var6 = NULL, const float * gain6 = NULL,
			const float * var7 = NULL, const float * gain7 = NULL,
			const float * var8 = NULL, const float * gain8 = NULL) {
		if ((var1 != NULL) && (gain1 != NULL))
			add(var1, gain1);
		if ((var2 != NULL) && (gain2 != NULL))
			add(var2, gain2);
		if ((var3 != NULL) && (gain3 != NULL))
			add(var3, gain3);
		if ((var4 != NULL) && (gain4 != NULL))
			add(var4, gain4);
		if ((var5 != NULL) && (gain5 != NULL))
			add(var5, gain5);
		if ((var6 != NULL) && (gain6 != NULL))
			add(var6, gain6);
		if ((var7 != NULL) && (gain7 != NULL))
			add(var7, gain7);
		if ((var8 != NULL) && (gain8 != NULL))
			add(var8, gain8);

		// create output
		_output.push_back(new float(0.0));
	}
	void add(const float * var, const float * gain) {
		_input.push_back(var);
		_gain.push_back(gain);
	}
	virtual void update(const float & dt = 0) {
		//Serial.println("calling sumgain update");
		if (_output.getSize() < 1)
			return;
		float sum = 0;
		for (int i = 0; i < _input.getSize(); i++) {
			//Serial.println("input: "); Serial.println(input(i));
			//Serial.println("gain: ");Serial.println(gain(i));
			sum += input(i) * gain(i);
		}
		output(0) = sum;
	}
	float gain(int i) {
		return *(_gain[i]);
	}
protected:
	Vector<const float *> _gain;
};

/// PID block
class Pid: public AP_Controller::Block {
public:
	Pid(AP_Var::Key key, const prog_char_t * name, float kP = 0.0,
			float kI = 0.0, float kD = 0.0, float iMax = 0.0, float yMax = 0.0,
			uint8_t dFcut = 20.0) :
		_group(key, name), _eP(0), _eI(0), _eD(0),
				_kP(&_group, 1, kP, PSTR("P")), _kI(&_group, 2, kI, PSTR("I")),
				_kD(&_group, 3, kD, PSTR("D")),
				_iMax(&_group, 4, iMax, PSTR("IMAX")),
				_yMax(&_group, 5, yMax, PSTR("YMAX")),
				_fCut(&_group, 6, dFcut, PSTR("FCUT")) {
		// create output
		_output.push_back(new float(0.0));
	}

	virtual void update(const float & dt) {
		//Serial.println("calling pid update");
		//Serial.println("input: "); Serial.println(input(0));

		if (_output.getSize() < 1 || (!_input[0]) || (!_output[0]))
			return;

		// derivative with low pass
		float RC = 1 / (2 * M_PI * _fCut); // low pass filter
		_eD = _eD + ((_eP - input(0)) / dt - _eD) * (dt / (dt + RC));

		// proportional, note must come after derivative
		// because derivatve uses _eP as previous value
		_eP = input(0);

		// integral
		_eI += _eP * dt;

		// wind up guard
		if (_eI > _iMax)
			_eI = _iMax;
		else if (_eI < -_iMax)
			_eI = -_iMax;

		// pid sum
		float y = _kP * _eP + _kI * _eI + _kD * _eD;

		// saturation
		if (y > _yMax)
			y = _yMax;
		else if (y < -_yMax)
			y = -_yMax;

		output(0) = y;

		//Serial.println("output: "); Serial.println(output(0));

		// debug output
		/*
		 Serial.println("kP, kI, kD: ");
		 Serial.print(_kP,5); Serial.print(" ");
		 Serial.print(_kI,5); Serial.print(" ");
		 Serial.println(_kD,5);
		 Serial.print("eP, eI, eD: ");
		 Serial.print(_eP,5); Serial.print(" ");
		 Serial.print(_eI,5); Serial.print(" ");
		 Serial.println(_eD,5);
		 Serial.print("input: ");
		 Serial.println(input(0),5);
		 Serial.print("output: ");
		 Serial.println(output(0),5);
		 */
	}
protected:
	AP_Var_group _group; /// helps with parameter management
	float _eP; /// input
	float _eI; /// integral of input
	float _eD; /// derivative of input
	AP_Float _kP; /// proportional gain
	AP_Float _kI; /// integral gain
	AP_Float _kD; /// derivative gain
	AP_Float _iMax; /// integrator saturation
	AP_Float _yMax; /// output saturation
	AP_Uint8 _fCut; /// derivative low-pass cut freq (Hz)
};

/// Sink block
/// saves input port to variable
class Sink: public AP_Controller::Block {
public:
	Sink(float & var, uint8_t port = 0) :
		_var(var), _port(port) {
	}
	virtual void update(const float & dt) {
		//Serial.println("calling sink update");
		//Serial.println("input: "); Serial.println(input(0));
		_var = input(_port);
	}
protected:
	float & _var;
	uint8_t _port;
};

/// Saturate block
/// Constrains output to a range
class Saturate: public AP_Controller::Block {
public:
	Saturate(float & min, float & max, uint8_t port = 0) :
		_min(min), _max(max), _port(port) {
		// create output
		//Serial.println("calling satruate update");
		_output.push_back(new float(0.0));
	}
	virtual void update(const float & dt) {
		float u = input(_port);
		if (u > _max)
			u = _max;
		if (u < _min)
			u = _min;
		output(_port) = u;
	}
protected:
	uint8_t _port;
	float & _min;
	float & _max;
};

/// PID(DFB) block
class PidDFB: public AP_Controller::Block {
public:
	PidDFB(AP_Var::Key key, const prog_char_t * name, float * derivative,
			float kP = 0.0, float kI = 0.0, float kD = 0.0, float iMax = 0.0,
			float yMax = 0.0) :
		_group(key, name), _derivative(derivative), _eP(0), _eI(0), _eD(0),
				_kP(&_group, 1, kP, PSTR("P")), _kI(&_group, 2, kI, PSTR("I")),
				_kD(&_group, 3, kD, PSTR("D")),
				_iMax(&_group, 4, iMax, PSTR("IMAX")),
				_yMax(&_group, 4, yMax, PSTR("YMAX")) {
		// create output
		_output.push_back(new float(0.0));
	}

	virtual void update(const float & dt) {
		if (_output.getSize() < 1 || (!_input[0]) || (!_output[0]))
			return;

		// proportional, note must come after derivative
		// because derivative uses _eP as previous value
		_eP = input(0);

		// integral
		_eI += _eP * dt;

		// wind up guard
		if (_eI > _iMax)
			_eI = _iMax;
		else if (_eI < -_iMax)
			_eI = -_iMax;

		// pid sum
		float y = _kP * _eP + _kI * _eI - _kD * (*_derivative);

		// saturation
		if (y > _yMax)
			y = _yMax;
		else if (y < -_yMax)
			y = -_yMax;

		output(0) = y;

		// debug output
		/*
		 Serial.println("kP, kI, kD: ");
		 Serial.print(_kP,5); Serial.print(" ");
		 Serial.print(_kI,5); Serial.print(" ");
		 Serial.println(_kD,5);
		 Serial.print("eP, eI, eD: ");
		 Serial.print(_eP,5); Serial.print(" ");
		 Serial.print(_eI,5); Serial.print(" ");
		 Serial.println(_eD,5);
		 Serial.print("input: ");
		 Serial.println(input(0),5);
		 Serial.print("output: ");
		 Serial.println(output(0),5);
		 */
	}
protected:
	AP_Var_group _group; /// helps with parameter management
	float _eP; /// input
	float _eI; /// integral of input
	float _eD; /// derivative of input
	AP_Float _kP; /// proportional gain
	AP_Float _kI; /// integral gain
	AP_Float _kD; /// derivative gain
	AP_Float _iMax; /// integrator saturation
	AP_Float _yMax; /// integrator saturation
	float * _derivative; // derivative fed back
};

} // apo

#endif // AP_Controller_H
// vim:ts=4:sw=4:expandtab

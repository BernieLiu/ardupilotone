// -*- tab-width: 4; Mode: C++; c-basic-offset: 4; indent-tabs-mode: t -*-

/// @file	ACM_PI.cpp
/// @brief	Generic PI algorithm

#include <math.h>

#include "APM_PI.h"

long
APM_PI::get_pi(int32_t error, float dt, bool calc_i)
{
	if(calc_i){
		_integrator += ((float)error * _ki) * dt;

		if (_integrator < -_imax) {
			_integrator = -_imax;
		} else if (_integrator > _imax) {
			_integrator = _imax;
		}
	}
	return (float)error * _kp + _integrator;
}

void
APM_PI::reset_I()
{
	_integrator = 0;
}

void
APM_PI::load_gains()
{
    _group.load();
}

void
APM_PI::save_gains()
{
    _group.save();
}

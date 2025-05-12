/*
 * SPDX-FileName: attitude_indicator.cxx
 * SPDX-FileComment: a vacuum-powered attitude indicator.
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileContributor:  Written by David Megginson, started 2002.
 * SPDX-FileContributor: Enhanced by Benedikt Hallinger, 2023
 */

#include "config.h"

#include "attitude_indicator.hxx"

#include <simgear/compiler.h>

#include <iostream>
#include <string>
#include <sstream>

#include <cmath>    // fabs()

#include <Main/fg_props.hxx>
#include <Main/util.hxx>

using std::string;

AttitudeIndicator::AttitudeIndicator ( SGPropertyNode *node )
    :
    _name(node->getStringValue("name", "attitude-indicator")),
    _num(node->getIntValue("number", 0)),
    _suction(node->getStringValue("suction", "/systems/vacuum/suction-inhg")),
    spin_thresh(0.8),
    max_roll_error(40.0),
    max_pitch_error(12.0)
{
    _minVacuum = node->getDoubleValue("minimum-vacuum", 4.5);

    SGPropertyNode* limits_cfg = node->getChild("limits", 0, true);
    spin_thresh = limits_cfg->getDoubleValue("spin-thresh", 0.8);
    max_roll_error = limits_cfg->getDoubleValue("max-roll-error-deg", 40.0);
    max_pitch_error = limits_cfg->getDoubleValue("max-pitch-error-deg", 12.0);

    SGPropertyNode* gyro_cfg = node->getChild("gyro", 0, true);
    _gyro_spin_up = gyro_cfg->getDoubleValue("spin-up-sec", 4.0);
    _gyro_spin_down = gyro_cfg->getDoubleValue("spin-down-sec", 180.0);
}

AttitudeIndicator::~AttitudeIndicator ()
{
}

void
AttitudeIndicator::init ()
{
    string branch;
    branch = "/instrumentation/" + _name;

    SGPropertyNode *node = fgGetNode(branch, _num, true );
    SGPropertyNode *n;
    
    _pitch_in_node = fgGetNode("/orientation/pitch-deg", true);
    _roll_in_node = fgGetNode("/orientation/roll-deg", true);
    _suction_node = fgGetNode(_suction, true);
    SGPropertyNode *cnode = node->getChild("config", 0, true);
    _tumble_flag_node = cnode->getChild("tumble-flag", 0, true);
    _caged_node = node->getChild("caged-flag", 0, true);
    _tumble_node = node->getChild("tumble-norm", 0, true);
    if( ( n = cnode->getChild("spin-thresh", 0, false ) ) != NULL )
      spin_thresh = n->getDoubleValue();
    if( ( n = cnode->getChild("max-roll-error-deg", 0, false ) ) != NULL )
      max_roll_error = n->getDoubleValue();
    if( ( n = cnode->getChild("max-pitch-error-deg", 0, false ) ) != NULL )
      max_pitch_error = n->getDoubleValue();
    _pitch_int_node = node->getChild("internal-pitch-deg", 0, true);
    _roll_int_node = node->getChild("internal-roll-deg", 0, true);
    _pitch_out_node = node->getChild("indicated-pitch-deg", 0, true);
    _roll_out_node = node->getChild("indicated-roll-deg", 0, true);

    _spin_node = node->getChild("spin", 0, true);

    SGPropertyNode* gyro_node = node->getChild("gyro", 0, true);
    _gyro_spin_up_node = gyro_node->getChild("spin-up-sec", 0, true);
    _gyro_spin_down_node = gyro_node->getChild("spin-down-sec", 0, true);
    if (!_gyro_spin_up_node->hasValue())
        _gyro_spin_up_node->setDoubleValue(_gyro_spin_up);
    if (!_gyro_spin_down_node->hasValue())
        _gyro_spin_down_node->setDoubleValue(_gyro_spin_down);
    _minVacuum_node = node->getChild("minimum-vacuum", 0, true);
    if (!_minVacuum_node->hasValue())
        _minVacuum_node->setDoubleValue(_minVacuum);

    reinit();
}

void
AttitudeIndicator::reinit ()
{
    _roll_int_node->setDoubleValue(0.0);
    _pitch_int_node->setDoubleValue(0.0);
    _gyro.reinit();
}

void
AttitudeIndicator::bind ()
{
    std::ostringstream temp;
    string branch;
    temp << _num;
    branch = "/instrumentation/" + _name + "[" + temp.str() + "]";

    fgTie((branch + "/serviceable").c_str(),
          &_gyro, &Gyro::is_serviceable, &Gyro::set_serviceable);
}

void
AttitudeIndicator::unbind ()
{
    std::ostringstream temp;
    string branch;
    temp << _num;
    branch = "/instrumentation/" + _name + "[" + temp.str() + "]";

    fgUntie((branch + "/serviceable").c_str());
}

void
AttitudeIndicator::update (double dt)
{
    // Get the spin from the gyro
    _minVacuum = _minVacuum_node->getDoubleValue();
    _gyro.set_power_norm(_suction_node->getDoubleValue() / _minVacuum);
    _gyro.set_spin_up(_gyro_spin_up_node->getDoubleValue());
    _gyro.set_spin_down(_gyro_spin_down_node->getDoubleValue());
    _gyro.set_spin_norm(_spin_node->getDoubleValue());
    _gyro.update(dt);
    double spin = _gyro.get_spin_norm();
    _spin_node->setDoubleValue(spin);

    // Calculate the responsiveness
    double responsiveness = spin * spin * spin * spin * spin * spin;

                                // Get the indicated roll and pitch
    double roll = _roll_in_node->getDoubleValue();
    double pitch = _pitch_in_node->getDoubleValue();
    bool isCaged = _caged_node->getBoolValue();

    // Calculate the tumble for the
    // next pass.
    if (_tumble_flag_node->getBoolValue()) {
        double tumble = _tumble_node->getDoubleValue();
        if (fabs(roll) > 45.0) {
            double target = (fabs(roll) - 45.0) / 45.0;
            target *= target;   // exponential past +-45 degrees
            if (roll < 0)
                target = -target;

            if (fabs(target) > fabs(tumble))
                tumble = target;

            if (tumble > 1.0)
                tumble = 1.0;
            else if (tumble < -1.0)
                tumble = -1.0;
        }
                                    // Reerect in 5 minutes
        double t_reerect = isCaged ? 1.0 : 300.0;
        double step = dt / t_reerect;
        if (tumble < -step)
            tumble += step;
        else if (tumble > step)
            tumble -= step;

        roll += tumble * 45;
        _tumble_node->setDoubleValue(tumble);
    }

    // If it's caged, it doesn't indicate
    if (isCaged) {
        _roll_int_node->setDoubleValue(0.0);
        _pitch_int_node->setDoubleValue(0.0);
        return;
    }

    roll = fgGetLowPass(_roll_int_node->getDoubleValue(), roll,
                        responsiveness);
    pitch = fgGetLowPass(_pitch_int_node->getDoubleValue(), pitch,
                         responsiveness);

                                // Assign the new values
    _roll_int_node->setDoubleValue(roll);
    _pitch_int_node->setDoubleValue(pitch);

    // add in a gyro underspin "error" if gyro is spinning too slowly
    double roll_error;
    double pitch_error;
    if ( spin <= spin_thresh ) {
        double roll_error_factor = (spin_thresh - spin) / spin_thresh;
        double pitch_error_factor = (spin_thresh - spin) / spin_thresh;
        roll_error = roll_error_factor * roll_error_factor * max_roll_error;
        pitch_error = pitch_error_factor * pitch_error_factor * max_pitch_error;
    } else {
        roll_error = 0.0;
        pitch_error = 0.0;
    }

    _roll_out_node->setDoubleValue(roll + roll_error);
    _pitch_out_node->setDoubleValue(pitch + pitch_error);
}


// Register the subsystem.
#if 0
SGSubsystemMgr::InstancedRegistrant<AttitudeIndicator> registrantAttitudeIndicator(
    SGSubsystemMgr::FDM,
    {{"instrumentation", SGSubsystemMgr::Dependency::HARD}});
#endif

// end of attitude_indicator.cxx

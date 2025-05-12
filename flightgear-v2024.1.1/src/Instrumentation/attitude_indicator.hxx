/*
 * SPDX-FileName: attitude_indicator.hxx
 * SPDX-FileComment: a vacuum-powered attitude indicator.
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileContributor:  Written by David Megginson, started 2002.
 */

#pragma once

#include <simgear/props/props.hxx>
#include <simgear/structure/subsystem_mgr.hxx>

#include "gyro.hxx"


/**
 * Model a vacuum-powered attitude indicator.
 *
 * Config:
 *   minimum-vacuum       Default 4.5 inHG
 *   gyro/spin-up-sec     If given, seconds to spin up until power-norm (from 0->100%)
 *   gyro/spin-down-sec   If given, seconds the gyro will loose spin without power (from 100%->0)
 *   limits/spin-thresh
 *   limits/max-roll-error-deg
 *   limits/max-pitch-error-deg
 * 
 * Input properties:
 *
 * /instrumentation/"name"/config/tumble-flag
 * /instrumentation/"name"/serviceable
 * /instrumentation/"name"/caged-flag
 * /instrumentation/"name"/tumble-norm
 * /orientation/pitch-deg
 * /orientation/roll-deg
 * "vacuum-system"/suction-inhg
 *
 * Output properties:
 *
 * /instrumentation/"name"/indicated-pitch-deg
 * /instrumentation/"name"/indicated-roll-deg
 * /instrumentation/"name"/tumble-norm
 */
class AttitudeIndicator : public SGSubsystem
{
public:
    AttitudeIndicator ( SGPropertyNode *node );
    virtual ~AttitudeIndicator ();

    // Subsystem API.
    void bind() override;
    void init() override;
    void reinit() override;
    void unbind() override;
    void update(double dt) override;

    // Subsystem identification.
    static const char* staticSubsystemClassId() { return "attitude-indicator"; }

private:
    std::string _name;
    int _num;
    std::string _suction;

    Gyro _gyro;
    double _gyro_spin_up, _gyro_spin_down;
    double _minVacuum;
    SGPropertyNode_ptr _minVacuum_node;

    SGPropertyNode_ptr _tumble_flag_node;
    SGPropertyNode_ptr _caged_node;
    SGPropertyNode_ptr _tumble_node;
    SGPropertyNode_ptr _pitch_in_node;
    SGPropertyNode_ptr _roll_in_node;
    SGPropertyNode_ptr _suction_node;
    SGPropertyNode_ptr _pitch_int_node;
    SGPropertyNode_ptr _roll_int_node;
    SGPropertyNode_ptr _pitch_out_node;
    SGPropertyNode_ptr _roll_out_node;
    SGPropertyNode_ptr _spin_node, _gyro_spin_up_node, _gyro_spin_down_node;

    double spin_thresh;
    double max_roll_error;
    double max_pitch_error;
};

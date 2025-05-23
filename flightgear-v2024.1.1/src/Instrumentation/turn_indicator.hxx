/*
 * SPDX-FileName: turn_indicator.hxx
 * SPDX-FileComment: an electric-powered turn indicator.
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileContributor:  Written by David Megginson, started 2002.
 */


#pragma once

#include <Instrumentation/AbstractInstrument.hxx>

#include "gyro.hxx"


/**
 * Model 
 *
 * This class does not model the slip/skid ball; that is properly
 * a separate instrument.
 *
 * Input properties:
 *
 * /instrumentation/"name"/serviceable
 * /instrumentation/"name"/spin
 * /orientation/roll-rate-degps
 * /orientation/yaw-rate-degps
 * /systems/electrical/outputs/turn-coordinator (see below)
 *
 * Output properties:
 *
 * /instrumentation/"name"/indicated-turn-rate
 * 
 * Configuration:
 * 
 *   name
 *   number
 *   new-default-power-path: use /systems/electrical/outputs/turn-indicator[ number ] instead of 
 *                           /systems/electrical/outputs/turn-coordinator as the default power
 *                           supply path (not used when power-supply is set)
 *   power-supply
 *   minimum-supply-volts
 *   gyro-spin-up-sec        If given, seconds to spin up until power-norm (from 0->100%)
 *   gyro-spin-down-sec      If given, seconds the gyro will loose spin without power (from 100%->0)
 * 
 * Notes on the power supply path:
 *   
 *   For backwards compatibility reasons, the default power path is 
 *   /systems/electrical/outputs/turn-coordinator, unless new-default-power-path is set to 1,
 *   in which case the new default path /systems/electrical/outputs/turn-indicator[ number ]
 *   is used. As the new path is more logical and consistent with instrument naming, newly
 *   developed and actively maintained aircraft should switch their electrical system to write
 *   to /systems/electrical/outputs/turn-indicator[ number ] and set new-default-power-path.
 *   The legacy default path will eventually be phased out.
 *   The power path can always be set manually by using the power-supply config tag.
 * 
 */
class TurnIndicator : public AbstractInstrument
{
public:
    TurnIndicator ( SGPropertyNode *node );
    virtual ~TurnIndicator ();

    // Subsystem API.
    void init() override;
    void reinit() override;
    void update(double dt) override;

    // Subsystem identification.
    static const char* staticSubsystemClassId() { return "turn-indicator"; }

private:
    Gyro _gyro;
    double _last_rate;
    double _gyro_spin_up, _gyro_spin_down;

    SGPropertyNode_ptr _roll_rate_node;
    SGPropertyNode_ptr _yaw_rate_node;
    SGPropertyNode_ptr _rate_out_node;
    SGPropertyNode_ptr _spin_node, _gyro_spin_up_node, _gyro_spin_down_node;
};

/*
 * SPDX-FileName: heading_indicator.hxx
 * SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileContributor:  Written by David Megginson, started 2002.
 */

#pragma once


#include <simgear/props/props.hxx>
#include <simgear/structure/subsystem_mgr.hxx>

#include "gyro.hxx"


/**
 * Model a vacuum-powered heading indicator.
 *
 * Input properties:
 *
 * /instrumentation/"name"/serviceable
 * /instrumentation/"name"/spin
 * /instrumentation/"name"/offset-deg
 * /orientation/heading-deg
 * "vacuum_system"/suction-inhg
 *
 * Output properties:
 *
 * /instrumentation/"name"/indicated-heading-deg
 */
class HeadingIndicator : public SGSubsystem
{
public:
    HeadingIndicator ( SGPropertyNode *node );
    HeadingIndicator ();
    virtual ~HeadingIndicator ();

    // Subsystem API.
    void bind() override;
    void init() override;
    void reinit() override;
    void unbind() override;
    void update(double dt) override;

    // Subsystem identification.
    static const char* staticSubsystemClassId() { return "heading-indicator"; }

private:
    Gyro _gyro;
    double _last_heading_deg;

    std::string _name;
    int _num;
    std::string _suction;

    SGPropertyNode_ptr _offset_node;
    SGPropertyNode_ptr _heading_in_node;
    SGPropertyNode_ptr _suction_node;
    SGPropertyNode_ptr _heading_out_node;
    SGPropertyNode_ptr _heading_bug_error_node;
    SGPropertyNode_ptr _heading_bug_node;
};

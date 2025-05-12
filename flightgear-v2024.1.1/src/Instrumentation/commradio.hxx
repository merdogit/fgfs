/*
 * SPDX-FileComment: class to manage a comm radio instance
 * SPDX-FileCopyrightText: Copyright (C) 2014 Torsten Dreyer
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <simgear/props/props.hxx>
#include <Instrumentation/AbstractInstrument.hxx>

namespace Instrumentation {

class SignalQualityComputer : public SGReferenced
{
public:
    virtual ~SignalQualityComputer();
    virtual double computeSignalQuality( double distance_nm ) const = 0;
};

typedef SGSharedPtr<SignalQualityComputer> SignalQualityComputerRef;

class CommRadio : public AbstractInstrument
{
public:
    // Subsystem identification.
    static const char* staticSubsystemClassId() { return "comm-radio"; }

    static SGSubsystem * createInstance( SGPropertyNode_ptr rootNode );
};

}


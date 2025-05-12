// beacon.cxx -- Morse code generation class
//
// Written by Curtis Olson, started March 2001.
//
// Copyright (C) 2001  Curtis L. Olson - http://www.flightgear.org/~curt
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//

#include <cstdlib>
#include <cstring>
#include <utility>

#include "beacon.hxx"

#include <simgear/sound/sample.hxx>
#include <simgear/structure/exception.hxx>
#include <simgear/debug/logstream.hxx>

// constructor
FGBeacon::FGBeacon()
{
}

// destructor
FGBeacon::~FGBeacon() {
}


// allocate and initialize sound samples
bool FGBeacon::init() {
    unsigned char *ptr;
    size_t i, len;

    auto inner_buf = std::unique_ptr<unsigned char, decltype(free)*>{
          reinterpret_cast<unsigned char*>( malloc( INNER_SIZE ) ),
          free
    };
    auto middle_buf = std::unique_ptr<unsigned char, decltype(free)*>{
          reinterpret_cast<unsigned char*>( malloc( MIDDLE_SIZE ) ),
          free
    };
    auto outer_buf = std::unique_ptr<unsigned char, decltype(free)*>{
          reinterpret_cast<unsigned char*>( malloc( OUTER_SIZE ) ),
          free
    };

    // Make inner marker beacon sound
    len= (int)(INNER_DIT_LEN / 2.0 );
    unsigned char inner_dit[INNER_DIT_LEN];
    make_tone( inner_dit, INNER_FREQ, len, INNER_DIT_LEN, TRANSITION_BYTES );

    ptr = inner_buf.get();
    for ( i = 0; i < 6; ++i ) {
	memcpy( ptr, inner_dit, INNER_DIT_LEN );
	ptr += INNER_DIT_LEN;
    }

    try {
        inner = new SGSoundSample( std::move(inner_buf),
                                   INNER_SIZE, BYTES_PER_SECOND );
        inner->set_reference_dist( 10.0 );
        inner->set_max_dist( 20.0 );

        // Make middle marker beacon sound
        len= (int)(MIDDLE_DIT_LEN / 2.0 );
        unsigned char middle_dit[MIDDLE_DIT_LEN];
        make_tone( middle_dit, MIDDLE_FREQ, len, MIDDLE_DIT_LEN,
	            TRANSITION_BYTES );

        len= (int)(MIDDLE_DAH_LEN * 3 / 4.0 );
        unsigned char middle_dah[MIDDLE_DAH_LEN];
        make_tone( middle_dah, MIDDLE_FREQ, len, MIDDLE_DAH_LEN,
	            TRANSITION_BYTES );

        ptr = (unsigned char*)middle_buf.get();
        memcpy( ptr, middle_dit, MIDDLE_DIT_LEN );
        ptr += MIDDLE_DIT_LEN;
        memcpy( ptr, middle_dah, MIDDLE_DAH_LEN );

        middle = new SGSoundSample( std::move(middle_buf),
                                    MIDDLE_SIZE, BYTES_PER_SECOND );
        middle->set_reference_dist( 10.0 );
        middle->set_max_dist( 20.0 );

        // Make outer marker beacon sound
        len= (int)(OUTER_DAH_LEN * 3.0 / 4.0 );
        unsigned char outer_dah[OUTER_DAH_LEN];
        make_tone( outer_dah, OUTER_FREQ, len, OUTER_DAH_LEN,
	            TRANSITION_BYTES );

        ptr = (unsigned char*)outer_buf.get();
        memcpy( ptr, outer_dah, OUTER_DAH_LEN );
        ptr += OUTER_DAH_LEN;
        memcpy( ptr, outer_dah, OUTER_DAH_LEN );

        outer = new SGSoundSample( std::move(outer_buf), OUTER_SIZE,
                                   BYTES_PER_SECOND );
        outer->set_reference_dist( 10.0 );
        outer->set_max_dist( 20.0 );
    } catch ( sg_io_exception &e ) {
        SG_LOG(SG_SOUND, SG_ALERT, e.getFormattedMessage());
    }

    return true;
}

FGBeacon * FGBeacon::_instance = NULL;

FGBeacon * FGBeacon::instance()
{
    if( _instance == NULL ) {
        _instance = new FGBeacon();
        _instance->init();
    }
    return _instance;
}

static const uint64_t sizeToUSec = 1000000UL / FGBeacon::BYTES_PER_SECOND;

FGBeacon::BeaconTiming FGBeacon::getTimingForInner() const
{
    BeaconTiming r;

    const uint64_t ditLen = INNER_DIT_LEN * sizeToUSec;
    r.durationUSec = ditLen;
    r.periodsUSec[0] = ditLen / 2;
    r.periodsUSec[1] = ditLen / 2;
    return r;
}

FGBeacon::BeaconTiming FGBeacon::getTimingForMiddle() const
{
    BeaconTiming r;
    const uint64_t ditLen = MIDDLE_DIT_LEN * sizeToUSec;
    const uint64_t dahLen = MIDDLE_DAH_LEN * sizeToUSec;

    r.durationUSec = MIDDLE_SIZE * sizeToUSec;
    r.periodsUSec[0] = ditLen;
    r.periodsUSec[1] = ditLen;
    r.periodsUSec[2] = (dahLen * 3) / 4;
    r.periodsUSec[3] = dahLen - r.periodsUSec[2];
    return r;
}

FGBeacon::BeaconTiming FGBeacon::getTimingForOuter() const
{
    BeaconTiming r;
    const uint64_t dahLen = OUTER_DAH_LEN * sizeToUSec;

    r.durationUSec = dahLen;
    r.periodsUSec[0] = (dahLen * 3) / 4;
    r.periodsUSec[1] = dahLen - r.periodsUSec[0];
    return r;
}

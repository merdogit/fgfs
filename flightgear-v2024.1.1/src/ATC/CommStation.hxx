/*
 * SPDX-FileName: CommStation.hxx
 * SPDX-FileComment: class describing a single comm station in the Nav DB
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <Airports/airports_fwd.hxx>
#include <Navaids/positioned.hxx>

namespace flightgear
{

class CommStation : public FGPositioned
{
public:
    CommStation(PositionedID aGuid, const std::string& name, FGPositioned::Type t, const SGGeod& pos, int range, int freq);

    void setAirport(PositionedID apt);
    FGAirportRef airport() const;
    
    int rangeNm() const
        { return mRangeNM; }
        
    int freqKHz() const
        { return mFreqKhz; }
        
    double freqMHz() const;
    
    static CommStationRef findByFreq(int freqKhz, const SGGeod& pos, FGPositioned::Filter* filt = NULL);

    static bool isType(FGPositioned::Type ty)
    {
        return (ty >= FGPositioned::FREQ_GROUND) && (ty <= FGPositioned::FREQ_UNICOM);
    }

private:
    int mRangeNM;
    int mFreqKhz;
    PositionedID mAirport;
};

} // namespace flightgear

/*
 * SPDX-FileName: runwayprefs.cxx
 * SPDX-FileComment: class implementations corresponding to runwayprefs.hxx assignments by the AI code
 * SPDX-FileCopyrightText: Copyright (C) 2004 Durk Talsma.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <config.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include <simgear/compiler.h>
#include <simgear/debug/logstream.hxx>
#include <simgear/misc/strutils.hxx>
#include <simgear/structure/exception.hxx>

#include <Airports/runways.hxx>
#include <Main/globals.hxx>

#include "airport.hxx"
#include "runwayprefs.hxx"


using namespace std::string_literals;
using namespace simgear;


/******************************************************************************
 * ScheduleTime
 ***************e*************************************************************/
void ScheduleTime::clear()
{
    start.clear();
    end.clear();
    scheduleNames.clear();
}


ScheduleTime::ScheduleTime(const ScheduleTime& other)
{
    timeVecConstIterator i;
    for (i = other.start.begin(); i != other.start.end(); ++i)
        start.push_back(*i);
    for (i = other.end.begin(); i != other.end.end(); ++i)
        end.push_back(*i);
    stringVecConstIterator k;
    for (k = other.scheduleNames.begin(); k != other.scheduleNames.end(); ++k)
        scheduleNames.push_back(*k);

    tailWind = other.tailWind;
    crssWind = other.tailWind;
}


ScheduleTime& ScheduleTime::operator=(const ScheduleTime& other)
{
    clear();
    timeVecConstIterator i;
    for (i = other.start.begin(); i != other.start.end(); ++i)
        start.push_back(*i);
    for (i = other.end.begin(); i != other.end.end(); ++i)
        end.push_back(*i);
    stringVecConstIterator k;
    for (k = other.scheduleNames.begin(); k != other.scheduleNames.end(); ++k)
        scheduleNames.push_back(*k);

    tailWind = other.tailWind;
    crssWind = other.tailWind;

    return *this;
}

std::string ScheduleTime::getName(time_t dayStart)
{
    if ((start.size() != end.size()) || (start.size() != scheduleNames.size())) {
        SG_LOG(SG_GENERAL, SG_INFO, "Unable to parse schedule times");
        throw sg_exception("Unable to parse schedule times");
    } else {
        int nrItems = start.size();
        //cerr << "Nr of items to process: " << nrItems << endl;
        if (nrItems > 0) {
            for (unsigned int i = 0; i < start.size(); i++) {
                //cerr << i << endl;
                if ((dayStart >= start[i]) && (dayStart <= end[i]))
                    return scheduleNames[i];
            }
        }
        //couldn't find one so return 0;
        //cerr << "Returning 0 " << endl;
    }

    return ""s;
}

/******************************************************************************
 * RunwayList
 *****************************************************************************/

RunwayList::RunwayList(const RunwayList& other) : type{other.type}
{
    stringVecConstIterator i;
    for (i = other.preferredRunways.begin();
         i != other.preferredRunways.end(); ++i)
        preferredRunways.push_back(*i);
}

RunwayList& RunwayList::operator=(const RunwayList& other)
{
    type = other.type;
    preferredRunways.clear();
    stringVecConstIterator i;
    for (i = other.preferredRunways.begin();
         i != other.preferredRunways.end(); ++i)
        preferredRunways.push_back(*i);
    return *this;
}

void RunwayList::set(const std::string& tp, const std::string& lst)
{
    //weekday          = atoi(timeCopy.substr(0,1).c_str());
    //    timeOffsetInDays = weekday - currTimeDate->getGmt()->tm_wday;
    //    timeCopy = timeCopy.substr(2,timeCopy.length());
    type = tp;
    for (const auto& s : strutils::split(lst, ",")) {
        auto ident = strutils::strip(s);
        // http://code.google.com/p/flightgear-bugs/issues/detail?id=1137
        if ((ident.size() < 2) || !isdigit(ident[1])) {
            ident = "0" + ident;
        }

        preferredRunways.push_back(ident);
    }
}

void RunwayList::clear()
{
    type = "";
    preferredRunways.clear();
}

/****************************************************************************
 *
 ***************************************************************************/

RunwayGroup::RunwayGroup(const RunwayGroup& other) : name{other.name}
{
    RunwayListVecConstIterator i;
    for (i = other.rwyList.begin(); i != other.rwyList.end(); ++i)
        rwyList.push_back(*i);
    choice[0] = other.choice[0];
    choice[1] = other.choice[1];
    nrActive = other.nrActive;
}

RunwayGroup& RunwayGroup::operator=(const RunwayGroup& other)
{
    rwyList.clear();
    name = other.name;
    RunwayListVecConstIterator i;
    for (i = other.rwyList.begin(); i != other.rwyList.end(); ++i)
        rwyList.push_back(*i);
    active = other.active;
    choice[0] = other.choice[0];
    choice[1] = other.choice[1];
    nrActive = other.nrActive;

    return *this;
}

void RunwayGroup::setActive(const FGAirport* airport,
                            double windSpeed,
                            double windHeading,
                            double maxTail,
                            double maxCross, stringVec* currentlyActive)
{
    FGRunway* rwy;
    int activeRwys = rwyList.size(); // get the number of runways active
    int nrOfPreferences;
    // bool found = true;
    // double heading;
    double hdgDiff;
    double crossWind;
    double tailWind;
    //stringVec names;
    int bestMatch = 0, bestChoice = 0;

    if (activeRwys == 0) {
        active = -1;
        nrActive = 0;
        return;
    }

    // Now downward iterate across all the possible preferences
    // starting by the least preferred choice working toward the most preferred choice

    nrOfPreferences = rwyList[0].getPreferredRunways().size();
    bool foundValidSelection = false;
    for (int i = nrOfPreferences - 1; i >= 0; i--) {
        int match = 0;

        // Test each runway listed in the preference to see if it's possible to use
        // If one runway of the selection isn't allowed, we need to exclude this
        // preference, however, we don't want to stop right there, because we also
        // don't want to randomly swap runway preferences, unless there is a need to.
        //
        bool validSelection = true;

        for (int j = 0; j < activeRwys; j++) {
            const auto& currentRunwayPrefs = rwyList.at(j).getPreferredRunways();

            // if this rwyList has shorter preferences vec than the first one,
            // don't crash accessing an invalid index.
            // see https://sourceforge.net/p/flightgear/codetickets/2439/
            if ((int)currentRunwayPrefs.size() <= i) {
                validSelection = false;
                continue;
            }

            const auto& ident(currentRunwayPrefs.at(i));
            if (!airport->hasRunwayWithIdent(ident)) {
                SG_LOG(SG_GENERAL, SG_WARN,
                       "no such runway:" << ident << " at " << airport->ident());
                validSelection = false;
                continue;
            }

            rwy = airport->getRunwayByIdent(ident);

            //cerr << "Succes" << endl;
            hdgDiff = fabs(windHeading - rwy->headingDeg());
            std::string l_name = rwy->name();

            if (hdgDiff > 180)
                hdgDiff = 360 - hdgDiff;
            //cerr << "Heading diff: " << hdgDiff << endl;
            hdgDiff *= ((2 * M_PI) / 360.0); // convert to radians
            crossWind = windSpeed * sin(hdgDiff);
            tailWind = -windSpeed * cos(hdgDiff);
            //cerr << "Runway : " << rwy->name() << ": " << rwy->headingDeg() << endl;
            //cerr << ". Tailwind : " << tailWind;
            //cerr << ". Crosswnd : " << crossWind;
            if ((tailWind > maxTail) || (crossWind > maxCross)) {
                //cerr << ". [Invalid] " << endl;
                validSelection = false;
            } else {
                //cerr << ". [Valid] ";
            }
            //cerr << endl;
            for (stringVecIterator it = currentlyActive->begin();
                 it != currentlyActive->end(); ++it) {
                //cerr << "Checking : \"" << (*it) << "\". vs \"" << l_name << "\"" << endl;
                if ((*it) == l_name) {
                    match++;
                }
            }
        } // of active runways iteration

        if (validSelection) {
            //cerr << "Valid selection  : " << i << endl;;
            foundValidSelection = true;
            if (match >= bestMatch) {
                bestMatch = match;
                bestChoice = i;
            }
        }
        //cerr << "Preference " << i << "Match " << match << " bestMatch " << bestMatch << " choice " << bestChoice << " valid selection " << validSelection << endl;
    }

    if (foundValidSelection) {
        //cerr << "Valid runway selection : " << bestChoice << endl;
        nrActive = activeRwys;
        active = bestChoice;
        return;
    }

    // If this didn't work, due to heavy winds, try again
    // but select only one landing and one takeoff runway.
    choice[0] = 0;
    choice[1] = 0;
    for (int i = activeRwys - 1; i; i--) {
        if (rwyList[i].getType() == "landing"s)
            choice[0] = i;
        if (rwyList[i].getType() == "takeoff"s)
            choice[1] = i;
    }
    //cerr << "Choosing " << choice[0] << " for landing and " << choice[1] << "for takeoff" << endl;
    nrOfPreferences = rwyList[0].getPreferredRunways().size();
    for (int i = 0; i < nrOfPreferences; i++) {
        bool validSelection = true;
        for (int j = 0; j < 2; j++) {
            const auto& currentRunwayPrefs = rwyList.at(choice[j]).getPreferredRunways();
            if (i >= (int)currentRunwayPrefs.size()) {
                validSelection = false;
                continue;
            }

            const auto& l_name = currentRunwayPrefs.at(i);
            if (!airport->hasRunwayWithIdent(l_name)) {
                validSelection = false;
                continue;
            }

            rwy = airport->getRunwayByIdent(l_name);
            //cerr << "Success" << endl;
            hdgDiff = fabs(windHeading - rwy->headingDeg());
            if (hdgDiff > 180)
                hdgDiff = 360 - hdgDiff;
            hdgDiff *= ((2 * M_PI) / 360.0); // convert to radians
            crossWind = windSpeed * sin(hdgDiff);
            tailWind = -windSpeed * cos(hdgDiff);
            if ((tailWind > maxTail) || (crossWind > maxCross)) {
                validSelection = false;
            }
        }

        if (validSelection) {
            //cerr << "Valid runway selection : " << i << endl;
            active = i;
            nrActive = 2;
            return;
        }
    }

    // failed to find any valid runways
    active = -1;
    nrActive = 0;
}

void RunwayGroup::getActive(int i, std::string& name, std::string& type)
{
    if (i == -1) {
        return;
    }

    if (i >= nrActive) {
        SG_LOG(SG_AI, SG_DEV_ALERT, "RunwayGroup::getActive: invalid index " << i);
        return;
    }

    // nrActive is either the full size of the list *or* two, if we
    // fell back due to heavy winds
    const bool usingFullList = nrActive == (int)rwyList.size();
    if (usingFullList) {
        const auto& runways = rwyList.at(i).getPreferredRunways();
        name = runways.at(active);
        type = rwyList[i].getType();
    } else {
        const auto& runways = rwyList.at(choice[i]).getPreferredRunways();
        name = runways.at(active);
        type = rwyList[choice[i]].getType();
    }
}

/*****************************************************************************
 * FGRunway preference
 ****************************************************************************/
FGRunwayPreference::FGRunwayPreference(FGAirport* ap) : _ap{ap}
{
    //cerr << "Running default Constructor" << endl;
    initialized = false;
}

FGRunwayPreference::FGRunwayPreference(const FGRunwayPreference& other) : _ap{other._ap},
                                                                          comTimes{other.comTimes},
                                                                          genTimes{other.genTimes},
                                                                          milTimes{other.milTimes}
{
    initialized = other.initialized;

    PreferenceListConstIterator i;
    for (i = other.preferences.begin(); i != other.preferences.end(); ++i)
        preferences.push_back(*i);
}

FGRunwayPreference& FGRunwayPreference::operator=(const FGRunwayPreference& other)
{
    _ap = other._ap;

    initialized = other.initialized;

    comTimes = other.comTimes; // Commercial Traffic;
    genTimes = other.genTimes; // General Aviation;
    milTimes = other.milTimes; // Military Traffic;

    PreferenceListConstIterator i;
    preferences.clear();
    for (i = other.preferences.begin(); i != other.preferences.end(); ++i)
        preferences.push_back(*i);

    return *this;
}

ScheduleTime* FGRunwayPreference::getSchedule(const char* trafficType)
{
    if (!(strcmp(trafficType, "com"))) {
        return &comTimes;
    }
    if (!(strcmp(trafficType, "gen"))) {
        return &genTimes;
    }
    if (!(strcmp(trafficType, "mil"))) {
        return &milTimes;
    }
    return 0;
}

RunwayGroup* FGRunwayPreference::getGroup(const std::string& groupName)
{
    auto it = std::find_if(preferences.begin(), preferences.end(),
                           [&groupName](const RunwayGroup& g) {
                               return g.getName() == groupName;
                           });

    if (it == preferences.end())
        return nullptr;

    return &(*it); // not thrilled about this, but ok
}

std::string FGRunwayPreference::getId()
{
    return _ap->getId();
};

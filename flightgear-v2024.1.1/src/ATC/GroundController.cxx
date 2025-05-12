// GroundController.hxx - forked from groundnetwork.cxx

// Written by Durk Talsma, started June 2005.
//
// Copyright (C) 2004 Durk Talsma.
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
// $Id$

#include <config.h>

#include <cmath>
#include <algorithm>
#include <fstream>
#include <map>
#include <algorithm>

#include <osg/Geode>
#include <osg/Geometry>
#include <osg/MatrixTransform>
#include <osg/Shape>

#include <simgear/debug/logstream.hxx>
#include <simgear/scene/material/EffectGeode.hxx>
#include <simgear/scene/material/matlib.hxx>
#include <simgear/scene/material/mat.hxx>
#include <simgear/scene/util/OsgMath.hxx>
#include <simgear/structure/exception.hxx>
#include <simgear/timing/timestamp.hxx>
#include <simgear/timing/sg_time.hxx>

#include <Airports/airport.hxx>
#include <Airports/dynamics.hxx>
#include <Airports/runways.hxx>
#include <Airports/groundnetwork.hxx>

#include <Main/globals.hxx>
#include <Main/fg_props.hxx>
#include <AIModel/AIAircraft.hxx>
#include <AIModel/performancedata.hxx>
#include <AIModel/AIFlightPlan.hxx>
#include <Navaids/NavDataCache.hxx>

#include <ATC/atc_mgr.hxx>

#include <Scenery/scenery.hxx>

#include <ATC/ATCController.hxx>
#include <ATC/GroundController.hxx>

using std::string;


/***************************************************************************
 * FGGroundController()
 **************************************************************************/
FGGroundController::FGGroundController(FGAirportDynamics *par)
{
    hasNetwork = false;
    count = 0;
    group = 0;
    version = 0;
    networkInitialized = false;
    FGATCController::init();

    parent = par;
    hasNetwork = true;
    networkInitialized = true;

}

FGGroundController::~FGGroundController()
{
}

bool compare_trafficrecords(FGTrafficRecord a, FGTrafficRecord b)
{
    return (a.getIntentions().size() < b.getIntentions().size());
}

void FGGroundController::announcePosition(int id,
                                       FGAIFlightPlan * intendedRoute,
                                       int currentPosition, double lat,
                                       double lon, double heading,
                                       double speed, double alt,
                                       double radius, int leg,
                                       FGAIAircraft * aircraft)
{
    if (!aircraft || !aircraft->getPerformance()) {
        SG_LOG(SG_ATC, SG_ALERT, "announcePosition: missing aircraft performance");
        return;
    }

    // Search the activeTraffic vector to find a traffic vector with our id
    TrafficVectorIterator i = FGATCController::searchActiveTraffic(id);

    // Add a new TrafficRecord if none exists for this aircraft
    // otherwise set the information for the TrafficRecord
    if (i == activeTraffic.end() || (activeTraffic.empty())) {
        FGTrafficRecord rec;
        rec.setId(id);
        rec.setLeg(leg);
        rec.setPositionAndIntentions(currentPosition, intendedRoute);
        rec.setPositionAndHeading(lat, lon, heading, speed, alt);
        rec.setRadius(radius);  // only need to do this when creating the record.
        rec.setCallsign(aircraft->getCallSign());
        rec.setAircraft(aircraft);
        // add to the front of the list of activeTraffic if the aircraft is already taxiing
        if (leg == 2) {
            activeTraffic.push_front(rec);
        } else {
            activeTraffic.push_back(rec);
        }
    } else {
        i->setPositionAndIntentions(currentPosition, intendedRoute);
        i->setPositionAndHeading(lat, lon, heading, speed, alt);
    }
}

/**
* The ground network can deal with the following states:
* 0 =  Normal; no action required
* 1 = "Acknowledge "Hold position
* 2 = "Acknowledge "Resume taxi".
* 3 = "Issue TaxiClearance"
* 4 = Acknowledge Taxi Clearance"
* 5 = Post acknowlegde taxiclearance: Start taxiing
* 6 = Report runway
* 7 = Acknowledge report runway
* 8 = Switch tower frequency
* 9 = Acknowledge switch tower frequency
*/

void FGGroundController::updateAircraftInformation(int id, SGGeod geod,
        double heading, double speed, double alt,
        double dt)
{
    // Check whether aircraft are on hold due to a preceding pushback. If so, make sure to
    // Transmit air-to-ground "Ready to taxi request:
    // Transmit ground to air approval / hold
    // Transmit confirmation ...
    // Probably use a status mechanism similar to the Engine start procedure in the startup controller.

    // Search the activeTraffic vector to find a traffic vector with our id
    TrafficVectorIterator i = FGATCController::searchActiveTraffic(id);

    // update position of the current aircraft
    if (i == activeTraffic.end() || activeTraffic.empty()) {
        SG_LOG(SG_GENERAL, SG_DEV_WARN,
               "AI error: updating aircraft without traffic record at " << SG_ORIGIN << ", id=" << id);
        return;
    }

    i->setPositionAndHeading(geod.getLatitudeDeg(), geod.getLongitudeDeg(), heading, speed, alt);
    TrafficVectorIterator current = i;

    setDt(getDt() + dt);

    // Update every three secs, but add some randomness
    // to prevent all IA objects doing this in synchrony
    //if (getDt() < (3.0) + (rand() % 10))
    //  return;
    //else
    //  setDt(0);
    current->clearResolveCircularWait();
    current->setWaitsForId(0);
    checkSpeedAdjustment(id, geod.getLatitudeDeg(), geod.getLongitudeDeg(), heading, speed, alt);
    bool needsTaxiClearance = current->getAircraft()->getTaxiClearanceRequest();
    if (!needsTaxiClearance) {
        checkHoldPosition(id, geod.getLatitudeDeg(), geod.getLongitudeDeg(), heading, speed, alt);
        //if (checkForCircularWaits(id)) {
        //    i->setResolveCircularWait();
        //}
    } else {
        current->setHoldPosition(true);
        int state = current->getState();
        time_t now = globals->get_time_params()->get_cur_time();

        if ((now - lastTransmission) > 15) {
            available = true;
        }
        if (checkTransmissionState(ATCMessageState::NORMAL,ATCMessageState::ACK_RESUME_TAXI, current, now, MSG_REQUEST_TAXI_CLEARANCE, ATC_AIR_TO_GROUND)) {
            current->setState(ATCMessageState::TAXI_CLEARED);
        }
        if (checkTransmissionState(ATCMessageState::TAXI_CLEARED,ATCMessageState::TAXI_CLEARED, current, now, MSG_ISSUE_TAXI_CLEARANCE, ATC_GROUND_TO_AIR)) {
            current->setState(ATCMessageState::ACK_TAXI_CLEARED);
        }
        if (checkTransmissionState(ATCMessageState::ACK_TAXI_CLEARED,ATCMessageState::ACK_TAXI_CLEARED, current, now, MSG_ACKNOWLEDGE_TAXI_CLEARANCE, ATC_AIR_TO_GROUND)) {
            current->setState(ATCMessageState::START_TAXI);
        }
        if ((state == ATCMessageState::START_TAXI) && available) {
            current->setState(ATCMessageState::NORMAL);
            current->getAircraft()->setTaxiClearanceRequest(false);
            current->setHoldPosition(false);
            available = false;
        }
    }
}

/**
* Scan for a speed adjustment change. Find the nearest aircraft that is in front
* and adjust speed when we get too close. Only do this when current position and/or
* intentions of the current aircraft match current taxiroute position of the proximate
* aircraft. For traffic that is on other routes we need to issue a "HOLD Position"
* instruction. See below for the hold position instruction.

* Note that there currently still is one flaw in the logic that needs to be addressed.
* There can be situations where one aircraft is in front of the current aircraft, on a separate
* route, but really close after an intersection coming off the current route. This
* aircraft is still close enough to block the current aircraft. This situation is currently
* not addressed yet, but should be.
*/

void FGGroundController::checkSpeedAdjustment(int id, double lat,
        double lon, double heading,
        double speed, double alt)
{

    TrafficVectorIterator current, closest, closestOnNetwork;
    // bool previousInstruction;
	TrafficVectorIterator i = FGATCController::searchActiveTraffic(id);
    if (!activeTraffic.size()) {
        return;
	}
    if (i == activeTraffic.end() || (activeTraffic.size() == 0)) {
        SG_LOG(SG_GENERAL, SG_ALERT,
               "AI error: Trying to access non-existing aircraft in FGGroundNetwork::checkSpeedAdjustment at " << SG_ORIGIN);
    }
    current = i;
    //closest = current;

    // previousInstruction = current->getSpeedAdjustment();
    double mindist = HUGE_VAL;

    // First check all our activeTraffic
    if (activeTraffic.size()) {
        bool otherReasonToSlowDown = false;
        double course, dist, bearing, az2; // minbearing,
        SGGeod curr(SGGeod::fromDegM(lon, lat, alt));
        closest = current;
        closestOnNetwork = current;

        for (TrafficVectorIterator iter = activeTraffic.begin();
                iter != activeTraffic.end(); ++iter) {
            if (iter == current) {
                continue;
            }

            SGGeod other = iter->getPos();
            SGGeodesy::inverse(curr, other, course, az2, dist);
            bearing = fabs(heading - course);
            if (bearing > 180)
                bearing = 360 - bearing;
            if ((dist < mindist) && (bearing < 60.0)) {
                mindist = dist;
                closest = iter;
                closestOnNetwork = iter;
                // minbearing = bearing;
            }
        }

        // Next check with the tower controller
        if (towerController->hasActiveTraffic()) {
            for (TrafficVectorIterator iter =
                        towerController->getActiveTraffic().begin();
                    iter != towerController->getActiveTraffic().end(); ++iter) {
                if( current->getId() == iter->getId()) {
                    continue;
                }
                SG_LOG(SG_ATC, SG_BULK, current->getCallsign() << "| Comparing with " << iter->getCallsign() << " Id: " << iter->getId());
                SGGeod other = iter->getPos();
                SGGeodesy::inverse(curr, other, course, az2, dist);
                bearing = fabs(heading - course);
                if (bearing > 180)
                    bearing = 360 - bearing;
                if ((dist < mindist) && (bearing < 60.0)) {
                    //SG_LOG(SG_ATC, SG_BULK, "Current aircraft " << current->getAircraft()->getTrafficRef()->getCallSign()
                    //   << " is closest to " << iter->getAircraft()->getTrafficRef()->getCallSign()
                    //   << ", which has status " << iter->getAircraft()->isScheduledForTakeoff());
                    mindist = dist;
                    closest = iter;
                    // minbearing = bearing;
                    otherReasonToSlowDown = true;
                }
            }
        }

        // Finally, check UserPosition
        // Note, as of 2011-08-01, this should no longer be necessary.
        /*
        double userLatitude = fgGetDouble("/position/latitude-deg");
        double userLongitude = fgGetDouble("/position/longitude-deg");
        SGGeod user(SGGeod::fromDeg(userLongitude, userLatitude));
        SGGeodesy::inverse(curr, user, course, az2, dist);

        bearing = fabs(heading - course);
        if (bearing > 180)
            bearing = 360 - bearing;
        if ((dist < mindist) && (bearing < 60.0)) {
            mindist = dist;
            //closest = i;
            minbearing = bearing;
            otherReasonToSlowDown = true;
        }
        */

        // Clear any active speed adjustment, check if the aircraft needs to brake
        current->clearSpeedAdjustment();
        bool needBraking = false;

        if (current->checkPositionAndIntentions(*closest)
                || otherReasonToSlowDown) {
            double maxAllowableDistance =
                (1.1 * current->getRadius()) +
                (1.1 * closest->getRadius());
            if (mindist < 2 * maxAllowableDistance) {
                if (current->getId() == closest->getWaitsForId())
                    return;
                else
                    current->setWaitsForId(closest->getId());

                if (closest->getId() != current->getId()) {
                    current->setSpeedAdjustment(closest->getSpeed() *
                                                (mindist / 100));
                    needBraking = true;

//                     if (
//                         closest->getAircraft()->getTakeOffStatus() &&
//                         (current->getAircraft()->getTrafficRef()->getDepartureAirport() ==  closest->getAircraft()->getTrafficRef()->getDepartureAirport()) &&
//                         (current->getAircraft()->GetFlightPlan()->getRunway() == closest->getAircraft()->GetFlightPlan()->getRunway())
//                     )
//                         current->getAircraft()->scheduleForATCTowerDepartureControl(1);
                } else {
                    current->setSpeedAdjustment(0);     // This can only happen when the user aircraft is the one closest
                }

                if (mindist < maxAllowableDistance) {
                    //double newSpeed = (maxAllowableDistance-mindist);
                    //current->setSpeedAdjustment(newSpeed);
                    //if (mindist < 0.5* maxAllowableDistance)
                    //  {
                    current->setSpeedAdjustment(0);
                    //  }
                }
            }
        }

        if ((closest->getId() == closestOnNetwork->getId()) && (current->getPriority() < closest->getPriority()) && needBraking) {
            swap(current, closest);
        }
    }
}

/**
* Check for "Hold position instruction".
* The hold position should be issued under the following conditions:
* 1) For aircraft entering or crossing a runway with active traffic on it, or landing aircraft near it
* 2) For taxiing aircraft that use one taxiway in opposite directions
* 3) For crossing or merging taxiroutes.
*/

void FGGroundController::checkHoldPosition(int id, double lat,
                                        double lon, double heading,
                                        double speed, double alt)
{
    FGGroundNetwork* network = parent->parent()->groundNetwork();
    TrafficVectorIterator current;
    TrafficVectorIterator i = activeTraffic.begin();
    if (activeTraffic.size()) {
        //while ((i->getId() != id) && i != activeTraffic.end())
        while (i != activeTraffic.end()) {
            if (i->getId() == id) {
                break;
            }
            i++;
        }
    } else {
        return;
    }

    time_t now = globals->get_time_params()->get_cur_time();
    if (i == activeTraffic.end() || (activeTraffic.size() == 0)) {
        SG_LOG(SG_GENERAL, SG_ALERT,
               "AI error: Trying to access non-existing aircraft in FGGroundNetwork::checkHoldPosition at " << SG_ORIGIN);
    }
    current = i;
    if (current->getAircraft()->getTakeOffStatus() == AITakeOffStatus::QUEUED) {
        current->setHoldPosition(true);
        return;
    }
    if ((now - lastTransmission) > 15) {
        available = true;
    }

    if (current->getAircraft()->getTakeOffStatus() == AITakeOffStatus::CLEARED_FOR_TAKEOFF) {
        current->setHoldPosition(false);
        current->clearSpeedAdjustment();
        return;
    }
    bool origStatus = current->hasHoldPosition();
    current->setHoldPosition(false);
    //SGGeod curr(SGGeod::fromDegM(lon, lat, alt));
    int currentRoute = i->getCurrentPosition();
    int nextRoute;
    if (i->getIntentions().size()) {
        nextRoute    = (*(i->getIntentions().begin()));
    } else {
        nextRoute = 0;
    }
    if (currentRoute > 0) {
        FGTaxiSegment *tx = network->findSegment(currentRoute);
        FGTaxiSegment *nx;
        if (nextRoute) {
            nx = network->findSegment(nextRoute);
        } else {
            nx = tx;
        }
        //if (tx->hasBlock(now) || nx->hasBlock(now) ) {
        //   current->setHoldPosition(true);
        //}
        SGGeod start = i->getPos();
        SGGeod end  (nx->getStart()->geod());

        double distance = SGGeodesy::distanceM(start, end);
        if (nx->hasBlock(now) && (distance < i->getRadius() * 4)) {
            current->setHoldPosition(true);
        } else {
            intVecIterator ivi = i->getIntentions().begin();
            while (ivi != i->getIntentions().end()) {
                if ((*ivi) > 0) {
                    FGTaxiSegment* seg = network->findSegment(*ivi);
                    distance += seg->getLength();
                    if ((seg->hasBlock(now)) && (distance < i->getRadius() * 4)) {
                        current->setHoldPosition(true);
                        break;
                    }
                }
                ivi++;
            }
        }
    }
    bool currStatus = current->hasHoldPosition();
    current->setHoldPosition(origStatus);
    // Either a Hold Position or a resume taxi transmission has been issued
    if ((now - lastTransmission) > 2) {
        available = true;
    }
    if (current->getState() == ATCMessageState::NORMAL) {
        if ((origStatus != currStatus) && available) {
            SG_LOG(SG_ATC, SG_DEBUG, "Issuing hold short instruction " << currStatus << " " << available);
            if (currStatus == true) { // No has a hold short instruction
                transmit(&(*current), parent, MSG_HOLD_POSITION, ATC_GROUND_TO_AIR, true);
                SG_LOG(SG_ATC, SG_DEBUG, "Transmitting hold short instruction " << currStatus << " " << available);
                current->setState(ATCMessageState::ACK_HOLD);
            } else {
                transmit(&(*current), parent, MSG_RESUME_TAXI, ATC_GROUND_TO_AIR, true);
                SG_LOG(SG_ATC, SG_DEBUG, "Transmitting resume instruction " << currStatus << " " << available);
                current->setState(ATCMessageState::ACK_RESUME_TAXI);
            }
            lastTransmission = now;
            available = false;
            // Don't act on the changed instruction until the transmission is confirmed
            // So set back to original status
            SG_LOG(SG_ATC, SG_DEBUG, "Current state " << current->getState());
        }

    }
    // 6 = Report runway
    // 7 = Acknowledge report runway
    // 8 = Switch tower frequency
    //9 = Acknowledge switch tower frequency

    //int state = current->getState();
    if (checkTransmissionState(ATCMessageState::ACK_HOLD, ATCMessageState::ACK_HOLD, current, now, MSG_ACKNOWLEDGE_HOLD_POSITION, ATC_AIR_TO_GROUND)) {
        current->setState(ATCMessageState::NORMAL);
        current->setHoldPosition(true);
    }
    if (checkTransmissionState(ATCMessageState::ACK_RESUME_TAXI, ATCMessageState::ACK_RESUME_TAXI, current, now, MSG_ACKNOWLEDGE_RESUME_TAXI, ATC_AIR_TO_GROUND)) {
        current->setState(ATCMessageState::NORMAL);
        current->setHoldPosition(false);
    }
    if (current->getAircraft()->getTakeOffStatus() && (current->getState() == 0)) {
        SG_LOG(SG_ATC, SG_DEBUG, "Scheduling " << current->getAircraft()->getCallSign() << " for hold short");
        current->setState(ATCMessageState::REPORT_RUNWAY);
    }
    if (checkTransmissionState(ATCMessageState::REPORT_RUNWAY ,ATCMessageState::REPORT_RUNWAY , current, now, MSG_REPORT_RUNWAY_HOLD_SHORT, ATC_AIR_TO_GROUND)) {
    }
    if (checkTransmissionState(ATCMessageState::ACK_REPORT_RUNWAY, ATCMessageState::ACK_REPORT_RUNWAY, current, now, MSG_ACKNOWLEDGE_REPORT_RUNWAY_HOLD_SHORT, ATC_GROUND_TO_AIR)) {
    }
    if (checkTransmissionState(ATCMessageState::SWITCH_GROUND_TOWER, ATCMessageState::SWITCH_GROUND_TOWER, current, now, MSG_SWITCH_TOWER_FREQUENCY, ATC_GROUND_TO_AIR)) {
    }
    if (checkTransmissionState(ATCMessageState::ACK_SWITCH_GROUND_TOWER, ATCMessageState::ACK_SWITCH_GROUND_TOWER, current, now, MSG_ACKNOWLEDGE_SWITCH_TOWER_FREQUENCY, ATC_AIR_TO_GROUND)) {
    }

    //current->setState(0);
}

/*
* Check whether situations occur where the current aircraft is waiting for itself
* due to higher order interactions.
* A 'circular' wait is a situation where a waits for b, b waits for c, and c waits
* for a. Ideally each aircraft only waits for one other aircraft, so by tracing
* through this list of waiting aircraft, we can check if we'd eventually end back
* at the current aircraft.
*
* Note that we should consider the situation where we are actually checking aircraft
* d, which is waiting for aircraft a. d is not part of the loop, but is held back by
* the looping aircraft. If we don't check for that, this function will get stuck into
* endless loop.
*/

bool FGGroundController::checkForCircularWaits(int id)
{
    SG_LOG(SG_ATC, SG_DEBUG, "Performing circular check for " << id);
    int target = 0;
    TrafficVectorIterator current, other;
    TrafficVectorIterator i = activeTraffic.begin();
    int trafficSize = activeTraffic.size();
    if (trafficSize) {
        while (i != activeTraffic.end()) {
            if (i->getId() == id) {
                break;
            }
            i++;
        }
    } else {
        return false;
    }

    if (i == activeTraffic.end()) {
        SG_LOG(SG_GENERAL, SG_ALERT,
               "AI error: Trying to access non-existing aircraft in FGGroundNetwork::checkForCircularWaits at " << SG_ORIGIN);
    }

    current = i;
    target = current->getWaitsForId();
    //bool printed = false; // Note that this variable is for debugging purposes only.
    int counter = 0;

    if (id == target) {
        SG_LOG(SG_ATC, SG_DEBUG, "aircraft is waiting for user");
        return false;
    }

    while ((target > 0) && (target != id) && counter++ < trafficSize) {
        //printed = true;
        TrafficVectorIterator iter = activeTraffic.begin();
        if (trafficSize) {
            while (iter != activeTraffic.end()) {
                if (iter->getId() == target) {
                    break;
                }
                ++iter;
            }
        } else {
            return false;
        }

        if (iter == activeTraffic.end()) {
            SG_LOG(SG_ATC, SG_DEBUG, "[Waiting for traffic at Runway: DONE] ");
            // The target id is not found on the current network, which means it's at the tower
            SG_LOG(SG_ATC, SG_ALERT, "AI error: Trying to access non-existing aircraft in FGGroundNetwork::checkForCircularWaits");
            return false;
        }

        other = iter;
        target = other->getWaitsForId();

        // actually this trap isn't as impossible as it first seemed:
        // the setWaitsForID(id) is set to current when the aircraft
        // is waiting for the user controlled aircraft.
        if (current->getId() == other->getId())
            return false;
    }

    //if (printed)
        SG_LOG(SG_ATC, SG_DEBUG, "[done] ");
    if (id == target) {
        SG_LOG(SG_GENERAL, SG_WARN,
               "Detected circular wait condition: Id = " << id <<
               "target = " << target);
        return true;
    } else {
        return false;
    }
}

// Note that this function is copied from simgear. for maintanance purposes, it's probabtl better to make a general function out of that.
static void WorldCoordinate(osg::Matrix& obj_pos, double lat,
                            double lon, double elev, double hdg, double slope)
{
    SGGeod geod = SGGeod::fromDegM(lon, lat, elev);
    obj_pos = makeZUpFrame(geod);
    // hdg is not a compass heading, but a counter-clockwise rotation
    // around the Z axis
    obj_pos.preMult(osg::Matrix::rotate(hdg * SGD_DEGREES_TO_RADIANS,
                                        0.0, 0.0, 1.0));
    obj_pos.preMult(osg::Matrix::rotate(slope * SGD_DEGREES_TO_RADIANS,
                                        0.0, 1.0, 0.0));
}

/** Draw visible taxi routes */
void FGGroundController::render(bool visible)
{
    SGMaterialLib *matlib = globals->get_matlib();
    FGGroundNetwork* network = parent->parent()->groundNetwork();

    if (group) {
        //int nr = ;
        globals->get_scenery()->get_scene_graph()->removeChild(group);
        //while (group->getNumChildren()) {
        //   SG_LOG(SG_ATC, SG_DEBUG, "Number of children: " << group->getNumChildren());
        //simgear::EffectGeode* geode = (simgear::EffectGeode*) group->getChild(0);
        //osg::MatrixTransform *obj_trans = (osg::MatrixTransform*) group->getChild(0);
        //geode->releaseGLObjects();
        //group->removeChild(geode);
        //delete geode;
        group = 0;
    }
    if (visible) {
        group = new osg::Group;
        FGScenery * local_scenery = globals->get_scenery();
        // double elevation_meters = 0.0;
//        double elevation_feet = 0.0;
        time_t now = globals->get_time_params()->get_cur_time();

        //for ( FGTaxiSegmentVectorIterator i = segments.begin(); i != segments.end(); i++) {
        //double dx = 0;

        for   (TrafficVectorIterator i = activeTraffic.begin(); i != activeTraffic.end(); i++) {
            // Handle start point i.e. the segment that is connected to the aircraft itself on the starting end
            // and to the the first "real" taxi segment on the other end.
            const int pos = i->getCurrentPosition();
            if (pos > 0) {
                FGTaxiSegment* segment = network->findSegment(pos);
                SGGeod start = i->getPos();
                SGGeod end  (segment->getEnd()->geod());

                double length = SGGeodesy::distanceM(start, end);
                //heading = SGGeodesy::headingDeg(start->geod(), end->geod());

                double az2, heading; //, distanceM;
                SGGeodesy::inverse(start, end, heading, az2, length);
                double coveredDistance = length * 0.5;
                SGGeod center;
                SGGeodesy::direct(start, heading, coveredDistance, center, az2);
                SG_LOG(SG_ATC, SG_BULK, "Active Aircraft : Centerpoint = (" << center.getLatitudeDeg() << ", " << center.getLongitudeDeg() << "). Heading = " << heading);
                ///////////////////////////////////////////////////////////////////////////////
                // Make a helper function out of this
                osg::Matrix obj_pos;
                osg::MatrixTransform *obj_trans = new osg::MatrixTransform;
                obj_trans->setDataVariance(osg::Object::STATIC);
                // Experimental: Calculate slope here, based on length, and the individual elevations
                double elevationStart;
                if (isUserAircraft((i)->getAircraft())) {
                    elevationStart = fgGetDouble("/position/ground-elev-m");
                } else {
                    elevationStart = ((i)->getAircraft()->_getAltitude());
                }
                double elevationEnd   = segment->getEnd()->getElevationM();
                SG_LOG(SG_ATC, SG_DEBUG, "Using elevation " << elevationEnd);

                if ((elevationEnd == 0) || (elevationEnd = parent->getElevation())) {
                    SGGeod center2 = end;
                    center2.setElevationM(SG_MAX_ELEVATION_M);
                    if (local_scenery->get_elevation_m( center2, elevationEnd, NULL )) {
//                        elevation_feet = elevationEnd * SG_METER_TO_FEET + 0.5;
                        //elevation_meters += 0.5;
                    }
                    else {
                        elevationEnd = parent->getElevation();
                    }
                    segment->getEnd()->setElevation(elevationEnd);
                }
                double elevationMean  = (elevationStart + elevationEnd) / 2.0;
                double elevDiff       = elevationEnd - elevationStart;

                double slope = atan2(elevDiff, length) * SGD_RADIANS_TO_DEGREES;

                SG_LOG(SG_ATC, SG_DEBUG, "1. Using mean elevation : " << elevationMean << " and " << slope);

                WorldCoordinate( obj_pos, center.getLatitudeDeg(), center.getLongitudeDeg(), elevationMean+ 0.5, -(heading), slope );

                obj_trans->setMatrix( obj_pos );
                //osg::Vec3 center(0, 0, 0)

                float width = length /2.0;
                osg::Vec3 corner(-width, 0, 0.25f);
                osg::Vec3 widthVec(2*width + 1, 0, 0);
                osg::Vec3 heightVec(0, 1, 0);
                osg::Geometry* geometry;
                geometry = osg::createTexturedQuadGeometry(corner, widthVec, heightVec);
                simgear::EffectGeode* geode = new simgear::EffectGeode;
                geode->setName("test");
                geode->addDrawable(geometry);
                //osg::Node *custom_obj;
                SGMaterial *mat;
                if (segment->hasBlock(now)) {
                    mat = matlib->find("UnidirectionalTaperRed", center);
                } else {
                    mat = matlib->find("UnidirectionalTaperGreen", center);
                }
                if (mat)
                    geode->setEffect(mat->get_effect());
                obj_trans->addChild(geode);
                // wire as much of the scene graph together as we can
                //->addChild( obj_trans );
                group->addChild( obj_trans );
                /////////////////////////////////////////////////////////////////////
            } else {
                SG_LOG(SG_ATC, SG_INFO, "BIG FAT WARNING: current position is here : " << pos);
            }
            // Next: Draw the other taxi segments.
            for (intVecIterator j = (i)->getIntentions().begin(); j != (i)->getIntentions().end(); j++) {
                osg::Matrix obj_pos;
                const int k = (*j);
                if (k > 0) {
                    osg::MatrixTransform *obj_trans = new osg::MatrixTransform;
                    obj_trans->setDataVariance(osg::Object::STATIC);
                    FGTaxiSegment* segmentK = network->findSegment(k);
                    // Experimental: Calculate slope here, based on length, and the individual elevations
                    double elevationStart = segmentK->getStart()->getElevationM();
                    double elevationEnd   = segmentK->getEnd  ()->getElevationM();
                    if ((elevationStart == 0)  || (elevationStart == parent->getElevation())) {
                        SGGeod center2 = segmentK->getStart()->geod();
                        center2.setElevationM(SG_MAX_ELEVATION_M);
                        if (local_scenery->get_elevation_m( center2, elevationStart, NULL )) {
//                            elevation_feet = elevationStart * SG_METER_TO_FEET + 0.5;
                            //elevation_meters += 0.5;
                        }
                        else {
                            elevationStart = parent->getElevation();
                        }
                        segmentK->getStart()->setElevation(elevationStart);
                    }
                    if ((elevationEnd == 0) || (elevationEnd == parent->getElevation())) {
                        SGGeod center2 = segmentK->getEnd()->geod();
                        center2.setElevationM(SG_MAX_ELEVATION_M);
                        if (local_scenery->get_elevation_m( center2, elevationEnd, NULL )) {
//                            elevation_feet = elevationEnd * SG_METER_TO_FEET + 0.5;
                            //elevation_meters += 0.5;
                        }
                        else {
                            elevationEnd = parent->getElevation();
                        }
                        segmentK->getEnd()->setElevation(elevationEnd);
                    }

                    double elevationMean  = (elevationStart + elevationEnd) / 2.0;
                    double elevDiff       = elevationEnd - elevationStart;
                    double length         = segmentK->getLength();
                    double slope = atan2(elevDiff, length) * SGD_RADIANS_TO_DEGREES;

                    SG_LOG(SG_ATC, SG_DEBUG, "2. Using mean elevation : " << elevationMean << " and " << slope);

                    SGGeod segCenter = segmentK->getCenter();
                    WorldCoordinate( obj_pos, segCenter.getLatitudeDeg(), segCenter.getLongitudeDeg(),
                                     elevationMean+ 0.5, -(segmentK->getHeading()), slope );

                    obj_trans->setMatrix( obj_pos );
                    //osg::Vec3 center(0, 0, 0)

                    float width = segmentK->getLength() /2.0;
                    osg::Vec3 corner(-width, 0, 0.25f);
                    osg::Vec3 widthVec(2*width + 1, 0, 0);
                    osg::Vec3 heightVec(0, 1, 0);
                    osg::Geometry* geometry;
                    geometry = osg::createTexturedQuadGeometry(corner, widthVec, heightVec);
                    simgear::EffectGeode* geode = new simgear::EffectGeode;
                    geode->setName("test");
                    geode->addDrawable(geometry);
                    //osg::Node *custom_obj;
                    SGMaterial *mat;
                    if (segmentK->hasBlock(now)) {
                        mat = matlib->find("UnidirectionalTaperRed", segCenter);
                    } else {
                        mat = matlib->find("UnidirectionalTaperGreen", segCenter);
                    }
                    if (mat)
                        geode->setEffect(mat->get_effect());
                    obj_trans->addChild(geode);
                    // wire as much of the scene graph together as we can
                    //->addChild( obj_trans );
                    group->addChild( obj_trans );
                }
            }
            //dx += 0.1;
        }
        globals->get_scenery()->get_scene_graph()->addChild(group);
    }
}

string FGGroundController::getName() {
    return string(parent->parent()->getName() + "-ground");
}

void FGGroundController::update(double dt)
{
    time_t now = globals->get_time_params()->get_cur_time();
    FGGroundNetwork* network = parent->parent()->groundNetwork();
    network->unblockAllSegments(now);
    int priority = 1;

    TrafficVector& startupTraffic(parent->getStartupController()->getActiveTraffic());
    TrafficVectorIterator i;

    //sort(activeTraffic.begin(), activeTraffic.end(), compare_trafficrecords);
    // Handle traffic that is under ground control first; this way we'll prevent clutter at the gate areas.
    // Don't allow an aircraft to pushback when a taxiing aircraft is currently using part of the intended route.
    for (i = startupTraffic.begin(); i != startupTraffic.end(); ++i) {
        updateStartupTraffic(i, priority, now);
    }

    for (i = activeTraffic.begin(); i != activeTraffic.end(); i++) {
        updateActiveTraffic(i, priority, now);
    }

    //FIXME
    //FGATCController::eraseDeadTraffic(startupTraffic);
    FGATCController::eraseDeadTraffic();
}

void FGGroundController::updateStartupTraffic(TrafficVectorIterator i,
                                              int& priority,
                                              time_t now)
{
    if (!i->getAircraft()) {
        SG_LOG(SG_ATC, SG_ALERT, "updateStartupTraffic: missing aircraft");
        return;
    }

    if (!i->getAircraft()->getPerformance()) {
        SG_LOG(SG_ATC, SG_ALERT, "updateStartupTraffic: missing aircraft performance");
        return;
    }

    i->allowPushBack();
    i->setPriority(priority++);
    // in meters per second;
    double vTaxi = (i->getAircraft()->getPerformance()->vTaxi() * SG_NM_TO_METER) / 3600;
    if (!i->isActive(0)) {
        return;
    }

    FGGroundNetwork* network = parent->parent()->groundNetwork();

    if (!network) {
        SG_LOG(SG_ATC, SG_ALERT, "updateStartupTraffic: missing ground network");
        return;
    }

    // Check for all active aircraft whether it's current pos segment is
    // an opposite of one of the departing aircraft's intentions
    for (TrafficVectorIterator j = activeTraffic.begin(); j != activeTraffic.end(); j++) {
        int pos = j->getCurrentPosition();
        if (pos > 0) {
            FGTaxiSegment *seg = network->findOppositeSegment(pos-1);
            if (seg) {
                int posReverse = seg->getIndex();
                for (intVecIterator k = i->getIntentions().begin(); k != i->getIntentions().end(); k++) {
                    if ((*k) == posReverse) {
                        i->denyPushBack();
                        network->findSegment(posReverse)->block(i->getId(), now, now);
                    }
                }
            }
        }
    }
    // if the current aircraft is still allowed to pushback, we can start reserving a route for if by blocking all the entry taxiways.
    if (!i->pushBackAllowed()) {
        return;
    }

    double length = 0;
    int pos = i->getCurrentPosition();
    if (pos > 0) {
        FGTaxiSegment *seg = network->findSegment(pos);
        length = seg->getLength();
        network->blockSegmentsEndingAt(seg, i->getId(), now, now);
    }

    for (intVecIterator j = i->getIntentions().begin(); j != i->getIntentions().end(); j++) {
        pos = (*j);
        if (pos > 0) {
            FGTaxiSegment *seg = network->findSegment(pos);
            length += seg->getLength();
            time_t blockTime = now + (length / vTaxi);
            network->blockSegmentsEndingAt(seg, i->getId(), blockTime - 30, now);
        }
    }
}

bool FGGroundController::updateActiveTraffic(TrafficVectorIterator i,
                                             int& priority,
                                             time_t now)
{
    if (!i->getAircraft()) {
        SG_LOG(SG_ATC, SG_ALERT, "updateActiveTraffic: missing aircraft");
        return false;
    }

    if (i->getAircraft()->getDie()) {
        // aircraft has died
        return false;
    }

    if (!i->getAircraft()->getPerformance()) {
        SG_LOG(SG_ATC, SG_ALERT, "updateActiveTraffic: missing aircraft performance");
        return false;
    }

    double length = 0;
    double vTaxi = (i->getAircraft()->getPerformance()->vTaxi() * SG_NM_TO_METER) / 3600;
    FGGroundNetwork* network = parent->parent()->groundNetwork();

    if (!network) {
        SG_LOG(SG_ATC, SG_ALERT, "updateActiveTraffic: missing ground network");
        return false;
    }

    i->setPriority(priority++);
    int pos = i->getCurrentPosition();
    if (pos > 0) {
        FGTaxiSegment* segment = network->findSegment(pos);
        length = segment->getLength();
        if (segment->hasBlock(now)) {
            SG_LOG(SG_ATC, SG_BULK, "Taxiway incursion for AI aircraft" << i->getAircraft()->getCallSign());
        }

    }

    intVecIterator ivi;
    for (ivi = i->getIntentions().begin(); ivi != i->getIntentions().end(); ivi++) {
        int segIndex = (*ivi);
        if (segIndex > 0) {
            FGTaxiSegment* seg = network->findSegment(segIndex);
            if (seg->hasBlock(now)) {
                break;
            }
        }
    }

    //after this, ivi points just behind the last valid unblocked taxi segment.
    for (intVecIterator j = i->getIntentions().begin(); j != ivi; j++) {
        pos = (*j);
        if (pos > 0) {
            FGTaxiSegment *seg = network->findSegment(pos);
            length += seg->getLength();
            time_t blockTime = now + (length / vTaxi);
            network->blockSegmentsEndingAt(seg, i->getId(), blockTime - 30, now);
        }
    }

    return true;
}

int FGGroundController::getFrequency() {
    int groundFreq = parent->getGroundFrequency(2);
    int towerFreq = parent->getTowerFrequency(2);
    return groundFreq>0?groundFreq:towerFreq;
}

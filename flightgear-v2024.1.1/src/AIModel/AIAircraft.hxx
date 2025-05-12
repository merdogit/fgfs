/*
 * SPDX-FileName: AIAircraft.hxx
 * SPDX-FileComment: AIBase derived class creates an AI aircraft
 * SPDX-FileCopyrightText: Copyright (C) 2003  David P. Culp - davidculp2@comcast.net
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <iostream>
#include <string>
#include <string_view>

#include "AIBaseAircraft.hxx"


class PerformanceData;
class FGAISchedule;
class FGAIFlightPlan;
class FGATCController;
class FGATCInstruction;
class FGAIWaypoint;
class sg_ofstream;

namespace AILeg {
enum Type {
    STARTUP_PUSHBACK = 1,
    TAXI = 2,
    TAKEOFF = 3,
    CLIMB = 4,
    CRUISE = 5,
    APPROACH = 6,
    HOLD = 7,
    LANDING = 8,
    PARKING_TAXI = 9,
    PARKING = 10
};
}

// 1 = joined departure queue; 2 = Passed DepartureHold waypoint; handover control to tower; 0 = any other state.
namespace AITakeOffStatus {
enum Type {
    NONE = 0,
    QUEUED = 1,             // joined departure queue
    CLEARED_FOR_TAKEOFF = 2 // Passed DepartureHold waypoint; handover control to tower;
};
}

class FGAIAircraft : public FGAIBaseAircraft
{
public:
    explicit FGAIAircraft(FGAISchedule* ref = 0);
    virtual ~FGAIAircraft();

    std::string_view getTypeString(void) const override { return "aircraft"; }
    void readFromScenario(SGPropertyNode* scFileNode) override;

    void bind() override;
    void update(double dt) override;
    void unbind() override;

    void setPerformance(const std::string& acType, const std::string& perfString);

    void setFlightPlan(const std::string& fp, bool repat = false);

#if 0
    void initializeFlightPlan();
#endif

    FGAIFlightPlan* GetFlightPlan() const
    {
        return fp.get();
    };
    void ProcessFlightPlan(double dt, time_t now);
    time_t checkForArrivalTime(const std::string& wptName);
    time_t calcDeparture();

    void AccelTo(double speed);
    void PitchTo(double angle);
    void RollTo(double angle);

#if 0
    void YawTo(double angle);
#endif

    void ClimbTo(double altitude);
    void TurnTo(double heading);

    void getGroundElev(double dt); // TODO: these 3 really need to be public?
    void doGroundAltitude();
    bool loadNextLeg(double dist = 0);
    void resetPositionFromFlightPlan();
    double getBearing(double crse);

    void setAcType(const std::string& ac) { acType = ac; };
    const std::string& getAcType() const { return acType; }

    const std::string& getCompany() const { return company; }
    void setCompany(const std::string& comp) { company = comp; };

    //ATC
    void announcePositionToController(); // TODO: have to be public?
    void processATC(const FGATCInstruction& instruction);
    void setTaxiClearanceRequest(bool arg) { needsTaxiClearance = arg; };
    bool getTaxiClearanceRequest() { return needsTaxiClearance; };
    FGAISchedule* getTrafficRef() { return trafficRef; };
    void setTrafficRef(FGAISchedule* ref) { trafficRef = ref; };
    void resetTakeOffStatus() { takeOffStatus = AITakeOffStatus::NONE; };
    void setTakeOffStatus(int status) { takeOffStatus = status; };
    int getTakeOffStatus() { return takeOffStatus; };
    void setTakeOffSlot(time_t timeSlot) { takeOffTimeSlot = timeSlot; };
    time_t getTakeOffSlot() { return takeOffTimeSlot; };
    void scheduleForATCTowerDepartureControl();

    const std::string& GetTransponderCode() { return transponderCode; };
    void SetTransponderCode(const std::string& tc) { transponderCode = tc; };

    // included as performance data needs them, who else?
    inline PerformanceData* getPerformance() { return _performance; };
    inline bool onGround() const { return no_roll; };
    inline double getSpeed() const { return speed; };
    inline double getRoll() const { return roll; };
    inline double getPitch() const { return pitch; };
    inline double getAltitude() const { return altitude_ft; };
    inline double getVerticalSpeedFPM() const { return vs_fps * 60; };
    inline double altitudeAGL() const { return props->getFloatValue("position/altitude-agl-ft"); };
    inline double airspeed() const { return props->getFloatValue("velocities/airspeed-kt"); };
    const std::string& atGate();
    std::string acwakecategory;

    void checkTcas();
    double calcVerticalSpeed(double vert_ft, double dist_m, double speed, double error);

    FGATCController* getATCController() { return controller; };

    void clearATCController();
    bool isBlockedBy(FGAIAircraft* other);
    void dumpCSVHeader(const std::unique_ptr<sg_ofstream>& o);
    void dumpCSV(const std::unique_ptr<sg_ofstream>& o, int lineIndex);

protected:
    void Run(double dt);

private:
    FGAISchedule* trafficRef;
    FGATCController *controller,
        *prevController,
        *towerController; // Only needed to make a pre-announcement

    bool hdg_lock;
    bool alt_lock;
    double dt_count;
    double dt_elev_count;
    double headingChangeRate;
    double headingError;
    double minBearing;
    double speedFraction;

    /**Zero if FP is not active*/
    double groundTargetSpeed;
    double groundOffset;

    bool use_perf_vs;
    SGPropertyNode_ptr refuel_node;
    SGPropertyNode_ptr tcasThreatNode;
    SGPropertyNode_ptr tcasRANode;

    // helpers for Run
    // TODO: sort out which ones are better protected virtuals to allow
    //subclasses to override specific behaviour
    bool fpExecutable(time_t now);
    void handleFirstWaypoint(void);
    bool leadPointReached(FGAIWaypoint* curr, FGAIWaypoint* next, int nextTurnAngle);
    bool handleAirportEndPoints(FGAIWaypoint* prev, time_t now);
    bool reachedEndOfCruise(double&);
    bool aiTrafficVisible(void);
    void controlHeading(FGAIWaypoint* curr,
                        FGAIWaypoint* next);
    void controlSpeed(FGAIWaypoint* curr, FGAIWaypoint* next);

    void updatePrimaryTargetValues(double dt, bool& flightplanActive, bool& aiOutOfSight);
    void updateSecondaryTargetValues(double dt);
    void updateHeading(double dt);
    void updateBankAngleTarget();
    void updateVerticalSpeedTarget(double dt);
    void updatePitchAngleTarget();
    void updateActualState(double dt);
    void updateModelProperties(double dt);
    /**Handle special cases for the User AI shadow*/
    void updateUserFlightPlan(double dt);

    /**Calculate the next leg (hold or not)*/
    int determineNextLeg(int leg);
    void handleATCRequests(double dt);

    inline bool isStationary()
    {
        return ((fabs(speed) <= 0.0001) && (fabs(tgt_speed) <= 0.0001));
    }

    inline bool needGroundElevation()
    {
        if (!isStationary())
            _needsGroundElevation = true;
        return _needsGroundElevation;
    }

    double sign(double x);

#if 0
    std::string getTimeString(int timeOffset);
#endif

    void lazyInitControlsNodes();

    std::string acType;
    std::string company;
    std::string transponderCode;

    int spinCounter;

    /**Kills a flight when it's stuck */
    const int AI_STUCK_LIMIT = 100;
    int stuckCounter = 0;
    bool tracked = false;
    /**
     * Signals a reset to leg 1 at a different airport.
     * The leg loading happens at a different place than the parking loading.
     * */
    bool repositioned = false;
    double prevSpeed;
    double prev_dist_to_go;

    bool holdPos = false;

    const char* _getTransponderCode() const;

    bool needsTaxiClearance = false;
    bool _needsGroundElevation = true;
    int takeOffStatus; // 1 = joined departure queue; 2 = Passed DepartureHold waypoint; handover control to tower; 0 = any other state.
    time_t takeOffTimeSlot{0};
    time_t timeElapsed{0};

    PerformanceData* _performance; // the performance data for this aircraft

#if 0
   void assertSpeed(double speed);
#endif

    struct
    {
        double remainingLength;
        std::string startWptName;
        std::string finalWptName;
    } trackCache;

    // these are init-ed on first use by lazyInitControlsNodes()
    SGPropertyNode_ptr _controlsLateralModeNode,
        _controlsVerticalModeNode,
        _controlsTargetHeadingNode,
        _controlsTargetRollNode,
        _controlsTargetAltitude,
        _controlsTargetPitch,
        _controlsTargetSpeed;

    std::unique_ptr<sg_ofstream> csvFile;
    long csvIndex{0};
};

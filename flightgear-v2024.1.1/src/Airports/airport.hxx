/*
 * SPDX-FileName: airport.hxx
 * SPDX-FileComment: a really simplistic class to manage airport ID, lat, lon of the center of one of it's runways, and elevation in feet.
 * SPDX-FileCopyrightText: Copyright (C) 1998  Curtis L. Olson  - http://www.flightgear.org/~curt
 * SPDX-FileContributor: Updated by Durk Talsma, started December 2004.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <simgear/compiler.h>
#include <simgear/misc/sg_path.hxx>

#include <Navaids/positioned.hxx>
#include <Navaids/procedure.hxx>

#include "airports_fwd.hxx"
#include "runways.hxx"

class FGGroundNetwork;


/***************************************************************************************
 *
 **************************************************************************************/
class FGAirport : public FGPositioned
{
public:
    // 'sceneryPath' is the scenery path that provided the apt.dat file for
    // the airport (except for the “default dat file” under $FG_ROOT and the
    // null SGPath special case, 'sceneryPath' should be an element of
    // globals->get_fg_scenery()). Knowing this allows one to stop looking for
    // files such as ils.xml or threshold.xml in scenery paths that come later
    // in globals->get_fg_scenery() order (cf. XMLLoader::findAirportData()).
    FGAirport(PositionedID aGuid, const std::string& id, const SGGeod& location,
              const std::string& name, bool has_metar, Type aType,
              SGPath sceneryPath = SGPath());
    ~FGAirport();

    static bool isType(FGPositioned::Type ty)
    {
        return (ty >= FGPositioned::AIRPORT) && (ty <= FGPositioned::SEAPORT);
    }

    // Return the realpath() of the scenery folder under which we found the
    // apt.dat file for this airport.
    SGPath sceneryPath() const;
    const std::string& getId() const { return ident(); }
    const std::string& getName() const { return _name; }
    std::string toString() const { return "an airport " + ident(); }

    double getLongitude() const { return longitude(); }
    // Returns degrees
    double getLatitude() const { return latitude(); }
    // Returns ft
    double getElevation() const { return elevation(); }
    bool getMetar() const { return _has_metar; }
    bool isAirport() const;
    bool isSeaport() const;
    bool isHeliport() const;

    /// is the airport closed (disused)?
    /// note at present we look for an [x] in the name, ideally the database
    /// would explicitly include this
    bool isClosed() const
    {
        return mIsClosed;
    }

    virtual const std::string& name() const
    {
        return _name;
    }

    /**
     * reload the ILS data from XML if required.
     */
    void validateILSData();

    bool hasTower() const;

    SGGeod getTowerLocation() const;

    void setMetar(bool value) { _has_metar = value; }

    FGRunwayRef getActiveRunwayForUsage() const;

    FGAirportDynamicsRef getDynamics() const;

    FGGroundNetwork* groundNetwork() const;

    unsigned int numRunways() const;
    unsigned int numHelipads() const;
    FGRunwayRef getRunwayByIndex(unsigned int aIndex) const;
    FGHelipadRef getHelipadByIndex(unsigned int aIndex) const;
    FGRunwayMap getRunwayMap() const;
    FGHelipadMap getHelipadMap() const;

    bool hasRunwayWithIdent(const std::string& aIdent) const;
    bool hasHelipadWithIdent(const std::string& aIdent) const;
    FGRunwayRef getRunwayByIdent(const std::string& aIdent) const;
    FGHelipadRef getHelipadByIdent(const std::string& aIdent) const;

    struct FindBestRunwayForHeadingParams {
        FindBestRunwayForHeadingParams()
        {
            lengthWeight = 0.01;
            widthWeight = 0.01;
            surfaceWeight = 10;
            deviationWeight = 1;
            ilsWeight = 0;
        }
        double lengthWeight;
        double widthWeight;
        double surfaceWeight;
        double deviationWeight;
        double ilsWeight;
    };
    FGRunwayRef findBestRunwayForHeading(double aHeading, struct FindBestRunwayForHeadingParams* parms = NULL) const;

    /**
     * return the most likely target runway based on a position.
     * Specifically, return the runway for which the course from aPos
     * to the runway end, mostly closely matches the runway heading.
     * This is a good approximation of which runway the position is on or
     * aiming towards.
     */
    FGRunwayRef findBestRunwayForPos(const SGGeod& aPos) const;

    /**
     * Retrieve all runways at the airport, but excluding the reciprocal
     * runways. For example at KSFO this might return 1L, 1R, 28L and 28R,
     * but would not then include 19L/R or 10L/R.
     *
     * Exactly which runways you get, is undefined (i.e, dont assumes it's
     * runways with heading < 180 degrees) - it depends on order in apt.dat.
     *
     * This is useful for code that wants to process each piece of tarmac at
     * an airport *once*, not *twice* - eg mapping and nav-display code.
     */
    FGRunwayList getRunwaysWithoutReciprocals() const;

    /**
     * Retrieve all runways at the airport
     */
    FGRunwayList getRunways() const;

    std::string findAPTRunwayForNewName(const std::string& newIdent) const;

    /**
     * Useful predicate for FMS/GPS/NAV displays and similar - check if this
     * airport has a hard-surfaced runway of at least the specified length.
     */
    bool hasHardRunwayOfLengthFt(double aLengthFt) const;

    FGRunwayRef longestRunway() const;

    unsigned int numTaxiways() const;
    FGTaxiwayRef getTaxiwayByIndex(unsigned int aIndex) const;
    FGTaxiwayList getTaxiways() const;

    unsigned int numPavements() const;
    FGPavementRef getPavementByIndex(unsigned int aIndex) const;
    FGPavementList getPavements() const;
    void addPavement(FGPavementRef pavement);

    unsigned int numBoundary() const;
    FGPavementRef getBoundaryIndex(unsigned int aIndex) const;
    FGPavementList getBoundary() const;
    void addBoundary(FGPavementRef boundary);

    unsigned int numLineFeatures() const;
    FGPavementList getLineFeatures() const;
    void addLineFeature(FGPavementRef linefeature);

    class AirportFilter : public Filter
    {
    public:
        virtual bool pass(FGPositioned* aPos) const
        {
            return passAirport(static_cast<FGAirport*>(aPos));
        }

        virtual Type minType() const
        {
            return AIRPORT;
        }

        virtual Type maxType() const
        {
            return AIRPORT;
        }

        virtual bool passAirport(FGAirport* aApt) const
        {
            return true;
        }
    };

    /**
      * Filter which passes heliports and seaports in addition to airports
      */
    class PortsFilter : public AirportFilter
    {
    public:
        virtual Type maxType() const override
        {
            return SEAPORT;
        }
    };

    class HardSurfaceFilter : public AirportFilter
    {
    public:
        explicit HardSurfaceFilter(double minLengthFt = -1);

        virtual bool passAirport(FGAirport* aApt) const override;

    private:
        double mMinLengthFt;
    };

    /**
      * Filter which passes specified port type and in case of airport checks
      * if a runway larger the /sim/navdb/min-runway-length-ft exists.
      */
    class TypeRunwayFilter : public AirportFilter
    {
    public:
        TypeRunwayFilter();

        /**
          * Construct from string containing type (airport, seaport or heliport)
          */
        bool fromTypeString(const std::string& type);

        virtual FGPositioned::Type minType() const override { return _type; }
        virtual FGPositioned::Type maxType() const override { return _type; }
        virtual bool pass(FGPositioned* pos) const override;

    protected:
        FGPositioned::Type _type;
        double _min_runway_length_ft;
    };


    void setProcedures(const std::vector<flightgear::SID*>& aSids,
                       const std::vector<flightgear::STAR*>& aStars,
                       const std::vector<flightgear::Approach*>& aApproaches);

    void addSID(flightgear::SID* aSid);
    void addSTAR(flightgear::STAR* aStar);
    void addApproach(flightgear::Approach* aApp);

    unsigned int numSIDs() const;
    flightgear::SID* getSIDByIndex(unsigned int aIndex) const;
    flightgear::SID* findSIDWithIdent(const std::string& aIdent) const;
    flightgear::SIDList getSIDs() const;

    flightgear::Transition* selectSIDByEnrouteTransition(FGPositioned* enroute) const;
    flightgear::Transition* selectSIDByTransition(const FGRunway* runway, const std::string& aIdent) const;

    unsigned int numSTARs() const;
    flightgear::STAR* getSTARByIndex(unsigned int aIndex) const;
    flightgear::STAR* findSTARWithIdent(const std::string& aIdent) const;
    flightgear::STARList getSTARs() const;

    flightgear::Transition* selectSTARByEnrouteTransition(FGPositioned* enroute) const;
    flightgear::Transition* selectSTARByTransition(const FGRunway* runway, const std::string& aIdent) const;

    unsigned int numApproaches() const;
    flightgear::Approach* getApproachByIndex(unsigned int aIndex) const;
    flightgear::Approach* findApproachWithIdent(const std::string& aIdent) const;
    flightgear::ApproachList getApproaches(
        flightgear::ProcedureType type = flightgear::PROCEDURE_INVALID) const;

    /**
      * Syntactic wrapper around FGPositioned::findClosest - find the closest
      * match for filter, and return it cast to FGAirport. The default filter
      * passes airports, but not seaports or heliports
      */
    static FGAirportRef findClosest(const SGGeod& aPos, double aCuttofNm, Filter* filter = NULL);

    /**
      * Helper to look up an FGAirport instance by unique ident. Throws an
      * exception if the airport could not be found - so callers can assume
      * the result is non-NULL.
      */
    static FGAirportRef getByIdent(const std::string& aIdent);

    /**
      * Helper to look up an FGAirport instance by unique ident. Returns NULL
      * if the airport could not be found.
      */
    static FGAirportRef findByIdent(const std::string& aIdent);

    /**
      * Specialised helper to implement the AirportList dialog. Performs a
      * case-insensitive search on airport names and ICAO codes, and returns
      * matches in a format suitable for use by a puaList.
      */
    static char** searchNamesAndIdents(const std::string& aFilter);


    /**
     * Sort an FGPositionedList of airports by size (number of runways + length)
     * this is meant to prioritise more important airports.
     */
    static void sortBySize(FGPositionedList&);

    flightgear::CommStationList commStationsOfType(FGPositioned::Type aTy) const;

    flightgear::CommStationList commStations() const;

    static void clearAirportsCache();

    // helper to allow testing without needing a full Airports hierarchy
    // only for use by the test-suite, not available outside of it.
    void testSuiteInjectGroundnetXML(const SGPath& path);

    void testSuiteInjectProceduresXML(const SGPath& path);

private:
    static flightgear::AirportCache airportCache;

    // disable these
    FGAirport operator=(FGAirport& other);
    FGAirport(const FGAirport&);

    /**
     * helper to read airport data from the scenery XML files.
     */
    void loadSceneryDefinitions() const;

    /**
     * Helpers to process property data loaded from an ICAO.threshold.xml file
     */
    void readThresholdData(SGPropertyNode* aRoot);
    void processThreshold(SGPropertyNode* aThreshold);

    void readILSData(SGPropertyNode* aRoot);

    void validateTowerData() const;

    /**
     * Helpers to parse property data loaded from an ICAO.twr.xml file
     */
    void readTowerData(SGPropertyNode* aRoot);

    /**
     * Helpers to parse property data loaded from an ICAO.runway_rename.xml file
     */
    void parseRunwayRenameData(SGPropertyNode* aRoot);

    PositionedIDVec itemsOfType(FGPositioned::Type ty) const;

    std::string _name;
    bool _has_metar = false;
    const SGPath _sceneryPath;

    void loadRunways() const;
    void loadHelipads() const;
    void loadTaxiways() const;
    void loadProcedures() const;
    void loadRunwayRenames() const;

    mutable bool mTowerDataLoaded;
    mutable bool mHasTower;
    mutable SGGeod mTowerPosition;

    mutable bool mRunwaysLoaded;
    mutable bool mHelipadsLoaded;
    mutable bool mTaxiwaysLoaded;
    mutable bool mProceduresLoaded;
    mutable bool mRunwayRenamesLoaded = false;
    bool mIsClosed;
    mutable bool mThresholdDataLoaded;
    bool mILSDataLoaded;

    mutable std::vector<FGRunwayRef> mRunways;

    mutable PositionedIDVec mHelipads;
    mutable PositionedIDVec mTaxiways;
    std::vector<FGPavementRef> mPavements;
    std::vector<FGPavementRef> mBoundary;
    std::vector<FGPavementRef> mLineFeatures;

    typedef SGSharedPtr<flightgear::SID> SIDRef;
    typedef SGSharedPtr<flightgear::STAR> STARRef;
    typedef SGSharedPtr<flightgear::Approach> ApproachRef;

    std::vector<SIDRef> mSIDs;
    std::vector<STARRef> mSTARs;
    std::vector<ApproachRef> mApproaches;

    mutable std::unique_ptr<FGGroundNetwork> _groundNetwork;

    using RunwayRenameMap = std::map<std::string, std::string>;
    // map from new name (eg in Navigraph) to old name (in apt.dat)
    RunwayRenameMap _renamedRunways;
};

// find basic airport location info from airport database
const FGAirport* fgFindAirportID(const std::string& id);

// get airport elevation
double fgGetAirportElev(const std::string& id);

#include "test_navaids2.hxx"

#include "test_suite/FGTestApi/testGlobals.hxx"
#include "test_suite/FGTestApi/NavDataCache.hxx"

#include <Airports/airport.hxx>

#include <Navaids/NavDataCache.hxx>
#include <Navaids/navrecord.hxx>
#include <Navaids/navlist.hxx>


// Set up function for each test.
void NavaidsTests::setUp()
{
    FGTestApi::setUp::initTestGlobals("navaids2");
    FGTestApi::setUp::initNavDataCache();
}


// Clean up after each test.
void NavaidsTests::tearDown()
{
    FGTestApi::tearDown::shutdownTestGlobals();
}


void NavaidsTests::testBasic()
{
    SGGeod egccPos = SGGeod::fromDeg(-2.27, 53.35);
    FGNavRecordRef tla = FGNavList::findByFreq(115.7, egccPos);

    CPPUNIT_ASSERT_EQUAL(strcmp(tla->get_ident(), "TNT"), 0);
    CPPUNIT_ASSERT(tla->ident() == "TNT");
    CPPUNIT_ASSERT(tla->name() == "TRENT VOR-DME");
    CPPUNIT_ASSERT_EQUAL(tla->get_freq(), 11570);
    CPPUNIT_ASSERT_EQUAL(tla->get_range(), 130);
}

void NavaidsTests::testCustomWaypoint()
{
    // create a transaction, which we don't commit, to avoid making permanent DB changes
    flightgear::NavDataCache::Transaction txn(flightgear::NavDataCache::instance());

    SGGeod egccPos = SGGeod::fromDeg(-2.27, 53.35);
    SGGeod offsetPos = SGGeodesy::direct(egccPos, 45.0, 20.0 * SG_NM_TO_METER);
    auto poi = FGPositioned::createWaypoint(FGPositioned::WAYPOINT,
                                            "TEST_WP0", offsetPos, false, "Lovely Waypoint");
    CPPUNIT_ASSERT(poi);
    CPPUNIT_ASSERT_EQUAL(poi->ident(), std::string{"TEST_WP0"});
    CPPUNIT_ASSERT_EQUAL(poi->name(), std::string{"Lovely Waypoint"});
    CPPUNIT_ASSERT(poi->guid() > 0);
    CPPUNIT_ASSERT_EQUAL(poi->type(), FGPositioned::WAYPOINT);
    CPPUNIT_ASSERT(FGTestApi::geodsApproximatelyEqual(offsetPos, poi->geod()));

    // same but a FIX
    SGGeod offsetPos2 = SGGeodesy::direct(egccPos, 180.0, 35.0 * SG_NM_TO_METER);
    auto fix = FGPositioned::createWaypoint(FGPositioned::FIX,
                                            "TEST_WP1", offsetPos2);
    CPPUNIT_ASSERT_EQUAL(fix->ident(), std::string{"TEST_WP1"});
    CPPUNIT_ASSERT_EQUAL(fix->name(), fix->ident());
    CPPUNIT_ASSERT(fix->guid() > 0);
    CPPUNIT_ASSERT_EQUAL(fix->type(), FGPositioned::FIX);
    CPPUNIT_ASSERT(FGTestApi::geodsApproximatelyEqual(offsetPos2, fix->geod()));

    // create same ident but far away
    FGAirportRef vhhh = FGAirport::getByIdent("VHHH");

    SGGeod pos3 = SGGeodesy::direct(vhhh->geod(), 10.0, 10.0 * SG_NM_TO_METER);
    auto poi3 = FGPositioned::createWaypoint(FGPositioned::WAYPOINT,
                                             "TEST_WP0", pos3, false, "Lovely Hong Kong Waypoint");
    CPPUNIT_ASSERT_EQUAL(poi3->ident(), std::string{"TEST_WP0"});
    CPPUNIT_ASSERT_EQUAL(poi3->name(), std::string{"Lovely Hong Kong Waypoint"});
    CPPUNIT_ASSERT(FGTestApi::geodsApproximatelyEqual(pos3, poi3->geod()));

    // create same ident but nearby
    SGGeod pos4 = SGGeodesy::direct(egccPos, 270.0, 10.0 * SG_NM_TO_METER);
    auto duplicatePoi = FGPositioned::createWaypoint(FGPositioned::WAYPOINT,
                                                     "TEST_WP0", pos4, false, "Lovely Waypoint");
    CPPUNIT_ASSERT_EQUAL(duplicatePoi, poi);

    // create with invalid type
    CPPUNIT_ASSERT_THROW(FGPositioned::createWaypoint(FGPositioned::VOR,
                                                      "TEST_WP99", offsetPos),
                         std::logic_error);


    FGPositioned::TypeFilter filt(FGPositioned::WAYPOINT);
    FGPositionedList wps = FGPositioned::findAllWithIdent("TEST_WP0", &filt);
    CPPUNIT_ASSERT_EQUAL(wps.size(), static_cast<size_t>(2));

    CPPUNIT_ASSERT(FGPositioned::deleteWaypoint(poi));
    wps = FGPositioned::findAllWithIdent("TEST_WP0", &filt);
    CPPUNIT_ASSERT_EQUAL(wps.size(), static_cast<size_t>(1));
}

void NavaidsTests::testTemporaryWaypoint()
{
    SGGeod egccPos = SGGeod::fromDeg(-2.27, 53.35);
    SGGeod offsetPos = SGGeodesy::direct(egccPos, 45.0, 5.0 * SG_NM_TO_METER);
    auto poi = FGPositioned::createWaypoint(FGPositioned::WAYPOINT,
                                            "TEST_WP_TEMP0", offsetPos, true, "Lovely Waypoint");
    CPPUNIT_ASSERT(poi);
    CPPUNIT_ASSERT_EQUAL(poi->ident(), std::string{"TEST_WP_TEMP0"});
    CPPUNIT_ASSERT_EQUAL(poi->name(), std::string{"Lovely Waypoint"});
    CPPUNIT_ASSERT(poi->guid() < 0);
    CPPUNIT_ASSERT_EQUAL(poi->type(), FGPositioned::WAYPOINT);
    CPPUNIT_ASSERT(FGTestApi::geodsApproximatelyEqual(offsetPos, poi->geod()));

    FGPositioned::TypeFilter filt(FGPositioned::WAYPOINT);
    FGPositionedList wps = FGPositioned::findAllWithIdent("TEST_WP_TEMP0", &filt);
    CPPUNIT_ASSERT_EQUAL(wps.size(), static_cast<size_t>(1));
    CPPUNIT_ASSERT_EQUAL(wps.front(), poi);

    // check temporary points are found spatially
    auto closest = FGPositioned::findClosestN(egccPos, 1, 50.0, &filt);
    CPPUNIT_ASSERT_EQUAL(closest.size(), static_cast<size_t>(1));
    CPPUNIT_ASSERT_EQUAL(closest.front(), poi);

    auto byName = FGPositioned::findAllWithName("lovely", &filt, false);
    CPPUNIT_ASSERT_EQUAL(byName.size(), static_cast<size_t>(1));
    CPPUNIT_ASSERT_EQUAL(byName.front(), poi);


    // check for gross position update working
    FGAirportRef vhhh = FGAirport::getByIdent("VHHH");
    //  auto asPoi = fgpositioned_cast<POI>(poi);
    SGGeod newOffsetPos = SGGeodesy::direct(vhhh->geod(), 10.0, 10.0 * SG_NM_TO_METER);
    // asPoi->modifyPosition(newOffsetPos);
    flightgear::NavDataCache::instance()->updatePosition(poi->guid(), newOffsetPos);

    CPPUNIT_ASSERT(FGTestApi::geodsApproximatelyEqual(newOffsetPos, poi->geod()));

    closest = FGPositioned::findClosestN(egccPos, 1, 50.0, &filt);
    CPPUNIT_ASSERT_EQUAL(closest.size(), static_cast<size_t>(0));

    closest = FGPositioned::findClosestN(vhhh->geod(), 1, 50.0, &filt);
    CPPUNIT_ASSERT_EQUAL(closest.size(), static_cast<size_t>(1));
    CPPUNIT_ASSERT_EQUAL(closest.front(), poi);

    // delete it
    CPPUNIT_ASSERT(FGPositioned::deleteWaypoint(poi));
    wps = FGPositioned::findAllWithIdent("TEST_WP_TEMP0", &filt);
    CPPUNIT_ASSERT_EQUAL(wps.size(), static_cast<size_t>(0));

    closest = FGPositioned::findClosestN(vhhh->geod(), 1, 50.0, &filt);
    CPPUNIT_ASSERT_EQUAL(closest.size(), static_cast<size_t>(0));
}

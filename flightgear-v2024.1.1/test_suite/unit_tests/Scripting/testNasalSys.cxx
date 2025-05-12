/*
 * Copyright (C) 2016 Edward d'Auvergne
 *
 * This file is part of the program FlightGear.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "testNasalSys.hxx"

#include "test_suite/FGTestApi/testGlobals.hxx"
#include "test_suite/FGTestApi/NavDataCache.hxx"

#include <simgear/structure/commands.hxx>

#include <Main/fg_props.hxx>
#include <Main/globals.hxx>
#include <Main/util.hxx>

#include <Airports/airport.hxx>

#include <Scripting/NasalSys.hxx>

#include <Main/FGInterpolator.hxx>

// Set up function for each test.
void NasalSysTests::setUp()
{
    FGTestApi::setUp::initTestGlobals("NasalSys");
    FGTestApi::setUp::initNavDataCache();

    fgInitAllowedPaths();
    globals->get_props()->getNode("nasal", true);

    globals->get_subsystem_mgr()->add<FGInterpolator>();

    globals->get_subsystem_mgr()->bind();
    globals->get_subsystem_mgr()->init();

    globals->get_subsystem_mgr()->add<FGNasalSys>();

    globals->get_subsystem_mgr()->postinit();
}


// Clean up after each test.
void NasalSysTests::tearDown()
{
    FGTestApi::tearDown::shutdownTestGlobals();
}

bool NasalSysTests::checkNoNasalErrors()
{
    auto nasalSys = globals->get_subsystem<FGNasalSys>();
    auto errors = nasalSys->getAndClearErrorList();
    return errors.empty();
}

// Test test
void NasalSysTests::testStructEquality()
{
    bool ok = FGTestApi::executeNasal(R"(
     var foo = {
      "name": "Bob",
          "size": [512, 512],
          "mipmapping": 1.9
        };
                                      
    var bar = {
      "name": "Bob",
          "size": [512, 512],
          "mipmapping": 1.9
        };       

    unitTest.assert_equal(foo, bar);
                                      
    append(bar.size, "Wowow");
    unitTest.assert(unitTest.equal(foo, bar) == 0);
                                      
    append(foo.size, "Wowow");
    unitTest.assert_equal(foo, bar);
                                      
    foo.wibble = 99.1;
    unitTest.assert(unitTest.equal(foo, bar) == 0);
                                      
    bar.wibble = 99;
    unitTest.assert(unitTest.equal(foo, bar) == 0);
    bar.wibble = 99.1;
    unitTest.assert_equal(foo, bar);

    )");
    CPPUNIT_ASSERT(ok);
}

void NasalSysTests::testCommands()
{
    auto nasalSys = globals->get_subsystem<FGNasalSys>();
    nasalSys->getAndClearErrorList();

    fgSetInt("/foo/test", 7);
    bool ok = FGTestApi::executeNasal(R"(
     var f = func { 
         var i = getprop('/foo/test');
         setprop('foo/test', i + 4);
     };
                                      
      addcommand('do-foo', f);
      var ok = fgcommand('do-foo');
      unitTest.assert(ok);
    )");
    CPPUNIT_ASSERT(ok);

    CPPUNIT_ASSERT_EQUAL(11, fgGetInt("/foo/test"));

    SGPropertyNode_ptr args(new SGPropertyNode);
    ok = globals->get_commands()->execute("do-foo", args);
    CPPUNIT_ASSERT(ok);
    CPPUNIT_ASSERT_EQUAL(15, fgGetInt("/foo/test"));

    ok = FGTestApi::executeNasal(R"(
       var g = func { print('fail'); };
       addcommand('do-foo', g);
    )");
    
    CPPUNIT_ASSERT(ok);
    auto errors = nasalSys->getAndClearErrorList();
    CPPUNIT_ASSERT_EQUAL(errors.size(), static_cast<size_t>(1));
    
    // old command should still be registered and work
    ok = globals->get_commands()->execute("do-foo", args);
    CPPUNIT_ASSERT(ok);
    CPPUNIT_ASSERT_EQUAL(19, fgGetInt("/foo/test"));

    ok = FGTestApi::executeNasal(R"(
      removecommand('do-foo');
   )");
    CPPUNIT_ASSERT(ok);

    ok = FGTestApi::executeNasal(R"(
     var ok = fgcommand('do-foo');
     unitTest.assert(!ok);
  )");
    CPPUNIT_ASSERT(ok);

    errors = nasalSys->getAndClearErrorList();
    CPPUNIT_ASSERT_EQUAL(0UL, (unsigned long) errors.size());

    // should fail, command is removed
    ok = globals->get_commands()->execute("do-foo", args);
    CPPUNIT_ASSERT(!ok);
    CPPUNIT_ASSERT_EQUAL(19, fgGetInt("/foo/test"));
}

void NasalSysTests::testAirportGhost()
{
    auto nasalSys = globals->get_subsystem<FGNasalSys>();
    nasalSys->getAndClearErrorList();

    bool ok = FGTestApi::executeNasal(R"(
        var apt = airportinfo('LFBD');
        var taxiways = apt.taxiways;    
        unitTest.assert_equal(size(taxiways), 0);
    )");
    CPPUNIT_ASSERT(ok);

}

void NasalSysTests::testFindComm()
{
    FGAirportRef apt = FGAirport::getByIdent("EDDM");
    FGTestApi::setPositionAndStabilise(apt->geod());

    auto nasalSys = globals->get_subsystem<FGNasalSys>();
    nasalSys->getAndClearErrorList();

    bool ok = FGTestApi::executeNasal(R"(
        var comm = findCommByFrequencyMHz(123.125);
        unitTest.assert_equal(comm.id, "ATIS");

    # explicit filter, should't match
        var noComm = findCommByFrequencyMHz(123.125, "tower");
        unitTest.assert_equal(noComm, nil);

    # match with filter
        var comm2 = findCommByFrequencyMHz(121.725, "clearance");
        unitTest.assert_equal(comm2.id, "CLNC DEL");
    )");

    CPPUNIT_ASSERT(ok && checkNoNasalErrors());
}


// https://sourceforge.net/p/flightgear/codetickets/2246/

void NasalSysTests::testCompileLarge()
{
//    auto nasalSys = globals->get_subsystem<FGNasalSys>();
//    nasalSys->getAndClearErrorList();
//
//
//    string code = "var foo = 0;\n";
//    for (int i=0; i<14; ++i) {
//        code = code + code;
//    }
//
//    nasalSys->parseAndRun(code);
    
//    bool ok = FGTestApi::executeNasal(R"(
//var try_compile = func(code) {
//    call(compile, [code], nil,nil,var err=[]);
//    return size(err);
//}
//
//var expression = "var foo = 0;\n";
//var code = "";
//
//for(var i=0;i<=10000;i+=1) {
//    code ~= expression;
//    if (try_compile(code) == 1) {
//        print("Error compiling, LOC count is:", i+1);
//        break;
//    }
//}
//    )");
//    CPPUNIT_ASSERT(ok);
}


void NasalSysTests::testRoundFloor()
{
    auto nasalSys = globals->get_subsystem<FGNasalSys>();
    nasalSys->getAndClearErrorList();

    bool ok = FGTestApi::executeNasal(R"(
        unitTest.assert_equal(math.round(121266, 1000), 121000);
        unitTest.assert_equal(math.round(121.1234, 0.01), 121.12);
        unitTest.assert_equal(math.round(121266, 10), 121270);
    
        unitTest.assert_equal(math.floor(121766, 1000), 121000);
        unitTest.assert_equal(math.floor(121.1299, 0.01), 121.12);
    
        # floor towards lower value
        unitTest.assert_equal(math.floor(-121.1229, 0.01), -121.13);
    
        # truncate towards zero
        unitTest.assert_equal(math.trunc(-121.1229, 0.01), -121.12);
        unitTest.assert_equal(math.trunc(-121.1299, 0.01), -121.12);
    )");
    CPPUNIT_ASSERT(ok);
}

void NasalSysTests::testRange()
{
    auto nasalSys = globals->get_subsystem<FGNasalSys>();
    nasalSys->getAndClearErrorList();

    bool ok = FGTestApi::executeNasal(R"(
        unitTest.assert_equal(range(5), [0, 1, 2, 3, 4]);
        unitTest.assert_equal(range(2, 8), [2, 3, 4, 5, 6, 7]);
        unitTest.assert_equal(range(2, 10, 3), [2, 5, 8]);

    )");
    CPPUNIT_ASSERT(ok);
}

void NasalSysTests::testKeywordArgInHash()
{
    auto nasalSys = globals->get_subsystem<FGNasalSys>();
    nasalSys->getAndClearErrorList();

    bool ok = FGTestApi::executeNasal(R"(
        var foo = func(arg1, kw1 = "", kw2 = nil)
        {
            return {'a':kw1, 'b':kw2};
        }
        
        var d = foo(arg1:42, kw2:'apples', kw1:'pears');
        unitTest.assert_equal(d.a, 'pears');
        unitTest.assert_equal(d.b, 'apples');

    )");
    CPPUNIT_ASSERT(ok);

    ok = FGTestApi::executeNasal(R"(
        var bar = func(h) {
            return h;
        }

        var foo = func(arg1, kw1 = "", kw2 = nil)
        {
            return bar({'a':kw1, 'b':kw2});
        }
        
        var d = foo(arg1:42, kw2:'apples', kw1:'pears');
        unitTest.assert_equal(d.a, 'pears');
        unitTest.assert_equal(d.b, 'apples');

    )");
    CPPUNIT_ASSERT(ok);

    ok = FGTestApi::executeNasal(R"(
        var bar = func(h) {
            unitTest.assert_equal(h.a, 'pears');
            unitTest.assert_equal(h.b, 'apples');
        }

        var foo = func(arg1, kw1 = "", kw2 = nil)
        {
            return bar({'a':kw1, 'b':kw2});
        }
        
        var d = foo(arg1:42, kw2:'apples', kw1:'pears');    
    )");
    CPPUNIT_ASSERT(ok);
}

void NasalSysTests::testNullAccess()
{
    auto nasalSys = globals->get_subsystem<FGNasalSys>();
    nasalSys->getAndClearErrorList();

    bool ok = FGTestApi::executeNasal(R"(
        var s =  {
           bar: 42
        };

        unitTest.assert_equal(s?.bar, 42);

        var t = nil;
        var z = t?.bar;
        unitTest.assert_equal(z, nil);
        

    )");
    CPPUNIT_ASSERT(ok);
    auto errors = nasalSys->getAndClearErrorList();
    CPPUNIT_ASSERT_EQUAL(errors.size(), static_cast<size_t>(0));
}

void NasalSysTests::testNullishChain()
{
    auto nasalSys = globals->get_subsystem<FGNasalSys>();
    nasalSys->getAndClearErrorList();

    bool ok = FGTestApi::executeNasal(R"(
        var t = nil;
        var s = 'abc';

        unitTest.assert_equal(t ?? 99, 99);
        unitTest.assert_equal(s ?? 'default', 'abc');
        unitTest.assert_equal(t ?? 'default', 'default');

        # check 0 is used, only nil should fail
        unitTest.assert_equal(0 ?? 'default', 0);
    )");

    CPPUNIT_ASSERT(ok);
    auto errors = nasalSys->getAndClearErrorList();
    CPPUNIT_ASSERT_EQUAL(errors.size(), static_cast<size_t>(0));
}

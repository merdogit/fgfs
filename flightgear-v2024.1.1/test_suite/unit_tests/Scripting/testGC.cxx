
// SPDX-FileCopyrightText: (C) 2020 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#include "testGC.hxx"

#include "test_suite/FGTestApi/testGlobals.hxx"

#include <Main/globals.hxx>
#include <Main/util.hxx>
#include <Scripting/NasalSys.hxx>

#include <Main/FGInterpolator.hxx>

extern bool global_nasalMinimalInit;

// Set up function for each test.
void NasalGCTests::setUp()
{
    FGTestApi::setUp::initTestGlobals("NasalGC");

    fgInitAllowedPaths();
    auto nasalNode = globals->get_props()->getNode("nasal", true);

    globals->get_subsystem_mgr()->add<FGInterpolator>();

    globals->get_subsystem_mgr()->bind();
    globals->get_subsystem_mgr()->init();
    
    global_nasalMinimalInit = true;
    globals->get_subsystem_mgr()->add<FGNasalSys>();
    
    globals->get_subsystem_mgr()->postinit();
}


// Clean up after each test.
void NasalGCTests::tearDown()
{
    global_nasalMinimalInit = false;
    FGTestApi::tearDown::shutdownTestGlobals();
}


// Test test
void NasalGCTests::testDummy()
{
    bool ok = FGTestApi::executeNasal(R"(
     var foo = {
      "name": "PFD-Test",
          "size": [512, 512],
          "view": [768, 1024],
          "mipmapping": 1
        };
                                      
      globals.foo1 = foo;
    )");
    CPPUNIT_ASSERT(ok);
}

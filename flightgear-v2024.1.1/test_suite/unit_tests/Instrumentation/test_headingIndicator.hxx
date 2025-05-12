/*
 * SPDX-FileCopyrightText: (C) 2024 James Turner <james@flightgear.org>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once


#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <simgear/structure/subsystem_mgr.hxx>

class FGNavRadio;
class SGGeod;

class HeadingIndicatorTests : public CppUnit::TestFixture
{
    // Set up the test suite.
    CPPUNIT_TEST_SUITE(HeadingIndicatorTests);

    CPPUNIT_TEST(testBasic);
    CPPUNIT_TEST(testTumble);
    CPPUNIT_TEST(testLatitudeNut);
    CPPUNIT_TEST(testVacuumGyro);

    CPPUNIT_TEST_SUITE_END();

    SGSubsystemRef setupInstrument(const std::string& name, int index);

public:
    // Set up function for each test.
    void setUp();

    // Clean up after each test.
    void tearDown();

    //  std::string formatFrequency(double f);

    // The tests.
    void testBasic();
    void testTumble();
    void testLatitudeNut();
    void testVacuumGyro();
};

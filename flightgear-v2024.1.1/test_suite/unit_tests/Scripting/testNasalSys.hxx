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


#ifndef _FG_NASALSYS_UNIT_TESTS_HXX
#define _FG_NASALSYS_UNIT_TESTS_HXX


#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestFixture.h>


// The unit tests of the FGNasalSys subsystem.
class NasalSysTests : public CppUnit::TestFixture
{
    // Set up the test suite.
    CPPUNIT_TEST_SUITE(NasalSysTests);
    CPPUNIT_TEST(testStructEquality);
    CPPUNIT_TEST(testCommands);
    CPPUNIT_TEST(testAirportGhost);
    CPPUNIT_TEST(testCompileLarge);
    CPPUNIT_TEST(testRoundFloor);
    CPPUNIT_TEST(testRange);
    CPPUNIT_TEST(testKeywordArgInHash);
    CPPUNIT_TEST(testNullAccess);
    CPPUNIT_TEST(testNullishChain);
    CPPUNIT_TEST(testFindComm);
    CPPUNIT_TEST_SUITE_END();

    bool checkNoNasalErrors();

public:
    // Set up function for each test.
    void setUp();

    // Clean up after each test.
    void tearDown();

    // The tests.
    void testStructEquality();
    void testCommands();
    void testAirportGhost();
    void testCompileLarge();
    void testRoundFloor();
    void testRange();
    void testKeywordArgInHash();
    void testNullAccess();
    void testNullishChain();
    void testFindComm();
};

#endif  // _FG_NASALSYS_UNIT_TESTS_HXX

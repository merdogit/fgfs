/*
 * Copyright (C) 2023 Stuart Buchanan
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


#ifndef _FG_SCENE_VPBRASTERRENDER_SIMGEAR_UNIT_TESTS_HXX
#define _FG_SCENE_VPBRASTERRENDER_SIMGEAR_UNIT_TESTS_HXX

#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestFixture.h>

#include <simgear/bucket/newbucket.hxx>

// The unit tests.
class VPBRasterRenderTests : public CppUnit::TestFixture
{
    // Set up the test suite.
    CPPUNIT_TEST_SUITE(VPBRasterRenderTests);
    CPPUNIT_TEST(testRaster);
    CPPUNIT_TEST_SUITE_END();

    SGBucket _bucket;

public:
    // Set up function for each test.
    void setUp();

    // Clean up after each test.
    void tearDown() {}

    // The tests.
    void testRaster();
};


#endif    // _FG_SCENE_VPBRASTERRENDER_SIMGEAR_UNIT_TESTS_HXX

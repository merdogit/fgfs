/*
 * Copyright (C) 2018 Edward d'Auvergne
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


#ifndef _FG_YASIM_ATMOSPHERE_UNIT_TESTS_HXX
#define _FG_YASIM_ATMOSPHERE_UNIT_TESTS_HXX

#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestFixture.h>

#include <src/FDM/YASim/Atmosphere.hpp>


// The unit tests.
class YASimAtmosphereTests : public CppUnit::TestFixture
{
    // Set up the test suite.
    CPPUNIT_TEST_SUITE(YASimAtmosphereTests);
    CPPUNIT_TEST(testAtmosphere);
    CPPUNIT_TEST_SUITE_END();

public:
    // Set up function for each test.
    void setUp();

    // Clean up after each test.
    void tearDown();

    // The tests.
    void testAtmosphere();

    // Data.
    std::unique_ptr<yasim::Atmosphere> a;
};

#endif  // _FG_YASIM_ATMOSPHERE_UNIT_TESTS_HXX

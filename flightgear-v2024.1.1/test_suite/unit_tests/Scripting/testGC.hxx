
// SPDX-FileCopyrightText: (C) 2020 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/TestFixture.h>


class NasalGCTests : public CppUnit::TestFixture
{
    // Set up the test suite.
    CPPUNIT_TEST_SUITE(NasalGCTests);
    CPPUNIT_TEST(testDummy);
    CPPUNIT_TEST_SUITE_END();

public:
    // Set up function for each test.
    void setUp();

    // Clean up after each test.
    void tearDown();

    // The tests.
    void testDummy();
};

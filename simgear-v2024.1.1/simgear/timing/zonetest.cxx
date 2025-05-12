
/*
 * Copyright (c) 2018, Bertold Van den Bergh (vandenbergh@bertold.org)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the author nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR DISTRIBUTOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <string>
#include <cstdio>
#include <cstdlib>

#include <simgear/misc/sg_path.hxx>

#define ZD_EXPORT
#include "zonedetect.h"

static void onError(int errZD, int errNative)
{
    fprintf(stderr, "ZD error: %s (0x%08X)\n", ZDGetErrorString(errZD), (unsigned)errNative);
}

int main(int argc, char *argv[])
{
    if(argc != 2) {
        printf("Usage: %s <fgdata-path>\n", argv[0]);
        return 1;
    }

    ZDSetErrorHandler(onError);

    std::string path = std::string{argv[1]} + "/Timezone/";
    std::string dbpath = path + "timezone16.bin";

    ZoneDetect *const cd = ZDOpenDatabase(dbpath.c_str());
    if(!cd) {
        printf("Failed to open: %s\n", dbpath.c_str());
        return 2;
    }

    printf("Testting timezone files...\n");
    std::string prev;
    for (float lat = -90.0f; lat < 90.0f; lat += 0.5f) {
        printf("lat: %.1f\r", lat);
        fflush(stdout);
        for (float lon = -180.0f; lon < 180.0f; lon += 0.5f) {

            float safezone = 0;
            ZoneDetectResult *results = ZDLookup(cd, lat, lon, &safezone);
 
            char *TimezoneIdPrefix = nullptr;
            char *TimezoneId = nullptr;
            for(unsigned i=0; i<results[0].numFields; ++i) {
                if(results[0].fieldNames[i] && results[0].data[i]) {
                  std::string fieldName = results[0].fieldNames[i];
                  if (fieldName == "TimezoneIdPrefix") {
                      TimezoneIdPrefix = results[0].data[i];
                  } else if (fieldName == "TimezoneId") {
                      TimezoneId = results[0].data[i];
                  }
                }
            }

            if (TimezoneIdPrefix && TimezoneId) {
                const auto desc = std::string{TimezoneIdPrefix} + std::string{TimezoneId};
                if (desc != prev) {
                    prev = desc;
                    SGPath tzfile = path + desc;
                    if (!tzfile.exists()) {
                        printf("Timezone file not found: %s\n", desc.c_str());
                    }
                }
            }
        }
    }
    ZDCloseDatabase(cd);

    return 0;
}

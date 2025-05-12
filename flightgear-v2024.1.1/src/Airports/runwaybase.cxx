/*
 * SPDX-FileCopyrightText: (C) 2000 Curtis L. Olson - http://www.flightgear.org/~curt
 * SPDX_FileComment: represent a runway or taxiway
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <config.h>

#include <simgear/compiler.h>
#include <simgear/props/props.hxx>

#include "runwaybase.hxx"

#include <Airports/airport.hxx>

using std::string;

/*
 * surface codes, defined here (as of Jan 2024) https://developer.x-plane.com/article/airport-data-apt-dat-12-00-file-format-specification/
 * 1,20-38 - asphalt
 * 2,50-57 - concrete
 * 3 - turf
 * 4 - dirt
 * 5 - gravel
 * 12 - lakebed
 * 13 - water runway
 * 14 - snow or ice
 * 15 - Transparent
 */
const char * FGRunwayBase::surfaceName( int surface_code )
{
  switch( surface_code ) {
    case 1:
    case 20: // Light colored asphalt
    case 21:
    case 22:
    case 23:
    case 24: // Asphalt
    case 25:
    case 26:
    case 27: // Darker colored asphalt
    case 28:
    case 29:
    case 30:
    case 31: // Very dark colored asphalt
    case 32:
    case 33:
    case 34:
    case 35: // Near black, ‘new’ looking asphalt
    case 36:
    case 37:
    case 38:
      return "asphalt";
    case 2:
    case 50: // Light “new” looking concrete
    case 51:
    case 52:
    case 53: // Concrete
    case 54:
    case 55: // Dark concrete
    case 56:
    case 57:
      return "concrete";
    case 3: return "turf";
    case 4: return "dirt";
    case 5: return "gravel";
    case 12: return "lakebed";
    case 13: return "water";
    case 14: return "ice";
    case 15: return "transparent";
    default: return "unknown";
  }
}

FGRunwayBase::FGRunwayBase(PositionedID aGuid, Type aTy, const string& aIdent,
                           const SGGeod& aGeod,
                           const double heading, const double length,
                           const double width,
                           const int surface_code,
                           const PositionedID airportId) : FGPositioned(aGuid, aTy, aIdent, aGeod)
{
  _heading = heading;
  _length = length;
  _width = width;
  _surface_code = surface_code;
  mAirport = airportId;
}

FGAirportRef FGRunwayBase::airport() const
{
    return FGPositioned::loadById<FGAirport>(mAirport);
}

SGGeod FGRunwayBase::pointOnCenterline(double aOffset) const
{
  SGGeod result = SGGeodesy::direct(geod(), _heading, aOffset);
  result.setElevationM(geod().getElevationM());
    
  return result;
}

SGGeod FGRunwayBase::pointOffCenterline(double aOffset, double lateralOffset) const
{
    SGGeod result = pointOnCenterline(aOffset);
    return SGGeodesy::direct(result, SGMiscd::normalizePeriodic(0, 360,_heading+90.0), lateralOffset);
}


bool FGRunwayBase::isHardSurface() const
{
  return ((_surface_code == 1 || (_surface_code >= 20 && _surface_code <= 38)) || (_surface_code == 2 || (_surface_code >= 50 && _surface_code <= 57)));
}

FGTaxiway::FGTaxiway(PositionedID aGuid,
                     const string& aIdent,
                     const SGGeod& aGeod,
                     const double heading, const double length,
                     const double width,
                     const int surface_code,
                     const PositionedID airportId) : FGRunwayBase(aGuid, TAXIWAY, aIdent, aGeod, heading, length, width, surface_code, airportId)
{
}

/*
 * SPDX-FileCopyrightText: (C) 2000 Curtis L. Olson - http://www.flightgear.org/~curt
 * SPDX_FileComment: represent a runway or taxiway
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <simgear/compiler.h>

#include <simgear/math/sg_geodesy.hxx>

#include <Airports/airports_fwd.hxx>
#include <Navaids/positioned.hxx>

#include <string>

/**
 * @class The base class for runways and taxiways. At present, FGTaxiway is
 * a direct instantiation of this class.
 */
class FGRunwayBase : public FGPositioned
{    
public:
    FGRunwayBase(PositionedID aGuid, Type aTy, const std::string& aIdent,
                 const SGGeod& aGeod,
                 const double heading, const double length,
                 const double width,
                 const int surface_code,
                 const PositionedID airportId);

    /**
   * Retrieve a position on the extended centerline. Positive values
   * are in the direction of the runway heading, negative values are in the
   * opposited direction. 0.0 corresponds to the (non-displaced) threshold
   */
    SGGeod pointOnCenterline(double aOffset) const;
    SGGeod pointOffCenterline(double aOffset, double lateralOffset) const;

    double lengthFt() const
    {
        return _length * SG_METER_TO_FEET; }
  
  double lengthM() const
  { return _length; }
  
  double widthFt() const
  { return _width * SG_METER_TO_FEET; }
  
  double widthM() const
  { return _width; }
  
   /**
   * Runway heading in degrees.
   */
  double headingDeg() const
  { return _heading; }
  
  /**
   * Predicate to test if this runway has a hard surface. For the moment, this
   * means concrete or asphalt
   */
  bool isHardSurface() const;
  
  /**
   * Retrieve runway surface code, as define in Robin Peel's data
   */
  int surface() const 
  { return _surface_code; }

  /**
   * Retrieve runway surface name, as define in Robin Peel's data
   */
  static const char * surfaceName( int surface_code );
  const char * surfaceName() { return surfaceName( _surface_code ); }

  FGAirportRef airport() const;

  protected:
    
  double _heading;
  double _length;
  double _width;

  /** surface, as defined by:
   * http://www.x-plane.org/home/robinp/Apt810.htm#RwySfcCodes
   */
  int _surface_code;

  PositionedID mAirport;
};

// for the moment, taxiways are simply a concrete RunwayBase
class FGTaxiway : public FGRunwayBase
{
public:
    FGTaxiway(PositionedID aGuid,
              const std::string& aIdent,
              const SGGeod& aGeod,
              const double heading, const double length,
              const double width,
              const int surface_code,
              const PositionedID airportId);
};

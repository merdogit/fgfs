/*
 * SPDX-FileCopyrightText: (C) 2000 Curtis L. Olson - http://www.flightgear.org/~curt
 * SPDX_FileComment: a simple class to manage airport runway info
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <simgear/compiler.h>

#include "runwaybase.hxx"
#include <Navaids/procedure.hxx>

class FGRunway : public FGRunwayBase
{
    PositionedID _reciprocal;
    double _displ_thresh;
    double _stopway;
    PositionedID _ils;
public:
  static bool isType(FGPositioned::Type ty)
  { return (ty ==  FGPositioned::RUNWAY); }

  FGRunway(PositionedID aGuid,
           PositionedID aAirport, const std::string& rwy_no,
            const SGGeod& aGeod,
            const double heading, const double length,
            const double width,
            const double displ_thresh,
            const double stopway,
            const int surface_code);
  
  /**
   * given a runway identifier (06, 18L, 31R) compute the identifier for the
   * reciprocal heading runway (24, 36R, 13L) string.
   */
  static std::string reverseIdent(const std::string& aRunayIdent);
    
  /**
   * score this runway according to the specified weights. Used by
   * FGAirport::findBestRunwayForHeading
   */
  double score(double aLengthWt, double aWidthWt, double aSurfaceWt, double aIlsWt) const;

  /**
   * Get the runway beginning point - this is syntatic sugar, equivalent to
   * calling pointOnCenterline(0.0);
   */
  SGGeod begin() const;
  
  /**
   * Get the (possibly displaced) threshold point.
   */
  SGGeod threshold() const;
  
  /**
   * Retrieve a position on the extended centerline. Positive values
   * are in the direction of the runway heading, negative values are in the
   * opposited direction. 0.0 corresponds to the possibly threshold   
   */
  SGGeod pointOnCenterlineDisplaced(double aOffset) const;

  /**
   * Get the 'far' end - this is equivalent to calling
   * pointOnCenterline(lengthFt());
   */
  SGGeod end() const;
  
  double displacedThresholdM() const
  { return _displ_thresh; }
  
  double stopwayM() const
  { return _stopway; }


  FGNavRecord* ILS() const;
  
  /**
   * retrieve the associated glideslope transmitter, if one is defined.
   */
  FGNavRecord* glideslope() const;
  
  void setILS(PositionedID nav) { _ils = nav; }
  
  FGRunway* reciprocalRunway() const;
  
  void setReciprocalRunway(PositionedID other);
  
  /**
   * Get SIDs (DPs) associated with this runway
   */
  flightgear::SIDList getSIDs() const;
  
  /**
   * Get STARs associared with this runway
   */ 
  flightgear::STARList getSTARs() const;
  
  
  flightgear::ApproachList getApproaches
  (
    flightgear::ProcedureType type = flightgear::PROCEDURE_INVALID
  ) const;
  
  void updateThreshold(const SGGeod& newThreshold,
                       double newHeading,
                       double newDisplacedThreshold,
                       double newStopway);
};

class FGHelipad : public FGRunwayBase
{
public:
    static bool isType(FGPositioned::Type ty)
    { return (ty ==  FGPositioned::HELIPAD); }

    FGHelipad(PositionedID aGuid,
           PositionedID aAirport, const std::string& rwy_no,
            const SGGeod& aGeod,
            const double heading, const double length,
            const double width,
            const int surface_code);
};

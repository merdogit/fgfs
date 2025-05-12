// route.cxx - classes supporting waypoints and route structures

// Written by James Turner, started 2009.
//
// Copyright (C) 2009  Curtis L. Olson
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "config.h"

#include "route.hxx"

// std
#include <map>
#include <fstream>


// SimGear
#include <simgear/structure/exception.hxx>
#include <simgear/misc/sg_path.hxx>
#include <simgear/magvar/magvar.hxx>
#include <simgear/timing/sg_time.hxx>
#include <simgear/io/iostreams/sgstream.hxx>
#include <simgear/misc/strutils.hxx>
#include <simgear/props/props_io.hxx>

// FlightGear
#include <Main/globals.hxx>
#include "Main/fg_props.hxx"
#include <Navaids/procedure.hxx>
#include <Navaids/waypoint.hxx>
#include <Navaids/LevelDXML.hxx>
#include <Airports/airport.hxx>
#include <Navaids/airways.hxx>
#include <Environment/atmosphere.hxx> // for Mach conversions

using std::string;
using std::vector;
using std::endl;
using std::fstream;


namespace flightgear {

const double NO_MAG_VAR = -1000.0; // an impossible mag-var value

bool isMachRestrict(RouteRestriction rr)
{
  return (rr == SPEED_RESTRICT_MACH) || (rr == SPEED_COMPUTED_MACH);
}

namespace {

double magvarDegAt(const SGGeod& pos)
{
    double jd = globals->get_time_params()->getJD();
    return sgGetMagVar(pos, jd) * SG_RADIANS_TO_DEGREES;
}

WayptRef intersectionFromString(FGPositionedRef p1,
                                const SGGeod& basePosition,
                                const double magvar,
                                const string_list& pieces)
{
    assert(pieces.size() == 4);
    // navid/radial/navid/radial notation
    FGPositionedRef p2 = FGPositioned::findClosestWithIdent(pieces[2], basePosition);
    if (!p2) {
        SG_LOG(SG_NAVAID, SG_INFO, "Unable to find FGPositioned with ident:" << pieces[2]);
        return {};
    }

    double r1 = atof(pieces[1].c_str()),
           r2 = atof(pieces[3].c_str());
    r1 += magvar;
    r2 += magvar;

    SGGeod intersection;
    bool ok = SGGeodesy::radialIntersection(p1->geod(), r1, p2->geod(), r2, intersection);
    if (!ok) {
        SG_LOG(SG_NAVAID, SG_INFO, "no valid intersection for:" << pieces[0] << "/" << pieces[2]);
        return {};
    }

    std::string name = p1->ident() + "-" + p2->ident();
    return new BasicWaypt(intersection, name, nullptr);
}

WayptRef viaFromString(const SGGeod& basePosition, const std::string& target)
{
    assert(target.find("VIA-") == 0);
    string_list pieces(simgear::strutils::split(target.substr(4), "/"));
    if (pieces.size() != 2) {
        SG_LOG(SG_NAVAID, SG_WARN, "Malformed VIA specification string:" << target);
        return {};
    }

    // TO navaid is pieces[1]
    FGPositionedRef nav = FGPositioned::findClosestWithIdent(pieces[1], basePosition, nullptr);
    if (!nav) {
        SG_LOG(SG_NAVAID, SG_WARN, "TO navaid:" << pieces[3] << " unknown");
        return {};
    }

    // airway ident is pieces[1]
    AirwayRef airway = Airway::findByIdentAndNavaid(pieces[0], nav);
    if (!airway) {
        SG_LOG(SG_NAVAID, SG_WARN, "Unknown airway:" << pieces[0]);
        return {};
    }

    return new Via(nullptr, airway, nav);
}

static double convertSpeedToKnots(RouteUnits aUnits, double aAltitudeFt, double aValue)
{
    switch (aUnits) {
    case SPEED_KNOTS:   return aValue;
    case SPEED_KPH:     return aValue * SG_KMH_TO_MPS * SG_MPS_TO_KT;
    case SPEED_MACH:    return FGAtmo::knotsFromMachAtAltitudeFt(aValue, aAltitudeFt);
                
    default:
        throw sg_format_exception("Can't convert unit to Knots", "convertSpeedToKnots");
    }
}

static double convertSpeedFromKnots(RouteUnits aUnits, double aAltitudeFt, double aValue)
{
    if (aUnits == DEFAULT_UNITS) {
        // TODO : use KPH is simulator is in metric
        aUnits = SPEED_KNOTS;
    }
    
    switch (aUnits) {
    case SPEED_KNOTS:   return aValue;
    case SPEED_KPH:     return aValue * SG_KT_TO_MPS * SG_MPS_TO_KMH;
    case SPEED_MACH:    return FGAtmo::machFromKnotsAtAltitudeFt(aValue, aAltitudeFt);
                
    default:
        throw sg_format_exception("Can't convert to unit", "convertSpeedFromKnots");
    }
}

} // anonymous namespace

double convertSpeedUnits(RouteUnits aSrc, RouteUnits aDest, double aAltitudeFt, double aValue)
{
    const double valueKnots = convertSpeedToKnots(aSrc, aAltitudeFt, aValue);
    return convertSpeedFromKnots(aDest, aAltitudeFt, valueKnots);
}

double convertAltitudeUnits(RouteUnits aSrc, RouteUnits aDest, double aValue)
{
    if (aDest == DEFAULT_UNITS) {
        // TODO : use meters if sim is in metric
        aDest = ALTITUDE_FEET;
    }
    
    double altFt = 0.0;
    switch (aSrc) {
    case ALTITUDE_FEET: altFt = aValue; break;
    case ALTITUDE_METER: altFt = aValue * SG_METER_TO_FEET; break;
    case ALTITUDE_FLIGHTLEVEL: altFt = aValue * 100; break;
    default:
        throw sg_format_exception("Unsupported source altitude units", "convertAltitudeUnits");
    }
    
    switch (aDest) {
    case ALTITUDE_FEET: return altFt;
    case ALTITUDE_METER: return altFt * SG_FEET_TO_METER;
    case ALTITUDE_FLIGHTLEVEL: return round(altFt / 100);
    default:
        throw sg_format_exception("Unsupported destination altitude units", "convertAltitudeUnits");
    }
}

Waypt::Waypt(RouteBase* aOwner) :
  _owner(aOwner),
  _magVarDeg(NO_MAG_VAR)
{
}

Waypt::~Waypt()
{
}

std::string Waypt::ident() const
{
  return {};
}

bool Waypt::flag(WayptFlag aFlag) const
{
  return ((_flags & aFlag) != 0);
}

void Waypt::setFlag(WayptFlag aFlag, bool aV)
{
    if (aFlag == 0) {
        throw sg_range_exception("invalid waypoint flag set");
    }

  _flags = (_flags & ~aFlag);
  if (aV) _flags |= aFlag;
}

bool Waypt::matches(Waypt* aOther) const
{
  assert(aOther);
  if (ident() != aOther->ident()) { // cheap check first
    return false;
  }

  return matches(aOther->position());
}
    
bool Waypt::matches(FGPositioned* aPos) const
{
    if (!aPos)
        return false;
    
    // if w ehave no source, match on position and ident
    if (!source()) {
        return (ident() == aPos->ident()) && matches(aPos->geod());
    }
    
    return (aPos == source());
}

bool Waypt::matches(const SGGeod& aPos) const
{
  double d = SGGeodesy::distanceM(position(), aPos);
  return (d < 100.0); // 100 metres seems plenty
}

void Waypt::setAltitude(double aAlt, RouteRestriction aRestrict, RouteUnits aUnit)
{
    if (aUnit == DEFAULT_UNITS) {
        aUnit = ALTITUDE_FEET;
    }
    
  _altitude = aAlt;
    _altitudeUnits = aUnit;
  _altRestrict = aRestrict;
}

void Waypt::setConstraintAltitude(double aAlt)
{
    _constraintAltitude = aAlt;
}

void Waypt::setSpeed(double aSpeed, RouteRestriction aRestrict, RouteUnits aUnit)
{
    if (aUnit == DEFAULT_UNITS) {
        if ((aRestrict == SPEED_RESTRICT_MACH) || (aRestrict == SPEED_RESTRICT_MACH)) {
            aUnit = SPEED_MACH;
        } else {
            aUnit = SPEED_KNOTS;
        }
    }
    
  _speed = aSpeed;
  _speedUnits = aUnit;
  _speedRestrict = aRestrict;
}

double Waypt::speedKts() const
{
  return speed(SPEED_KNOTS);
}

double Waypt::speedMach() const
{
  return speed(SPEED_MACH);
}

double Waypt::altitudeFt() const
{
    return altitude(ALTITUDE_FEET);
}

double Waypt::speed(RouteUnits aUnits) const
{
    if (aUnits == _speedUnits) {
        return _speed;
    }
    
    return convertSpeedUnits(_speedUnits, aUnits, altitudeFt(), _speed);
}

double Waypt::altitude(RouteUnits aUnits) const
{
    if (aUnits == _altitudeUnits) {
        return _altitude;
    }
    
    return convertAltitudeUnits(_altitudeUnits, aUnits, _altitude);
}

double Waypt::constraintAltitude(RouteUnits aUnits) const
{
    if (!_constraintAltitude.has_value())
        return 0.0;
    
    if (aUnits == _altitudeUnits) {
        return _constraintAltitude.value_or(0.0);
    }
    
    return convertAltitudeUnits(_altitudeUnits, aUnits, _constraintAltitude.value_or(0.0));
}

double Waypt::magvarDeg() const
{
  if (_magVarDeg == NO_MAG_VAR) {
    // derived classes with a default pos must override this method
    assert(!(position() == SGGeod()));

    double jd = globals->get_time_params()->getJD();
    _magVarDeg = sgGetMagVar(position(), jd) * SG_RADIANS_TO_DEGREES;
  }

  return _magVarDeg;
}

double Waypt::headingRadialDeg() const
{
  return 0.0;
}

std::string Waypt::icaoDescription() const
{
    return ident();
}

///////////////////////////////////////////////////////////////////////////
// persistence

RouteRestriction restrictionFromString(const std::string& aStr)
{
  std::string l = simgear::strutils::lowercase(aStr);

  if (l == "at") return RESTRICT_AT;
  if (l == "above") return RESTRICT_ABOVE;
  if (l == "below") return RESTRICT_BELOW;
  if (l == "between") return RESTRICT_BETWEEN;
  if (l == "none") return RESTRICT_NONE;
  if (l == "mach") return SPEED_RESTRICT_MACH;

  if (l.empty()) return RESTRICT_NONE;
  throw sg_io_exception("unknown restriction specification:" + l,
    "Route restrictFromString");
}

const char* restrictionToString(RouteRestriction aRestrict)
{
  switch (aRestrict) {
  case RESTRICT_AT: return "at";
  case RESTRICT_BELOW: return "below";
  case RESTRICT_ABOVE: return "above";
  case RESTRICT_NONE: return "none";
  case RESTRICT_BETWEEN: return "between";
  case SPEED_RESTRICT_MACH: return "mach";

  default:
    throw sg_exception("invalid route restriction",
      "Route restrictToString");
  }
}

WayptRef Waypt::createInstance(RouteBase* aOwner, const std::string& aTypeName)
{
  WayptRef r;
  if (aTypeName == "basic") {
    r = new BasicWaypt(aOwner);
  } else if (aTypeName == "navaid") {
    r = new NavaidWaypoint(aOwner);
  } else if (aTypeName == "offset-navaid") {
    r = new OffsetNavaidWaypoint(aOwner);
  } else if (aTypeName == "hold") {
    r = new Hold(aOwner);
  } else if (aTypeName == "runway") {
    r = new RunwayWaypt(aOwner);
  } else if (aTypeName == "hdgToAlt") {
    r = new HeadingToAltitude(aOwner);
  } else if (aTypeName == "dmeIntercept") {
    r = new DMEIntercept(aOwner);
  } else if (aTypeName == "radialIntercept") {
    r = new RadialIntercept(aOwner);
  } else if (aTypeName == "vectors") {
    r = new ATCVectors(aOwner);
  } else if (aTypeName == "discontinuity") {
    r = new Discontinuity(aOwner);
  } else if (aTypeName == "via") {
      r = new Via(aOwner);
  }

  if (!r || (r->type() != aTypeName)) {
    throw sg_exception("broken factory method for type:" + aTypeName,
      "Waypt::createInstance");
  }

  return r;
}

WayptRef Waypt::createFromProperties(RouteBase* aOwner, SGPropertyNode_ptr aProp)
{
  if (!aProp->hasChild("type")) {
      SG_LOG(SG_GENERAL, SG_WARN, "Bad waypoint node: missing type");
      return {};
  }

  flightgear::AirwayRef via;
  if (aProp->hasChild("airway")) {
      auto level = Airway::Both;
      if (aProp->hasValue("network")) {
          level = static_cast<flightgear::Airway::Level>(aProp->getIntValue("network"));
      }
      
      via = flightgear::Airway::findByIdent(aProp->getStringValue("airway"), level);
      if (via) {
          // override owner if we are from an airway
          aOwner = via.get();
      }
  }

    WayptRef nd(createInstance(aOwner, aProp->getStringValue("type")));
    if (nd->initFromProperties(aProp)) {
        return nd;
    }
    SG_LOG(SG_GENERAL, SG_WARN, "failed to create waypoint, trying basic");


    // if we failed to make the waypoint, try again making a basic waypoint.
    // this handles the case where a navaid waypoint is missing, for example
    // we also reject navaids that don't look correct (too far form the specified
    // lat-lon, eg see https://sourceforge.net/p/flightgear/codetickets/1814/ )
    // and again fallback to here.
    WayptRef bw(new BasicWaypt(aOwner));
    if (bw->initFromProperties(aProp)) {
        return bw;
    }

    return {}; // total failure
}

WayptRef Waypt::fromLatLonString(RouteBase* aOwner, const std::string& target)
{
    // permit lat,lon/radial/offset format
    string_list pieces(simgear::strutils::split(target, "/"));
    if ((pieces.size() != 1) && (pieces.size() != 3)) {
        return {};
    }

    SGGeod g;
    const bool defaultToLonLat = true; // parseStringAsGeod would otherwise default to lat,lon
    if (!simgear::strutils::parseStringAsGeod(pieces[0], &g, defaultToLonLat)) {
        return {};
    }

    if (pieces.size() == 3) {
        // process offset
        const double bearing = std::stod(pieces[1]);
        const double distanceNm = std::stod(pieces[2]);
        g = SGGeodesy::direct(g, bearing, distanceNm * SG_NM_TO_METER);
    }

    // build a short name
    const int lonDeg = static_cast<int>(g.getLongitudeDeg());
    const int latDeg = static_cast<int>(g.getLatitudeDeg());

    char buf[32];
    char ew = (lonDeg < 0) ? 'W' : 'E';
    char ns = (latDeg < 0) ? 'S' : 'N';
    snprintf(buf, 32, "%c%03d%c%03d", ew, std::abs(lonDeg), ns, std::abs(latDeg));

    return new BasicWaypt(g, buf, aOwner);
}

WayptRef Waypt::createFromString(RouteBase* aOwner, const std::string& s, const SGGeod& aVicinity)
{
    auto vicinity = aVicinity;
    if (!vicinity.isValid()) {
        vicinity = globals->get_aircraft_position();
    }

    auto target = simgear::strutils::uppercase(s);
    // extract altitude
    double alt = 0.0;
    RouteRestriction altSetting = RESTRICT_NONE;
    RouteUnits altitudeUnits = ALTITUDE_FEET;
    
    size_t pos = target.find('@');
    if (pos != string::npos) {
        auto altStr = simgear::strutils::uppercase(target.substr(pos + 1));
        if (simgear::strutils::starts_with(altStr, "FL")) {
            altitudeUnits = ALTITUDE_FLIGHTLEVEL;
            altStr = altStr.substr(2); // trim leading 'FL'
        } else if (fgGetString("/sim/startup/units") == "meter") {
            altitudeUnits = ALTITUDE_METER;
        }
        
        alt = std::stof(altStr);
        target = target.substr(0, pos);
        altSetting = RESTRICT_AT;
    }

    // check for lon,lat
    WayptRef wpt = fromLatLonString(aOwner, target);

    const double magvar = magvarDegAt(vicinity);

    if (wpt) {
        // already handled in the lat/lon test above
    } else if (target.find("VIA-") == 0) {
        wpt = viaFromString(vicinity, target);
    } else {
        FGPositioned::TypeFilter filter({FGPositioned::Type::AIRPORT, FGPositioned::Type::HELIPORT,
                                         FGPositioned::Type::SEAPORT, FGPositioned::Type::NDB, FGPositioned::Type::VOR,
                                         FGPositioned::Type::FIX, FGPositioned::Type::WAYPOINT});
        string_list pieces(simgear::strutils::split(target, "/"));
        FGPositionedRef p = FGPositioned::findClosestWithIdent(pieces.front(), vicinity, &filter);
        if (!p) {
            SG_LOG(SG_NAVAID, SG_INFO, "Unable to find FGPositioned with ident:" << pieces.front());
            return {};
        }

        if (pieces.size() == 1) {
            wpt = new NavaidWaypoint(p, aOwner);
        } else if (pieces.size() == 3) {
            // navaid/radial/distance-nm notation
            double radial = atof(pieces[1].c_str()),
                   distanceNm = atof(pieces[2].c_str());
            radial += magvar;
            wpt = new OffsetNavaidWaypoint(p, aOwner, radial, distanceNm);
        } else if (pieces.size() == 2) {
            FGAirport* apt = dynamic_cast<FGAirport*>(p.ptr());
            if (!apt) {
                SG_LOG(SG_NAVAID, SG_INFO, "Waypoint is not an airport:" << pieces.front());
                return {};
            }

            if (!apt->hasRunwayWithIdent(pieces[1])) {
                SG_LOG(SG_NAVAID, SG_INFO, "No runway: " << pieces[1] << " at " << pieces[0]);
                return {};
            }

            FGRunway* runway = apt->getRunwayByIdent(pieces[1]);
            wpt = new NavaidWaypoint(runway, aOwner);
        } else if (pieces.size() == 4) {
            wpt = intersectionFromString(p, vicinity, magvar, pieces);
        }
    }

    if (!wpt) {
        SG_LOG(SG_NAVAID, SG_INFO, "Unable to parse waypoint:" << target);
        return {};
    }

    if (altSetting != RESTRICT_NONE) {
        wpt->setAltitude(alt, altSetting, altitudeUnits);
    }
    return wpt;
}


void Waypt::saveAsNode(SGPropertyNode* n) const
{
  n->setStringValue("type", type());
  writeToProperties(n);
}

bool Waypt::initFromProperties(SGPropertyNode_ptr aProp)
{
    if (aProp->hasChild("generated")) {
        setFlag(WPT_GENERATED, aProp->getBoolValue("generated"));
    }
    
    if (aProp->hasChild("overflight")) {
        setFlag(WPT_OVERFLIGHT, aProp->getBoolValue("overflight"));
    }
    
    if (aProp->hasChild("arrival")) {
        setFlag(WPT_ARRIVAL, aProp->getBoolValue("arrival"));
    }
    
    if (aProp->hasChild("approach")) {
        setFlag(WPT_APPROACH, aProp->getBoolValue("approach"));
    }
    
    if (aProp->hasChild("departure")) {
        setFlag(WPT_DEPARTURE, aProp->getBoolValue("departure"));
    }
    
    if (aProp->hasChild("miss")) {
        setFlag(WPT_MISS, aProp->getBoolValue("miss"));
    }
    
    if (aProp->hasChild("airway")) {
        setFlag(WPT_VIA, true);
    }
    
    if (aProp->hasChild("alt-restrict")) {
        _altRestrict = restrictionFromString(aProp->getStringValue("alt-restrict"));
        if (aProp->hasChild("altitude-ft")) {
            _altitude = aProp->getDoubleValue("altitude-ft");
            _altitudeUnits = ALTITUDE_FEET;
        } else if (aProp->hasChild("altitude-m")) {
            _altitude = aProp->getDoubleValue("altitude-m");
            _altitudeUnits = ALTITUDE_METER;
        } else if (aProp->hasChild("flight-level")) {
            _altitude = aProp->getIntValue("flight-level");
            _altitudeUnits = ALTITUDE_FLIGHTLEVEL;
        }
        
        if (aProp->hasChild("constraint-altitude")) {
            _constraintAltitude = aProp->getDoubleValue("constraint-altitude");
        }
  }

  if (aProp->hasChild("speed-restrict")) {
    _speedRestrict = restrictionFromString(aProp->getStringValue("speed-restrict"));
    RouteUnits units = SPEED_KNOTS;
      if (_speedRestrict == SPEED_RESTRICT_MACH) {
          units = SPEED_MACH;
      }
      
      if (aProp->hasChild("speed-mach")) {
          units = SPEED_MACH;
          _speed = aProp->getDoubleValue("speed-mach");
      } else if (aProp->hasChild("speed-kph")) {
        units = SPEED_KPH;
        _speed = aProp->getDoubleValue("speed-kph");
    } else {
        _speed = aProp->getDoubleValue("speed");
    }
    
      _speedUnits = units;
  }

  return true;
}

void Waypt::writeToProperties(SGPropertyNode_ptr aProp) const
{
  if (flag(WPT_OVERFLIGHT)) {
    aProp->setBoolValue("overflight", true);
  }

  if (flag(WPT_DEPARTURE)) {
    aProp->setBoolValue("departure", true);
  }

  if (flag(WPT_ARRIVAL)) {
    aProp->setBoolValue("arrival", true);
  }

  if (flag(WPT_APPROACH)) {
    aProp->setBoolValue("approach", true);
  }

  if (flag(WPT_VIA) && _owner) {
      flightgear::AirwayRef awy = (flightgear::Airway*) _owner;
      aProp->setStringValue("airway", awy->ident());
      aProp->setIntValue("network", awy->level());
  }

  if (flag(WPT_MISS)) {
    aProp->setBoolValue("miss", true);
  }

  if (flag(WPT_GENERATED)) {
    aProp->setBoolValue("generated", true);
  }

  if (_altRestrict != RESTRICT_NONE) {
    aProp->setStringValue("alt-restrict", restrictionToString(_altRestrict));
      if (_altitudeUnits == ALTITUDE_METER) {
          aProp->setDoubleValue("altitude-m", _altitude);
      } else if (_altitudeUnits == ALTITUDE_FLIGHTLEVEL) {
          aProp->setDoubleValue("flight-level", _altitude);

      } else {
          aProp->setDoubleValue("altitude-ft", _altitude);
      }
  }
    
    if (_constraintAltitude.has_value()) {
        aProp->setDoubleValue("constraint-altitude", _constraintAltitude.value_or(0.0));
    }

  if (_speedRestrict != RESTRICT_NONE) {
    aProp->setStringValue("speed-restrict", restrictionToString(_speedRestrict));
    aProp->setDoubleValue("speed", _speed);
  }
}

RouteBase::~RouteBase()
{
}
    
void RouteBase::dumpRouteToKML(const WayptVec& aRoute, const std::string& aName)
{
  SGPath p = SGPath::desktop() / (aName + ".kml");
  sg_ofstream f(p);
  if (!f.is_open()) {
    SG_LOG(SG_NAVAID, SG_WARN, "unable to open:" << p);
    return;
  }

// pre-amble
  f << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n"
    "<Document>\n";

  dumpRouteToKMLLineString(aName, aRoute, f);

// post-amble
  f << "</Document>\n"
    "</kml>" << endl;
  f.close();
}

void RouteBase::dumpRouteToKMLLineString(const std::string& aIdent,
  const WayptVec& aRoute, std::ostream& aStream)
{
  // preamble
  aStream << "<Placemark>\n";
  aStream << "<name>" << aIdent << "</name>\n";
  aStream << "<LineString>\n";
  aStream << "<tessellate>1</tessellate>\n";
  aStream << "<coordinates>\n";

  // waypoints
  for (unsigned int i=0; i<aRoute.size(); ++i) {
    SGGeod pos = aRoute[i]->position();
    aStream << pos.getLongitudeDeg() << "," << pos.getLatitudeDeg() << " " << endl;
  }

  // postable
  aStream << "</coordinates>\n"
    "</LineString>\n"
    "</Placemark>\n" << endl;
}

void RouteBase::loadAirportProcedures(const SGPath& aPath, FGAirport* aApt)
{
  assert(aApt);
  try {
    NavdataVisitor visitor(aApt, aPath);
      readXML(aPath, visitor);
  } catch (sg_io_exception& ex) {
    SG_LOG(SG_NAVAID, SG_WARN, "failure parsing procedures: " << aPath <<
      "\n\t" << ex.getMessage() << "\n\tat:" << ex.getLocation().asString());
  } catch (sg_exception& ex) {
    SG_LOG(SG_NAVAID, SG_WARN, "failure parsing procedures: " << aPath <<
      "\n\t" << ex.getMessage());
  }
}

} // of namespace flightgear

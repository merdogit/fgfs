/*
 * SPDX-FileCopyrightText: (C) 2008 James Turner <james@flightgear.org>
 * SPDX_FileComment: base class for objects which are spatially located in the simulated world
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include "positioned.hxx"

#include <map>
#include <set>
#include <cstring> // strcmp
#include <algorithm> // for sort
#include <queue>
#include <memory>


#include <simgear/timing/timestamp.hxx>
#include <simgear/debug/logstream.hxx>
#include <simgear/structure/exception.hxx>
#include <simgear/math/SGGeometry.hxx>
#include <simgear/sg_inlines.h>

#include "Navaids/PositionedOctree.hxx"

using std::string;
using namespace flightgear;

static void validateSGGeod(const SGGeod& geod)
{
  if (SGMisc<double>::isNaN(geod.getLatitudeDeg()) ||
      SGMisc<double>::isNaN(geod.getLongitudeDeg()))
  {
    throw sg_range_exception("position is invalid, NaNs");
  }
}

static bool validateFilter(FGPositioned::Filter* filter)
{
    if (filter->maxType() < filter->minType()) {
        SG_LOG(SG_GENERAL, SG_WARN, "invalid positioned filter specified");
        return false;
    }
    
    return true;
}

///////////////////////////////////////////////////////////////////////////////

FGPositioned::FGPositioned( PositionedID aGuid,
                            Type ty,
                            const std::string& aIdent,
                            const SGGeod& aPos ):
  mGuid(aGuid),
  mType(ty),
  mIdent(aIdent),
  mPosition(aPos),
  mCart(SGVec3d::fromGeod(mPosition))
{

}

FGPositioned::~FGPositioned()
{
}

// Static method
bool FGPositioned::isAirportType(FGPositioned* pos)
{
    if (!pos) {
      return false;
    }

    return (pos->type() >= AIRPORT) && (pos->type() <= SEAPORT);
}

// Static method
bool FGPositioned::isRunwayType(FGPositioned* pos)
{
  return (pos && pos->type() == RUNWAY);
}

// Static method
bool FGPositioned::isNavaidType(FGPositioned* pos)
{
  if (!pos) {
    return false;
  }

  switch (pos->type()) {
  case NDB:
  case VOR:
  case ILS:
  case LOC:
  case GS:
  case DME:
  case TACAN:
    return true;
  default:
    return false;
  }
}

static bool isValidCustomWaypointType(FGPositioned::Type ty)
{
    switch (ty) {
    case FGPositioned::WAYPOINT:
    case FGPositioned::FIX:
    case FGPositioned::VISUAL_REPORTING_POINT:
    case FGPositioned::OBSTACLE:
        return true;
    default:
        return false;
    }
}

FGPositionedRef
FGPositioned::createWaypoint(FGPositioned::Type aType, const std::string& aIdent, const SGGeod& aPos,
                             bool isTemporary,
                             const std::string& aName)
{
  NavDataCache* cache = NavDataCache::instance();

  if (!isValidCustomWaypointType(aType)) {
      throw std::logic_error(std::string{"Create waypoint: not allowed for type:"} + nameForType(aType));
  }

  TypeFilter filter(aType);

  FGPositionedRef existing = cache->findClosestWithIdent(aIdent, aPos, &filter);
  if (existing) {
      auto distanceNm = SGGeodesy::distanceNm(existing->geod(), aPos);
      if (distanceNm < 100) {
          SG_LOG(SG_NAVAID, SG_WARN, "attempt to insert duplicate waypoint:" << aIdent << " within 100nm of existing waypoint with same ident");
          return existing;
      }
  }

  const PositionedID id = cache->createPOI(aType, aIdent, aPos, aName, isTemporary);
  return cache->loadById(id);
}

bool FGPositioned::deleteWaypoint(FGPositionedRef ref)
{
  NavDataCache* cache = NavDataCache::instance();
  const auto ty = ref->type();
  if (!POI::isType(ty) && (ty != FGPositioned::FIX)) {
      SG_LOG(SG_NAVAID, SG_DEV_WARN, "attempt to remove non-POI waypoint:" << ref->ident());
      return false;
  }

  return cache->removePOI(ref);
}


const SGVec3d&
FGPositioned::cart() const
{
  return mCart;
}

FGPositioned::Type FGPositioned::typeFromName(const std::string& aName)
{
  if (aName.empty() || (aName == "")) {
    return INVALID;
  }

  typedef struct {
    const char* _name;
    Type _ty;
  } NameTypeEntry;

  const NameTypeEntry names[] = {
      {"airport", AIRPORT},
      {"heliport", HELIPORT},
      {"seaport", SEAPORT},
      {"vor", VOR},
      {"loc", LOC},
      {"ils", ILS},
      {"gs", GS},
      {"ndb", NDB},
      {"wpt", WAYPOINT},
      {"fix", FIX},
      {"tacan", TACAN},
      {"dme", DME},
      {"atis", FREQ_ATIS},
      {"awos", FREQ_AWOS},
      {"tower", FREQ_TOWER},
      {"ground", FREQ_GROUND},
      {"approach", FREQ_APP_DEP},
      {"departure", FREQ_APP_DEP},
      {"clearance", FREQ_CLEARANCE},
      {"unicom", FREQ_UNICOM},
      {"runway", RUNWAY},
      {"helipad", HELIPAD},
      {"country", COUNTRY},
      {"city", CITY},
      {"town", TOWN},
      {"village", VILLAGE},
      {"taxiway", TAXIWAY},
      {"pavement", PAVEMENT},
      {"om", OM},
      {"mm", MM},
      {"im", IM},
      {"mobile-tacan", MOBILE_TACAN},
      {"obstacle", OBSTACLE},
      {"parking", PARKING},
      {"taxi-node", TAXI_NODE},
      {"visual-reporting-point", VISUAL_REPORTING_POINT},

      // aliases
      {"localizer", LOC},
      {"gnd", FREQ_GROUND},
      {"twr", FREQ_TOWER},
      {"waypoint", WAYPOINT},
      {"apt", AIRPORT},
      {"arpt", AIRPORT},
      {"rwy", RUNWAY},
      {"any", INVALID},
      {"all", INVALID},
      {"outer-marker", OM},
      {"middle-marker", MM},
      {"inner-marker", IM},
      {"parking-stand", PARKING},
      {"vrp", VISUAL_REPORTING_POINT},

      {NULL, INVALID}};

  std::string lowerName = simgear::strutils::lowercase(aName);
  
  for (const NameTypeEntry* n = names; (n->_name != NULL); ++n) {
    if (::strcmp(n->_name, lowerName.c_str()) == 0) {
      return n->_ty;
    }
  }
  
  SG_LOG(SG_NAVAID, SG_WARN, "FGPositioned::typeFromName: couldn't match:" << aName);
  return INVALID;
}

const char* FGPositioned::nameForType(Type aTy)
{
 switch (aTy) {
 case RUNWAY: return "runway";
 case HELIPAD: return "helipad";
 case TAXIWAY: return "taxiway";
 case PAVEMENT: return "pavement";
 case PARKING: return "parking stand";
 case FIX: return "fix";
 case VOR: return "VOR";
 case NDB: return "NDB";
 case ILS: return "ILS";
 case LOC: return "localizer";
 case GS: return "glideslope";
 case OM: return "outer-marker";
 case MM: return "middle-marker";
 case IM: return "inner-marker";
 case AIRPORT: return "airport";
 case HELIPORT: return "heliport";
 case SEAPORT: return "seaport";
 case WAYPOINT: return "waypoint";
 case DME: return "dme";
 case TACAN: return "tacan";
 case FREQ_TOWER: return "tower";
 case FREQ_ATIS: return "atis";
 case FREQ_AWOS: return "awos";
 case FREQ_GROUND: return "ground";
 case FREQ_CLEARANCE: return "clearance";
 case FREQ_UNICOM: return "unicom";
 case FREQ_APP_DEP: return "approach-departure";
 case TAXI_NODE: return "taxi-node";
 case COUNTRY: return "country";
 case CITY: return "city";
 case TOWN: return "town";
 case VILLAGE: return "village";
 case VISUAL_REPORTING_POINT: return "visual-reporting-point";
 case MOBILE_TACAN: return "mobile-tacan";
 case OBSTACLE: return "obstacle";
 default:
  return "unknown";
 }
}

///////////////////////////////////////////////////////////////////////////////
// search / query functions

FGPositionedRef
FGPositioned::findClosestWithIdent(const std::string& aIdent, const SGGeod& aPos, Filter* aFilter)
{
  validateSGGeod(aPos);  
  return NavDataCache::instance()->findClosestWithIdent(aIdent, aPos, aFilter);
}

FGPositionedRef
FGPositioned::findFirstWithIdent(const std::string& aIdent, Filter* aFilter)
{
  if (aIdent.empty()) {
    return NULL;
  }
  
  FGPositionedList r =
    NavDataCache::instance()->findAllWithIdent(aIdent, aFilter, true);
  if (r.empty()) {
    return NULL;
  }
  
  return r.front();
}

FGPositionedList
FGPositioned::findWithinRange(const SGGeod& aPos, double aRangeNm, Filter* aFilter)
{
  validateSGGeod(aPos);

    if (!validateFilter(aFilter)) {
        return FGPositionedList();
    }
    
  FGPositionedList result;
  Octree::findAllWithinRange(SGVec3d::fromGeod(aPos), 
    aRangeNm * SG_NM_TO_METER, aFilter, result, 0xffffff);
  return result;
}

FGPositionedList
FGPositioned::findWithinRangePartial(const SGGeod& aPos, double aRangeNm, Filter* aFilter, bool& aPartial)
{
  validateSGGeod(aPos);
  
    if (!validateFilter(aFilter)) {
        return FGPositionedList();
    }
    
  int limitMsec = 32;
  FGPositionedList result;
  aPartial = Octree::findAllWithinRange(SGVec3d::fromGeod(aPos),
                             aRangeNm * SG_NM_TO_METER, aFilter, result,
                                        limitMsec);
  return result;
}

FGPositionedList
FGPositioned::findAllWithIdent(const std::string& aIdent, Filter* aFilter, bool aExact)
{
    if (!validateFilter(aFilter)) {
        return FGPositionedList();
    }
    
  return NavDataCache::instance()->findAllWithIdent(aIdent, aFilter, aExact);
}

FGPositionedList
FGPositioned::findAllWithName(const std::string& aName, Filter* aFilter, bool aExact)
{
    if (!validateFilter(aFilter)) {
        return FGPositionedList();
    }
    
  return NavDataCache::instance()->findAllWithName(aName, aFilter, aExact);
}

FGPositionedRef
FGPositioned::findClosest(const SGGeod& aPos, double aCutoffNm, Filter* aFilter)
{
  validateSGGeod(aPos);
  
    if (!validateFilter(aFilter)) {
        return NULL;
    }
    
  FGPositionedList l(findClosestN(aPos, 1, aCutoffNm, aFilter));
  if (l.empty()) {
    return NULL;
  }

  assert(l.size() == 1);
  return l.front();
}

FGPositionedList
FGPositioned::findClosestN(const SGGeod& aPos, unsigned int aN, double aCutoffNm, Filter* aFilter)
{
  validateSGGeod(aPos);
  
  FGPositionedList result;
  int limitMsec = 0xffff;
  Octree::findNearestN(SGVec3d::fromGeod(aPos), aN, aCutoffNm * SG_NM_TO_METER, aFilter, result, limitMsec);
  return result;
}

FGPositionedList
FGPositioned::findClosestNPartial(const SGGeod& aPos, unsigned int aN, double aCutoffNm, Filter* aFilter, bool &aPartial)
{
    validateSGGeod(aPos);
    
    FGPositionedList result;
    int limitMsec = 32;
    aPartial = Octree::findNearestN(SGVec3d::fromGeod(aPos), aN, aCutoffNm * SG_NM_TO_METER, aFilter, result,
                        limitMsec);
    return result;
}

void
FGPositioned::sortByRange(FGPositionedList& aResult, const SGGeod& aPos)
{
  validateSGGeod(aPos);
  
  SGVec3d cartPos(SGVec3d::fromGeod(aPos));
// computer ordering values
  Octree::FindNearestResults r;
  FGPositionedList::iterator it = aResult.begin(), lend = aResult.end();
  for (; it != lend; ++it) {
    double d2 = distSqr((*it)->cart(), cartPos);
    r.push_back(Octree::OrderedPositioned(*it, d2));
  }
  
// sort
  std::sort(r.begin(), r.end());
  
// convert to a plain list
  unsigned int count = aResult.size();
  for (unsigned int i=0; i<count; ++i) {
    aResult[i] = r[i].get();
  }
}

//------------------------------------------------------------------------------
void FGPositioned::modifyPosition(const SGGeod& newPos)
{
  const_cast<SGGeod&>(mPosition) = newPos;
  const_cast<SGVec3d&>(mCart) = SGVec3d::fromGeod(newPos);
}

//------------------------------------------------------------------------------
void FGPositioned::invalidatePosition()
{
  const_cast<SGGeod&>(mPosition) = SGGeod::fromDeg(999,999);
  const_cast<SGVec3d&>(mCart) = SGVec3d::zeros();
}

//------------------------------------------------------------------------------
FGPositionedRef FGPositioned::loadByIdImpl(PositionedID id)
{
  return flightgear::NavDataCache::instance()->loadById(id);
}

FGPositioned::TypeFilter::TypeFilter(Type aTy)
{
  addType(aTy);
}

FGPositioned::TypeFilter::TypeFilter(std::initializer_list<Type> types)
{
    for (auto t : types) {
        addType(t);
    }
}

FGPositioned::TypeFilter::TypeFilter(Type aMinType, Type aMaxType)
{
    for (int t = aMinType; t <= aMaxType; t++) {
        addType(static_cast<FGPositioned::Type>(t));
    }
}


void FGPositioned::TypeFilter::addType(Type aTy)
{
  if (aTy == INVALID) {
    return;
  }
  
  types.push_back(aTy);
  mMinType = std::min(mMinType, aTy);
  mMaxType = std::max(mMaxType, aTy);
}

FGPositioned::TypeFilter
FGPositioned::TypeFilter::fromString(const std::string& aFilterSpec)
{
  if (aFilterSpec.empty()) {
    throw sg_format_exception("empty filter spec:", aFilterSpec);
  }
  
  string_list parts = simgear::strutils::split(aFilterSpec, ",");
  TypeFilter f;
  
  for (std::string token : parts) {
    f.addType(typeFromName(token));
  }
  
  return f;
}

bool
FGPositioned::TypeFilter::pass(FGPositioned* aPos) const
{
  if (types.empty()) {
    return true;
  }
  
    std::vector<Type>::const_iterator it = types.begin(),
        end = types.end();
    for (; it != end; ++it) {
        if (aPos->type() == *it) {
            return true;
        }
    }
    
    return false;
}

POI::POI(PositionedID aGuid, Type ty, const std::string& aIdent, const SGGeod& aPos, const std::string& aName) : FGPositioned(aGuid, ty, aIdent, aPos),
                                                                                                                 mName(aName)
{
}

bool POI::isType(FGPositioned::Type ty)
{
    return (ty == FGPositioned::WAYPOINT) || (ty == FGPositioned::OBSTACLE) ||
           ((ty >= FGPositioned::COUNTRY) && (ty < FGPositioned::LAST_POI_TYPE));
}

// fg_props.cxx -- support for FlightGear properties.
//
// Written by David Megginson, started 2000.
//
// Copyright (C) 2000, 2001 David Megginson - david@megginson.com
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
//
// $Id$

#include "config.h"

#include <simgear/compiler.h>
#include <simgear/structure/exception.hxx>
#include <simgear/props/props_io.hxx>
#include <simgear/misc/strutils.hxx>
#include <simgear/timing/sg_time.hxx>
#include <simgear/misc/sg_path.hxx>
#include <simgear/scene/model/particles.hxx>
#include <simgear/sound/soundmgr.hxx>

#include <GUI/gui.h>

#include "globals.hxx"
#include "fg_props.hxx"

using std::string;
////////////////////////////////////////////////////////////////////////
// Default property bindings (not yet handled by any module).
////////////////////////////////////////////////////////////////////////

/**
 * Get the logging classes.
 */
// XXX Making the result buffer be global is a band-aid that hopefully
// delays its destruction 'til after its last use.
namespace
{
string loggingResult;
}

static const char *
getLoggingClasses ()
{
    loggingResult = sglog().getLogClassesAsString();
    return loggingResult.c_str();
}

static void
addLoggingClass (const string &name)
{
    sglog().addLogClass(name);
}


/**
 * Set the logging classes.
 */
void
setLoggingClasses (const char * c)
{
    sglog().parseLogClasses(c);
}

/**
 * Get the logging priority.
 */
static const char *
getLoggingPriority ()
{
  switch (sglog().get_log_priority()) {
  case SG_BULK:
    return "bulk";
  case SG_DEBUG:
    return "debug";
  case SG_INFO:
    return "info";
  case SG_WARN:
    return "warn";
  case SG_ALERT:
  case SG_POPUP:
    return "alert";
  default:
    SG_LOG(SG_GENERAL, SG_WARN, "Internal: Unknown logging priority number: "
	   << sglog().get_log_priority());
    return "unknown";
  }
}


/**
 * Set the logging priority.
 */
void
setLoggingPriority (const char * p)
{
  if (p == 0)
      return;

  string priority = p;
  if (priority.empty()) {
      sglog().set_log_priority(SG_INFO);
  } else {
      try {
          sglog().set_log_priority(logstream::priorityFromString(priority));
      } catch (std::exception& e) {
          SG_LOG(SG_GENERAL, SG_WARN, "Unknown logging priority: " << priority);
      }
  }

  SG_LOG(SG_GENERAL, SG_DEBUG, "Logging priority is " << getLoggingPriority());
}


/**
 * Return the current frozen state.
 */
bool FGProperties::getFreeze() const
{
  return _simFreeze;
}


/**
 * Set the current frozen state.
 */
void FGProperties::setFreeze(bool f)
{
  if (_simFreeze == f)
      return;

  _simFreeze = f;

  // Pause the particle system
  auto p = simgear::ParticlesGlobalManager::instance();
  p->setFrozen(f);

  _simFreezeNode->fireValueChanged();
}


/**
 * Return the number of milliseconds elapsed since simulation started.
 */
static double
getElapsedTime_sec ()
{
  return globals->get_sim_time_sec();
}


/**
 * Return the current Zulu time.
 */
static const char *
getDateString ()
{
  static char buf[64];		// FIXME
  
  SGTime * st = globals->get_time_params();
  if (!st) {
    buf[0] = 0;
    return buf;
  }
  
  struct tm * t = st->getGmt();
  snprintf(buf, 64, "%.4d-%.2d-%.2dT%.2d:%.2d:%.2d",
           t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
           t->tm_hour, t->tm_min, t->tm_sec);
  return buf;
}


/**
 * Set the current Zulu time.
 */
static void
setDateString (const char * date_string)
{
  SGTime * st = globals->get_time_params();
  struct tm * current_time = st->getGmt();
  struct tm new_time;

				// Scan for basic ISO format
				// YYYY-MM-DDTHH:MM:SS
  new_time.tm_isdst = 0;
  int ret = sscanf(date_string, "%d-%d-%dT%d:%d:%d",
		   &(new_time.tm_year), &(new_time.tm_mon),
		   &(new_time.tm_mday), &(new_time.tm_hour),
		   &(new_time.tm_min), &(new_time.tm_sec));

				// Be pretty picky about this, so
				// that strange things don't happen
				// if the save file has been edited
				// by hand.
  if (ret != 6) {
    SG_LOG(SG_INPUT, SG_WARN, "Date/time string " << date_string
	   << " not in YYYY-MM-DDTHH:MM:SS format; skipped");
    return;
  }
				// OK, it looks like we got six
				// values, one way or another.
  new_time.tm_year -= 1900;
  new_time.tm_mon -= 1;
				// Now, tell flight gear to use
				// the new time.  This was far
				// too difficult, by the way.
  long int warp =
    mktime(&new_time) - mktime(current_time) + globals->get_warp();
    
  fgSetInt("/sim/time/warp", warp);
}

/**
 * Return the GMT as a string.
 */
static const char *
getGMTString ()
{
  static char buf[16];
  SGTime * st = globals->get_time_params();
  if (!st) {
    buf[0] = 0;
    return buf;
  }
  
  struct tm *t = st->getGmt();
  snprintf(buf, 16, "%.2d:%.2d:%.2d",
      t->tm_hour, t->tm_min, t->tm_sec);
  return buf;
}

////////////////////////////////////////////////////////////////////////
// Tie the properties.
////////////////////////////////////////////////////////////////////////
SGConstPropertyNode_ptr FGProperties::_longDeg;
SGConstPropertyNode_ptr FGProperties::_latDeg;
SGConstPropertyNode_ptr FGProperties::_lonLatformat;

using namespace simgear;

const char* FGProperties::getLongitudeString ()
{
  const double d = _longDeg->getDoubleValue();
  auto format = static_cast<strutils::LatLonFormat>(_lonLatformat->getIntValue());
  const char c = d < 0.0 ? 'W' : 'E';
  
  static char longitudeBuffer[64];
  const auto s = strutils::formatLatLonValueAsString(d, format, c);
  memcpy(longitudeBuffer, s.c_str(), s.size() + 1);
  return longitudeBuffer;
}

const char* FGProperties::getLatitudeString ()
{
  const double d = _latDeg->getDoubleValue();
  auto format = static_cast<strutils::LatLonFormat>(_lonLatformat->getIntValue());
  const char c = d < 0.0 ? 'S' : 'N';
  
  static char latitudeBuffer[64];
  const auto s = strutils::formatLatLonValueAsString(d, format, c);
  memcpy(latitudeBuffer, s.c_str(), s.size() + 1);
  return latitudeBuffer;
}


FGProperties::FGProperties ()
{
}

FGProperties::~FGProperties ()
{
}

void
FGProperties::init ()
{
  memset(&_lastUtc, 0, sizeof(struct tm));
  memset(&_lastRealTime, 0, sizeof(struct tm));
  _simFreeze = false;
}

static SGPropertyNode_ptr initDoubleNode(const std::string& path, const double v)
{
    auto r = fgGetNode(path, true);
    if (r->getType() == simgear::props::NONE) {
        r->setDoubleValue(v);
    }
    return r;
}

void
FGProperties::bind ()
{
  _longDeg      = fgGetNode("/position/longitude-deg", true);
  _latDeg       = fgGetNode("/position/latitude-deg", true);
  _lonLatformat = fgGetNode("/sim/lon-lat-format", true);

  _offset = fgGetNode("/sim/time/local-offset", true);

  // utc date/time
  _uyear = fgGetNode("/sim/time/utc/year", true);
  _umonth = fgGetNode("/sim/time/utc/month", true);
  _uday = fgGetNode("/sim/time/utc/day", true);
  _uhour = fgGetNode("/sim/time/utc/hour", true);
  _umin = fgGetNode("/sim/time/utc/minute", true);
  _usec = fgGetNode("/sim/time/utc/second", true);
  _uwday = fgGetNode("/sim/time/utc/weekday", true);
  _udsec = fgGetNode("/sim/time/utc/day-seconds", true);

  // real local date/time
  _ryear = fgGetNode("/sim/time/real/year", true);
  _rmonth = fgGetNode("/sim/time/real/month", true);
  _rday = fgGetNode("/sim/time/real/day", true);
  _rhour = fgGetNode("/sim/time/real/hour", true);
  _rmin = fgGetNode("/sim/time/real/minute", true);
  _rsec = fgGetNode("/sim/time/real/second", true);
  _rwday = fgGetNode("/sim/time/real/weekday", true);

  _tiedProperties.setRoot(globals->get_props());

  // Simulation
  _tiedProperties.Tie("/sim/logging/priority", getLoggingPriority, setLoggingPriority);
  _tiedProperties.Tie("/sim/logging/classes", getLoggingClasses, setLoggingClasses);
  _simFreezeNode = fgGetNode("/sim/freeze/master", true);
  _tiedProperties.Tie(_simFreezeNode, this, &FGProperties::getFreeze, &FGProperties::setFreeze);
  _simFreezeNode->setAttribute(SGPropertyNode::LISTENER_SAFE, true);

  _tiedProperties.Tie<double>("/sim/time/elapsed-sec", getElapsedTime_sec);
  _timeGmtNode = _tiedProperties.Tie("/sim/time/gmt", getDateString, setDateString);
  _timeGmtNode->setAttribute(SGPropertyNode::LISTENER_SAFE, true);
  fgSetArchivable("/sim/time/gmt");
  _timeGmtStringNode = _tiedProperties.Tie<const char*>("/sim/time/gmt-string", getGMTString);
  _timeGmtStringNode->setAttribute(SGPropertyNode::LISTENER_SAFE, true);
  // Position
  _tiedProperties.Tie<const char*>("/position/latitude-string", getLatitudeString);
  _tiedProperties.Tie<const char*>("/position/longitude-string", getLongitudeString);

    _headingMagnetic = initDoubleNode("/orientation/heading-magnetic-deg", 0.0);
    _trackMagnetic = initDoubleNode("/orientation/track-magnetic-deg", 0.0);
    _magVar = initDoubleNode("/environment/magnetic-variation-deg", 0.0);
    _trueHeading = initDoubleNode("/orientation/heading-deg", 0.0);
    _trueTrack = initDoubleNode("/orientation/track-deg", 0.0);
}

void
FGProperties::unbind ()
{
    _tiedProperties.Untie();

    // drop static references to properties
    _longDeg = 0;
    _latDeg = 0;
    _lonLatformat = 0;

    _timeGmtNode.clear();
    _timeGmtStringNode.clear();
    _simFreezeNode.clear();
}

void
FGProperties::update (double dt)
{
    _offset->setIntValue(globals->get_time_params()->get_local_offset());

    // utc date/time
    struct tm *u = globals->get_time_params()->getGmt();
    if (memcmp(u, &_lastUtc, sizeof(struct tm)) != 0) {
        _lastUtc = *u;
        _uyear->setIntValue(u->tm_year + 1900);
        _umonth->setIntValue(u->tm_mon + 1);
        _uday->setIntValue(u->tm_mday);
        _uhour->setIntValue(u->tm_hour);
        _umin->setIntValue(u->tm_min);
        _usec->setIntValue(u->tm_sec);
        _uwday->setIntValue(u->tm_wday);
        _udsec->setIntValue(u->tm_hour * 3600 + u->tm_min * 60 + u->tm_sec);

        _timeGmtNode->fireValueChanged();
        _timeGmtStringNode->fireValueChanged();
    }


    // real local date/time
    time_t real = time(0);
    struct tm *r = localtime(&real);
    if (memcmp(r, &_lastRealTime, sizeof(struct tm)) != 0) {
        _lastRealTime = *r;
        _ryear->setIntValue(r->tm_year + 1900);
        _rmonth->setIntValue(r->tm_mon + 1);
        _rday->setIntValue(r->tm_mday);
        _rhour->setIntValue(r->tm_hour);
        _rmin->setIntValue(r->tm_min);
        _rsec->setIntValue(r->tm_sec);
        _rwday->setIntValue(r->tm_wday);
    }

    const double magvar = _magVar->getDoubleValue();
    const auto hdgMag = SGMiscd::normalizePeriodic(0, 360.0, _trueHeading->getDoubleValue() - magvar);
    _headingMagnetic->setDoubleValue(hdgMag);
    
    const auto trackMag = SGMiscd::normalizePeriodic(0, 360.0, _trueTrack->getDoubleValue() - magvar);
    _trackMagnetic->setDoubleValue(trackMag);
}


// Register the subsystem.
SGSubsystemMgr::Registrant<FGProperties> registrantFGProperties;

////////////////////////////////////////////////////////////////////////
// Save and restore.
////////////////////////////////////////////////////////////////////////


/**
 * Save the current state of the simulator to a stream.
 */
bool
fgSaveFlight (std::ostream &output, bool write_all)
{

  fgSetBool("/sim/presets/onground", false);
  fgSetArchivable("/sim/presets/onground");
  fgSetBool("/sim/presets/trim", false);
  fgSetArchivable("/sim/presets/trim");
  fgSetString("/sim/presets/speed-set", "UVW");
  fgSetArchivable("/sim/presets/speed-set");

  try {
    writeProperties(output, globals->get_props(), write_all);
  } catch (const sg_exception &e) {
    guiErrorMessage("Error saving flight: ", e);
    return false;
  }
  return true;
}


/**
 * Restore the current state of the simulator from a stream.
 */
bool
fgLoadFlight (std::istream &input)
{
  SGPropertyNode props;
  try {
    readProperties(input, &props);
  } catch (const sg_exception &e) {
    guiErrorMessage("Error reading saved flight: ", e);
    return false;
  }

  fgSetBool("/sim/presets/onground", false);
  fgSetBool("/sim/presets/trim", false);
  fgSetString("/sim/presets/speed-set", "UVW");

  copyProperties(&props, globals->get_props());

  return true;
}


bool
fgLoadProps (const std::string& path, SGPropertyNode * props, bool in_fg_root, int default_mode)
{
    SGPath fullpath;
    if (in_fg_root) {
        SGPath loadpath(globals->get_fg_root());
        loadpath.append(path);
        fullpath = loadpath;
    } else {
        fullpath = SGPath::fromUtf8(path);
    }

    try {
        readProperties(fullpath, props, default_mode);
    } catch (const sg_exception &e) {
        guiErrorMessage("Error reading properties: ", e);
        return false;
    }
    return true;
}


////////////////////////////////////////////////////////////////////////
// Property convenience functions.
////////////////////////////////////////////////////////////////////////

SGPropertyNode *
fgGetNode (const char * path, bool create)
{
  return globals->get_props()->getNode(path, create);
}

SGPropertyNode * 
fgGetNode (const char * path, int index, bool create)
{
  return globals->get_props()->getNode(path, index, create);
}

bool
fgHasNode (const char * path)
{
  return (fgGetNode(path, false) != 0);
}

void
fgAddChangeListener (SGPropertyChangeListener * listener, const char * path)
{
  fgGetNode(path, true)->addChangeListener(listener);
}

void
fgAddChangeListener (SGPropertyChangeListener * listener,
		     const char * path, int index)
{
  fgGetNode(path, index, true)->addChangeListener(listener);
}

bool
fgGetBool (const char * name, bool defaultValue)
{
  return globals->get_props()->getBoolValue(name, defaultValue);
}

int
fgGetInt (const char * name, int defaultValue)
{
  return globals->get_props()->getIntValue(name, defaultValue);
}

long
fgGetLong (const char * name, long defaultValue)
{
  return globals->get_props()->getLongValue(name, defaultValue);
}

float
fgGetFloat (const char * name, float defaultValue)
{
  return globals->get_props()->getFloatValue(name, defaultValue);
}

double
fgGetDouble (const char * name, double defaultValue)
{
  return globals->get_props()->getDoubleValue(name, defaultValue);
}

std::string
fgGetString (const char * name, const char * defaultValue)
{
  return globals->get_props()->getStringValue(name, defaultValue);
}

bool
fgSetBool (const char * name, bool val)
{
  return globals->get_props()->setBoolValue(name, val);
}

bool
fgSetInt (const char * name, int val)
{
  return globals->get_props()->setIntValue(name, val);
}

bool
fgSetLong (const char * name, long val)
{
  return globals->get_props()->setLongValue(name, val);
}

bool
fgSetFloat (const char * name, float val)
{
  return globals->get_props()->setFloatValue(name, val);
}

bool
fgSetDouble (const char * name, double val)
{
  return globals->get_props()->setDoubleValue(name, val);
}

bool
fgSetString (const char * name, const char * val)
{
  return globals->get_props()->setStringValue(name, val);
}

void
fgSetArchivable (const char * name, bool state)
{
  SGPropertyNode * node = globals->get_props()->getNode(name);
  if (node == 0)
    SG_LOG(SG_GENERAL, SG_DEBUG,
	   "Attempt to set archive flag for non-existant property "
	   << name);
  else
    node->setAttribute(SGPropertyNode::ARCHIVE, state);
}

void
fgSetReadable (const char * name, bool state)
{
  SGPropertyNode * node = globals->get_props()->getNode(name);
  if (node == 0)
    SG_LOG(SG_GENERAL, SG_DEBUG,
	   "Attempt to set read flag for non-existant property "
	   << name);
  else
    node->setAttribute(SGPropertyNode::READ, state);
}

void
fgSetWritable (const char * name, bool state)
{
  SGPropertyNode * node = globals->get_props()->getNode(name);
  if (node == 0)
    SG_LOG(SG_GENERAL, SG_DEBUG,
	   "Attempt to set write flag for non-existant property "
	   << name);
  else
    node->setAttribute(SGPropertyNode::WRITE, state);
}

void
fgUntie(const char * name)
{
  SGPropertyNode* node = globals->get_props()->getNode(name);
  if (!node) {
    SG_LOG(SG_GENERAL, SG_WARN, "fgUntie: unknown property " << name);
    return;
  }
  
  if (!node->isTied()) {
    return;
  }
  
  if (!node->untie()) {
    SG_LOG(SG_GENERAL, SG_WARN, "Failed to untie property " << name);
  }
}

void fgUntieIfDefined(const std::string& name)
{
    SGPropertyNode* node = globals->get_props()->getNode(name);
    if (!node) {
        return;
    }

    if (!node->isTied()) {
        return;
    }

    if (!node->untie()) {
        SG_LOG(SG_GENERAL, SG_WARN, "Failed to untie property " << name);
    }
}


// end of fg_props.cxx

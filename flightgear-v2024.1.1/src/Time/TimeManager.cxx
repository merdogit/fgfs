/*
 * SPDX-FileName: TimeManager.cxx
 * SPDX-FileComment: simulation-wide time management
 * SPDX-FileCopyrightText: Written by James Turner, started July 2010.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "TimeManager.hxx"

#include <simgear/misc/sg_path.hxx>
#include <simgear/timing/lowleveltime.h>
#include <simgear/structure/commands.hxx>
#include <simgear/timing/sg_time.hxx>
#include <simgear/math/SGMath.hxx>

#include <Main/fg_props.hxx>
#include <Main/globals.hxx>
#include <Time/bodysolver.hxx>

#include <algorithm>
#include <chrono>
#include <thread>


static bool do_timeofday (const SGPropertyNode * arg, SGPropertyNode * root)
{
    const std::string &offset_type = arg->getStringValue("timeofday", "noon");
    int offset = arg->getIntValue("offset", 0);
    auto self = globals->get_subsystem<TimeManager>();
    if (offset_type == "real") {
    // without this, setting 'real' time is a no-op, since the current
    // wrap value (orig_warp) is retained in setTimeOffset. Ick.
        fgSetInt("/sim/time/warp", 0);
    }
    
    self->setTimeOffset(offset_type, offset);
    return true;
}

TimeManager::TimeManager() :
  _inited(false),
  _impl(NULL)
{
  globals->get_commands()->addCommand("timeofday", do_timeofday);
}

TimeManager::~TimeManager()
{
   globals->get_commands()->removeCommand("timeofday");
}

void TimeManager::init()
{
  if (_inited) {
    // time manager has to be initialised early, so needs to be defensive
    // about multiple initialisation 
    return; 
  }
  
  _firstUpdate = true;
  _inited = true;
  _dtRemainder = 0.0;
  _mpProtocolClock = _steadyClock = 0.0;
  _adjustWarpOnUnfreeze = false;
  
  _maxDtPerFrame = fgGetNode("/sim/max-simtime-per-frame", true);
  _clockFreeze = fgGetNode("/sim/freeze/clock", true);
  _timeOverride = fgGetNode("/sim/time/cur-time-override", true);
  _warp = fgGetNode("/sim/time/warp", true);
  _warp->addChangeListener(this);
  _maxFrameRate  = fgGetNode("/sim/frame-rate-throttle-hz", true);
  _localTimeStringNode = fgGetNode("/sim/time/local-time-string", true);
  _localTimeZoneNode = fgGetNode("/sim/time/local-timezone", true);
  _warpDelta = fgGetNode("/sim/time/warp-delta", true);
  _frameNumber = fgGetNode("/sim/frame-number", true);
  _simFixedDt = fgGetNode("/sim/time/fixed-dt", true);
  
  SGPath zone(globals->get_fg_root());
  zone.append("Timezone");
  
  _impl = new SGTime(globals->get_aircraft_position(), zone, _timeOverride->getLongValue());
  
  _warpDelta->setDoubleValue(0.0);
  updateLocalTime();
  
  _impl->update(globals->get_aircraft_position(), _timeOverride->getLongValue(),
               _warp->getIntValue());
  globals->set_time_params(_impl);
    
  // frame-rate / worst-case latency / update-rate counters
  _frameRate = fgGetNode("/sim/frame-rate", true);
  _frameLatency = fgGetNode("/sim/frame-latency-max-ms", true);
  _frameRateWorst = fgGetNode("/sim/frame-rate-worst", true);
  _lastFrameTime = 0;
  _frameLatencyMax = 0.0;
  _frameCount = 0;
    
    _sceneryLoaded = fgGetNode("sim/sceneryloaded", true);
    _modelHz = fgGetNode("sim/model-hz", true);
    _timeDelta = fgGetNode("sim/time/delta-realtime-sec", true);
    _simTimeDelta = fgGetNode("sim/time/delta-sec", true);
    _mpProtocolClockNode = fgGetNode("sim/time/mp-clock-sec", true);
    _steadyClockNode = fgGetNode("sim/time/steady-clock-sec", true);
    _frameTimeOffsetNode = fgGetNode("sim/time/frame-time-offset-ms", true);
    _dtRemainderNode = fgGetNode("sim/time/dt-remainder-sec", true);
    _mpClockOffset = fgGetNode("sim/time/mp-clock-offset-sec", true);
    _steadyClockDrift = fgGetNode("sim/time/steady-clock-drift-ms", true);
    _computeDrift = fgGetNode("sim/time/compute-clock-drift", true);
    _frameWait = fgGetNode("sim/time/frame-wait-ms", true);
    _simTimeFactor = fgGetNode("/sim/speed-up", true);
    // use pre-set value but ensure we get a sane default
    if (!_simTimeDelta->hasValue()) {
        _simTimeFactor->setDoubleValue(1.0);
    }
    if (!_mpClockOffset->hasValue()) {
        _mpClockOffset->setDoubleValue(0.0);
    }
    _computeDrift->setBoolValue(true);

  _simpleTimeEnabledPrev = false;
  _simpleTimeEnabled = fgGetNode("/sim/time/simple-time/enabled", true);
  _simpleTimeUtc = fgGetNode("/sim/time/simple-time/utc", true);
  _simpleTimeFdm = fgGetNode("/sim/time/simple-time/fdm", true);
  _simple_time_utc = 0;
  _simple_time_fdm = 0;
}

void TimeManager::unbind()
{
    _maxDtPerFrame.clear();
    _clockFreeze.clear();
    _timeOverride.clear();
    _warp.clear();
    _warpDelta.clear();
    _frameRate.clear();
    _frameLatency.clear();
    _frameRateWorst.clear();
    _frameWait.clear();
    _maxFrameRate.clear();

    _sceneryLoaded.clear();
    _modelHz.clear();
    _timeDelta.clear();
    _simTimeDelta.clear();
    _mpProtocolClockNode.clear();
    _steadyClockNode.clear();
    _frameTimeOffsetNode.clear();
    _dtRemainderNode.clear();
    _mpClockOffset.clear();
    _steadyClockDrift.clear();
    _computeDrift.clear();
    _simTimeFactor.clear();
}

void TimeManager::postinit()
{
  initTimeOffset();
}

void TimeManager::reinit()
{
  shutdown();
  init();
  postinit();
}

void TimeManager::shutdown()
{
  _warp->removeChangeListener(this);
  
  globals->set_time_params(NULL);
  delete _impl;
  _impl = NULL;
  _inited = false;
}

void TimeManager::valueChanged(SGPropertyNode* aProp)
{
  if (aProp == _warp) {
    if (_clockFreeze->getBoolValue()) {
    // if the warp is changed manually while frozen, don't modify it when
    // un-freezing - the user wants to unfreeze with exactly the warp
    // they specified.
      _adjustWarpOnUnfreeze = false;
    }
    
    _impl->update(globals->get_aircraft_position(),
                   _timeOverride->getLongValue(),
                   _warp->getIntValue());
  }
}

// simple-time mode requires UTC time.
//
// SGTimeStamp() doesn't return UTC time on some systems, e.g. Linux with
// _POSIX_TIMERS > 0 uses _POSIX_MONOTONIC_CLOCK if available.
//
// So we define our own time function here.
//
static double TimeUTC()
{
    auto t = std::chrono::system_clock::now().time_since_epoch();
    typedef std::chrono::duration<double, std::ratio<1, 1>>  duration_hz_fp;
    auto ret = std::chrono::duration_cast<duration_hz_fp>(t);
    return ret.count();
}

void TimeManager::computeTimeDeltasSimple(double& simDt, double& realDt)
{
    double t;
    double fixed_dt = _simFixedDt->getDoubleValue();
    static double fixed_dt_prev = 0.0;
    if (fixed_dt)
    {
        // Always increase time by fixed amount, regardless of elapsed
        // time. E.g. this can be used to generate high-quality videos.
        t = _simple_time_fdm + fixed_dt;
        fixed_dt_prev = fixed_dt;
    }
    else
    {
        t = TimeUTC();
        
        if (fixed_dt_prev)
        {
            // We are changing from fixed-dt mode to normal mode; avoid bogus
            // sleep to match _maxFrameRate, otherwise we can end up pausing
            // for a long time.
            _simple_time_fdm = _simple_time_utc = t - fixed_dt_prev;
            fixed_dt_prev = 0.0;
        }
    }
    double modelHz = _modelHz->getDoubleValue();
    bool scenery_loaded = _sceneryLoaded->getBoolValue();

    if (_firstUpdate) {
        _firstUpdate = false;
        _simple_time_utc = t;
        _simple_time_fdm = t;
        SGSubsystemGroup* fdmGroup = globals->get_subsystem_mgr()->get_group(SGSubsystemMgr::FDM);
        fdmGroup->set_fixed_update_time(1.0 / modelHz);
    }

    // Sleep if necessary to respect _maxFrameRate. It's simpler to do this
    // inline instead of calling throttleUpdateRate().
    //
    double sleep_time = 0;
    if (scenery_loaded && !fixed_dt) {
        double max_frame_rate = _maxFrameRate->getDoubleValue();
        if (max_frame_rate != 0) {
            double delay_end = _simple_time_utc + 1.0/max_frame_rate;
            if (delay_end > t) {
                sleep_time = delay_end - t;
                std::this_thread::sleep_for(std::chrono::milliseconds((int) (sleep_time * 1000)));
                t = delay_end;
            }
        }
    }
    else {
        // suppress framerate while initial scenery isn't loaded yet (splash screen still active) 
        _lastFrameTime=0;
        _frameCount = 0;
    }
    
    // Increment <_simple_time_fdm> by a multiple of the FDM interval, such
    // that it is as close as possible, but not greater than, the current UTC
    // time <t>.
    //
    double dt_fdm = floor( (t - _simple_time_fdm) * modelHz) / modelHz;
    _simple_time_fdm += dt_fdm;
    _frameLatencyMax = std::max(_frameLatencyMax, t - _simple_time_utc);
    _simple_time_utc = t;
    
    _simpleTimeUtc->setDoubleValue(_simple_time_utc);
    _simpleTimeFdm->setDoubleValue(_simple_time_fdm);

    // simDt defaults to dt_fdm, but is affected by whether we are paused or
    // running the FDM at faster/slowe than normal.
    if (_clockFreeze->getBoolValue() || !scenery_loaded) {
        simDt = 0;
    }
    else {
        simDt = dt_fdm * _simTimeFactor->getDoubleValue();
    }
    realDt = dt_fdm;
    globals->inc_sim_time_sec(simDt);

    _mpProtocolClock = _simple_time_fdm;
    _mpProtocolClockNode->setDoubleValue(_mpProtocolClock);

    // Not sure anyone calls getSteadyClockSec()?
    _steadyClock = _simple_time_fdm;
    _steadyClockNode->setDoubleValue(_steadyClock);

    // These are used by Nasal scripts, e.g. when interpolating property
    // values.
    _timeDelta->setDoubleValue(realDt);
    _simTimeDelta->setDoubleValue(simDt);

    SG_LOG(SG_GENERAL, SG_DEBUG, ""
            << std::setprecision(5)
            << std::fixed
            << std::setw(16)
            << " " << ((simDt >= 1.0) ? "*" : " ")
            << " simDt=" << simDt
            << " realDt=" << realDt
            << " sleep_time=" << sleep_time
            << " _simple_time_utc=" << _simple_time_utc
            << " _simple_time_fdm=" << _simple_time_fdm
            << " utc-fdm=" << (_simple_time_utc - _simple_time_fdm)
            << " _steadyClock=" << _steadyClock
            << " _mpProtocolClock=" << _mpProtocolClock
            );
}

void TimeManager::computeTimeDeltas(double& simDt, double& realDt)
{
    bool simple_time = _simpleTimeEnabled->getBoolValue();
    if (simple_time != _simpleTimeEnabledPrev) {
        _simpleTimeEnabledPrev = simple_time;
        _firstUpdate = true;
    }
    if (simple_time) {
        computeTimeDeltasSimple(simDt, realDt);
        return;
    }

    const double modelHz = _modelHz->getDoubleValue();

    // Update the elapsed time.
    if (_firstUpdate) {
        _lastStamp.stamp();

        // Initialise the mp protocol / steady  clock with the system clock.
        // later, the clock follows steps of 1/modelHz (120 by default),
        // so the MP clock remains aligned to these boundaries

        _systemStamp.systemClockHoursAndMinutes();
        const double systemStamp = _systemStamp.toSecs();
        _steadyClock = floor(systemStamp * modelHz) / modelHz;

        // add offset if defined
        const double frameOffsetMsec = _frameTimeOffsetNode->getDoubleValue();
        _steadyClock += frameOffsetMsec / 1000.0;

        // initialize the remainder with offset from the system clock
        _dtRemainder = systemStamp - _steadyClock;

        _firstUpdate = false;
        _lastClockFreeze = _clockFreeze->getBoolValue();
  }

  bool wait_for_scenery = !_sceneryLoaded->getBoolValue();
  if (!wait_for_scenery) {
    throttleUpdateRate();
  } else {
      // suppress framerate while initial scenery isn't loaded yet (splash screen still active) 
      _lastFrameTime=0;
      _frameCount = 0;
  }

  SGTimeStamp currentStamp;
  currentStamp.stamp();

  // if asked, we compute the drift between the steady clock and the system clock

  if (_computeDrift->getBoolValue()) {
      _systemStamp.systemClockHoursAndMinutes();
      double clockdrift = _steadyClock + (currentStamp - _lastStamp).toSecs()
        + _dtRemainder - _systemStamp.toSecs();
      _steadyClockDrift->setDoubleValue(clockdrift * 1000.0);
      _computeDrift->setBoolValue(false);
  }

  // this dt will be clamped by the max sim time by frame.
  double fixed_dt = _simFixedDt->getDoubleValue();
  double dt = (fixed_dt) ? fixed_dt : (currentStamp - _lastStamp).toSecs();

  // here we have a true real dt for a clock "real time".
  double mpProtocolDt = dt;

  if (dt > _frameLatencyMax)
      _frameLatencyMax = dt;

  // Limit the time we need to spend in simulation loops
  // That means, if the /sim/max-simtime-per-frame value is strictly positive
  // you can limit the maximum amount of time you will do simulations for
  // one frame to display. The cpu time spent in simulations code is roughly
  // at least O(real_delta_time_sec). If this is (due to running debug
  // builds or valgrind or something different blowing up execution times)
  // larger than the real time you will no longer get any response
  // from flightgear. This limits that effect. Just set to property from
  // your .fgfsrc or commandline ...
  double dtMax = _maxDtPerFrame->getDoubleValue();
  if (0 < dtMax && dtMax < dt) {
    dt = dtMax;
  }
    
  SGSubsystemGroup* fdmGroup = 
    globals->get_subsystem_mgr()->get_group(SGSubsystemMgr::FDM);
  fdmGroup->set_fixed_update_time(1.0 / modelHz);

  // round the real time down to a multiple of 1/model-hz.
  // this way all systems are updated the _same_ amount of dt.
  dt += _dtRemainder;

  // we keep the mp clock sync with the sim time, as it's used as timestamp
  // in fdm state,
  mpProtocolDt += _dtRemainder;
  int multiLoop = long(floor(dt * modelHz));
  multiLoop = SGMisc<long>::max(0, multiLoop);
  _dtRemainder = dt - double(multiLoop)/modelHz;
  dt = double(multiLoop)/modelHz;
  mpProtocolDt -= _dtRemainder;

  realDt = dt;
  if (_clockFreeze->getBoolValue() || wait_for_scenery) {
    simDt = 0;
  } else {
      // sim time can be scaled
      simDt = dt * _simTimeFactor->getDoubleValue();
  }

  _lastStamp = currentStamp;
  globals->inc_sim_time_sec(simDt);
  _steadyClock += mpProtocolDt;
  _mpProtocolClock = _steadyClock + _mpClockOffset->getDoubleValue();

  _dtRemainderNode->setDoubleValue(_dtRemainder);
  _steadyClockNode->setDoubleValue(_steadyClock);
  _mpProtocolClockNode->setDoubleValue(_mpProtocolClock);

  // These are useful, especially for Nasal scripts.
  _timeDelta->setDoubleValue(realDt);
  _simTimeDelta->setDoubleValue(simDt);
}

void TimeManager::update(double dt)
{
  _frameNumber->setIntValue(_frameNumber->getIntValue() + 1);
  bool freeze = _clockFreeze->getBoolValue();
  time_t now = time(NULL);

  if (freeze) {
    // clock freeze requested
    if (_timeOverride->getLongValue() == 0) {
      _timeOverride->setLongValue(now);
      _adjustWarpOnUnfreeze = true;
    }
  } else {
    // no clock freeze requested
    if (_lastClockFreeze) {
      if (_adjustWarpOnUnfreeze) {
      // clock just unfroze, let's set warp as the difference
      // between frozen time and current time so we don't get a
      // time jump (and corresponding sky object and lighting
      // jump.)
        int adjust = _timeOverride->getLongValue() - now;
        SG_LOG(SG_GENERAL, SG_DEBUG, "adjusting on un-freeze:" << adjust);
        _warp->setIntValue(_warp->getIntValue() + adjust);
      }
      _timeOverride->setLongValue(0);
    }

      // account for speed-up in warp value. This implies when speed-up is not
      // 1.0 we need to continually adjust warp, either forwards for speed-up,
      // or backwards for a slow-down. Eg for a speed up of 4x, we want to
      // incease warp by 3 additional seconds per elapsed real second.
      // for a 1/2x factor, we want to decrease warp by half a second per
      // elapsed real second.
      double speedUp = _simTimeFactor->getDoubleValue() - 1.0;
      if (speedUp != 0.0) {
          double realDt = _timeDelta->getDoubleValue();
          double speedUpOffset = speedUp * realDt;
          _warp->setDoubleValue(_warp->getDoubleValue() + speedUpOffset);
      }
  } // of sim not frozen

    // scale warp-delta by real-dt, so rate is constant with frame-rate,
    // but warping works while paused
    int warpDelta = _warpDelta->getIntValue();
    if (warpDelta) {
        _adjustWarpOnUnfreeze = false;
        double warpOffset = warpDelta * _timeDelta->getDoubleValue();
        _warp->setDoubleValue(_warp->getDoubleValue() + warpOffset);
    }

    const auto d2 = distSqr(_lastTimeZoneCheckPosition, globals->get_aircraft_position_cart());
    const auto oneNmSqr = SG_NM_TO_METER * SG_NM_TO_METER;
    if (d2 > oneNmSqr) {
        updateLocalTime();
    }

  _lastClockFreeze = freeze;
  _impl->update(globals->get_aircraft_position(),
               _timeOverride->getLongValue(),
               _warp->getIntValue());

  updateLocalTimeString();
  computeFrameRate();
}

void TimeManager::computeFrameRate()
{
  // Calculate frame rate average
  if ((_impl->get_cur_time() != _lastFrameTime)) {
    _frameRate->setIntValue(_frameCount);
    _frameLatency->setDoubleValue(_frameLatencyMax*1000);
    if (_frameLatencyMax>0)
        _frameRateWorst->setIntValue(1/_frameLatencyMax);
    _frameCount = 0;
    _frameLatencyMax = 0.0;
  }
  
  _lastFrameTime = _impl->get_cur_time();
  ++_frameCount;
}

void TimeManager::throttleUpdateRate()
{
    const double throttleHz = _maxFrameRate->getDoubleValue();
    // no delay required.
    if (throttleHz <= 0) {
        _frameWait->setDoubleValue(0);
        return;
    }

    const double modelHz = _modelHz->getDoubleValue();
    SGTimeStamp frameWaitStart = SGTimeStamp::now();

    // we want to sleep until just after the next ideal timestamp wanted, we will
    // gain time from a 1/Hz step if the last timestamp was late.
    const double t = (round(modelHz / throttleHz) / modelHz) - _dtRemainder;
    SGTimeStamp::sleepUntil(_lastStamp + SGTimeStamp::fromSec(t));
    _frameWait->setDoubleValue(frameWaitStart.elapsedMSec());
}

void TimeManager::reposition()
{
    // force a zone check, next update()
    _lastTimeZoneCheckPosition = SGVec3d::zeros();
}

// periodic time updater wrapper
void TimeManager::updateLocalTime() 
{
    _lastTimeZoneCheckPosition = globals->get_aircraft_position_cart();
    _impl->updateLocal(globals->get_aircraft_position(), globals->get_fg_root() / "Timezone");
    // synchronous update, since somebody might need that
    updateLocalTimeString();
}

void TimeManager::updateLocalTimeString()
{
    time_t cur_time = _impl->get_cur_time();
    if (!_impl->get_zonename()) {
        return;
    }
    
    struct tm* aircraftLocalTime = fgLocaltime(&cur_time, _impl->get_zonename());
    static char buf[16];
    snprintf(buf, 16, "%.2d:%.2d:%.2d",
             aircraftLocalTime->tm_hour,
             aircraftLocalTime->tm_min, aircraftLocalTime->tm_sec);

    // check against current string to avoid changes all the time
    std::string s = _localTimeStringNode->getStringValue();
    if (s != std::string(buf)) {
        _localTimeStringNode->setStringValue(buf);
    }

    std::string zs = _localTimeZoneNode->getStringValue();
    if (zs != std::string(_impl->get_description())) {
        _localTimeZoneNode->setStringValue(_impl->get_description());
    }
}

void TimeManager::initTimeOffset()
{

  long int offset = fgGetLong("/sim/startup/time-offset");
  std::string offset_type = fgGetString("/sim/startup/time-offset-type");
  setTimeOffset(offset_type, offset);
}

void TimeManager::setTimeOffset(const std::string& offset_type, long int offset)
{
  // Handle potential user specified time offsets
  int orig_warp = _warp->getIntValue();
  time_t cur_time = _impl->get_cur_time();
  time_t currGMT = sgTimeGetGMT( gmtime(&cur_time) );
  time_t systemLocalTime = sgTimeGetGMT( localtime(&cur_time) );
  time_t aircraftLocalTime = 
      sgTimeGetGMT( fgLocaltime(&cur_time, _impl->get_zonename() ) );
    
  // Okay, we now have several possible scenarios
  SGGeod loc = globals->get_aircraft_position();
  int warp = 0;
  
  if ( offset_type == "real" ) {
      warp = 0;
  } else if ( offset_type == "dawn" ) {
      warp = fgTimeSecondsUntilBodyAngle( cur_time, loc, 90.0, true, true );
  } else if ( offset_type == "morning" ) {
     warp = fgTimeSecondsUntilBodyAngle( cur_time, loc, 75.0, true, true );
  } else if ( offset_type == "noon" ) {
     warp = fgTimeSecondsUntilBodyAngle( cur_time, loc, 0.0, true, true );
  } else if ( offset_type == "afternoon" ) {
    warp = fgTimeSecondsUntilBodyAngle( cur_time, loc, 75.0, false, true );
  } else if ( offset_type == "dusk" ) {
    warp = fgTimeSecondsUntilBodyAngle( cur_time, loc, 90.0, false, true );
  } else if ( offset_type == "evening" ) {
    warp = fgTimeSecondsUntilBodyAngle( cur_time, loc, 100.0, false, true );
  } else if ( offset_type == "midnight" ) {
    warp = fgTimeSecondsUntilBodyAngle( cur_time, loc, 180.0, false, true );
  } else if ( offset_type == "system-offset" ) {
    warp = offset;
    orig_warp = 0;
  } else if ( offset_type == "gmt-offset" ) {
    warp = offset - (currGMT - systemLocalTime);
    orig_warp = 0;
  } else if ( offset_type == "latitude-offset" ) {
    warp = offset - (aircraftLocalTime - systemLocalTime);
    orig_warp = 0;
  } else if ( offset_type == "system" ) {
    warp = offset - (systemLocalTime - currGMT) - cur_time;
  } else if ( offset_type == "gmt" ) {
      warp = offset - cur_time;
  } else if ( offset_type == "latitude" ) {
      warp = offset - (aircraftLocalTime - currGMT)- cur_time; 
  } else {
    SG_LOG( SG_GENERAL, SG_ALERT,
          "TimeManager::setTimeOffset: unsupported offset: " << offset_type );
     warp = 0;
  }
  
  if( fgGetBool("/sim/time/warp-easing", false) && !fgGetBool("/devices/status/keyboard/ctrl", false)) {
    double duration = fgGetDouble("/sim/time/warp-easing-duration-secs", 5.0 );
    const std::string easing = fgGetString("/sim/time/warp-easing-method", "swing" );
    SGPropertyNode n;
    n.setDoubleValue( orig_warp + warp );
    _warp->interpolate( "numeric", n, duration, easing );
  } else {
    _warp->setIntValue( orig_warp + warp );
  }

  SG_LOG(SG_GENERAL, SG_INFO, "After TimeManager::setTimeOffset(): " << offset_type << ", warp = " << _warp->getIntValue());
}

double TimeManager::getSimSpeedUpFactor() const
{
    return _simTimeFactor->getDoubleValue();
}

// Register the subsystem.
SGSubsystemMgr::Registrant<TimeManager> registrantTimeManager(
    SGSubsystemMgr::INIT,
    {{"FDM", SGSubsystemMgr::Dependency::HARD}});

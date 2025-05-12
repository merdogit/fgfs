/*
 * SPDX-FileName: soundmanager.cxx
 * SPDX-FileComment: Wraps the SimGear OpenAl sound manager class
 * SPDX-FileCopyrightText: Copyright (C) 2001  Curtis L. Olson - http://www.flightgear.org/~curt
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <config.h>

#include <simgear/sound/soundmgr.hxx>
#include <simgear/structure/commands.hxx>

#include "VoiceSynthesizer.hxx"
#include "sample_queue.hxx"
#include "soundmanager.hxx"
#include <Main/globals.hxx>
#include <Main/fg_props.hxx>
#include <Viewer/view.hxx>

#include <stdio.h>

#include <vector>
#include <string>

#ifdef ENABLE_AUDIO_SUPPORT
/**
 * Listener class that monitors the sim state.
 */
class Listener : public SGPropertyChangeListener
{
public:
    Listener(FGSoundManager *wrapper) : _wrapper(wrapper) {}
    virtual void valueChanged (SGPropertyNode * node);

private:
    FGSoundManager* _wrapper;
};

void Listener::valueChanged(SGPropertyNode * node)
{
    _wrapper->activate(node->getBoolValue());
}

FGSoundManager::FGSoundManager()
  : _active_dt(0.0),
    _is_initialized(false),
    _enabled(false),
    _listener(new Listener(this))
{
}

FGSoundManager::~FGSoundManager()
{
}

void FGSoundManager::init()
{
    _sound_working = fgGetNode("/sim/sound/working");
    _sound_enabled = fgGetNode("/sim/sound/enabled");
    _volume        = fgGetNode("/sim/sound/volume");
    _device_name   = fgGetNode("/sim/sound/device-name");

    _velocityNorthFPS = fgGetNode("velocities/speed-north-fps", true);
    _velocityEastFPS  = fgGetNode("velocities/speed-east-fps", true);
    _velocityDownFPS  = fgGetNode("velocities/speed-down-fps", true);

    _frozen = fgGetNode("sim/freeze/master");

    SGPropertyNode_ptr scenery_loaded = fgGetNode("sim/sceneryloaded", true);
    scenery_loaded->addChangeListener(_listener.get());

    globals->get_commands()->addCommand("play-audio-sample", this, &FGSoundManager::playAudioSampleCommand);


    reinit();
}

void FGSoundManager::shutdown()
{
    SGPropertyNode_ptr scenery_loaded = fgGetNode("sim/sceneryloaded", true);
    scenery_loaded->removeChangeListener(_listener.get());

    stop();

    _queue.clear();
    globals->get_commands()->removeCommand("play-audio-sample");


    SGSoundMgr::shutdown();
}

void FGSoundManager::reinit()
{
    _is_initialized = false;

    if (_is_initialized && !_sound_working->getBoolValue())
    {
        // shutdown sound support
        stop();
        return;
    }

    if (!_sound_working->getBoolValue())
    {
        return;
    }

    update_device_list();

    select_device(_device_name->getStringValue().c_str());
    SGSoundMgr::reinit();
    _is_initialized = true;

    activate(fgGetBool("sim/sceneryloaded", true));
}

void FGSoundManager::activate(bool State)
{
    if (_is_initialized)
    {
        if (State)
        {
            SGSoundMgr::activate();
        }
    }
}

void FGSoundManager::update_device_list()
{
    std::vector <std::string>devices = get_available_devices();
    for (unsigned int i=0; i<devices.size(); i++) {
        SGPropertyNode *p = fgGetNode("/sim/sound/devices/device", i, true);
        p->setStringValue(devices[i].c_str());
    }
    devices.clear();
}

bool FGSoundManager::stationaryView() const
{
  // this is an ugly hack to decide if the *viewer* is stationary,
  // since we don't model the viewer velocity directly.
  flightgear::View* _view = globals->get_current_view();
  return (_view->getXOffset_m () == 0.0 && _view->getYOffset_m () == 0.0 &&
          _view->getZOffset_m () == 0.0);
}

// Update sound manager and propagate property values,
// since the sound manager doesn't read any properties itself.
// Actual sound update is triggered by the subsystem manager.
void FGSoundManager::update(double dt)
{
    if (is_working() && _is_initialized && _sound_working->getBoolValue())
    {
        bool enabled = _sound_enabled->getBoolValue() && !_frozen->getBoolValue();
        if (enabled != _enabled)
        {
            if (enabled)
                resume();
            else
                suspend();
            _enabled = enabled;
        }
        if (enabled)
        {
            flightgear::View* _view = globals->get_current_view();
            set_position( _view->getViewPosition(), _view->getPosition() );
            set_orientation( _view->getViewOrientation() );

            SGVec3d velocity(SGVec3d::zeros());
            if (!stationaryView()) {
              velocity = SGVec3d(_velocityNorthFPS->getDoubleValue(),
                                 _velocityEastFPS->getDoubleValue(),
                                 _velocityDownFPS->getDoubleValue() );
            }

            set_velocity( velocity );

            float vf = 1.0f;
            if (_active_dt < 5.0) {
                _active_dt += dt;
                vf = std::min(std::pow(_active_dt*0.2, 5.0), 1.0);
            }

            set_volume(vf*_volume->getFloatValue());
            SGSoundMgr::update(dt);
        }
    }
}

/**
 * Built-in command: play an audio message (i.e. a wav file) This is
 * fire and forget. Call this once per message and it will get dumped
 * into a queue. Except for the special 'instant' queue, messages within
 * a given queue are played sequentially so they do not overlap.
 */
bool FGSoundManager::playAudioSampleCommand(const SGPropertyNode * arg, SGPropertyNode * root)
{
    std::string qname = arg->getStringValue("queue", "");
    std::string name = !qname.empty() ? qname : "chatter";
    std::string path = arg->getStringValue("path");
    std::string file = arg->getStringValue("file");
    float volume = arg->getFloatValue("volume");

    const auto fullPath = SGPath(path) / file;
    const auto foundPath = globals->resolve_maybe_aircraft_path(
        fullPath.utf8Str());
    if (!foundPath.exists()) {
        SG_LOG(SG_GENERAL, SG_ALERT, "play-audio-sample: no such file: '" <<
               fullPath.utf8Str() << "'");
        return false;
    }

    // SG_LOG(SG_GENERAL, SG_ALERT, "Playing '" << foundPath.utf8Str() << "'");
    try {
        SGSoundSample *msg = new SGSoundSample(foundPath);
        msg->set_volume( volume );

        if (name == "instant")
        {
            static const char *r = "0123456789abcdefghijklmnopqrstuvwxyz"
                                   "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
           std::string rstr = "NASAL: ";
           for (int i=0; i<10; i++) {
               rstr.push_back( r[rand() % strlen(r)] );
           }

            // Add a special queue-name 'instant' which does not put samples
            // into a sample queue but plays them instantly.
            SGSampleGroup* sgr = find("NASAL instant queue", true);
            sgr->tie_to_listener();
            sgr->add(msg, rstr);
            sgr->play_once(rstr);
        }
        else
        {
            if ( !_queue[name] ) {
                _queue[name] = new FGSampleQueue(this, name);
                _queue[name]->tie_to_listener();
            }
            _queue[name]->add( msg );
         }

        return true;

    } catch (const sg_io_exception&) {
        SG_LOG(SG_GENERAL, SG_ALERT, "play-audio-sample: "
               "failed to load '" << foundPath.utf8Str() << "'");
        return false;
    }
}

VoiceSynthesizer * FGSoundManager::getSynthesizer( const std::string & voice )
{
  std::map<std::string,VoiceSynthesizer*>::iterator it = _synthesizers.find(voice);
  if( it == _synthesizers.end() ) {
    VoiceSynthesizer * synthesizer = new FLITEVoiceSynthesizer( voice );
    _synthesizers[voice] = synthesizer;
    return synthesizer;
  }
  return it->second;
}
#endif // ENABLE_AUDIO_SUPPORT


// Register the subsystem.
SGSubsystemMgr::Registrant<FGSoundManager> registrantFGSoundManager(
    SGSubsystemMgr::SOUND,
    {{"SGSoundMgr", SGSubsystemMgr::Dependency::HARD}});

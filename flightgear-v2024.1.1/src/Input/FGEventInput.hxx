// FGEventInput.hxx -- handle event driven input devices
//
// Written by Torsten Dreyer, started July 2009
//
// Copyright (C) 2009 Torsten Dreyer, Torsten (at) t3r _dot_ de
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

#ifndef __FGEVENTINPUT_HXX
#define __FGEVENTINPUT_HXX

#include "FGCommonInput.hxx"

#include <vector>
#include <memory>

#include "FGButton.hxx"
#include "FGDeviceConfigurationMap.hxx"
#include <simgear/structure/subsystem_mgr.hxx>

// forward decls
class SGInterpTable;

/*
 * A base structure for event data.
 * To be extended for O/S specific implementation data
 */
struct FGEventData {
    FGEventData( double aValue, double aDt, int aModifiers ) : modifiers(aModifiers), value(aValue), dt(aDt) {}
    int modifiers {0};
    double value {0.0};
    double dt {0.0};
};

/*
  FGEventSetting
  stores one value or property node together with an optional condition
  Multiple FGEventSetting can be assigned to one FGInputEvent
*/
class FGEventSetting : public SGReferenced
{
public:
    FGEventSetting( SGPropertyNode_ptr base );
    // return evaluted condition or true if condition is nullptr
    bool Test();
    // return either value of valueNode or value if valueNode is nullptr
    double GetValue();

protected:
    double value {0.0};
    SGPropertyNode_ptr valueNode;
    SGSharedPtr<const SGCondition> condition;
};

typedef SGSharedPtr<FGEventSetting> FGEventSetting_ptr;
typedef std::vector<FGEventSetting_ptr> setting_list_t;

class FGReportSetting : public SGReferenced,
                        public SGPropertyChangeListener
{
public:
    FGReportSetting( SGPropertyNode_ptr base );
    unsigned int getReportId() const { return reportId; }
    std::string getNasalFunctionName() const { return nasalFunction; }
    bool Test();
    std::string reportBytes(const std::string& moduleName) const;
    virtual void valueChanged(SGPropertyNode * node);

protected:
    unsigned int reportId;
    std::string nasalFunction;
    bool dirty;
};

typedef SGSharedPtr<FGReportSetting> FGReportSetting_ptr;
typedef std::vector<FGReportSetting_ptr> report_setting_list_t;

/*
 * A wrapper class for a configured event.
 *
 * <event>
 *   <desc>Change the view pitch</desc>
 *   <name>rel-x-rotate</name>
 *   <binding>
 *     <command>property-adjust</command>
 *     <property>sim/current-view/pitch-offset-deg</property>
 *     <factor type="double">0.01</factor>
 *     <min type="double">-90.0</min>
 *     <max type="double">90.0</max>
 *     <wrap type="bool">false</wrap>
 *   </binding>
 *   <mod-xyz>
 *    <binding>
 *      ...
 *    </binding>
 *   </mod-xyz>
 * </event>
 */
class FGInputDevice;
class FGInputEvent : public SGReferenced,
                     FGCommonInput
{
public:
    FGInputEvent( FGInputDevice * device, SGPropertyNode_ptr eventNode );
    virtual ~FGInputEvent();

    // dispatch the event value through all bindings
    virtual void fire( FGEventData & eventData );

    std::string GetName() const { return name; }
    std::string GetDescription() const { return desc; }

    virtual void update( double dt );
    static FGInputEvent * NewObject( FGInputDevice * device, SGPropertyNode_ptr node );

protected:
    virtual void fire(SGAbstractBinding* binding, FGEventData& eventData);
    /* A more or less meaningfull description of the event */
    std::string desc;

    /* One of the predefined names of the event */
    std::string name;

    /* A list of SGBinding objects */
    binding_list_t bindings[KEYMOD_MAX];

    /* A list of FGEventSetting objects */
    setting_list_t settings;

    /* A pointer to the associated device */
    FGInputDevice * device;

    double lastDt;
    double intervalSec;
    double lastSettingValue;
};

class FGButtonEvent : public FGInputEvent
{
public:
    FGButtonEvent( FGInputDevice * device, SGPropertyNode_ptr node );
    virtual void fire( FGEventData & eventData );

  void update( double dt ) override;
protected:
    bool repeatable;
    bool lastState;
};

class FGAxisEvent : public FGInputEvent
{
public:
    FGAxisEvent( FGInputDevice * device, SGPropertyNode_ptr eventNode );
    ~FGAxisEvent();

    void SetMaxRange( double value ) { maxRange = value; }
    void SetMinRange( double value ) { minRange = value; }
    void SetRange( double min, double max ) { minRange = min; maxRange = max; }

protected:
    virtual void fire( FGEventData & eventData );
    double tolerance;
    double minRange;
    double maxRange;
    double center;
    double deadband;
    double lowThreshold;
    double highThreshold;
    double lastValue;
    std::unique_ptr<SGInterpTable> interpolater;
    bool mirrorInterpolater = false;
};

class FGRelAxisEvent : public FGAxisEvent
{
public:
    FGRelAxisEvent( FGInputDevice * device, SGPropertyNode_ptr eventNode );

protected:
    void fire(SGAbstractBinding* binding, FGEventData& eventData) override;
};

class FGAbsAxisEvent : public FGAxisEvent
{
public:
    FGAbsAxisEvent( FGInputDevice * device, SGPropertyNode_ptr eventNode ) : FGAxisEvent( device, eventNode ) {}

protected:
    void fire(SGAbstractBinding* binding, FGEventData& eventData) override;
};

typedef class SGSharedPtr<FGInputEvent> FGInputEvent_ptr;

/*
 * A abstract class implementing basic functionality of input devices for
 * all operating systems. This is the base class for the O/S-specific
 * implementation of input device handlers
 */
class FGInputDevice : public SGReferenced
{
public:
    FGInputDevice() {}
    FGInputDevice( std::string aName, std::string aSerial = {} ) :
        name(aName), serialNumber(aSerial) {}

    virtual ~FGInputDevice();

    virtual bool Open() = 0;
    virtual void Close() = 0;

    virtual void Send( const char * eventName, double value ) = 0;

    inline void Send( const std::string & eventName, double value ) {
        Send( eventName.c_str(), value );
    }

    virtual void SendFeatureReport(unsigned int reportId, const std::string& data);

    virtual const char * TranslateEventName( FGEventData & eventData ) = 0;


    void SetName( std::string name );
    std::string & GetName() { return name; }

    void SetUniqueName(const std::string& name);
    const std::string GetUniqueName() const { return _uniqueName; }

    void SetSerialNumber( std::string serial );
    std::string& GetSerialNumber() { return serialNumber; }

    void HandleEvent( FGEventData & eventData );

    virtual void AddHandledEvent( FGInputEvent_ptr handledEvent );

    virtual void Configure( SGPropertyNode_ptr deviceNode );

    virtual void update( double dt );

    bool GetDebugEvents () const { return debugEvents; }

    bool GetGrab() const { return grab; }

    const std::string & GetNasalModule() const { return nasalModule; }
    std::string class_id = "FGInputDevice";
protected:
    // A map of events, this device handles
    std::map<std::string,FGInputEvent_ptr> handledEvents;

    // the device has a name to be recognized
    std::string name;

    // serial number string to disambiguate multiple instances
    // of the same device
    std::string serialNumber;

    // print out events comming in from the device
    // if true
    bool debugEvents = false;

    // grab the device exclusively, if O/S supports this
    // so events are not sent to other applications
    bool grab = false;

    //configuration in property tree
    SGPropertyNode_ptr deviceNode;
    SGPropertyNode_ptr lastEventName;
    SGPropertyNode_ptr lastEventValue;

    std::string nasalModule;

    report_setting_list_t reportSettings;

  /// name, but with suffix / serial appended. This is important
  /// when loading the device multiple times, to ensure the Nasal
  /// module is unique
  std::string _uniqueName;
};

typedef SGSharedPtr<FGInputDevice> FGInputDevice_ptr;


/*
 * The Subsystem for the event input device
 */
class FGEventInput : public SGSubsystem,
                     FGCommonInput
{
public:
    FGEventInput();
    FGEventInput(const char* filePath, const char* propertyRoot);
    virtual ~FGEventInput();

    // Subsystem API.
    void init() override;
    void postinit() override;
    void shutdown() override;
    void update(double dt) override;

    const static unsigned MAX_DEVICES = 1000;
    const static unsigned INVALID_DEVICE_INDEX = MAX_DEVICES + 1;

protected:
    // where to search for configs and where to put them in the property tree
    const char* filePath;
    const char* propertyRoot;

    unsigned AddDevice(FGInputDevice* inputDevice);
    void RemoveDevice( unsigned index );

    std::map<int,FGInputDevice*> inputDevices;
    FGDeviceConfigurationMap configMap;

    SGPropertyNode_ptr nasalClose;

private:
    std::string computeDeviceIndexName(FGInputDevice *dev) const;
};

#endif

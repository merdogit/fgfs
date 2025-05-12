/*
 * SPDX-FileName: electrical.hxx
 * SPDX-FileComment: a flexible, generic electrical system model
 * SPDX-FileCopyrightText: Copyright (C) 2002  Curtis L. Olson  - http://www.flightgear.org/~curt
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <string>
#include <vector>

#include <simgear/props/props.hxx>
#include <simgear/structure/subsystem_mgr.hxx>


// Forward declaration
class FGElectricalSystem;


// Base class for other electrical components
class FGElectricalComponent
{
public:
    enum FGElectricalComponentType {
        FG_UNKNOWN,
        FG_SUPPLIER,
        FG_BUS,
        FG_OUTPUT,
        FG_CONNECTOR
    };

protected:
    using comp_list = std::vector<FGElectricalComponent*>;

    int kind;
    std::string name;
    float volts;
    float load_amps;      // sum of current draw (load) due to
                          // this node and all it's children
    float available_amps; // available current (after the load
                          // is subtracted)

    comp_list inputs;
    comp_list outputs;

    simgear::PropertyList props;

public:
    FGElectricalComponent();
    virtual ~FGElectricalComponent() = default;

    inline const std::string& get_name() { return name; }

    inline int get_kind() const { return kind; }

    inline float get_volts() const { return volts; }
    inline void set_volts(float val) { volts = val; }

    inline float get_load_amps() const { return load_amps; }
    inline void set_load_amps(float val) { load_amps = val; }

    inline float get_available_amps() const { return available_amps; }
    inline void set_available_amps(float val) { available_amps = val; }

    inline int get_num_inputs() const { return outputs.size(); }
    inline FGElectricalComponent* get_input(const int i)
    {
        return inputs[i];
    }
    inline void add_input(FGElectricalComponent* c)
    {
        inputs.push_back(c);
    }

    inline int get_num_outputs() const { return outputs.size(); }
    inline FGElectricalComponent* get_output(const int i)
    {
        return outputs[i];
    }
    inline void add_output(FGElectricalComponent* c)
    {
        outputs.push_back(c);
    }

    void add_prop(const std::string& s);

    void publishVoltageToProps() const;
};


// Electrical supplier
class FGElectricalSupplier : public FGElectricalComponent
{
public:
    enum FGSupplierType {
        FG_BATTERY,
        FG_ALTERNATOR,
        FG_EXTERNAL,
        FG_UNKNOWN
    };

private:
    SGPropertyNode_ptr _rpm_node;

    FGSupplierType model; // store supplier type
    float ideal_volts;    // ideal volts

    // alternator fields
    std::string rpm_src; // property name of alternator power source
    float rpm_threshold; // minimal rpm to generate full power

    // alt & ext supplier fields
    float ideal_amps; // total amps produced (above rpm threshold).

    // battery fields
    float amp_hours;         // fully charged battery capacity
    float percent_remaining; // percent of charge remaining
    float charge_amps;       // maximum charge load battery can draw

public:
    FGElectricalSupplier(SGPropertyNode* node);
    ~FGElectricalSupplier() {}

    inline FGSupplierType get_model() const { return model; }
    float apply_load(float amps, float dt);
    float get_output_volts();
    float get_output_amps();
    float get_charge_amps() const { return charge_amps; }
};


// Electrical bus (can take multiple inputs and provide multiple
// outputs)
class FGElectricalBus : public FGElectricalComponent
{
public:
    FGElectricalBus(SGPropertyNode* node);
    ~FGElectricalBus() {}
};


// A lot like an FGElectricalBus, but here for convenience and future
// flexibility
class FGElectricalOutput : public FGElectricalComponent
{
public:
    FGElectricalOutput(SGPropertyNode* node);
    ~FGElectricalOutput() {}
};


// Model an electrical switch.  If the rating_amps > 0 then this
// becomes a circuit breaker type switch that can trip
class FGElectricalSwitch
{
private:
    SGPropertyNode_ptr switch_node;
    float rating_amps;
    bool circuit_breaker;

public:
    FGElectricalSwitch(SGPropertyNode* node);

    ~FGElectricalSwitch(){};

    inline bool get_state() const { return switch_node->getBoolValue(); }
    void set_state(bool val) { switch_node->setBoolValue(val); }
};


// Connects multiple sources to multiple destinations with optional
// switches/fuses/circuit breakers inline
class FGElectricalConnector : public FGElectricalComponent
{
    comp_list inputs;
    comp_list outputs;
    typedef std::vector<FGElectricalSwitch> switch_list;
    switch_list switches;

public:
    FGElectricalConnector(SGPropertyNode* node, FGElectricalSystem* es);
    ~FGElectricalConnector() {}

    void add_switch(FGElectricalSwitch s)
    {
        switches.push_back(s);
    }

    // set all switches to the specified state
    void set_switches(bool state);

    bool get_state();
};


/**
 * Model an electrical system.  This is a fairly simplistic system
 *
 */

class FGElectricalSystem : public SGSubsystem
{
public:
    FGElectricalSystem(SGPropertyNode* node);
    virtual ~FGElectricalSystem();

    // Subsystem API.
    void bind() override;
    void init() override;
    void shutdown() override;
    void unbind() override;
    void update(double dt) override;

    // Subsystem identification.
    static const char* staticSubsystemClassId() { return "electrical"; }

    bool build(SGPropertyNode* config_props);
    float propagate(FGElectricalComponent* node, double dt,
                    float input_volts, float input_amps,
                    std::string s = "");
    FGElectricalComponent* find(const std::string& name);

protected:
    typedef std::vector<FGElectricalComponent*> comp_list;

private:
    void deleteComponents(comp_list& comps);

    std::string name;
    int num;
    std::string path;

    bool enabled;

    comp_list suppliers;
    comp_list buses;
    comp_list outputs;
    comp_list connectors;

    SGPropertyNode_ptr _volts_out;
    SGPropertyNode_ptr _amps_out;
    SGPropertyNode_ptr _serviceable_node;
    bool _serviceable = true;
};

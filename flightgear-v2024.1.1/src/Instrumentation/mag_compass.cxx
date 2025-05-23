// mag_compass.cxx - a magnetic compass.
// Written by David Megginson, started 2003.
//
// This file is in the Public Domain and comes with no warranty.

// This implementation is derived from an earlier one by Alex Perry,
// which appeared in src/Cockpit/steam.cxx

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <simgear/sg_inlines.h>
#include <simgear/math/SGMath.hxx>

#include <Main/fg_props.hxx>
#include <Main/util.hxx>

#include "mag_compass.hxx"

MagCompass::MagCompass ( SGPropertyNode *node )
    : _rate_degps(0.0),
      _name(node->getStringValue("name", "magnetic-compass")),
      _num(node->getIntValue("number", 0))
{
    SGPropertyNode_ptr n = node->getNode( "deviation", false );
    if( n ) {
      SGPropertyNode_ptr deviation_table_node = n->getNode( "table", false );
      if( NULL != deviation_table_node ) {
        _deviation_table = new SGInterpTable( deviation_table_node );
      } else {
        std::string deviation_node_name = n->getStringValue();
        if( !deviation_node_name.empty() )
          _deviation_node = fgGetNode( deviation_node_name, true );
      }
    }

    _cfg_viscosity = node->getDoubleValue("fluid-viscosity", 8.2);
}

MagCompass::~MagCompass ()
{
}

void
MagCompass::init ()
{
    std::string branch;
    branch = "/instrumentation/" + _name;

    SGPropertyNode *node = fgGetNode(branch, _num, true );
    _serviceable_node = node->getChild("serviceable", 0, true);
    _pitch_offset_node = node->getChild("pitch-offset-deg", 0, true);
    _roll_node = fgGetNode("/orientation/roll-deg", true);
    _pitch_node = fgGetNode("/orientation/pitch-deg", true);
    _heading_node = fgGetNode("/orientation/heading-magnetic-deg", true);
    _beta_node = fgGetNode("/orientation/side-slip-deg", true);
    _dip_node = fgGetNode("/environment/magnetic-dip-deg", true);
    _x_accel_node = fgGetNode("/accelerations/pilot/x-accel-fps_sec", true);
    _y_accel_node = fgGetNode("/accelerations/pilot/y-accel-fps_sec", true);
    _z_accel_node = fgGetNode("/accelerations/pilot/z-accel-fps_sec", true);
    _out_node = node->getChild("indicated-heading-deg", 0, true);
    _roll_out_node = node->getChild("roll-deg", 0, true);
    _pitch_out_node = node->getChild("pitch-deg", 0, true);
    _fluid_viscosity = node->getChild("fluid-viscosity", 0, true);

    reinit();
}

void
MagCompass::reinit ()
{
    _rate_degps = 0.0;
    _fluid_viscosity->setDoubleValue(_cfg_viscosity);
}

void
MagCompass::update (double delta_time_sec)
{
                                // This is the real magnetic
                                // which would be displayed
                                // if the compass had no errors.
    //double heading_mag_deg = _heading_node->getDoubleValue();


                                // don't update if the compass
                                // is broken
    if (!_serviceable_node->getBoolValue())
        return;

    /*
     * Calculate roll/pitch-filter-factor based on fluid viscosity
     * 
     * Note: This is currently very naive/simple. I lack the physics to do a good
     *       formula here; it is guesstimeated on Kerosene (viscosity about 8) and
     *       visual damping on a standard compass.
     */
    double fluid_damping = 5.0/8.0 * _fluid_viscosity->getDoubleValue() * 10;

    
    /*
     * Vassilii: commented out because this way, even when parked,
     * w/o any accelerations and level, the compass is jammed.
     * If somebody wants to model jamming, real forces (i.e. accelerations)
     * and not sideslip angle must be considered.
     */
#if 0
				// jam on excessive sideslip
    if (fabs(_beta_node->getDoubleValue()) > 12.0) {
        _rate_degps = 0.0;
        return;
    }
#endif


    /*
      Formula for northernly turning error from
      http://williams.best.vwh.net/compass/node4.html:

      Hc: compass heading
      psi: magnetic heading
      theta: bank angle (right positive; should be phi here)
      mu: dip angle (down positive)

      Hc = atan2(sin(Hm)cos(theta)-tan(mu)sin(theta), cos(Hm))

      This function changes the variable names to the more common psi
      for the heading, theta for the pitch, and phi for the roll (and
      target_deg for Hc).  It also modifies the equation to
      incorporate pitch as well as roll, as suggested by Chris
      Metzler.
    */

                                // bank angle (radians)
    double phi = _roll_node->getDoubleValue() * SGD_DEGREES_TO_RADIANS;

                                // pitch angle (radians)
    double theta = _pitch_node->getDoubleValue() * SGD_DEGREES_TO_RADIANS
                   + _pitch_offset_node->getDoubleValue() * SGD_DEGREES_TO_RADIANS;

                                // magnetic heading (radians)
    double psi = _heading_node->getDoubleValue() * SGD_DEGREES_TO_RADIANS;

                                // magnetic dip (radians)
    double mu = _dip_node->getDoubleValue() * SGD_DEGREES_TO_RADIANS;


    /*
      Tilt adjustments for accelerations.
      
      The magnitudes of these are totally made up, but in real life,
      they would depend on the fluid level, the amount of friction,
      etc. anyway.  Basically, the compass float tilts forward for
      acceleration and backward for deceleration.  Tilt about 4
      degrees (0.07 radians) for every G (32 fps/sec) of
      acceleration.

      TODO: do something with the vertical acceleration.
    */
    double x_accel_g = _x_accel_node->getDoubleValue() / 32;
    double y_accel_g = _y_accel_node->getDoubleValue() / 32;
    //double z_accel_g = _z_accel_node->getDoubleValue() / 32;

    theta -= 0.07 * x_accel_g;
    phi -= 0.07 * y_accel_g;
    
    // Expose pitch and roll of the disc
    double d = -_z_accel_node->getDoubleValue();
    if (d < 1.0) d = 1.0;
    double x_factor_norm = _x_accel_node->getDoubleValue() / d * 10.0;
    double y_factor_norm = _y_accel_node->getDoubleValue() / d * 10.0;
    
    double roll = phi * SGD_RADIANS_TO_DEGREES * abs(y_factor_norm);
    roll = flightgear::filterExponential(_last_roll, roll, fluid_damping);
    _roll_out_node->setDoubleValue(roll);
    _last_roll = roll;
    
    double pitch = -theta * SGD_RADIANS_TO_DEGREES * abs(x_factor_norm);
    pitch = flightgear::filterExponential(_last_pitch, pitch, fluid_damping);
    _pitch_out_node->setDoubleValue(pitch);
    _last_pitch = pitch;
    
    
    ////////////////////////////////////////////////////////////////////
    // calculate target compass heading degrees
    ////////////////////////////////////////////////////////////////////

                                // these are expensive: don't repeat
    double sin_phi = sin(phi);
    double sin_theta = sin(theta);
    double sin_mu = sin(mu);
    double cos_theta = cos(theta);
    double cos_psi = cos(psi);
    double cos_mu = cos(mu);

    double a = cos(phi) * sin(psi) * cos_mu
        - sin_phi * cos_theta * sin_mu
        - sin_phi* sin_theta * cos_mu * cos_psi;

    double b = cos_theta * cos_psi * cos(mu)
        - sin_theta * sin_mu;

                                // This is the value that the compass
                                // is *trying* to display.
    double target_deg = atan2(a, b) * SGD_RADIANS_TO_DEGREES;

    if( _deviation_node ) {
      target_deg -= _deviation_node->getDoubleValue();
    } else if( _deviation_table ) { 
       target_deg -= _deviation_table->interpolate( SGMiscd::normalizePeriodic( 0.0, 360.0, target_deg ) );  
    }

    double old_deg = _out_node->getDoubleValue();

    while ((target_deg - old_deg) > 180.0)
        target_deg -= 360.0;
    while ((target_deg - old_deg) < -180.0)
        target_deg += 360.0;

                                // The compass has a current rate of
                                // rotation -- move the rate of rotation
                                // towards one that will turn the compass
                                // to the correct heading, but lag a bit.
                                // (so that the compass can keep overshooting
                                // and coming back).
    double error = target_deg - old_deg;
    _rate_degps = fgGetLowPass(_rate_degps, error, delta_time_sec / 5.0);
    double indicated_deg = old_deg + _rate_degps * delta_time_sec;
    SG_NORMALIZE_RANGE(indicated_deg, 0.0, 360.0);

                                // That's it -- set the messed-up heading.
    _out_node->setDoubleValue(indicated_deg);
}


// Register the subsystem.
#if 0
SGSubsystemMgr::InstancedRegistrant<MagCompass> registrantMagCompass(
    SGSubsystemMgr::FDM,
    {{"instrumentation", SGSubsystemMgr::Dependency::HARD}});
#endif

// end of altimeter.cxx

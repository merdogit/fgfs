/*
 * SPDX-FileName: AICarrier.hxx
 * SPDX-FileComment: AIShip-derived class creates an AI aircraft carrier
 * SPDX-FileCopyrightText: Copyright (C) 2004  David P. Culp - davidculp2@comcast.net
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <list>
#include <string>
#include <string_view>

#include <simgear/compiler.h>
#include <simgear/emesary/Emesary.hxx>

#include "AIShip.hxx"
#include "AIBase.hxx"
#include "AIManager.hxx"


class FGAIManager;
class FGAICarrier;

class FGAICarrier : public FGAIShip, simgear::Emesary::IReceiver
{
public:
    FGAICarrier();
    virtual ~FGAICarrier();

    std::string_view getTypeString(void) const override { return "carrier"; }
    void readFromScenario(SGPropertyNode* scFileNode) override;

    void setSign(const std::string&);
    void setDeckAltitudeFt(const double altitude_feet);
    void setTACANChannelID(const std::string&);
    double getDefaultModelRadius() override { return 350.0; }

    void bind() override;
    void UpdateWind(double dt);
    void setWind_from_east(double fps);
    void setWind_from_north(double fps);
    void setMaxLat(double deg);
    void setMinLat(double deg);
    void setMaxLong(double deg);
    void setMinLong(double deg);
    void setMPControl(bool c);
    void setAIControl(bool c);
    void TurnToLaunch();
    void TurnToRecover();
    void TurnToBase();
    void ReturnToBox();
    bool OutsideBox();

    bool init(ModelSearchOrder searchOrder) override;

    bool getParkPosition(const std::string& id, SGGeod& geodPos, double& hdng, SGVec3d& uvw);

    /**
     * @brief type-safe wrapper around AIManager::getObjectFromProperty
     */
    static SGSharedPtr<FGAICarrier> findCarrierByNameOrPennant(const std::string& namePennant);

    static std::pair<bool, SGGeod> initialPositionForCarrier(const std::string& namePennant);

    /**
     * for a given scenario node, check for carriers within, and write nodes with
     * names, pennants and initial position into the second argument.
     * This is used to support 'start on a carrier', since we can quickly find
     * the corresponding scenario file to be loaded.
     */
    static void extractCarriersFromScenario(SGPropertyNode_ptr xmlNode, SGPropertyNode_ptr scenario);

    bool getFLOLSPositionHeading(SGGeod& pos, double& heading) const;

    double getFLOLFSGlidepathAngleDeg() const;
    double getDeckAltitudeFt() const { return _deck_altitude_ft; }
    virtual simgear::Emesary::ReceiptStatus Receive(simgear::Emesary::INotificationPtr n) override;

private:
    /// Is sufficient to be private, stores a possible position to place an
    /// aircraft on start
    struct ParkPosition {
        explicit ParkPosition(const ParkPosition& pp)
            : name(pp.name), offset(pp.offset), heading_deg(pp.heading_deg)
        {
        }
        ParkPosition(const std::string& n, const SGVec3d& off = SGVec3d(), double heading = 0)
            : name(n), offset(off), heading_deg(heading)
        {
        }

        std::string name;
        SGVec3d offset;
        double heading_deg;
    };

    void update(double dt) override;
    bool InToWind(); // set if the carrier is in to wind
    void UpdateElevator(double dt);
    void UpdateJBD(double dt);

    bool                 _AIControl                               = false;    // under AI control. Either this or MPControl will be true
    SGPropertyNode_ptr   _ai_latch_node;
    SGPropertyNode_ptr   _altitude_node;                                      
    double               _angled_deck_degrees                     = -8.55;    // angled deck offset from carrier heading. usually negative
    double               _base_course                             = 0;        
    double               _base_speed                              = 0;        
    double               _deck_altitude_ft                        = 65.0065;  
    double               _elevator_pos_norm                       = 0;        
    double               _elevator_pos_norm_raw                   = 0;        
    double               _elevator_time_constant                  = 0;        
    double               _elevator_transition_time                = 0;        
    bool                 _elevators                               = false;    
    double               _flols_angle                             = 0;        
    double               _flols_dist                              = 0;        // the distance of the eyepoint from the flols
    int                  _flols_visible_light                     = 0;        // the flols light which is visible at the moment
    SGPropertyNode_ptr   _flols_x_node;                                       
    SGPropertyNode_ptr   _flols_y_node;                                       
    SGPropertyNode_ptr   _flols_z_node;                                       
    double               _flolsApproachAngle                      = 3.0;      ///< glidepath angle for the FLOLS
    double               _flolsHeadingOffsetDeg                   = 0.0;      /// angle in degrees offset from the carrier centerline
    SGVec3d              _flolsPosOffset;                                     
    SGVec3d              _flolsTouchdownPosition;                             
    SGPropertyNode_ptr   _heading_node;
    bool                 _in_to_wind                              = false;    
    bool                 _jbd                                     = false;    
    double               _jbd_elevator_pos_norm                   = 0;        
    double               _jbd_elevator_pos_norm_raw               = 0;        
    double               _jbd_time_constant                       = 0;        
    double               _jbd_transition_time                     = 0;        
    SGPropertyNode_ptr   _latitude_node;                                      
    SGPropertyNode_ptr   _launchbar_state_node;                               
    double               _lineup                                  = 0;        // lineup angle deviation from carrier;
    SGPropertyNode_ptr   _longitude_node;                                     
    SGVec3d              _lsoPosition;                                        /// LSO position
    double               _max_lat                                 = 0;        
    double               _max_lon                                 = 0;        
    double               _min_lat                                 = 0;        
    double               _min_lon                                 = 0;        
    SGGeod               _mOpBoxPos;                                          // operational box limit for carrier.
    bool                 _MPControl                               = false;    // being controlled by MP. Either this or AIControl will be true
    SGPropertyNode_ptr   _pitch_node;
    std::list<ParkPosition>   _ppositions;                                    // List of positions where an aircraft can start.
    double               _rel_wind                                = 0;        
    double               _rel_wind_from_deg                       = 0;        
    double               _rel_wind_speed_kts                      = 0;        
    bool                 _returning                               = false;    // set if the carrier is returning to an operating box
    SGPropertyNode_ptr   _roll_node;
    std::string          _sign;                                               // The sign (pennant) of this carrier; e.g. CVN-68
    SGPropertyNode_ptr   _surface_wind_from_deg_node;                         
    SGPropertyNode_ptr   _surface_wind_speed_node;                            
    std::string          _TACAN_channel_id;                                   
    SGVec3d              _towerPosition;                                      
    bool                 _turn_to_base_course                     = true;     
    bool                 _turn_to_launch_hdg                      = true;     
    bool                 _turn_to_recovery_hdg                    = true;     
    int                  _view_index                              = 0;        
    SGPropertyNode_ptr   _view_position_alt_ft_node;                          
    SGPropertyNode_ptr   _view_position_lat_deg_node;                         
    SGPropertyNode_ptr   _view_position_lon_deg_node;                         
    bool                 _wave_off_lights_demand                  = false;    // when waveoff requested.
    double               _wind_from_deg                           = 0;        //true wind direction
    double               _wind_from_east                          = 0;        // fps
    double               _wind_from_north                         = 0;        // fps
    double               _wind_speed_kts                          = 0;        //true wind speed
    SGPropertyNode_ptr   _is_user_craft;
};

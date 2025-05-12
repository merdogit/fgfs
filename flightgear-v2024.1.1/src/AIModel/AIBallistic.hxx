/*
 * SPDX-FileName: AIBallistic.hxx
 * SPDX-FileComment: AIBase derived class creates an AI ballistic object
 * SPDX-FileCopyrightText: Copyright (C) 2003  David P. Culp - davidculp2@comcast.net
 * SPDX-FileContributor: With major additions by Vivian Meazza, Feb 2008
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <cmath>
#include <string_view>
#include <vector>

#include <simgear/scene/material/mat.hxx>
#include <simgear/structure/SGSharedPtr.hxx>

#include "AIBase.hxx"
#include "AIManager.hxx"


class FGAIBallistic : public FGAIBase
{
public:
    explicit FGAIBallistic(object_type ot = object_type::otBallistic);
    virtual ~FGAIBallistic() = default;

    std::string_view getTypeString(void) const override { return "ballistic"; }
    void readFromScenario(SGPropertyNode* scFileNode) override;

    bool init(ModelSearchOrder searchOrder) override;
    void bind() override;
    void reinit() override;
    void update(double dt) override;

    void Run(double dt);

    void setAzimuth(double az);
    void setElevation(double el);
    void setAzimuthRandomError(double error);
    void setElevationRandomError(double error);
    void setRoll(double rl);
    void setStabilisation(bool val);
    void setDragArea(double a);
    void setLife(double seconds);
    void setBuoyancy(double fpss);
    void setWind_from_east(double fps);
    void setWind_from_north(double fps);
    void setWind(bool val);
    void setCd(double cd);
    void setCdRandomness(double randomness);
    void setMass(double m);
    void setWeight(double w);
    void setNoRoll(bool nr);
    void setRandom(bool r);
    void setLifeRandomness(double randomness);
    void setCollision(bool c);
    void setExpiry(bool e);
    void setImpact(bool i);
    void setImpactReportNode(const std::string&);
    void setContentsNode(const SGPropertyNode_ptr);
    void setFuseRange(double f);
    void setSMPath(const std::string&);
    void setSubID(int i);
    void setSubmodel(const std::string&);
    void setExternalForce(bool f);
    void setForcePath(const std::string&);
    void setContentsPath(const std::string&);
    void setForceStabilisation(bool val);
    void setGroundOffset(double g);
    void setLoadOffset(double l);
    void setSlaved(bool s);
    void setSlavedLoad(bool s);
    void setPch(double e, double dt, double c);
    int setHdg(double az, double dt, double c);
    void setBnk(double r, double dt, double c);
    void setHt(double h, double dt, double c);
    void setSpd(double s, double dt, double c);
    void setParentNodes(const SGPropertyNode_ptr);
    void setParentPos();
    void setOffsetPos(SGGeod pos, double heading, double pitch, double roll);
    void setOffsetVelocity(double dt, SGGeod pos);
    void setTime(double sec);

    double _getTime() const;
    double getRelBrgHitchToUser() const;
    double getElevHitchToUser() const;
    double getLoadOffset() const;
    double getContents();
    double getDistanceToHitch() const;
    double getElevToHitch() const;
    double getBearingToHitch() const;

    SGVec3d getCartHitchPos() const;

    bool getHtAGL(double start);
    bool getSlaved() const;
    bool getSlavedLoad() const;

    FGAIBallistic* ballistic = nullptr;

    static const double slugs_to_kgs; //conversion factor
    static const double slugs_to_lbs; //conversion factor

    SGPropertyNode_ptr _force_node;
    SGPropertyNode_ptr _force_azimuth_node;
    SGPropertyNode_ptr _force_elevation_node;

    // Node for parent model
    SGPropertyNode_ptr _pnode;

    // Nodes for parent parameters
    SGPropertyNode_ptr _p_pos_node;
    SGPropertyNode_ptr _p_lat_node;
    SGPropertyNode_ptr _p_lon_node;
    SGPropertyNode_ptr _p_alt_node;
    SGPropertyNode_ptr _p_agl_node;
    SGPropertyNode_ptr _p_ori_node;
    SGPropertyNode_ptr _p_pch_node;
    SGPropertyNode_ptr _p_rll_node;
    SGPropertyNode_ptr _p_hdg_node;
    SGPropertyNode_ptr _p_vel_node;
    SGPropertyNode_ptr _p_spd_node;

    double _height;
    double _speed;
    double _ht_agl_ft; // height above ground level
    double _azimuth;   // degrees true
    double _elevation; // degrees
    double _rotation;  // degrees
    double _speed_north_fps = 0.0;
    double _speed_east_fps = 0.0;
    double _wind_from_east = 0.0;  // fps
    double _wind_from_north = 0.0; // fps

    double hs;

    void setTgtXOffset(double x);
    void setTgtYOffset(double y);
    void setTgtZOffset(double z);
    void setTgtOffsets(double dt, double c);

    double getTgtXOffset() const;
    double getTgtYOffset() const;
    double getTgtZOffset() const;

    double _tgt_x_offset = 0.0;
    double _tgt_y_offset = 0.0;
    double _tgt_z_offset = 0.0;
    double _elapsed_time;

    SGGeod _parentpos;
    SGGeod _oldpos;
    SGGeod _offsetpos;
    SGGeod _oldoffsetpos;

private:
    double _az_random_error;      // maximum azimuth error in degrees
    double _el_random_error;      // maximum elevation error in degrees
    bool _aero_stabilised;        // if true, object will align with trajectory
    double _drag_area;            // equivalent drag area in ft2
    double _cd;                   // current drag coefficient
    double _init_cd;              // initial drag coefficient
    double _cd_randomness;        // randomness of Cd. 1.0 means +- 100%, 0.0 means no randomness
    double _buoyancy;             // fps^2
    double _life_timer;           // seconds
    bool _wind;                   // if true, local wind will be applied to object
    double _mass;                 // slugs
    bool _random;                 // modifier for Cd, life, az
    double _life_randomness;      // dimension for _random, only applies to life at present
    double _load_resistance;      // ground load resistance N/m^2
    double _frictionFactor = 0.0; // dimensionless modifier for Coefficient of Friction
    bool _solid;                  // if true ground is solid for FDMs
                                  //    double _elevation_m = 0.0;     // ground elevation in meters
    bool _force_stabilised;       // if true, object will align to external force
    bool _slave_to_ac;            // if true, object will be slaved to the parent ac pos and orientation
    bool _slave_load_to_ac;       // if true, object will be slaved to the parent ac pos
    double _contents_lb;          // contents of the object
    double _weight_lb = 0.0;      // weight of the object (no contents if appropriate) (lbs)
    std::string _mat_name;

    bool _report_collision; // if true a collision point with AI Objects is calculated
    bool _report_impact;    // if true an impact point on the terrain is calculated
    bool _external_force;   // if true then apply external force
    bool _report_expiry;

    SGPropertyNode_ptr _impact_report_node; // report node for impact and collision
    SGPropertyNode_ptr _contents_node;      // node for droptank etc. contents

    double _fuse_range = 0.0;

    std::string _force_path;
    std::string _contents_path;

    void handleEndOfLife(double);
    void handle_collision();
    void handle_expiry();
    void handle_impact();
    void report_impact(double elevation, const FGAIBase* target = 0);
    void slaveToAC(double dt);
    void setContents(double c);
    void calcVSHS();
    void calcNE();

    SGVec3d getCartOffsetPos(SGGeod pos, double heading, double pitch, double roll) const;

    double getRecip(double az);
    double getMass() const;

    double _ground_offset = 0.0;
    double _load_offset = 0.0;

    SGVec3d _oldcartoffsetPos;
    SGVec3d _oldcartPos;
};

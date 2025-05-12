// Copyright (C) 2018 - 2024 Fernando García Liñán
// SPDX-License-Identifier: LGPL-2.0-or-later

#pragma once

#include <osgDB/ReaderWriter>
#include <osg/Group>

#include <simgear/props/props.hxx>
#include <simgear/misc/inputvalue.hxx>
#include <simgear/misc/inputcolor.hxx>

class SGLight : public osg::Node {
public:
    enum Type {
        POINT,
        SPOT
    };

    enum Priority {
        HIGH,
        MEDIUM,
        LOW
    };

    class UpdateCallback : public osg::NodeCallback {
    public:
        UpdateCallback() {}
        void operator()(osg::Node* node, osg::NodeVisitor* nv) override;
    };

    SGLight(bool legacyMode = false);
    SGLight(const SGLight& l, const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY);

    META_Node(simgear, SGLight);

    static osg::ref_ptr<osg::Node> appendLight(const SGPropertyNode* configNode,
                                               SGPropertyNode* modelRoot,
                                               bool legacyParseMode);

    void configure(const SGPropertyNode* configNode);

    Type getType() const { return _type; }
    Priority getPriority() { return _priority; }

    // raw accesors : these are used to update shader data,
    // must not access properties
    float getRange() { return _range; }

    osg::Vec4f getAmbient() { return _ambient; }
    osg::Vec4f getDiffuse() { return _diffuse; }
    osg::Vec4f getSpecular() { return _specular; }

    float getConstantAttenuation() { return _constant_attenuation; }
    float getLinearAttenuation() { return _linear_attenuation; }
    float getQuadraticAttenuation() { return _quadratic_attenuation; }
    float getSpotExponent() { return _spot_exponent; }
    float getSpotCutoff() { return _spot_cutoff; }

    osg::Vec3 getColor() { return _color; }
    float getIntensity() const { return _intensity; }

protected:
    virtual ~SGLight();

    simgear::Value_ptr buildValue(const SGPropertyNode* node, double defaultVal);
    simgear::RGBColorValue_ptr buildRGBColorValue(const SGPropertyNode *node, const osg::Vec3f &defaultVal);
    simgear::RGBAColorValue_ptr buildRGBAColorValue(const SGPropertyNode *node, const osg::Vec4f &defaultVal);

    bool _legacyPropertyNames;
    osg::MatrixTransform *_transform {nullptr};
    SGPropertyNode_ptr _modelRoot;

    Type _type {Type::POINT};
    Priority _priority {Priority::LOW};

    simgear::Value_ptr _dim_factor_value;
    float _dim_factor {1.0};
    simgear::Value_ptr _range_value;
    float _range {0.0f};
    simgear::RGBAColorValue_ptr _ambient_value;
    osg::Vec4f _ambient {0.05f, 0.05f, 0.05f, 1.0f};
    simgear::RGBAColorValue_ptr _diffuse_value;
    osg::Vec4f _diffuse {0.8f, 0.8f, 0.8f, 1.0f};
    simgear::RGBAColorValue_ptr _specular_value;
    osg::Vec4f _specular {0.05f, 0.05f, 0.05f, 1.0f};
    simgear::Value_ptr _constant_attenuation_value;
    float _constant_attenuation {1.0f};
    simgear::Value_ptr _linear_attenuation_value;
    float _linear_attenuation {0.0f};
    simgear::Value_ptr _quadratic_attenuation_value;
    float _quadratic_attenuation {0.0f};
    simgear::Value_ptr _spot_exponent_value;
    float _spot_exponent {0.0f};
    simgear::Value_ptr _spot_cutoff_value;
    float _spot_cutoff {180.0f};

    // Physically-based parameters. These are an alternative to the classic
    // ambient/diffuse/specular scheme.

    // Color of emitted light, as a linear sRGB color
    simgear::RGBColorValue_ptr _color_value;
    osg::Vec3f _color {1.0f, 1.0f, 1.0f};
    // The light's brightness. The unit depends on the renderer
    simgear::Value_ptr _intensity_value;
    float _intensity {1.0f};
};

typedef std::vector<osg::ref_ptr<SGLight>> SGLightList;

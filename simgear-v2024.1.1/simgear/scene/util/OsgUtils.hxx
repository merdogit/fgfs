/*
 * SPDX-FileName: OsgUtils.hxx
 * SPDX-FileComment: Useful templates for interfacing to Open Scene Graph
 * SPDX-FileCopyrightText: Copyright (C) 2008  Tim Moore <timoore@redhat.com>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#pragma once

#include <osg/CopyOp>
#include <osg/StateAttribute>
#include <osg/StateSet>
#include <osgText/Text>

/** Typesafe wrapper around OSG's object clone function. Something
 * very similar is in current OSG sources.
 */
namespace osg
{
template <typename T> class ref_ptr;
class CopyOp;
}

namespace simgear
{
template <typename T>
T* clone(const T* object, const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY)
{
    return static_cast<T*>(object->clone(copyop));
}

template<typename T>
T* clone_ref(const osg::ref_ptr<T>& object,
             const osg::CopyOp& copyop  = osg::CopyOp::SHALLOW_COPY)
{
    return static_cast<T*>(object->clone(copyop));
}

}

namespace osg
{
class AlphaFunc;
class BlendColor;
class BlendEquation;
class BlendFunc;
class ClampColor;
class ColorMask;
class ColorMatrix;
class CullFace;
class Depth;
class Fog;
class FragmentProgram;
class FrontFace;
class LightModel;
class LineStipple;
class LineWidth;
class LogicOp;
class Material;
class Multisample;
class Point;
class PointSprite;
class PolygonMode;
class PolygonOffset;
class PolygonStipple;
class Program;
class Scissor;
class ShadeModel;
class Stencil;
class StencilTwoSided;
class TexEnv;
class TexEnvCombine;
class TexEnvFilter;
class TexGen;
class TexMat;
class Texture1D;
class Texture2D;
class Texture2DArray;
class Texture3D;
class TextureCubeMap;
class TextureRectangle;
class VertexProgram;
class Viewport;
}

namespace simgear
{
namespace osgutils
{
using namespace osg;

template<StateAttribute::Type T>
struct TypeHolder
{
    static const StateAttribute::Type type = T;
};

template<typename AT> struct AttributeType;
template<typename AT> struct TexAttributeType;

template<>
struct AttributeType<AlphaFunc>
    : public TypeHolder<StateAttribute::ALPHAFUNC>
{};

template<>
struct AttributeType<BlendColor>
    : public TypeHolder<StateAttribute::BLENDCOLOR>
{};

template<>
struct AttributeType<BlendEquation>
    : public TypeHolder<StateAttribute::BLENDEQUATION>
{};

template<>
struct AttributeType<BlendFunc>
    : public TypeHolder<StateAttribute::BLENDFUNC>
{};

template<>
struct AttributeType<ClampColor>
    : public TypeHolder<StateAttribute::CLAMPCOLOR>
{};

template<>
struct AttributeType<ColorMask>
    : public TypeHolder<StateAttribute::COLORMASK>
{};

template<>
struct AttributeType<ColorMatrix>
    : public TypeHolder<StateAttribute::COLORMATRIX>
{};

template<>
struct AttributeType<CullFace>
    : public TypeHolder<StateAttribute::CULLFACE>
{};


template<>
struct AttributeType<osg::Depth> // Conflicts with Xlib
    : public TypeHolder<StateAttribute::DEPTH>
{};

template<>
struct AttributeType<Fog>
    : public TypeHolder<StateAttribute::FOG>
{};

template<>
struct AttributeType<FragmentProgram>
    : public TypeHolder<StateAttribute::FRAGMENTPROGRAM>
{};

template<>
struct AttributeType<FrontFace>
    : public TypeHolder<StateAttribute::FRONTFACE>
{};

template<>
struct AttributeType<LightModel>
    : public TypeHolder<StateAttribute::LIGHTMODEL>
{};

template<>
struct AttributeType<LineStipple>
    : public TypeHolder<StateAttribute::LINESTIPPLE>
{};

template<>
struct AttributeType<LineWidth>
    : public TypeHolder<StateAttribute::LINEWIDTH>
{};

template<>
struct AttributeType<LogicOp>
    : public TypeHolder<StateAttribute::LOGICOP>
{};

template<>
struct AttributeType<Material>
    : public TypeHolder<StateAttribute::MATERIAL>
{};

template<>
struct AttributeType<Multisample>
    : public TypeHolder<StateAttribute::MULTISAMPLE>
{};

template<>
struct AttributeType<Point>
    : public TypeHolder<StateAttribute::POINT>
{};

template<>
struct TexAttributeType<PointSprite>
    : public TypeHolder<StateAttribute::POINTSPRITE>
{};

template<>
struct AttributeType<PolygonMode>
    : public TypeHolder<StateAttribute::POLYGONMODE>
{};

template<>
struct AttributeType<PolygonOffset>
    : public TypeHolder<StateAttribute::POLYGONOFFSET>
{};

template<>
struct AttributeType<PolygonStipple>
    : public TypeHolder<StateAttribute::POLYGONSTIPPLE>
{};

template<>
struct AttributeType<Program>
    : public TypeHolder<StateAttribute::PROGRAM>
{};

template<>
struct AttributeType<Scissor>
    : public TypeHolder<StateAttribute::SCISSOR>
{};

template<>
struct AttributeType<ShadeModel>
    : public TypeHolder<StateAttribute::SHADEMODEL>
{};

template<>
struct AttributeType<Stencil>
    : public TypeHolder<StateAttribute::STENCIL>
{};

template<>
struct AttributeType<StencilTwoSided>
    : public TypeHolder<StateAttribute::STENCIL>
{};

// TexEnvCombine is not a subclass of TexEnv, so we can't do a
// typesafe access of the attribute.
#if 0
template<>
struct TexAttributeType<TexEnv>
    : public TypeHolder<StateAttribute::TEXENV>
{};

template<>
struct TexAttributeType<TexEnvCombine>
    : public TypeHolder<StateAttribute::TEXENV>
{};
#endif

template<>
struct TexAttributeType<TexEnvFilter>
    : public TypeHolder<StateAttribute::TEXENVFILTER>
{};

template<>
struct TexAttributeType<TexGen>
    : public TypeHolder<StateAttribute::TEXGEN>
{};

template<>
struct TexAttributeType<TexMat>
    : public TypeHolder<StateAttribute::TEXMAT>
{};

template<>
struct TexAttributeType<Texture>
    : public TypeHolder<StateAttribute::TEXTURE>
{};

template<>
struct AttributeType<VertexProgram>
    : public TypeHolder<StateAttribute::VERTEXPROGRAM>
{};

template<>
struct AttributeType<Viewport>
    : public TypeHolder<StateAttribute::VIEWPORT>
{};

osgText::Text::AlignmentType mapAlignment(const std::string& val);

} // namespace osgutils

template<typename AT>
inline AT* getStateAttribute(osg::StateSet* ss)
{
    return static_cast<AT*>(ss->getAttribute(osgutils::AttributeType<AT>::type));
}

template<typename AT>
inline const AT* getStateAttribute(const osg::StateSet* ss)
{
    return static_cast<const AT*>(ss->getAttribute(osgutils::AttributeType<AT>::type));
}

template<typename AT>
inline AT* getStateAttribute(unsigned int unit, osg::StateSet* ss)
{
    return static_cast<AT*>(ss->getTextureAttribute(unit, osgutils::TexAttributeType<AT>
                                                    ::type));
}

template<typename AT>
inline const AT* getStateAttribute(unsigned int unit, const osg::StateSet* ss)
{
    return static_cast<const AT*>(ss->getTextureAttribute(unit,
                                                          osgutils::TexAttributeType<AT>
                                                          ::type));
}
} // namespace simgear

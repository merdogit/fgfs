// VPBRasterRenderer.hxx -- Raster renderer for water and other features
//
// Copyright (C) 2024 Stuart Buchanan
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

#ifndef VPBRASTERRENDERER
#define VPBRASTERRENDERER 1

#include <mutex>

#include <osg/MatrixTransform>
#include <osg/Geode>
#include <osg/Geometry>

#include <osgTerrain/TerrainTechnique>
#include <osgTerrain/Locator>

#include <simgear/bucket/newbucket.hxx>
#include <simgear/bvh/BVHMaterial.hxx>
#include <simgear/math/SGGeometry.hxx>
#include <simgear/scene/material/EffectGeode.hxx>
#include <simgear/scene/material/matlib.hxx>
#include <simgear/scene/tgdb/AreaFeatureBin.hxx>
#include <simgear/scene/tgdb/LightBin.hxx>
#include <simgear/scene/tgdb/LineFeatureBin.hxx>
#include <simgear/scene/tgdb/CoastlineBin.hxx>

using namespace osgTerrain;

namespace simgear {

class VPBRasterRenderer
{
    public:
        VPBRasterRenderer(const SGPropertyNode* propertyNode, osg::ref_ptr<TerrainTile> tile, const osg::Vec3d world, unsigned int tile_width, unsigned int tile_height);

        virtual osg::ref_ptr<osg::Texture2D> generateCoastTexture();
        virtual osg::ref_ptr<osg::Image> generateCoastImage();

        static void addCoastlineList(SGBucket bucket, CoastlineBinList areaList);
        static void unloadFeatures(SGBucket bucket);
        static osg::ref_ptr<osg::Texture2D> getDefaultCoastlineTexture() { return _defaultCoastlineTexture; }

    protected:

        virtual void addCoastline(osg::Image* waterTexture, LineFeatureBin::LineFeature line, unsigned int waterTextureSize, float tileSize, float coastWidth);
        virtual void updateWaterTexture(osg::Image* waterTexture, unsigned int waterTextureSize, osg::Vec4 color, float x, float y);
        virtual void writeShoreStripe(osg::Image* waterTexture, unsigned int waterTextureSize, float tileSize, float coastWidth, float x, float y, int dx, int dy);

        osg::ref_ptr<osgTerrain::Locator> _masterLocator;
        osg::ref_ptr<TerrainTile> _tile;
        osg::Vec3d _world;
        unsigned int _tile_width;
        unsigned int _tile_height;

        typedef std::pair<SGBucket, CoastlineBinList> BucketCoastlineBinList;
        inline static std::list<BucketCoastlineBinList>  _coastFeatureLists;
        inline static std::mutex _coastFeatureLists_mutex;  // protects the _areaFeatureLists;

        inline static osg::ref_ptr<osg::Texture2D> _defaultCoastlineTexture;
        inline static std::mutex _defaultCoastlineTexture_mutex;

        inline static int _coast_features_lod_range = 4;
        inline static unsigned int _waterTextureSize = 2048;
        inline static float _coastWidth = 150.0;
};

};

#endif

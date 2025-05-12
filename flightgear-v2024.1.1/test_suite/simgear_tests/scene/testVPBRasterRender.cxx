// do some test relating to the VPBRasterRenderer

#include <iostream>
#include <config.h>

#include <osgTerrain/TerrainTile>
#include <osg/Texture2D>
#include <osgDB/WriteFile>
#include <osgDB/Registry>

#include <simgear/scene/tgdb/CoastlineBin.hxx>
#include <simgear/scene/tgdb/VPBRasterRenderer.hxx>

#include "testVPBRasterRender.hxx"

using std::cout;
using std::endl;
using namespace simgear;

void VPBRasterRenderTests::setUp()
{
    CoastlineBinList coastlineBinList;

    SGPath test_data = SGPath(SGPath::fromUtf8(FG_TEST_SUITE_DATA), "/scenery/2892939_Coastline.txt");
    CPPUNIT_ASSERT(test_data.exists());
    osg::ref_ptr<CoastlineBin> coastlineBinA = new CoastlineBin(test_data);
    coastlineBinList.push_back(coastlineBinA);
    _bucket = SGBucket(2892939);
    VPBRasterRenderer::addCoastlineList(_bucket, coastlineBinList);
}


// The VPBRasterRenderer unit test.
void VPBRasterRenderTests::testRaster()
{

    // Generate a raster that includes some coastline
    osg::ref_ptr<osgTerrain::TerrainTile> tile = new osgTerrain::TerrainTile();
    float frac = 1.0/32.0;
    //unsigned int x = (unsigned int) ((_bucket.get_corner(0).getLongitudeDeg() - _bucket.get_chunk_lon()) * 32.0);
    //unsigned int y = (unsigned int) ((_bucket.get_corner(0).getLatitudeDeg() - _bucket.get_chunk_lat()) * 32.0);
    unsigned int x = 28;
    unsigned int y = 4;
    SGVec3d center, bottom_left, top_right, bottom_right, top_left;
    float lat[] = { _bucket.get_chunk_lat() + y * frac, _bucket.get_chunk_lat() + y * frac + frac};
    float lon[] = { _bucket.get_chunk_lon() + x * frac, _bucket.get_chunk_lon() + x * frac + frac};


    SGGeodesy::SGGeodToCart(SGGeod::fromDeg(_bucket.get_chunk_lon() + x * frac + 0.5 * frac, _bucket.get_chunk_lat() + y * frac + 0.5 * frac), center);
    SGGeodesy::SGGeodToCart(SGGeod::fromDeg(lon[0], lat[0]), bottom_left);
    SGGeodesy::SGGeodToCart(SGGeod::fromDeg(lon[1], lat[0]), bottom_right);
    SGGeodesy::SGGeodToCart(SGGeod::fromDeg(lon[0], lat[1]), top_left);
    SGGeodesy::SGGeodToCart(SGGeod::fromDeg(lon[1], lat[1]), top_right);

    SGVec3d s = bottom_right - bottom_left;
    SGVec3d t = top_left - bottom_left;
    SGVec3d u = top_right - top_left;
    SGVec3d v = top_right - bottom_right;
    auto width = 0.5 * (length(s) + length(u));
    auto height = 0.5 * (length(t) + length(v));

    osgTerrain::TileID tileID = osgTerrain::TileID(6, x, y);
    osgTerrain::Locator* locator = new osgTerrain::Locator();
    locator->setCoordinateSystemType(osgTerrain::Locator::PROJECTED);

    // Do some matrix math.
    // We need (0,0,0) to map to bottom_left
    //         (1,0,0) to map to bottom_right
    //         (0,1,0) to map to top_left
    //         (1,1,0) to map to top_right


    SGVec3d o = bottom_left;
    osg::Matrixd matrix = osg::Matrixd(s.x(), s.y(), s.z(), 0.0,
                                       t.x(), t.y(), t.z(), 0.0,
                                       0.0, 0.0, 1.0, 0.0,
                                        o.x(),  o.y(), o.z(), 1.0);
    locator->setTransform(matrix);

    tile->setTileID(tileID);
    tile->setLocator(locator);

    osg::Texture2D* texture;
    VPBRasterRenderer* renderer = new VPBRasterRenderer(0, tile, toOsg(center), width, height);
    texture = renderer->generateCoastTexture();
    CPPUNIT_ASSERT(texture != renderer->getDefaultCoastlineTexture());

    // Uncomment these next lines to output the raster as a png file.
    // auto image = renderer->generateCoastImage();
    // osgDB::Registry* registry = osgDB::Registry::instance();
    // osgDB::Options* nopt = registry->getOptions();
    //osgDB::ReaderWriter::WriteResult res = registry->writeImage(*image, "/tmp/raster.png", nopt);

    // Generate a raster at a higher LOD. which should generate the default raster.
    tileID = osgTerrain::TileID(1, x, y);
    tile->setTileID(tileID);
    renderer = new VPBRasterRenderer(0, tile, toOsg(center), width, height);
    texture = renderer->generateCoastTexture();
    CPPUNIT_ASSERT(texture == renderer->getDefaultCoastlineTexture());

    // Generate a raster where we know there is no coastline, which should generate the default raster
    tileID = osgTerrain::TileID(1, 0, 0);
    tile->setTileID(tileID);
    renderer = new VPBRasterRenderer(0, tile, toOsg(center), width, height);
    texture = renderer->generateCoastTexture();
    CPPUNIT_ASSERT(texture == renderer->getDefaultCoastlineTexture());
}

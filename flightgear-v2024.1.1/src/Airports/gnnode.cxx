#include "config.h"

#include <Main/globals.hxx>
#include <Navaids/NavDataCache.hxx>
#include <Scenery/scenery.hxx>

#include "gnnode.hxx"
#include "groundnetwork.hxx"

using namespace flightgear;


/**************************************************************************
 * FGTaxiNode
 *************************************************************************/

FGTaxiNode::FGTaxiNode(FGPositioned::Type ty, int index, const SGGeod& pos,
                       bool aOnRunway, int aHoldType,
                       const std::string& ident) : FGPositioned(NavDataCache::instance()->createTransientID(), ty, ident, pos),
                                                   m_index(index),
                                                   isOnRunway(aOnRunway),
                                                   holdType(aHoldType),
                                                   m_isPushback(false)
{
}

void FGTaxiNode::setElevation(double val)
{
    // ignored for the moment
}

double FGTaxiNode::getElevationFt()
{
    const SGGeod& pos = geod();
    if (pos.getElevationFt() == 0.0) {
        SGGeod center2 = pos;
        FGScenery* local_scenery = globals->get_scenery();
        center2.setElevationM(SG_MAX_ELEVATION_M);
        if (local_scenery) {
            double elevationEnd = -100;
            if (local_scenery->get_elevation_m(center2, elevationEnd, NULL)) {
                SGGeod newPos = pos;
                newPos.setElevationM(elevationEnd);
                // this will call modifyPosition to update mPosition
                modifyPosition(newPos);
            }
        } else {
            SG_LOG(SG_TERRAIN, SG_ALERT, "Terrain not inited");
        }
    }

    return pos.getElevationFt();
}

int FGTaxiNode::getIndex() const
{
    return m_index;
}

void FGTaxiNode::setIsPushback()
{
    m_isPushback = true;
}

double FGTaxiNode::getElevationM()
{
    return getElevationFt() * SG_FEET_TO_METER;
}

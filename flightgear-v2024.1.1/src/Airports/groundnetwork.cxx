/*
 * SPDX-FileName: groundnet.cxx
 * SPDX-FileComment: Implementation of the FlightGear airport ground handling code
 * SPDX-FileCopyrightText: Copyright (C) 2004 Durk Talsma
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <config.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <map>

#include <simgear/scene/util/OsgMath.hxx>
#include <simgear/debug/logstream.hxx>
#include <simgear/math/SGGeometryFwd.hxx>
#include <simgear/math/SGIntersect.hxx>
#include <simgear/math/SGLineSegment.hxx>
#include <simgear/structure/exception.hxx>
#include <simgear/timing/timestamp.hxx>

#include <Airports/airport.hxx>
#include <Airports/runways.hxx>
#include <Scenery/scenery.hxx>

#include "groundnetwork.hxx"

using std::string;


/***************************************************************************
 * FGTaxiSegment
 **************************************************************************/

FGTaxiSegment::FGTaxiSegment(FGTaxiNode* aStart, FGTaxiNode* aEnd) : startNode(aStart),
                                                                     endNode(aEnd),
                                                                     isActive(0),
                                                                     index(0),
                                                                     oppositeDirection(0)
{
    if (!aStart || !aEnd) {
        throw sg_exception("Missing node arguments creating FGTaxiSegment");
    }
}

SGGeod FGTaxiSegment::getCenter() const
{
    FGTaxiNode *start(getStart()), *end(getEnd());
    double heading, length, az2;
    SGGeodesy::inverse(start->geod(), end->geod(), heading, az2, length);
    return SGGeodesy::direct(start->geod(), heading, length * 0.5);
}

FGTaxiNodeRef FGTaxiSegment::getEnd() const
{
    return const_cast<FGTaxiNode*>(endNode);
}

FGTaxiNodeRef FGTaxiSegment::getStart() const
{
    return const_cast<FGTaxiNode*>(startNode);
}

double FGTaxiSegment::getLength() const
{
    return dist(getStart()->cart(), getEnd()->cart());
}

double FGTaxiSegment::getHeading() const
{
    return SGGeodesy::courseDeg(getStart()->geod(), getEnd()->geod());
}

void FGTaxiSegment::block(int id, time_t blockTime, time_t now)
{
    BlockListIterator i = blockTimes.begin();
    while (i != blockTimes.end()) {
        if (i->getId() == id)
            break;
        i++;
    }
    if (i == blockTimes.end()) {
        blockTimes.push_back(Block(id, blockTime, now));
        sort(blockTimes.begin(), blockTimes.end());
    } else {
        i->updateTimeStamps(blockTime, now);
    }
}

// The segment has a block if any of the block times listed in the block list is
// smaller than the current time.
bool FGTaxiSegment::hasBlock(time_t now)
{
    for (BlockListIterator i = blockTimes.begin(); i != blockTimes.end(); i++) {
        if (i->getBlockTime() < now)
            return true;
    }
    return false;
}

void FGTaxiSegment::unblock(time_t now)
{
    if (blockTimes.empty())
        return;

    if (blockTimes.front().getTimeStamp() < (now - 30)) {
        blockTimes.erase(blockTimes.begin());
    }
}

/***************************************************************************
 * FGTaxiRoute
 **************************************************************************/

FGTaxiRoute::FGTaxiRoute(const FGTaxiNodeVector& nds, const intVec& rts, double dist, int /* dpth */) : nodes(nds),
                                                                                                        routes(rts),
                                                                                                        distance(dist)
{
    currNode = nodes.begin();
    currRoute = routes.begin();

    if (nodes.size() != (routes.size()) + 1) {
        SG_LOG(SG_GENERAL, SG_ALERT, "ALERT: Misconfigured TaxiRoute : " << nodes.size() << " " << routes.size());
    }
};

bool FGTaxiRoute::next(FGTaxiNodeRef& node, int* rte)
{
    if (nodes.size() != (routes.size()) + 1) {
        throw sg_range_exception("Misconfigured taxi route");
    }

    if (currNode == nodes.end())
        return false;
    node = *(currNode);
    if (currNode != nodes.begin()) {
        // work-around for FLIGHTGEAR-NJN: throw an exception here
        // instead of crashing, to aid debugging
        if (currRoute == routes.end()) {
            throw sg_range_exception("Misconfigured taxi route");
        }

        *rte = *(currRoute);
        ++currRoute;
    } else {
        // Handle special case for the first node.
        *rte = -1 * *(currRoute);
    }
    ++currNode;
    return true;
};

/***************************************************************************
 * FGGroundNetwork()
 **************************************************************************/

FGGroundNetwork::FGGroundNetwork(FGAirport* airport) : parent(airport)
{
    hasNetwork = false;
    version = 0;
    networkInitialized = false;
}

FGGroundNetwork::~FGGroundNetwork()
{
    for (auto seg : segments) {
        delete seg;
    }
}

void FGGroundNetwork::init()
{
    if (networkInitialized) {
        SG_LOG(SG_GENERAL, SG_WARN, "duplicate ground-network init");
        return;
    }

    hasNetwork = true;
    int index = 1;

    // establish pairing of segments
    for (auto segment : segments) {
        segment->setIndex(index++);

        if (segment->oppositeDirection) {
            continue; // already established
        }

        FGTaxiSegment* opp = findSegment(segment->endNode, segment->startNode);
        if (opp) {
            assert(opp->oppositeDirection == NULL);
            segment->oppositeDirection = opp;
            opp->oppositeDirection = segment;
        }

        // establish node -> segment end cache
        m_segmentsEndingAtNodeMap.insert(NodeFromSegmentMap::value_type{segment->getEnd(), segment});
    }

    networkInitialized = true;
}

FGTaxiNodeRef FGGroundNetwork::findNearestNode(const SGGeod& aGeod) const
{
    double d = DBL_MAX;
    SGVec3d cartPos = SGVec3d::fromGeod(aGeod);
    FGTaxiNodeRef result;

    FGTaxiNodeVector::const_iterator it;
    for (it = m_nodes.begin(); it != m_nodes.end(); ++it) {
        double localDistanceSqr = distSqr(cartPos, (*it)->cart());
        if (localDistanceSqr < d) {
            d = localDistanceSqr;
            result = *it;
        }
    }

    return result;
}

FGTaxiNodeRef FGGroundNetwork::findNearestNodeOffRunway(const SGGeod& aGeod, FGRunway* rwy, double marginM) const
{
    FGTaxiNodeVector nodes;
    const SGLineSegmentd runwayLine(rwy->cart(), SGVec3d::fromGeod(rwy->end()));
    const double marginMSqr = marginM * marginM;
    const SGVec3d cartPos = SGVec3d::fromGeod(aGeod);

    std::copy_if(m_nodes.begin(), m_nodes.end(), std::back_inserter(nodes),
                 [runwayLine, cartPos, marginMSqr](const FGTaxiNodeRef& a) {
                     if (a->getIsOnRunway()) return false;

                     // exclude parking positions from consideration. This helps to
                     // exclude airports whose ground nets only list parking positions,
                     // since these typically produce bad results. See discussion in
                     // https://sourceforge.net/p/flightgear/codetickets/2110/
                     if (a->type() == FGPositioned::PARKING) return false;

                     return (distSqr(runwayLine, a->cart()) >= marginMSqr);
                 });


    // find closest of matching nodes
    auto node = std::min_element(nodes.begin(), nodes.end(),
                                 [cartPos](const FGTaxiNodeRef& a, const FGTaxiNodeRef& b) { return distSqr(cartPos, a->cart()) < distSqr(cartPos, b->cart()); });

    if (node == nodes.end()) {
        return FGTaxiNodeRef();
    }

    return *node;
}

FGTaxiNodeRef FGGroundNetwork::findNearestNodeOnRunwayEntry(const SGGeod& aGeod) const
{
    double d = DBL_MAX;
    SGVec3d cartPos = SGVec3d::fromGeod(aGeod);
    FGTaxiNodeRef result;
    for (auto it = m_nodes.begin(); it != m_nodes.end(); ++it) {
        if (!(*it)->getIsOnRunway())
            continue;
        double localDistanceSqr = distSqr(cartPos, (*it)->cart());
        if (localDistanceSqr < d) {
            SG_LOG(SG_AI, SG_BULK, "findNearestNodeOnRunway from Threshold " << localDistanceSqr);
            d = localDistanceSqr;
            result = *it;
        }
    }

    return result;
}

FGTaxiNodeRef FGGroundNetwork::findNearestNodeOnRunwayExit(const SGGeod& aGeod, FGRunway* aRunway) const
{
    double d = DBL_MAX;
    SGVec3d cartPos = SGVec3d::fromGeod(aGeod);
    FGTaxiNodeRef result = 0;
    FGTaxiNodeVector::const_iterator it;
    if (aRunway) {
        SG_LOG(SG_AI, SG_BULK, "findNearestNodeOnRunwayExit " << aRunway->ident() << " " << aRunway->headingDeg());
        for (it = m_nodes.begin(); it != m_nodes.end(); ++it) {
            if (!(*it)->getIsOnRunway())
                continue;
            double localDistanceSqr = distSqr(cartPos, (*it)->cart());
            double headingTowardsExit = SGGeodesy::courseDeg(aGeod, (*it)->geod());
            double diff = fabs(SGMiscd::normalizePeriodic(-180, 180, aRunway->headingDeg() - headingTowardsExit));
            SG_LOG(SG_AI, SG_BULK, "findNearestNodeOnRunwayExit Diff : " << diff << " Id : " << (*it)->getIndex());
            if (diff > 10) {
                // Only ahead
                continue;
            }
            FGTaxiNodeVector exitSegments = findSegmentsFrom((*it));
            // Some kind of star
            if (exitSegments.size() > 2) {
                continue;
            }
            // two segments and next points are on runway, too. Must be a segment before end
            // single runway point not at end is ok
            if (exitSegments.size() == 2 &&
                ((*exitSegments.at(0)).getIsOnRunway() || (*exitSegments.at(0)).getIsOnRunway())) {
                continue;
            }
            if (exitSegments.empty()) {
                SG_LOG(SG_AI, SG_ALERT, "findNearestNodeOnRunwayExit Broken :" << diff << " Id : " << (*it)->getIndex() << " Apt : " << aRunway->airport()->getId());
                continue;
            }
            double exitHeading = SGGeodesy::courseDeg((*it)->geod(),
                                                      (exitSegments.back())->geod());
            diff = fabs(SGMiscd::normalizePeriodic(-180, 180, aRunway->headingDeg() - exitHeading));
            SG_LOG(SG_AI, SG_BULK, "findNearestNodeOnRunwayExit2 Diff :" << diff << " Id : " << (*it)->getIndex());
            if (diff > 70) {
                // Only exits going in our direction
                continue;
            }
            if (localDistanceSqr < d) {
                SG_LOG(SG_AI, SG_BULK, "findNearestNodeOnRunwayExit3 " << localDistanceSqr << " " << (*it)->getIndex());
                d = localDistanceSqr;
                result = *it;
            }
        }
    } else {
        SG_LOG(SG_AI, SG_BULK, "No Runway findNearestNodeOnRunwayExit");
    }
    if (result) {
        SG_LOG(SG_AI, SG_BULK, "findNearestNodeOnRunwayExit found :" << result->getIndex());
        return result;
    }
    // Ok then fallback only exits ahead
    for (it = m_nodes.begin(); it != m_nodes.end(); ++it) {
        if (!(*it)->getIsOnRunway())
            continue;
        double localDistanceSqr = distSqr(cartPos, (*it)->cart());
        if (aRunway) {
            double headingTowardsExit = SGGeodesy::courseDeg(aGeod, (*it)->geod());
            double diff = fabs(aRunway->headingDeg() - headingTowardsExit);
            SG_LOG(SG_AI, SG_BULK, "findNearestNodeOnRunwayExitFallback1 " << aRunway->headingDeg() << " "
                                                                           << " Diff : " << diff << " " << (*it)->getIndex());
            if (diff > 10) {
                // Only ahead
                continue;
            }
        }
        if (localDistanceSqr < d) {
            SG_LOG(SG_AI, SG_BULK, "findNearestNodeOnRunwayExitFallback1 " << localDistanceSqr);
            d = localDistanceSqr;
            result = *it;
        }
    }
    if (result) {
        return result;
    }
    // Ok then fallback only exits
    for (it = m_nodes.begin(); it != m_nodes.end(); ++it) {
        if (!(*it)->getIsOnRunway())
            continue;
        double localDistanceSqr = distSqr(cartPos, (*it)->cart());
        if (localDistanceSqr < d) {
            SG_LOG(SG_AI, SG_BULK, "findNearestNodeOnRunwayExitFallback2 " << localDistanceSqr);
            d = localDistanceSqr;
            result = *it;
        }
    }
    if (result) {
        return result;
    } else {
        SG_LOG(SG_AI, SG_WARN, "No runway exit found " << aRunway->airport()->getId() << "/" << aRunway->name());
        return 0;
    }
}

FGTaxiSegment* FGGroundNetwork::findOppositeSegment(unsigned int index) const
{
    FGTaxiSegment* seg = findSegment(index);
    if (!seg)
        return NULL;
    return seg->opposite();
}

const FGParkingList& FGGroundNetwork::allParkings() const
{
    return m_parkings;
}

FGTaxiSegment* FGGroundNetwork::findSegment(unsigned idx) const
{
    if ((idx > 0) && (idx <= segments.size()))
        return segments[idx - 1];
    else {
        //cerr << "Alert: trying to find invalid segment " << idx << endl;
        return 0;
    }
}

FGTaxiSegment* FGGroundNetwork::findSegment(const FGTaxiNode* from, const FGTaxiNode* to) const
{
    if (from == 0) {
        return NULL;
    }

    // completely boring linear search of segments. Can be improved if/when
    // this ever becomes a hot-spot
    for (auto seg : segments) {
        if (seg->startNode != from) {
            continue;
        }

        if ((to == 0) || (seg->endNode == to)) {
            return seg;
        }
    }

    return NULL; // not found
}

static int edgePenalty(FGTaxiNode* tn)
{
    return (tn->type() == FGPositioned::PARKING ? 10000 : 0) +
           (tn->getIsOnRunway() ? 1000 : 0);
}

class ShortestPathData
{
public:
    ShortestPathData() : score(HUGE_VAL)
    {
    }

    double score;
    FGTaxiNodeRef previousNode;
};

FGTaxiRoute FGGroundNetwork::findShortestRoute(FGTaxiNode* start, FGTaxiNode* end, bool fullSearch)
{
    if (!start || !end) {
        throw sg_exception("Bad arguments to findShortestRoute");
    }
    //implements Dijkstra's algorithm to find shortest distance route from start to end
    //taken from http://en.wikipedia.org/wiki/Dijkstra's_algorithm
    FGTaxiNodeVector unvisited(m_nodes);
    std::map<FGTaxiNode*, ShortestPathData> searchData;

    searchData[start].score = 0.0;

    while (!unvisited.empty()) {
        // find lowest scored unvisited
        FGTaxiNodeRef best = unvisited.front();
        for (auto i : unvisited) {
            if (searchData[i].score < searchData[best].score) {
                best = i;
            }
        }

        // remove 'best' from the unvisited set
        FGTaxiNodeVectorIterator newend =
            remove(unvisited.begin(), unvisited.end(), best);
        unvisited.erase(newend, unvisited.end());

        if (best == end) { // found route or best not connected
            break;
        }

        for (auto target : findSegmentsFrom(best)) {
            double edgeLength = dist(best->cart(), target->cart());
            double alt = searchData[best].score + edgeLength + edgePenalty(target);
            if (alt < searchData[target].score) { // Relax (u,v)
                searchData[target].score = alt;
                searchData[target].previousNode = best;
            }
        } // of outgoing arcs/segments from current best node iteration
    }     // of unvisited nodes remaining

    if (searchData[end].score == HUGE_VAL) {
        // no valid route found
        if (fullSearch && start && end) {
            SG_LOG(SG_GENERAL, SG_ALERT,
                   "Failed to find route from waypoint " << start->getIndex() << " to "
                                                         << end->getIndex() << " at " << parent->getId());
        }

        return FGTaxiRoute();
    }

    // assemble route from backtrace information
    FGTaxiNodeVector nodes;
    intVec routes;
    FGTaxiNode* bt = end;

    while (searchData[bt].previousNode != 0) {
        nodes.push_back(bt);
        FGTaxiSegment* segment = findSegment(searchData[bt].previousNode, bt);
        int idx = segment->getIndex();
        routes.push_back(idx);
        bt = searchData[bt].previousNode;
    }
    nodes.push_back(start);
    reverse(nodes.begin(), nodes.end());
    reverse(routes.begin(), routes.end());
    return FGTaxiRoute(nodes, routes, searchData[end].score, 0);
}

void FGGroundNetwork::unblockAllSegments(time_t now)
{
    for (auto& seg : segments) {
        seg->unblock(now);
    }
}

void FGGroundNetwork::blockSegmentsEndingAt(const FGTaxiSegment* seg, int blockId, time_t blockTime, time_t now)
{
    if (!seg)
        throw sg_exception("Passed invalid segment");

    const auto range = m_segmentsEndingAtNodeMap.equal_range(seg->getEnd());
    for (auto it = range.first; it != range.second; ++it) {
        // our inbound segment will be included, so skip it
        if (it->second == seg)
            continue;

        it->second->block(blockId, blockTime, now);
    }
}

FGTaxiNodeRef FGGroundNetwork::findNodeByIndex(int index) const
{
    FGTaxiNodeVector::const_iterator it;
    for (it = m_nodes.begin(); it != m_nodes.end(); ++it) {
        if ((*it)->getIndex() == index) {
            return *it;
        }
    }

    return FGTaxiNodeRef();
}

FGParkingRef FGGroundNetwork::getParkingByIndex(unsigned int index) const
{
    FGTaxiNodeRef tn = findNodeByIndex(index);
    if (!tn.valid() || (tn->type() != FGPositioned::PARKING)) {
        return FGParkingRef();
    }

    return FGParkingRef(static_cast<FGParking*>(tn.ptr()));
}

FGParkingRef FGGroundNetwork::findParkingByName(const string& name) const
{
    auto it = std::find_if(m_parkings.begin(), m_parkings.end(), [name](const FGParkingRef& p) {
        return p->ident() == name;
    });

    if (it == m_parkings.end())
        return nullptr;

    return *it;
}

void FGGroundNetwork::addSegment(const FGTaxiNodeRef& from, const FGTaxiNodeRef& to)
{
    FGTaxiSegment* seg = new FGTaxiSegment(from, to);
    segments.push_back(seg);

    FGTaxiNodeVector::iterator it = std::find(m_nodes.begin(), m_nodes.end(), from);
    if (it == m_nodes.end()) {
        m_nodes.push_back(from);
    }

    it = std::find(m_nodes.begin(), m_nodes.end(), to);
    if (it == m_nodes.end()) {
        m_nodes.push_back(to);
    }
}

void FGGroundNetwork::addParking(const FGParkingRef& park)
{
    m_parkings.push_back(park);


    FGTaxiNodeVector::iterator it = std::find(m_nodes.begin(), m_nodes.end(), park);
    if (it == m_nodes.end()) {
        m_nodes.push_back(park);
    }
}

FGTaxiNodeVector FGGroundNetwork::findSegmentsFrom(const FGTaxiNodeRef& from) const
{
    FGTaxiNodeVector result;
    FGTaxiSegmentVector::const_iterator it;
    for (it = segments.begin(); it != segments.end(); ++it) {
        if ((*it)->getStart() == from) {
            result.push_back((*it)->getEnd());
        }
    }

    return result;
}

FGTaxiSegment* FGGroundNetwork::findSegmentByHeading(const FGTaxiNode* from, const double heading) const
{
    if (from == 0) {
        return NULL;
    }

    FGTaxiSegment* best = nullptr;

    // completely boring linear search of segments. Can be improved if/when
    // this ever becomes a hot-spot
    for (auto seg : segments) {
        if (seg->startNode != from) {
            continue;
        }

        if (!best || fabs(best->getHeading() - heading) > fabs(seg->getHeading() - heading)) {
            best = seg;
        }
    }

    return best; // not found
}

const intVec& FGGroundNetwork::getApproachFrequencies() const
{
    return freqApproach;
}

const intVec& FGGroundNetwork::getTowerFrequencies() const
{
    return freqTower;
}

const intVec& FGGroundNetwork::getGroundFrequencies() const
{
    return freqGround;
}

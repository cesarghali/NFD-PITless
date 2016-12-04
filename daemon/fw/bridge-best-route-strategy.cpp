/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014-2015,  Regents of the University of California,
 *                           Arizona Board of Regents,
 *                           Colorado State University,
 *                           University Pierre & Marie Curie, Sorbonne University,
 *                           Washington University in St. Louis,
 *                           Beijing Institute of Technology,
 *                           The University of Memphis.
 *
 * This file is part of NFD (Named Data Networking Forwarding Daemon).
 * See AUTHORS.md for complete list of NFD authors and contributors.
 *
 * NFD is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NFD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NFD, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "bridge-best-route-strategy.hpp"

namespace nfd {
namespace fw {

const Name BridgeBestRouteStrategy::STRATEGY_NAME("ndn:/localhost/nfd/strategy/bridge-best-route/%FD%01");
NFD_REGISTER_BRIDGE_STRATEGY(BridgeBestRouteStrategy);

BridgeBestRouteStrategy::BridgeBestRouteStrategy(Forwarder& forwarder, const Name& name)
  : BridgeStrategy(forwarder, name)
{
}

BridgeBestRouteStrategy::~BridgeBestRouteStrategy()
{
}

static inline bool
predicate_canForwardTo_NextHop(const Face& inFace,
                               const fib::NextHop& nexthop)
{
  return (inFace.getId() != nexthop.getFace()->getId());
}

void
BridgeBestRouteStrategy::afterReceiveInterestBridge(const Face& inFace,
                                                      const Interest& interest,
                                                      shared_ptr<fib::Entry> fibEntry)
{
  const fib::NextHopList& nexthops = fibEntry->getNextHops();
  fib::NextHopList::const_iterator it;
  for (it = nexthops.begin(); it != nexthops.end(); ++it) {
    if (predicate_canForwardTo_NextHop(inFace, *it) == true)
      break;
  }

  if (it == nexthops.end()) {
    return;
  }

  shared_ptr<Face> outFace = it->getFace();
  this->sendInterestBridge(interest, outFace);
}

} // namespace fw
} // namespace nfd

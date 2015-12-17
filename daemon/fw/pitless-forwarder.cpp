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

#include "pitless-forwarder.hpp"
#include "pitless-strategy.hpp"
#include "pitless-best-route-strategy.hpp"
#include "core/logger.hpp"
#include "core/random.hpp"
#include "strategy.hpp"
#include "face/null-face.hpp"

#include "utils/ndn-ns3-packet-tag.hpp"

#include <boost/random/uniform_int_distribution.hpp>

namespace nfd {

NFD_LOG_INIT("PITlessForwarder");

using fw::Strategy;
using fw::PITlessStrategy;
using fw::PITlessBestRouteStrategy;

PITlessForwarder::PITlessForwarder()
  : Forwarder()
{
  fw::installPITlessStrategies(*this);
}

PITlessForwarder::~PITlessForwarder()
{

}

void
PITlessForwarder::onIncomingInterest(Face& inFace, const Interest& interest)
{
  auto start = std::chrono::high_resolution_clock::now();

  // receive Interest
  NFD_LOG_DEBUG("onIncomingInterest face=" << inFace.getId() <<
                " interest=[N:" << interest.getName() <<
                ", SN:" << interest.getSupportingName() << "]");
  const_cast<Interest&>(interest).setIncomingFaceId(inFace.getId());
  ++m_counters.getNInInterests();

  // /localhost scope control
  bool isViolatingLocalhost = !inFace.isLocal() &&
    LOCALHOST_NAME.isPrefixOf(interest.getName());
  if (isViolatingLocalhost) {
    NFD_LOG_DEBUG("onIncomingInterest face=" << inFace.getId() <<
                  " interest=[N:" << interest.getName() <<
                  ", SN:" << interest.getSupportingName() <<
                  "] violates /localhost");
    // (drop)
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> duration = end - start;

    if (m_interestDelayCallback != 0) {
      m_interestDelayCallback(m_id, ns3::Simulator::Now(), duration.count());
    }
    return;
  }

  if (m_csFromNdnSim == nullptr) {
    m_cs.find(interest,
              bind(&PITlessForwarder::onContentStoreHit, this, ref(inFace), _1, _2),
              bind(&PITlessForwarder::onContentStoreMiss, this, ref(inFace), _1));
  }
  else {
    shared_ptr<Data> match = m_csFromNdnSim->Lookup(interest.shared_from_this());
    if (match != nullptr) {
      this->onContentStoreHit(inFace, interest, *match);
    }
    else {
      this->onContentStoreMiss(inFace, interest);
    }
  }

  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<float> duration = end - start;

  if (m_interestDelayCallback != 0) {
    m_interestDelayCallback(m_id, ns3::Simulator::Now(), duration.count());
  }
}

void
PITlessForwarder::onContentStoreMiss(const Face& inFace,
                                     const Interest& interest)
{
  NFD_LOG_DEBUG("onContentStoreMiss interest=[N:" << interest.getName() <<
                ", SN:" << interest.getSupportingName() << "]");

  // FIB lookup
  shared_ptr<fib::Entry> fibEntry = Forwarder::getFib().findLongestPrefixMatch(interest.getName());

  // dispatch to strategy
  // TODO(cesar): this is hard coded, find a better way.
  Name strategyName = PITlessBestRouteStrategy::STRATEGY_NAME;
  fw::Strategy* strategy = Forwarder::getStrategyChoice().getStrategy(strategyName);
  ((fw::PITlessStrategy*)(strategy))->afterReceiveInterestPITless(cref(inFace), cref(interest), fibEntry);
}

void
PITlessForwarder::onContentStoreHit(const Face& inFace,
                                    const Interest& interest,
                                    const Data& data)
{
  NFD_LOG_DEBUG("onContentStoreHit interest=[N:" << interest.getName() <<
                ", SN:" << interest.getSupportingName() << "]");

  // TODO(cesar): this is hard coded, find a better way.
  Name strategyName = PITlessBestRouteStrategy::STRATEGY_NAME;
  // TODO(cesar): this will not work of our pitless strategy implements its own
  //              beforeSatisfyInterest function, but for now it works.
  this->dispatchToPITlessStrategy(strategyName, bind(&Strategy::beforeSatisfyInterest, _1,
                                                    nullptr, cref(*Forwarder::getCsFace()), cref(data)));

  const_pointer_cast<Data>(data.shared_from_this())->setIncomingFaceId(FACEID_CONTENT_STORE);
  // XXX should we lookup PIT for other Interests that also match csMatch?

  // goto outgoing Data pipeline
  this->onOutgoingData(data, *const_pointer_cast<Face>(inFace.shared_from_this()));
}

static inline bool
predicate_canForwardTo_NextHop(const Face& inFace,
                               const fib::NextHop& nexthop)
{
  return (inFace.getId() != nexthop.getFace()->getId());
}

void
PITlessForwarder::onIncomingData(Face& inFace, const Data& data)
{
  auto start = std::chrono::high_resolution_clock::now();

  // receive Data
  NFD_LOG_DEBUG("onIncomingData face=" << inFace.getId() <<
                " data=[N:" << data.getName() <<
                ", SN:" << data.getSupportingName() << "]");
  const_cast<Data&>(data).setIncomingFaceId(inFace.getId());
  ++m_counters.getNInDatas();

  // /localhost scope control
  bool isViolatingLocalhost = !inFace.isLocal() &&
    LOCALHOST_NAME.isPrefixOf(Name(data.getSupportingName()));
  if (isViolatingLocalhost) {
    NFD_LOG_DEBUG("onIncomingData face=" << inFace.getId() <<
                  " data=[N:" << data.getName() <<
                  ", SN:" << data.getSupportingName() <<
                  "] violates /localhost");
    // (drop)
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> duration = end - start;

    if (m_interestDelayCallback != 0) {
      m_interestDelayCallback(m_id, ns3::Simulator::Now(), duration.count());
    }
    return;
  }

  // Remove Ptr<Packet> from the Data before inserting into cache, serving two purposes
  // - reduce amount of memory used by cached entries
  // - remove all tags that (e.g., hop count tag) that could have been associated with Ptr<Packet>
  //
  // Copying of Data is relatively cheap operation, as it copies (mostly) a collection of Blocks
  // pointing to the same underlying memory buffer.
  shared_ptr<Data> dataCopyWithoutPacket = make_shared<Data>(data);
  dataCopyWithoutPacket->removeTag<ns3::ndn::Ns3PacketTag>();

  // CS insert
  if (Forwarder::getCsFromNdnSim() == nullptr)
    m_cs.insert(*dataCopyWithoutPacket);
  else
    m_csFromNdnSim->Add(dataCopyWithoutPacket);

  // FIB lookup
  shared_ptr<fib::Entry> fibEntry = m_fib.findLongestPrefixMatch(data.getName());

  const fib::NextHopList& nexthops = fibEntry->getNextHops();
  fib::NextHopList::const_iterator it;
  for (it = nexthops.begin(); it != nexthops.end(); ++it) {
    if (predicate_canForwardTo_NextHop(inFace, *it) == true)
      break;
  }

  if (it == nexthops.end()) {
    NFD_LOG_DEBUG("onIncomingData face=" << inFace.getId() <<
                  " data=[N:" << data.getName() <<
                  ", SN:" << data.getSupportingName() <<
                  "] no out face to forward on");

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> duration = end - start;

    if (m_contentDelayCallback != 0) {
      m_contentDelayCallback(m_id, ns3::Simulator::Now(), duration.count());
    }
    return;
  }

  shared_ptr<Face> outFace = it->getFace();
  if (outFace.get() == &inFace) {
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> duration = end - start;

    if (m_contentDelayCallback != 0) {
      m_contentDelayCallback(m_id, ns3::Simulator::Now(), duration.count());
    }
    return;
  }

  // goto outgoing Data pipeline
  this->onOutgoingData(data, *outFace);

  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<float> duration = end - start;

  if (m_contentDelayCallback != 0) {
    m_contentDelayCallback(m_id, ns3::Simulator::Now(), duration.count());
  }
}

void
PITlessForwarder::onOutgoingInterestPITless(const Interest& interest, Face& outFace,
                                            bool wantNewNonce)
{
  if (outFace.getId() == INVALID_FACEID) {
    NFD_LOG_WARN("onOutgoingInterest face=invalid interest=" << interest.getName());
    return;
  }
  NFD_LOG_DEBUG("onOutgoingInterest face=" << outFace.getId() <<
                " interest=[N:" << interest.getName() <<
                ", SN:" << interest.getSupportingName() << "]");

  // send Interest
  outFace.sendInterest(interest);
}

} // namespace nfd

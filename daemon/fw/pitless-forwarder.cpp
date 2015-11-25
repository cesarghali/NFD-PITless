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
  // receive Interest
  NFD_LOG_DEBUG("onIncomingInterest face=" << inFace.getId() <<
                " interest=" << interest.getName());
  const_cast<Interest&>(interest).setIncomingFaceId(inFace.getId());

  // /localhost scope control
  bool isViolatingLocalhost = !inFace.isLocal() &&
    Forwarder::getLOCALHOSTNAME().isPrefixOf(interest.getName());
  if (isViolatingLocalhost) {
    NFD_LOG_DEBUG("onIncomingInterest face=" << inFace.getId() <<
                  " interest=" << interest.getName() << " violates /localhost");
    // (drop)
    return;
  }

  if (Forwarder::getCsFromNdnSim() == nullptr) {
    Forwarder::getCs().find(interest,
              bind(&PITlessForwarder::onContentStoreHit, this, ref(inFace), _1, _2),
              bind(&PITlessForwarder::onContentStoreMiss, this, ref(inFace), _1));
  }
  else {
    shared_ptr<Data> match = Forwarder::getCsFromNdnSim()->Lookup(interest.shared_from_this());
    if (match != nullptr) {
      this->onContentStoreHit(inFace, interest, *match);
    }
    else {
      this->onContentStoreMiss(inFace, interest);
    }
  }
}

void
PITlessForwarder::onContentStoreMiss(const Face& inFace,
                                     const Interest& interest)
{
  NFD_LOG_DEBUG("onContentStoreMiss interest=" << interest.getName());

  // FIB lookup
  shared_ptr<fib::Entry> fibEntry = Forwarder::getFib().findLongestPrefixMatch(interest.getName());

  // dispatch to strategy
  this->dispatchToPITlessStrategy(interest.getName(), bind(&PITlessStrategy::afterReceiveInterestPITless, _1,
                                                    cref(inFace), cref(interest), fibEntry));
}

void
PITlessForwarder::onContentStoreHit(const Face& inFace,
                                    const Interest& interest,
                                    const Data& data)
{
  NFD_LOG_DEBUG("onContentStoreHit interest=" << interest.getName());

  this->dispatchToPITlessStrategy(interest.getName(), bind(&Strategy::beforeSatisfyInterest, _1,
                                                    nullptr, cref(*Forwarder::getCsFace()), cref(data)));

  const_pointer_cast<Data>(data.shared_from_this())->setIncomingFaceId(FACEID_CONTENT_STORE);
  // XXX should we lookup PIT for other Interests that also match csMatch?

  // goto outgoing Data pipeline
  this->onOutgoingData(data, *const_pointer_cast<Face>(inFace.shared_from_this()));
}

static inline bool
predicate_canForwardTo_NextHop()
{
  return true;
}

void
PITlessForwarder::onIncomingData(Face& inFace, const Data& data)
{
  // receive Data
  NFD_LOG_DEBUG("onIncomingData face=" << inFace.getId() << " data=" << data.getSupportingName());
  const_cast<Data&>(data).setIncomingFaceId(inFace.getId());

  // /localhost scope control
  bool isViolatingLocalhost = !inFace.isLocal() &&
    Forwarder::getLOCALHOSTNAME().isPrefixOf(data.getSupportingName());
  if (isViolatingLocalhost) {
    NFD_LOG_DEBUG("onIncomingData face=" << inFace.getId() <<
                  " data=" << data.getSupportingName() << " violates /localhost");
    // (drop)
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
    Forwarder::getCs().insert(*dataCopyWithoutPacket);
  else
    Forwarder::getCsFromNdnSim()->Add(dataCopyWithoutPacket);

  // FIB lookup
  shared_ptr<fib::Entry> fibEntry = Forwarder::getFib().findLongestPrefixMatch(data.getName());

  const fib::NextHopList& nexthops = fibEntry->getNextHops();
  fib::NextHopList::const_iterator it = std::find_if(nexthops.begin(), nexthops.end(),
                                                     bind(&predicate_canForwardTo_NextHop));
  if (it == nexthops.end()) {
    return;
  }

  shared_ptr<Face> outFace = it->getFace();
  if (outFace.get() == &inFace) {
    return;
  }
  // goto outgoing Data pipeline
  this->onOutgoingData(data, *outFace);
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
                " interest=" << interest.getName());

  //if (wantNewNonce) {
  //  interest = make_shared<Interest>(*interest);
  //  static boost::random::uniform_int_distribution<uint32_t> dist;
  //  interest->setNonce(dist(getGlobalRng()));
  //}

  // send Interest
  outFace.sendInterest(interest);
}

} // namespace nfd

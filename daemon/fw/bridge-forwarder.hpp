/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014,  Regents of the University of California,
 *                      Arizona Board of Regents,
 *                      Colorado State University,
 *                      University Pierre & Marie Curie, Sorbonne University,
 *                      Washington University in St. Louis,
 *                      Beijing Institute of Technology,
 *                      The University of Memphis
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

#ifndef NFD_DAEMON_FW_PITLESS_BRIDGE_FORWARDER_HPP
#define NFD_DAEMON_FW_PITLESS_BRIDGE_FORWARDER_HPP

#include "forwarder.hpp"
#include "face-table.hpp"

namespace nfd {

namespace fw {
class PITlessStrategy;
class Strategy;
} // namespace fw

class NullFace;

/** \brief main class of NFD
 *
 *  Forwarder owns all faces and tables, and implements forwarding pipelines.
 */
class BridgeForwarder : public Forwarder
{
public:
  BridgeForwarder(std::string supportingName);

  VIRTUAL_WITH_TESTS
  ~BridgeForwarder();

public: // forwarding entrypoints and tables
  void
  onInterest(Face& face, const Interest& interest);

  void
  onData(Face& face, const Data& data);

PUBLIC_WITH_TESTS_ELSE_PRIVATE: // pipelines
  /** \brief incoming Interest pipeline
   */
  VIRTUAL_WITH_TESTS void
  onIncomingInterest(Face& inFace, const Interest& interest);

  /** \brief incoming Data pipeline
   */
  VIRTUAL_WITH_TESTS void
  onIncomingData(Face& inFace, const Data& data);

  /** \brief Content Store miss pipeline
  */
  void
  onContentStoreMiss(const Face& inFace, shared_ptr<pit::Entry> pitEntry, const Interest& interest);

  /** \brief Content Store hit pipeline
  */
  void
  onContentStoreHit(const Face& inFace,
                    const Interest& interest, const Data& data);

public:
  /** \brief outgoing Interest pipeline
   */
  void
  onOutgoingInterestPITless(const Interest& interest, Face& outFace,
                            bool wantNewNonce = false);


  std::string m_Name;

  inline void
  setSupportingName(std::string name) {
    m_Name = name;
  }

  /// call trigger (method) on the effective strategy of pitEntry
#ifdef WITH_TESTS
  virtual void
  dispatchPITlessToStrategy(const Name& prefix, function<void(fw::PITlessStrategy*)> trigger);
#else
  template<class Function>
  void
  dispatchToPITlessStrategy(const Name& prefix, Function trigger);
#endif
};

inline void
BridgeForwarder::onInterest(Face& face, const Interest& interest)
{
  this->onIncomingInterest(face, interest);
}

inline void
BridgeForwarder::onData(Face& face, const Data& data)
{
  this->onIncomingData(face, data);
}

#ifdef WITH_TESTS
inline void
BridgeForwarder::dispatchToPITlessStrategy(const Name& prefix, function<void(fw::PITlessStrategy*)> trigger)
#else
template<class Function>
inline void
BridgeForwarder::dispatchToPITlessStrategy(const Name& prefix, Function trigger)
#endif
{
  fw::Strategy& strategy = Forwarder::getStrategyChoice().findEffectiveStrategy(prefix);
  trigger(&((fw::PITlessStrategy&)(strategy)));
}

} // namespace nfd

#endif // NFD_DAEMON_FW_PITLESS_FORWARDER_HPP

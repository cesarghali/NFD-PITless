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

#ifndef NFD_DAEMON_FW_PITLESS_STRATEGY_HPP
#define NFD_DAEMON_FW_PITLESS_STRATEGY_HPP

#include "strategy.hpp"
#include "pitless-forwarder.hpp"

namespace nfd {

class PITlessForwarder;

namespace fw {

/** \brief PITless strategy version 1
 *
 *  This strategy is copied from Best Route strategy and modified to fit a
 *  PITless model.
 */
class PITlessStrategy : public Strategy
{
public:
  PITlessStrategy(Forwarder& forwarder, const Name& name = STRATEGY_NAME);

  virtual
  ~PITlessStrategy();

  void
  afterReceiveInterestPITless(const Face& inFace,
                              const Interest& interest,
                              shared_ptr<fib::Entry> fibEntry);

  virtual void
  afterReceiveInterest(const Face& inFace,
                       const Interest& interest,
                       shared_ptr<fib::Entry> fibEntry,
                       shared_ptr<pit::Entry> pitEntry) DECL_OVERRIDE;

public:
  static const Name STRATEGY_NAME;

protected:
  /// send Interest to outFace
  //  VIRTUAL_WITH_TESTS
  void
  sendInterestPITless(const Interest& interest,
                      shared_ptr<Face> outFace,
                      bool wantNewNonce = false);
};

inline void
PITlessStrategy::sendInterestPITless(const Interest& interest,
                                     shared_ptr<Face> outFace,
                                     bool wantNewNonce)
{
  ((nfd::PITlessForwarder&)(Strategy::getForwarder())).onOutgoingInterestPITless(interest, *outFace, wantNewNonce);
}

} // namespace fw
} // namespace nfd

#endif // NFD_DAEMON_FW_PITLESS_STRATEGY_HPP

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

#ifndef NFD_DAEMON_FW_STRATEGY_REGISTRY_HPP
#define NFD_DAEMON_FW_STRATEGY_REGISTRY_HPP

#include "common.hpp"

#include "ns3/ndnSIM/NFD/daemon/fw/pitless-forwarder.hpp"

namespace nfd {

class Forwarder;

namespace fw {

class Strategy;

shared_ptr<Strategy>
makeDefaultStrategy(Forwarder& forwarder);

void
installStrategies(Forwarder& forwarder);

void
installPITlessStrategies(PITlessForwarder& pitlessForwarder);


typedef std::function<shared_ptr<Strategy>(Forwarder&)> StrategyCreateFunc;

typedef std::function<shared_ptr<Strategy>(PITlessForwarder&)> PITlessStrategyCreateFunc;

void
registerStrategyImpl(const Name& strategyName, const StrategyCreateFunc& createFunc);

void
registerPITlessStrategyImpl(const Name& strategyName, const PITlessStrategyCreateFunc& createFunc);

/** \brief registers a strategy to be installed later
 */
template<typename S>
void
registerStrategy()
{
  registerStrategyImpl(S::STRATEGY_NAME,
                       [] (Forwarder& forwarder) { return make_shared<S>(ref(forwarder)); });
}

/** \brief registers a strategy to be installed later
 */
template<typename S>
void
registerPITlessStrategy()
{
  registerPITlessStrategyImpl(S::STRATEGY_NAME,
                              [] (PITlessForwarder& pitlessForwarder) { return make_shared<S>(ref(pitlessForwarder)); });
}

/** \brief registers a built-in strategy
 *
 *  This macro should appear once in .cpp of each built-in strategy.
 */
#define NFD_REGISTER_STRATEGY(StrategyType)                       \
static class NfdAuto ## StrategyType ## StrategyRegistrationClass \
{                                                                 \
public:                                                           \
  NfdAuto ## StrategyType ## StrategyRegistrationClass()          \
  {                                                               \
    ::nfd::fw::registerStrategy<StrategyType>();                  \
  }                                                               \
} g_nfdAuto ## StrategyType ## StrategyRegistrationVariable

/** \brief registers a built-in PITless strategy
 *
 *  This macro should appear once in .cpp of each built-in PITless strategy.
 */
#define NFD_REGISTER_PITLESS_STRATEGY(StrategyType)                             \
static class NfdAuto ## PITlessStrategyType ## PITlessStrategyRegistrationClass \
{                                                                               \
public:                                                                         \
  NfdAuto ## PITlessStrategyType ## PITlessStrategyRegistrationClass()          \
  {                                                                             \
    ::nfd::fw::registerPITlessStrategy<StrategyType>();                         \
  }                                                                             \
} g_nfdAuto ## PITlessStrategyType ## PITlessStrategyRegistrationVariable

} // namespace fw
} // namespace nfd

#endif // NFD_DAEMON_FW_STRATEGY_REGISTRY_HPP

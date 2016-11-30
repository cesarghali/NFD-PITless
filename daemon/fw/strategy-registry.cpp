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

#include "strategy-registry.hpp"
#include "best-route-strategy2.hpp"

namespace nfd {
namespace fw {

shared_ptr<Strategy>
makeDefaultStrategy(Forwarder& forwarder)
{
  return make_shared<BestRouteStrategy2>(ref(forwarder));
}

static std::map<Name, StrategyCreateFunc>&
getStrategyFactories()
{
  static std::map<Name, StrategyCreateFunc> strategyFactories;
  return strategyFactories;
}

static std::map<Name, PITlessStrategyCreateFunc>&
getPITlessStrategyFactories()
{
  static std::map<Name, PITlessStrategyCreateFunc> pitlessStrategyFactories;
  return pitlessStrategyFactories;
}

static std::map<Name, BridgeStrategyCreateFunc>&
getBridgeStrategyFactories()
{
  static std::map<Name, BridgeStrategyCreateFunc> bridgeStrategyFactories;
  return bridgeStrategyFactories;
}

void
registerStrategyImpl(const Name& strategyName, const StrategyCreateFunc& createFunc)
{
  getStrategyFactories().insert({strategyName, createFunc});
}

void
registerPITlessStrategyImpl(const Name& strategyName, const PITlessStrategyCreateFunc& createFunc)
{
  getPITlessStrategyFactories().insert({strategyName, createFunc});
}

void
registerBridgeStrategyImpl(const Name& strategyName, const BridgeStrategyCreateFunc& createFunc)
{
  getBridgeStrategyFactories().insert({strategyName, createFunc});
}

void
installStrategies(Forwarder& forwarder)
{
  StrategyChoice& sc = forwarder.getStrategyChoice();
  for (const auto& pair : getStrategyFactories()) {
    if (!sc.hasStrategy(pair.first, true)) {
      sc.install(pair.second(forwarder));
    }
  }
}

void
installPITlessStrategies(PITlessForwarder& pitlessForwarder)
{
  StrategyChoice& sc = pitlessForwarder.getStrategyChoice();
  for (const auto& pair : getPITlessStrategyFactories()) {
    if (!sc.hasStrategy(pair.first, true)) {
      sc.install(pair.second(pitlessForwarder));
    }
  }
}

void
installBridgeStrategies(BridgeForwarder& bridgeForwarder)
{
  StrategyChoice& sc = bridgeForwarder.getStrategyChoice();
  for (const auto& pair : getBridgeStrategyFactories()) {
    if (!sc.hasStrategy(pair.first, true)) {
      sc.install(pair.second(bridgeForwarder));
    }
  }
}


} // namespace fw
} // namespace nfd

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

#include "nrd.hpp"

#include "version.hpp"
#include "core/logger.hpp"
#include "core/global-io.hpp"

#include <string.h>

#include <boost/filesystem.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/parsers.hpp>

namespace nfd {
namespace rib {

NFD_LOG_INIT("NRD");

class NrdRunner : noncopyable
{
public:
  explicit
  NrdRunner(const std::string& configFile)
    : m_nrd(configFile, m_keyChain)
    , m_terminationSignalSet(getGlobalIoService())
  {
    m_terminationSignalSet.add(SIGINT);
    m_terminationSignalSet.add(SIGTERM);
    m_terminationSignalSet.async_wait(bind(&NrdRunner::terminate, this, _1, _2));
  }

  static void
  printUsage(std::ostream& os, const std::string& programName)
  {
    os << "Usage: \n"
       << "  " << programName << " [options]\n"
       << "\n"
       << "Run NRD daemon\n"
       << "\n"
       << "Options:\n"
       << "  [--help]    - print this help message\n"
       << "  [--version] - print version and exit\n"
       << "  [--modules] - list available logging modules\n"
       << "  [--config /path/to/nfd.conf] - path to configuration file "
       << "(default: " << DEFAULT_CONFIG_FILE << ")\n"
      ;
  }

  static void
  printModules(std::ostream& os)
  {
    os << "Available logging modules: \n";

    for (const auto& module : LoggerFactory::getInstance().getModules()) {
      os << module << "\n";
    }
  }

  void
  run()
  {
    getGlobalIoService().run();
  }

  void
  initialize()
  {
    m_nrd.initialize();
  }

  void
  terminate(const boost::system::error_code& error, int signalNo)
  {
    if (error)
      return;

    NFD_LOG_INFO("Caught signal '" << ::strsignal(signalNo) << "', exiting...");
    getGlobalIoService().stop();
  }

private:
  ndn::KeyChain           m_keyChain;
  Nrd                     m_nrd; // must be after m_io and m_keyChain
  boost::asio::signal_set m_terminationSignalSet;
};

} // namespace nfd
} // namespace rib

int
main(int argc, char** argv)
{
  using namespace nfd::rib;

  namespace po = boost::program_options;

  po::options_description description;

  std::string configFile = DEFAULT_CONFIG_FILE;
  description.add_options()
    ("help,h",    "print this help message")
    ("version,V", "print version and exit")
    ("modules,m", "list available logging modules")
    ("config,c",  po::value<std::string>(&configFile), "path to configuration file")
    ;

  po::variables_map vm;
  try {
      po::store(po::command_line_parser(argc, argv).options(description).run(), vm);
      po::notify(vm);
  }
  catch (const std::exception& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    NrdRunner::printUsage(std::cerr, argv[0]);
    return 1;
  }

  if (vm.count("help") > 0) {
    NrdRunner::printUsage(std::cout, argv[0]);
    return 0;
  }

  if (vm.count("version") > 0) {
    std::cout << NFD_VERSION_BUILD_STRING << std::endl;
    return 0;
  }

  if (vm.count("modules") > 0) {
    NrdRunner::printModules(std::cout);
    return 0;
  }

  NrdRunner runner(configFile);

  try {
    runner.initialize();
  }
  catch (const boost::filesystem::filesystem_error& e) {
    if (e.code() == boost::system::errc::permission_denied) {
      NFD_LOG_FATAL("Permissions denied for " << e.path1() << ". " <<
                    argv[0] << " should be run as superuser");
    }
    else {
      NFD_LOG_FATAL(e.what());
    }
    return 1;
  }
  catch (const std::exception& e) {
    NFD_LOG_FATAL(e.what());
    return 2;
  }

  try {
    runner.run();
  }
  catch (const std::exception& e) {
    NFD_LOG_FATAL(e.what());
    return 4;
  }

  return 0;
}

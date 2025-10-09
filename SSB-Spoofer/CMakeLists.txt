CMAKE_MINIMUM_REQUIRED(VERSION 3.5)
set(SPOOFER_SRC_DIR src)
set(SPOOFER_HDR_DIR hdr)

# Common source files (shared between executables)
set(COMMON_SOURCES
    "${SPOOFER_SRC_DIR}/config.cc"
    "${SPOOFER_SRC_DIR}/rf_handler.cc"
    "${SPOOFER_SRC_DIR}/ssb_processor.cc"
)

# Main SSB Spoofer executable
add_executable(ssb_spoofer 
    ${COMMON_SOURCES}
    "${SPOOFER_SRC_DIR}/main.cpp"
)

set_target_properties(ssb_spoofer PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    LINKER_LANGUAGE CXX
)

target_include_directories(ssb_spoofer PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/${SPOOFER_HDR_DIR}"
)

target_link_libraries(ssb_spoofer PRIVATE
    srsran_phch
    srsran_phy
    srsran_common
    support
    srslog
    srsran_rf
    srsran_phy
    pthread
)

# SSB File Analyzer executable
add_executable(ssb_file_analyzer
    "${SPOOFER_SRC_DIR}/config.cc"
    "${SPOOFER_SRC_DIR}/ssb_processor.cc"
    "${SPOOFER_SRC_DIR}/ssb_file_analyzer.cpp"
)

set_target_properties(ssb_file_analyzer PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    LINKER_LANGUAGE CXX
)

target_include_directories(ssb_file_analyzer PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/${SPOOFER_HDR_DIR}"
)

target_link_libraries(ssb_file_analyzer PRIVATE
    srsran_phch
    srsran_phy
    srsran_common
    support
    srslog
    pthread
)

install(TARGETS ssb_spoofer ssb_file_analyzer DESTINATION /usr/local/bin OPTIONAL)


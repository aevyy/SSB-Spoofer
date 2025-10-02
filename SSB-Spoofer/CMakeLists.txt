CMAKE_MINIMUM_REQUIRED(VERSION 3.5)
set(SPOOFER_SRC_DIR src)
set(SPOOFER_HDR_DIR hdr)

# Gather all source files
file(GLOB_RECURSE SPOOFER_SOURCES CONFIGURE_DEPENDS 
    "${SPOOFER_SRC_DIR}/*.cc"
    "${SPOOFER_SRC_DIR}/*.cpp"
)

add_executable(ssb_Spoofer ${SPOOFER_SOURCES})

set_target_properties(ssb_Spoofer PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    LINKER_LANGUAGE CXX
)

target_include_directories(ssb_Spoofer PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/${SPOOFER_HDR_DIR}"
)

target_link_libraries(ssb_Spoofer PRIVATE

    -Wl,--start-group
    srsran_phy
    srsran_phch
    srsran_common
    support
    srslog
    srsran_rf
    -Wl,--end-group

    pthread

)

install(TARGETS ssb_Spoofer DESTINATION /usr/local/bin OPTIONAL)


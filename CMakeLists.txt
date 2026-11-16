cmake_minimum_required(VERSION 3.16)
project(mastering_suite VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
include(GNUInstallDirs)

set(CLAP_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/clap"
    CACHE PATH "Directory where CLAP bundles are installed")

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(CAIRO REQUIRED IMPORTED_TARGET cairo)
pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)
pkg_check_modules(X11 REQUIRED IMPORTED_TARGET x11)
pkg_check_modules(EBUR128 REQUIRED IMPORTED_TARGET libebur128)

set(HEARTH_DSP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/hearth-dsp"
    CACHE PATH "Path to the hearth-dsp checkout")
if(NOT EXISTS "${HEARTH_DSP_DIR}/include/hdsp/hdsp.hpp")
  message(FATAL_ERROR "hearth-dsp not found at ${HEARTH_DSP_DIR}")
endif()
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src"
                    "${HEARTH_DSP_DIR}/include")

enable_testing()

add_library(mastering_ui src/ui/panel.cpp)
target_link_libraries(mastering_ui PUBLIC PkgConfig::CAIRO PkgConfig::FREETYPE)
target_compile_features(mastering_ui PUBLIC cxx_std_17)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(mastering_ui PRIVATE -Wall -Wextra)
endif()

add_executable(mastering_preview test/ui_preview.cpp)
target_link_libraries(mastering_preview PRIVATE mastering_ui)
target_compile_features(mastering_preview PRIVATE cxx_std_17)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(mastering_preview PRIVATE -Wall -Wextra)
endif()

add_library(mastering MODULE
  src/clap/mastering_clap.cpp
  src/ui/panel.cpp)
target_include_directories(mastering PRIVATE
  "${CMAKE_SOURCE_DIR}/src"
  "${CMAKE_SOURCE_DIR}/third_party/clap/include")
target_link_libraries(mastering PRIVATE
  PkgConfig::CAIRO PkgConfig::FREETYPE PkgConfig::X11 PkgConfig::EBUR128
  Threads::Threads ${CMAKE_DL_LIBS})
target_compile_features(mastering PRIVATE cxx_std_17)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(mastering PRIVATE -Wall -Wextra -fvisibility=hidden)
endif()
set_target_properties(mastering PROPERTIES
  PREFIX ""
  OUTPUT_NAME "MasteringSuite"
  SUFFIX ".clap"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/clap")

set(MASTERING_RESOURCE_FILES
  "${CMAKE_SOURCE_DIR}/assets/fonts/Jost-ExtraBold.ttf"
  "${CMAKE_SOURCE_DIR}/assets/fonts/OFL.txt")

add_custom_target(mastering_bundle ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory
          "${CMAKE_BINARY_DIR}/clap/MasteringSuite.resources"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MASTERING_RESOURCE_FILES}
          "${CMAKE_BINARY_DIR}/clap/MasteringSuite.resources/"
  DEPENDS mastering ${MASTERING_RESOURCE_FILES})

install(TARGETS mastering LIBRARY DESTINATION "${CLAP_DIR}")
install(FILES ${MASTERING_RESOURCE_FILES}
        DESTINATION "${CLAP_DIR}/MasteringSuite.resources")

add_custom_target(dist
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/dist"
  COMMAND ${CMAKE_COMMAND} -E tar czf
          "${CMAKE_BINARY_DIR}/dist/MasteringSuite-${PROJECT_VERSION}-clap.tar.gz"
          MasteringSuite.clap MasteringSuite.resources
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/clap"
  DEPENDS mastering_bundle
  COMMENT "Packing MasteringSuite-${PROJECT_VERSION}")

find_library(XTST_LIBRARY Xtst)
find_program(XVFB_RUN xvfb-run)
add_executable(mastering_clap_host_test test/clap_host_test.cpp)
target_include_directories(mastering_clap_host_test PRIVATE
  "${CMAKE_SOURCE_DIR}/src" "${CMAKE_SOURCE_DIR}/third_party/clap/include")
target_link_libraries(mastering_clap_host_test PRIVATE PkgConfig::X11
  ${CMAKE_DL_LIBS})
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(mastering_clap_host_test PRIVATE -Wall -Wextra)
endif()
if(XTST_LIBRARY)
  target_link_libraries(mastering_clap_host_test PRIVATE ${XTST_LIBRARY})
endif()
add_dependencies(mastering_clap_host_test mastering_bundle)
if(XVFB_RUN)
  add_test(NAME mastering_clap_host
    COMMAND "${XVFB_RUN}" -a -s "-screen 0 1600x1000x24"
      "$<TARGET_FILE:mastering_clap_host_test>"
      "${CMAKE_BINARY_DIR}/clap/MasteringSuite.clap")
else()
  add_test(NAME mastering_clap_host
    COMMAND mastering_clap_host_test "${CMAKE_BINARY_DIR}/clap/MasteringSuite.clap")
endif()

add_executable(mastering_dsp_test test/test_mastering.cpp)
target_link_libraries(mastering_dsp_test PRIVATE PkgConfig::EBUR128)
target_compile_features(mastering_dsp_test PRIVATE cxx_std_17)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(mastering_dsp_test PRIVATE -Wall -Wextra)
endif()
add_test(NAME mastering_dsp COMMAND mastering_dsp_test)

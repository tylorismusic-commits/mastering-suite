cmake_minimum_required(VERSION 3.16)
project(mastering_suite VERSION 0.4.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
include(GNUInstallDirs)

set(CLAP_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/clap"
    CACHE PATH "Directory where CLAP bundles are installed")
set(LV2_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/lv2"
    CACHE PATH "Directory where LV2 bundles are installed")

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(CAIRO REQUIRED IMPORTED_TARGET cairo)
pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)
pkg_check_modules(X11 REQUIRED IMPORTED_TARGET x11)
pkg_check_modules(LV2 REQUIRED IMPORTED_TARGET lv2)
pkg_check_modules(LILV QUIET IMPORTED_TARGET lilv-0)

set(HEARTH_DSP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/hearth-dsp"
    CACHE PATH "Path to the hearth-dsp checkout")
if(NOT EXISTS "${HEARTH_DSP_DIR}/include/hdsp/hdsp.hpp")
  message(FATAL_ERROR "hearth-dsp not found at ${HEARTH_DSP_DIR}")
endif()
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src"
                    "${HEARTH_DSP_DIR}/include")

# ---------------------------------------------------------------------------
# libebur128 is vendored so the shipped binaries have no separate runtime
# dependency for loudness metering.  MIT licensed; see third_party.
# ---------------------------------------------------------------------------
add_library(mastering_ebur128 STATIC
  third_party/libebur128/ebur128.c)
target_include_directories(mastering_ebur128 PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libebur128"
  "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libebur128/queue")
set_target_properties(mastering_ebur128 PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  C_VISIBILITY_PRESET hidden)
find_library(MATH_LIBRARY m)
if(MATH_LIBRARY)
  target_link_libraries(mastering_ebur128 PUBLIC ${MATH_LIBRARY})
endif()

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

# ---------------------------------------------------------------------------
# CLAP plugin: fixed mastering chain and X11/Cairo editor.
# ---------------------------------------------------------------------------
add_library(mastering MODULE
  src/clap/mastering_clap.cpp
  src/ui/panel.cpp)
target_include_directories(mastering PRIVATE
  "${CMAKE_SOURCE_DIR}/src"
  "${CMAKE_SOURCE_DIR}/third_party/clap/include")
target_link_libraries(mastering PRIVATE
  PkgConfig::CAIRO PkgConfig::FREETYPE PkgConfig::X11 mastering_ebur128
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
  "${CMAKE_SOURCE_DIR}/assets/fonts/OFL.txt"
  "${CMAKE_SOURCE_DIR}/LICENSE"
  "${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md")

add_custom_target(mastering_bundle ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory
          "${CMAKE_BINARY_DIR}/clap/MasteringSuite.resources"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MASTERING_RESOURCE_FILES}
          "${CMAKE_BINARY_DIR}/clap/MasteringSuite.resources/"
  DEPENDS mastering ${MASTERING_RESOURCE_FILES})

install(TARGETS mastering LIBRARY DESTINATION "${CLAP_DIR}")
install(FILES ${MASTERING_RESOURCE_FILES}
        DESTINATION "${CLAP_DIR}/MasteringSuite.resources")

# ---------------------------------------------------------------------------
# LV2 plugin: same DSP and panel, plus the LV2 UI and generated TTL.
# ---------------------------------------------------------------------------
set(LV2_BUNDLE_DIR "${CMAKE_BINARY_DIR}/lv2/mastering-suite.lv2")

add_library(mastering_lv2 MODULE src/mastering_lv2.cpp)
target_include_directories(mastering_lv2 PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(mastering_lv2 PRIVATE PkgConfig::LV2 mastering_ebur128)
target_compile_features(mastering_lv2 PRIVATE cxx_std_17)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(mastering_lv2 PRIVATE -Wall -Wextra -fvisibility=hidden)
endif()
set_target_properties(mastering_lv2 PROPERTIES
  PREFIX ""
  OUTPUT_NAME "mastering_suite"
  LIBRARY_OUTPUT_DIRECTORY "${LV2_BUNDLE_DIR}")

add_library(mastering_lv2_ui MODULE src/ui/lv2_ui.cpp src/ui/panel.cpp)
target_include_directories(mastering_lv2_ui PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(mastering_lv2_ui PRIVATE
  PkgConfig::CAIRO PkgConfig::FREETYPE PkgConfig::X11 PkgConfig::LV2
  Threads::Threads)
target_compile_features(mastering_lv2_ui PRIVATE cxx_std_17)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(mastering_lv2_ui PRIVATE -Wall -Wextra
    -fvisibility=hidden)
endif()
set_target_properties(mastering_lv2_ui PROPERTIES
  PREFIX ""
  OUTPUT_NAME "mastering_suite_ui"
  LIBRARY_OUTPUT_DIRECTORY "${LV2_BUNDLE_DIR}")

add_custom_target(mastering_lv2_bundle ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory "${LV2_BUNDLE_DIR}"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_SOURCE_DIR}/lv2/manifest.ttl"
          "${CMAKE_SOURCE_DIR}/lv2/mastering-suite.ttl"
          "${CMAKE_SOURCE_DIR}/lv2/presets.ttl"
          "${CMAKE_SOURCE_DIR}/assets/fonts/Jost-ExtraBold.ttf"
          "${CMAKE_SOURCE_DIR}/assets/fonts/OFL.txt"
          "${CMAKE_SOURCE_DIR}/LICENSE"
          "${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
          "${LV2_BUNDLE_DIR}/"
  DEPENDS mastering_lv2 mastering_lv2_ui)

install(TARGETS mastering_lv2 mastering_lv2_ui
        LIBRARY DESTINATION "${LV2_DIR}/mastering-suite.lv2")
install(FILES
  lv2/manifest.ttl
  lv2/mastering-suite.ttl
  lv2/presets.ttl
  assets/fonts/Jost-ExtraBold.ttf
  assets/fonts/OFL.txt
  LICENSE
  THIRD_PARTY_NOTICES.md
  DESTINATION "${LV2_DIR}/mastering-suite.lv2")

# ---------------------------------------------------------------------------
# Metadata generator: emits lv2/*.ttl from src/params.hpp and src/presets.hpp.
# The test fails if the committed TTL files are stale.
# ---------------------------------------------------------------------------
add_executable(mastering_ttl_gen tools/gen_ttl.cpp)
target_include_directories(mastering_ttl_gen PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_compile_features(mastering_ttl_gen PRIVATE cxx_std_17)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(mastering_ttl_gen PRIVATE -Wall -Wextra)
endif()
add_test(NAME mastering_ttl
  COMMAND mastering_ttl_gen --check --root "${CMAKE_SOURCE_DIR}")

# ---------------------------------------------------------------------------
# Release bundles: CLAP, LV2, and a combined Linux tarball.
# ---------------------------------------------------------------------------
add_custom_target(dist-clap
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/dist"
  COMMAND ${CMAKE_COMMAND} -E tar czf
          "${CMAKE_BINARY_DIR}/dist/MasteringSuite-${PROJECT_VERSION}-clap.tar.gz"
          MasteringSuite.clap MasteringSuite.resources
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/clap"
  DEPENDS mastering_bundle
  COMMENT "Packing MasteringSuite-${PROJECT_VERSION} CLAP")

add_custom_target(dist-lv2
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/dist"
  COMMAND ${CMAKE_COMMAND} -E tar czf
          "${CMAKE_BINARY_DIR}/dist/mastering-suite-${PROJECT_VERSION}-lv2.tar.gz"
          mastering-suite.lv2
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/lv2"
  DEPENDS mastering_lv2_bundle
  COMMENT "Packing MasteringSuite-${PROJECT_VERSION} LV2")

add_custom_target(dist
  COMMAND ${CMAKE_COMMAND} -E remove_directory
          "${CMAKE_BINARY_DIR}/dist/stage"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/dist/stage"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_BINARY_DIR}/clap/MasteringSuite.clap"
          "${CMAKE_BINARY_DIR}/dist/stage/"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_BINARY_DIR}/clap/MasteringSuite.resources"
          "${CMAKE_BINARY_DIR}/dist/stage/MasteringSuite.resources"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${LV2_BUNDLE_DIR}"
          "${CMAKE_BINARY_DIR}/dist/stage/mastering-suite.lv2"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_SOURCE_DIR}/LICENSE"
          "${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
          "${CMAKE_SOURCE_DIR}/README.md"
          "${CMAKE_BINARY_DIR}/dist/stage/"
  COMMAND ${CMAKE_COMMAND} -E chdir "${CMAKE_BINARY_DIR}/dist/stage"
          ${CMAKE_COMMAND} -E tar czf
          "${CMAKE_BINARY_DIR}/dist/MasteringSuite-${PROJECT_VERSION}-linux-x86_64.tar.gz"
          MasteringSuite.clap MasteringSuite.resources mastering-suite.lv2
          LICENSE THIRD_PARTY_NOTICES.md README.md
  DEPENDS mastering_bundle mastering_lv2_bundle
  COMMENT "Packing MasteringSuite-${PROJECT_VERSION} Linux bundle")

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
find_program(XVFB_RUN xvfb-run)
find_library(XTST_LIBRARY Xtst)
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
target_link_libraries(mastering_dsp_test PRIVATE mastering_ebur128)
target_compile_features(mastering_dsp_test PRIVATE cxx_std_17)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(mastering_dsp_test PRIVATE -Wall -Wextra)
endif()
add_test(NAME mastering_dsp COMMAND mastering_dsp_test)

if(LILV_FOUND)
  add_executable(mastering_lv2_host_test test/lv2_host_test.cpp)
  target_include_directories(mastering_lv2_host_test PRIVATE
    "${CMAKE_SOURCE_DIR}/src" ${PKG_CONFIG_LV2_INCLUDE_DIRS})
  target_link_libraries(mastering_lv2_host_test PRIVATE PkgConfig::LILV)
  target_compile_features(mastering_lv2_host_test PRIVATE cxx_std_17)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mastering_lv2_host_test PRIVATE -Wall -Wextra)
  endif()
  add_dependencies(mastering_lv2_host_test mastering_lv2_bundle)
  add_test(NAME mastering_lv2_host
    COMMAND mastering_lv2_host_test "${LV2_BUNDLE_DIR}")
endif()

add_executable(mastering_lv2_ui_host_test test/lv2_ui_host_test.cpp)
target_include_directories(mastering_lv2_ui_host_test PRIVATE
  "${CMAKE_SOURCE_DIR}/src" ${PKG_CONFIG_LV2_INCLUDE_DIRS})
target_link_libraries(mastering_lv2_ui_host_test PRIVATE PkgConfig::X11
  ${CMAKE_DL_LIBS})
target_compile_features(mastering_lv2_ui_host_test PRIVATE cxx_std_17)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(mastering_lv2_ui_host_test PRIVATE -Wall -Wextra)
endif()
add_dependencies(mastering_lv2_ui_host_test mastering_lv2_bundle)
if(XVFB_RUN)
  add_test(NAME mastering_lv2_ui_host
    COMMAND "${XVFB_RUN}" -a -s "-screen 0 1600x1000x24"
      "$<TARGET_FILE:mastering_lv2_ui_host_test>" "${LV2_BUNDLE_DIR}")
else()
  add_test(NAME mastering_lv2_ui_host
    COMMAND mastering_lv2_ui_host_test "${LV2_BUNDLE_DIR}")
endif()

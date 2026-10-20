cmake_minimum_required(VERSION 3.20)
project(AnotherFilter VERSION 0.1.0 LANGUAGES CXX)

set(VST3_SDK_ROOT "" CACHE PATH "Path to the VST3 SDK (github.com/steinbergmedia/vst3sdk)")
if(NOT VST3_SDK_ROOT OR NOT EXISTS "${VST3_SDK_ROOT}/CMakeLists.txt")
  message(FATAL_ERROR "Set -DVST3_SDK_ROOT=<path to vst3sdk>")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Keep the SDK build small (VSTGUI stays on: it draws the plugin window)
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ADD_VSTGUI ON CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VSTGUI_SUPPORT ON CACHE BOOL "" FORCE)
set(SMTG_CREATE_BUNDLE_FOR_WINDOWS ON CACHE BOOL "" FORCE)
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)

add_subdirectory(${VST3_SDK_ROOT} ${CMAKE_BINARY_DIR}/vst3sdk)
smtg_enable_vst3_sdk()

smtg_add_vst3plugin(AnotherFilter
  source/ms20_ids.h
  source/ms20_params.h
  source/ms20_filter.h
  source/ms20_synth.h
  source/ms20_presets.h
  source/ms20_scope.h
  source/ms20_processor.h
  source/ms20_processor.cpp
  source/ms20_controller.h
  source/ms20_controller.cpp
  source/ms20_entry.cpp
)
target_include_directories(AnotherFilter PRIVATE source)
target_link_libraries(AnotherFilter PRIVATE sdk vstgui_support)

smtg_target_add_plugin_resources(AnotherFilter
  RESOURCES
    resource/ms20.uidesc
    resource/knob.png
    resource/knob_2.0x.png
    resource/knob_s.png
    resource/knob_s_2.0x.png
)

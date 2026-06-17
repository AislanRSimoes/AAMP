# AAMP - CMakeLists.txt  (DOC-09 §5: CMake 3.20+, presets Windows/macOS)
# Este arquivo organiza o build de modo que o NUCLEO HEADLESS (L-03 Domain + DSP)
# seja compilado e testado sem GUI/hardware (CI Linux/Win/macOS), enquanto as camadas
# dependentes de plataforma (JUCE GUI, ASIO/CoreAudio/WASAPI, PDF/PNG) sao acopladas
# na maquina-alvo. Aderente a DOC-08 (camadas) e DOC-09 (estrutura/threading).
cmake_minimum_required(VERSION 3.20)
project(AAMP VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# P-07: zero warnings no codigo do projeto (src/). Headers externos via system headers.
if(MSVC)
  add_compile_options(/W4 /WX)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# ---- Nucleo headless determinístico (L-03 Domain + DSP) ----
add_library(aamp_core STATIC
  src/analysis/Fft.cpp
  src/analysis/Windows.cpp
  src/analysis/Hilbert.cpp
  src/analysis/Deconvolution.cpp
  src/analysis/AnalysisEngine.cpp
  src/stimulus/StimulusGenerator.cpp
  src/capture/IAudioDevice.cpp
  src/capture/CaptureEngine.cpp
  src/orchestration/StateMachine.cpp
  src/orchestration/MeasurementOrchestrator.cpp
  src/export/ExportEngine.cpp
  src/hardware/HardwareManager.cpp
  src/routing/RoutingManager.cpp
  src/dashboard/DashboardModel.cpp
)
target_include_directories(aamp_core PUBLIC src)

# ---- Testes (CTest; RD-04: Catch2/GTest na maquina-alvo) ----
enable_testing()
add_executable(aamp_tests
  tests/test_stimulus.cpp
  tests/test_analysis.cpp
  tests/test_windows.cpp
  tests/test_capture.cpp
  tests/test_orchestrator.cpp
  tests/test_export.cpp
  tests/test_hardware_routing.cpp
  tests/test_dashboard.cpp
  tests/main.cpp
)
target_link_libraries(aamp_tests PRIVATE aamp_core)
add_test(NAME aamp_headless_suite COMMAND aamp_tests)

# Cobertura (Debug): -DAAMP_COVERAGE=ON
option(AAMP_COVERAGE "Habilita gcov/lcov no nucleo headless" OFF)
if(AAMP_COVERAGE AND NOT MSVC)
  target_compile_options(aamp_core PRIVATE -O0 -g --coverage)
  target_compile_options(aamp_tests PRIVATE -O0 -g --coverage)
  target_link_options(aamp_tests PRIVATE --coverage)
  target_link_options(aamp_core PRIVATE --coverage)
endif()

# ---- Camadas de maquina-alvo (JUCE): GUI (M-06), backends (M-01/04), render (M-07) ----
option(AAMP_WITH_JUCE "Compila a aplicacao GUI/hardware na maquina-alvo" OFF)
if(AAMP_WITH_JUCE)
  find_package(JUCE CONFIG REQUIRED)
  juce_add_gui_app(aamp_app PRODUCT_NAME "AAMP")
  target_compile_definitions(aamp_app PRIVATE AAMP_WITH_JUCE=1)
  target_sources(aamp_app PRIVATE
    src/ui/Main.cpp                       # entry-point JUCE (turnkey)
  )
  target_link_libraries(aamp_app PRIVATE aamp_core
    juce::juce_audio_devices juce::juce_gui_basics juce::juce_graphics juce::juce_dsp)
  # Headers de scaffolding ja presentes: JuceDeviceBackend.h, JuceRenderWriters.h, DashboardComponent.h
  # Portoes P-03/P-07(MSVC/AppleClang)/E2E/SLAs cumpridos aqui (DOC-12 §3).
endif()

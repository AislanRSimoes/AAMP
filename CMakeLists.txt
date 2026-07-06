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

# ---- Ferramenta de regressao (tools/): roda metricas do AnalysisEngine sobre WAV externo ----
# Fora do produto. Reconciliar AAMP vs ferramentas de referencia (Ozone/REW) sobre o MESMO arquivo,
# sem captura/driver/latencia. Linka so no nucleo headless (aamp_core).
add_executable(wav_metric_harness tools/wav_metric_harness.cpp)
target_link_libraries(wav_metric_harness PRIVATE aamp_core)

# ---- Camadas de maquina-alvo (JUCE): GUI (M-06), backends (M-01/04), render (M-07) ----
option(AAMP_WITH_JUCE "Compila a aplicacao GUI/hardware na maquina-alvo" OFF)
if(AAMP_WITH_JUCE)
  find_package(JUCE CONFIG REQUIRED)
  # ATENCAO: NAO usar 'NEEDS_WEBVIEW2 TRUE' aqui. Essa opcao faz o JUCE chamar internamente
  # find_package(WebView2), que depende do modulo FindWebView2.cmake. No JUCE instalado via
  # find_package (C:\juce-install) esse modulo esta AUSENTE de lib/cmake/JUCE-8.0.12, e a
  # configuracao falha ("Could not find ... WebView2"). Em vez disso, fornecemos o SDK WebView2
  # explicitamente abaixo (equivalente ao que NEEDS_WEBVIEW2 faria: include + loader estatico).
  juce_add_gui_app(aamp_app PRODUCT_NAME "AAMP")
  target_compile_definitions(aamp_app PRIVATE AAMP_WITH_JUCE=1)
  target_compile_definitions(aamp_app PRIVATE JUCE_WEB_BROWSER=1)
  # Habilita o backend Edge/Chromium (Options::Backend::webview2) e link estatico do loader (Windows).
  target_compile_definitions(aamp_app PRIVATE JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1)
  target_compile_definitions(aamp_app PRIVATE JUCE_ASIO=1)
  target_include_directories(aamp_app PRIVATE "C:/asiosdk/common")

  # --- SDK WebView2 (Microsoft.Web.WebView2) fornecido explicitamente ---
  # Como o link estatico exige <WebView2.h> em tempo de compilacao, o caminho do pacote e
  # OBRIGATORIO. Aponte AAMP_WEBVIEW2_DIR para a pasta do pacote NuGet extraido (a que contem
  # build\native\include\WebView2.h), ex.:
  #   -DAAMP_WEBVIEW2_DIR="C:/webview2/pkg"
  set(AAMP_WEBVIEW2_DIR "" CACHE PATH
      "Pasta do pacote NuGet Microsoft.Web.WebView2 (contem build/native/include/WebView2.h)")
  if(NOT AAMP_WEBVIEW2_DIR)
    message(FATAL_ERROR
      "WebView2 SDK necessario. Defina -DAAMP_WEBVIEW2_DIR=<pasta do pacote Microsoft.Web.WebView2> "
      "(a que contem build/native/include/WebView2.h). Baixe o .nupkg de "
      "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2 e extraia-o.")
  endif()
  if(NOT EXISTS "${AAMP_WEBVIEW2_DIR}/build/native/include/WebView2.h")
    message(FATAL_ERROR
      "AAMP_WEBVIEW2_DIR invalido: nao encontrei build/native/include/WebView2.h em "
      "'${AAMP_WEBVIEW2_DIR}'. Aponte para a pasta do pacote Microsoft.Web.WebView2 extraido.")
  endif()
  target_include_directories(aamp_app PRIVATE "${AAMP_WEBVIEW2_DIR}/build/native/include")
  target_link_libraries(aamp_app PRIVATE
      "${AAMP_WEBVIEW2_DIR}/build/native/x64/WebView2LoaderStatic.lib")
  message(STATUS "AAMP: WebView2 SDK em ${AAMP_WEBVIEW2_DIR}")

  target_sources(aamp_app PRIVATE
    src/ui/Main.cpp
    src/bridge/AampWebBridge.cpp
)
target_link_libraries(aamp_app PRIVATE
    aamp_core
    juce::juce_audio_devices
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_graphics
    juce::juce_dsp
)
  # Headers de scaffolding ja presentes: JuceDeviceBackend.h, JuceRenderWriters.h, DashboardComponent.h
  # Portoes P-03/P-07(MSVC/AppleClang)/E2E/SLAs cumpridos aqui (DOC-12 §3).

  # --- Frontend: FONTE UNICA em C:\aamp\resources\aamp_laboratory.html (HTML oficial do projeto).
  #     O app carrega <exe>/resources/aamp_laboratory.html do DISCO em runtime; este passo garante que
  #     o build sempre use o HTML oficial (sem copias paralelas). Ajuste via -DAAMP_LAB_HTML=<caminho>.
  set(AAMP_LAB_HTML "C:/aamp/resources/aamp_laboratory.html"
      CACHE FILEPATH "HTML oficial do frontend (fonte unica) a ser copiado para <exe>/resources/")
  if(EXISTS "${AAMP_LAB_HTML}")
    add_custom_command(TARGET aamp_app POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:aamp_app>/resources"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${AAMP_LAB_HTML}" "$<TARGET_FILE_DIR:aamp_app>/resources/aamp_laboratory.html"
      COMMENT "AAMP: deploy do HTML oficial (${AAMP_LAB_HTML}) -> <exe>/resources/")
  else()
    message(WARNING "AAMP: HTML oficial nao encontrado em '${AAMP_LAB_HTML}'. "
                    "Defina -DAAMP_LAB_HTML=<caminho> para o aamp_laboratory.html oficial.")
  endif()
endif()
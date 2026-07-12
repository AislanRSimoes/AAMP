# AAMP - CMakeLists.txt  (DOC-09 §5: CMake 3.20+, presets Windows/macOS)
# Organiza o build de modo que o NÚCLEO HEADLESS (L-03 Domain + DSP) compile e teste sem
# GUI/hardware (CI Linux/Win/macOS), enquanto as camadas de plataforma (JUCE GUI, ASIO/
# CoreAudio/WASAPI, PDF/PNG) acoplam na máquina-alvo. Aderente a DOC-08/DOC-09.
#
# SOURCE OF TRUTH DO FRONTEND: resources/aamp_laboratory.html (raiz do projeto).
# O alvo aamp_app copia esse arquivo para <exe>/resources em POST_BUILD; QUALQUER HTML
# dentro de build/ é artefato gerado automaticamente e NUNCA deve ser editado.
cmake_minimum_required(VERSION 3.20)
project(AAMP VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# P-07: zero warnings no código do projeto (src/). Headers externos via system headers.
if(MSVC)
  add_compile_options(/W4 /WX)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# ---- Núcleo headless determinístico (L-03 Domain + DSP) ----
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

# ---- Testes (CTest; RD-04: Catch2/GTest na máquina-alvo) ----
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
option(AAMP_COVERAGE "Habilita gcov/lcov no núcleo headless" OFF)
if(AAMP_COVERAGE AND NOT MSVC)
  target_compile_options(aamp_core PRIVATE -O0 -g --coverage)
  target_compile_options(aamp_tests PRIVATE -O0 -g --coverage)
  target_link_options(aamp_tests PRIVATE --coverage)
  target_link_options(aamp_core PRIVATE --coverage)
endif()

# ---- Ferramenta de regressão (tools/): métricas do AnalysisEngine sobre WAV externo ----
add_executable(wav_metric_harness tools/wav_metric_harness.cpp)
target_link_libraries(wav_metric_harness PRIVATE aamp_core)

# ---- Scientific Validation Harness (validation/): infraestrutura permanente de validação ----
# Etapa 0: ponto de entrada permanente da validação científica, integrado ao CTest. Reutiliza
# o núcleo determinístico (StimulusGenerator/ExportEngine) e o framework de teste (tests/).
# Sem casos científicos, sem parser JSON, sem abstrações antecipadas.
add_executable(aamp_validation_harness validation/harness/validation_main.cpp)
target_link_libraries(aamp_validation_harness PRIVATE aamp_core)
target_include_directories(aamp_validation_harness PRIVATE tests)   # tests/TestRunner.h
target_compile_definitions(aamp_validation_harness PRIVATE
  AAMP_VALIDATION_DATASET_DIR="${CMAKE_SOURCE_DIR}/validation/dataset")
add_test(NAME aamp_scientific_validation COMMAND aamp_validation_harness)

# ---- Camadas de máquina-alvo (JUCE): casca WebView (M-06), backends (M-01/04), render (M-07) ----
option(AAMP_WITH_JUCE "Compila a aplicação GUI/hardware na máquina-alvo" OFF)
if(AAMP_WITH_JUCE)
  find_package(JUCE CONFIG REQUIRED)
  # ---- WebView2 (OBRIGATÓRIO): sem JUCE_USE_WIN_WEBVIEW2=1 o JUCE compila apenas o
  # backend IE11 e o pedido de Backend::webview2 é silenciosamente ignorado — o frontend
  # moderno (ES6) quebra com "Erro de Script". Ligamos o SDK diretamente, sem depender do
  # find_package(WebView2) do JUCE:
  #   1) baixe o pacote NuGet Microsoft.Web.WebView2 (o .nupkg é um zip);
  #   2) extraia para AAMP_WEBVIEW2_SDK de modo que exista
  #        <SDK>/build/native/include/WebView2.h
  #        <SDK>/build/native/x64/WebView2LoaderStatic.lib
  set(AAMP_WEBVIEW2_SDK "C:/webview2" CACHE PATH
      "Raiz do SDK Microsoft.Web.WebView2 (conteúdo do .nupkg extraído)")
  if(NOT EXISTS "${AAMP_WEBVIEW2_SDK}/build/native/include/WebView2.h")
    message(FATAL_ERROR "WebView2 SDK não encontrado em ${AAMP_WEBVIEW2_SDK}. "
      "Extraia o pacote NuGet Microsoft.Web.WebView2 para esse caminho "
      "(ou configure -DAAMP_WEBVIEW2_SDK=<caminho>).")
  endif()
  juce_add_gui_app(aamp_app PRODUCT_NAME "AAMP")
  target_compile_definitions(aamp_app PRIVATE
    AAMP_WITH_JUCE=1
    JUCE_WEB_BROWSER=1
    JUCE_USE_WIN_WEBVIEW2=1
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_ASIO=1
  )
  target_include_directories(aamp_app PRIVATE
    "${AAMP_WEBVIEW2_SDK}/build/native/include"
    "C:/asiosdk/common"
  )
  target_link_libraries(aamp_app PRIVATE
    "${AAMP_WEBVIEW2_SDK}/build/native/x64/WebView2LoaderStatic.lib"
  )
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

  # Deploy do frontend oficial junto ao executável (artefato; a FONTE permanece em resources/).
  add_custom_command(TARGET aamp_app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:aamp_app>/resources"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/resources/aamp_laboratory.html"
            "$<TARGET_FILE_DIR:aamp_app>/resources/aamp_laboratory.html"
    COMMENT "Deploy do frontend oficial (resources/aamp_laboratory.html -> artefato do build)"
  )
endif()

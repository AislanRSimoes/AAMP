#!/usr/bin/env bash
# Reproduz exatamente o build/execucao headless usado para o pacote de evidencias.
set -euo pipefail
set -x
cd "$(dirname "$0")"
SRC="src/analysis/Fft.cpp src/analysis/Windows.cpp src/analysis/Hilbert.cpp \
     src/analysis/Deconvolution.cpp src/analysis/AnalysisEngine.cpp src/stimulus/StimulusGenerator.cpp \
     src/capture/IAudioDevice.cpp src/capture/CaptureEngine.cpp \
     src/orchestration/StateMachine.cpp src/orchestration/MeasurementOrchestrator.cpp \
     src/export/ExportEngine.cpp \
     src/hardware/HardwareManager.cpp src/routing/RoutingManager.cpp src/dashboard/DashboardModel.cpp"
TST="tests/test_stimulus.cpp tests/test_analysis.cpp tests/test_windows.cpp tests/test_capture.cpp tests/test_orchestrator.cpp tests/test_export.cpp tests/test_hardware_routing.cpp tests/test_dashboard.cpp tests/main.cpp"
rm -f *.gcda *.gcno *.gcov *.o build_tests
for f in $SRC $TST; do
  g++ -std=c++17 -O0 -g -Wall -Wextra --coverage -I src -c "$f" -o "$(basename "${f%.cpp}").o"
done
g++ --coverage ./*.o -o build_tests
./build_tests
echo "--- coverage ---"
for o in StimulusGenerator AnalysisEngine Fft Windows Hilbert Deconvolution IAudioDevice CaptureEngine StateMachine MeasurementOrchestrator ExportEngine HardwareManager RoutingManager DashboardModel; do
  gcov -n "$o.o" 2>/dev/null | grep -A1 "$o.cpp" | grep "Lines executed" | head -1
done

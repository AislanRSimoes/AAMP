// AAMP - src/orchestration/MeasurementOrchestrator.h
// M-08 (DOC-08 §3): runSuite() automatica a partir de 1 comando (FR-037), maquina de estados,
// progresso amigavel (FR-030/DOC-04 §6), recuperacao DOC-07 §4 (3/1/1), degradacao graciosa
// (FR-040), confirmacao de descarte (FR-038), cancelamento seguro (FR-039), estimativa de
// memoria pre-suite (DOC-09 §4.1). A decisao de validade/repeticao/aborto e EXCLUSIVA de M-08.
#pragma once
#include <functional>
#include <string>
#include <vector>
#include "StateMachine.h"
#include "../capture/CaptureEngine.h"
#include "../capture/IAudioDevice.h"
#include "../analysis/AnalysisEngine.h"
#include "../common/ResultBundle.h"

namespace aamp {

// Feedback de progresso SEM detalhes tecnicos (DOC-04 §6): rotulos amigaveis, percentual,
// etapa atual, concluidas/pendentes. Nunca expoe nomes de modulo, timestamps ou logs.
struct Progress {
    std::string label;     // ex.: "Medindo", "Processando resultados", "Concluido"
    int percent = 0;       // 0..100
    int completed = 0;
    int total = 0;
};

enum class RunOutcome { Completed, Aborted, Cancelled, InsufficientMemory };

struct RunResult {
    RunOutcome outcome = RunOutcome::Aborted;
    ResultBundle bundle;             // valido se Completed
    std::string userMessage;         // mensagem amigavel (DOC-07) quando aborta
    int errorMetrics = 0;            // metricas isoladas por degradacao graciosa (FR-040)
};

class MeasurementOrchestrator {
public:
    MeasurementOrchestrator(double fs, ChannelMode mode);

    void setProgressCallback(std::function<void(const Progress&)> cb) { m_progress = std::move(cb); }

    // Valida o dispositivo e transita IDLE -> READY (BR-003).
    bool prepare(IAudioDevice& dev);

    // Executa a suite completa automaticamente (FR-037). Unico comando.
    RunResult runSuite(IAudioDevice& dev);

    // Cancelamento seguro durante MEASURING/PROCESSING (FR-039). Thread-safe-ish (flag).
    void requestCancel() { m_cancel = true; }

    // Nova medicao a partir de RESULTS exige confirmacao de descarte (FR-038, BR-032).
    bool startNewMeasurement(bool confirmDiscard, bool resultsExported);

    // Estimativa de memoria pre-suite (DOC-09 §4.1). Retorna bytes estimados.
    std::size_t estimateSuiteMemoryBytes() const;

    State state() const { return m_sm.state(); }

    // Limite operacional de memoria (DOC-09 §7): 2 GB.
    static constexpr std::size_t MEMORY_LIMIT_BYTES = 2ull * 1024 * 1024 * 1024;

private:
    double m_fs;
    ChannelMode m_mode;
    StateMachine m_sm;
    CaptureEngine m_capture;
    AnalysisEngine m_analysis;
    std::function<void(const Progress&)> m_progress;
    bool m_cancel = false;

    void report(const std::string& label, int completed, int total);
    // Captura com politica de repeticao/aborto (DOC-07 §4: 3 tentativas de captura).
    bool captureWithRetry(const Stimulus& stim, IAudioDevice& dev,
                          std::vector<double>& outSamples, std::string& abortMsg);
    bool levelPreCheck(IAudioDevice& dev, std::string& abortMsg);  // BR-021
};

} // namespace aamp

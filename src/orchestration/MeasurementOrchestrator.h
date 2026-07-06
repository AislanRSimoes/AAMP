// AAMP - src/orchestration/MeasurementOrchestrator.h
// M-08 (DOC-08 sec.3): runSuite() automatica a partir de 1 comando (FR-037), maquina de estados,
// progresso amigavel (FR-030/DOC-04 sec.6), recuperacao DOC-07 sec.4 (3/1/1), degradacao graciosa
// (FR-040), confirmacao de descarte (FR-038), cancelamento seguro (FR-039), estimativa de
// memoria pre-suite (DOC-09 sec.4.1). A decisao de validade/repeticao/aborto e EXCLUSIVA de M-08.
//
// Arquitetura orientada ao estimulo (substitui o batch): cada estimulo segue
//   gerar -> capturar resposta completa -> validar integridade -> processar metricas dependentes
//   -> emitir resultado/evidencia -> proximo estimulo.
// As metricas sao calculadas UMA vez, assim que TODOS os seus estimulos requeridos foram
// capturados, e emitidas imediatamente via callback (resultado em tempo real).
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
// Feedback de progresso SEM detalhes tecnicos (DOC-04 sec.6): rotulos amigaveis, percentual,
// etapa atual, concluidas/pendentes. Nunca expoe nomes de modulo, timestamps ou logs.
struct Progress {
    std::string label;     // ex.: "Medindo", "Processando resultados", "Concluido"
    int percent = 0;       // 0..100
    int completed = 0;
    int total = 0;
};
// Relatorio de integridade por estimulo (monitoramento em tempo real, Requisito 2):
// RMS, peak, energia residual, clipping, e o estado da captura adaptativa (decaiu vs nao-decaente).
struct StimulusReport {
    int    index = 0;                      // 1..N (posicao na suite)
    int    total = 0;
    int    stimSamples = 0;                // tamanho do estimulo gerado
    int    capturedSamples = 0;            // tamanho efetivamente entregue pela captura (janela completa)
    double rms = 0.0;
    double peak = 0.0;
    double residual = 0.0;                 // RMS da cauda apos o fim do estimulo (energia residual)
    bool   clipped = false;                // BR-022 / ER-020
    bool   nonDecayed = false;             // ainda com energia no fim -> freeze/auto-oscilacao (timeout)
    int    observedDurationSamples = 0;    // duracao real observada (estimulo + cauda ate o limiar)
};
// Contexto da emissao incremental de cada metrica (evidencia de execucao, Requisito 4):
// quais estimulos satisfizeram a dependencia, quantos estimulos ja terminaram e qual a ordem de emissao.
struct MetricEmissionInfo {
    std::string deps;          // rotulo das dependencias satisfeitas (ex.: "ST08+ST09")
    int stimuliCompleted = 0;  // estimulos concluidos no instante da emissao
    int totalStimuli = 0;
    int emittedIndex = 0;      // K em K/total
    int totalMetrics = 0;
};
enum class RunOutcome { Completed, Aborted, Cancelled, InsufficientMemory };
struct RunResult {
    RunOutcome outcome = RunOutcome::Aborted;
    ResultBundle bundle;             // valido se Completed
    std::string userMessage;         // mensagem amigavel (DOC-07) quando aborta
    int errorMetrics = 0;            // metricas isoladas por degradacao graciosa (FR-040)
    int emittedIncremental = 0;      // metricas emitidas DURANTE o laco (evidencia de nao-batch)
    int emittedAfterSuite = 0;       // metricas emitidas APOS o fim do laco (deve ser 0)
};
class MeasurementOrchestrator {
public:
    MeasurementOrchestrator(double fs, ChannelMode mode);
    void setProgressCallback(std::function<void(const Progress&)> cb) { m_progress = std::move(cb); }
    // Resultado de cada metrica assim que fica pronta (Requisito 4/5: apresentacao imediata),
    // com o contexto da emissao (deps satisfeitas, estimulos concluidos, ordem) p/ evidencia no log.
    void setResultCallback(std::function<void(const MetricResult&, const MetricEmissionInfo&)> cb) { m_result = std::move(cb); }
    // Integridade de cada estimulo logo apos a captura (Requisito 2: monitoramento continuo).
    void setStimulusCallback(std::function<void(const StimulusReport&)> cb) { m_stimcb = std::move(cb); }
    // Valida o dispositivo e transita IDLE -> READY (BR-003).
    bool prepare(IAudioDevice& dev);
    // Executa a suite completa automaticamente (FR-037). Unico comando. Fluxo por estimulo.
    RunResult runSuite(IAudioDevice& dev);
    // Cancelamento seguro durante MEASURING/PROCESSING (FR-039). Thread-safe-ish (flag).
    void requestCancel() { m_cancel = true; }
    // Nova medicao a partir de RESULTS exige confirmacao de descarte (FR-038, BR-032).
    bool startNewMeasurement(bool confirmDiscard, bool resultsExported);
    // Estimativa de memoria pre-suite (DOC-09 sec.4.1). Retorna bytes estimados.
    std::size_t estimateSuiteMemoryBytes() const;
    State state() const { return m_sm.state(); }
    // Limite operacional de memoria (DOC-09 sec.7): 2 GB.
    static constexpr std::size_t MEMORY_LIMIT_BYTES = 2ull * 1024 * 1024 * 1024;
private:
    double m_fs;
    ChannelMode m_mode;
    StateMachine m_sm;
    CaptureEngine m_capture;
    AnalysisEngine m_analysis;
    std::function<void(const Progress&)> m_progress;
    std::function<void(const MetricResult&, const MetricEmissionInfo&)> m_result;
    std::function<void(const StimulusReport&)> m_stimcb;
    bool m_cancel = false;
    double m_noiseFloorRms = 0.0;          // piso medido (ST-09) p/ limiar de decaimento adaptativo
    void report(const std::string& label, int completed, int total);
    // Captura com politica de repeticao/aborto (DOC-07 sec.4: 3 tentativas de captura).
    // Devolve o CaptureResult completo (status + duracao observada) para o relatorio de integridade.
    bool captureWithRetry(const Stimulus& stim, IAudioDevice& dev,
                          CaptureResult& outResult, std::string& abortMsg);
    bool levelPreCheck(IAudioDevice& dev, std::string& abortMsg);  // BR-021
    StimulusReport makeStimulusReport(const Stimulus& stim, const CaptureResult& c,
                                      int idx, int total) const;
};
} // namespace aamp

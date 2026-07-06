// AAMP - src/capture/CaptureEngine.h
// M-04 (DOC-08 sec.3): capture(), detectClipping(), coherentAverage(), detectLowSignal().
// Deteccao de clipping/sinal-baixo e EXCLUSIVA de M-04 (DOC-08 sec.3.1). A decisao de
// repeticao/aborto e de M-08, que consome estes sinais.
#pragma once
#include <vector>
#include "IAudioDevice.h"
#include "../stimulus/StimulusGenerator.h"
namespace aamp {
struct CaptureStatus {
    bool clipped   = false;  // BR-022 / ER-020
    bool lowSignal = false;  // DOC-07 sec.5 / ER-023 (aviso nao bloqueante)
    bool partial   = false;  // ER-024 (nunca processar dados parciais)
    bool valid     = false;  // !clipped && !partial
};
struct CaptureResult {
    std::vector<double> samples;  // vazio se partial (nunca processado)
    CaptureStatus status;
    int syncOffsetSamples = 0;    // alinhamento medido (FR-018)
    // Fase 1 (aquisicao completa) - preenchidos quando USE_ADAPTIVE_CAPTURE:
    int  observedDurationSamples = 0;            // estimulo + cauda observada acima do limiar
    bool nonDecayed = false;                     // ainda acima do limiar no fim (freeze/oscilacao)
    int  decay20 = -1, decay40 = -1, decay60 = -1; // amostras apos fim do estimulo p/ -20/-40/-60 dB
};
struct AverageResult {
    std::vector<double> samples;
    int validUsed = 0;
    int invalidDiscarded = 0;
    bool aborted = false;         // nao atingiu N validas dentro do limite de tentativas
};
class CaptureEngine {
public:
    explicit CaptureEngine(double fs, double clipThresholdDbfs = 0.0);
    // Captura sincronizada de um estimulo (FR-018/019/020/021/023).
    // noiseFloorRms: piso de ruido medido (ST-09) para deteccao de sinal baixo (DOC-07 sec.5).
    CaptureResult capture(const Stimulus& stim, IAudioDevice& dev,
                          double noiseFloorRms = 0.0) const;
    // Averaging coerente no tempo (FR-022, DOC-03 sec.4.3, BR-023): N capturas validas,
    // descartando invalidas, ate o limite de tentativas (DOC-07 sec.4 = 3 por etapa).
    AverageResult coherentAverage(const Stimulus& stim, IAudioDevice& dev,
                                  int targetValid, int maxRetries,
                                  double noiseFloorRms = 0.0) const;
    bool detectClipping(const std::vector<double>& x) const;
    bool detectLowSignal(const std::vector<double>& x, double noiseFloorRms) const;
    // Offset de sincronizacao via IR de ST-06 (pico) relativo a posicao emitida.
    static int measureSyncOffset(const std::vector<double>& captured, double fs);
private:
    double m_fs;
    double m_clipThreshold;               // linear (BR-022)
    mutable int  m_syncOffset   = 0;      // FR-018: offset de sincronizacao medido no ST-06
    mutable bool m_syncMeasured = false;  // FR-018: offset ja medido nesta medicao?
};
} // namespace aamp

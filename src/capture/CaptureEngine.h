// AAMP - src/capture/CaptureEngine.h
// M-04 (DOC-08 §3): capture(), detectClipping(), coherentAverage(), detectLowSignal().
// Deteccao de clipping/sinal-baixo e EXCLUSIVA de M-04 (DOC-08 §3.1). A decisao de
// repeticao/aborto e de M-08, que consome estes sinais.
#pragma once
#include <vector>
#include "IAudioDevice.h"
#include "../stimulus/StimulusGenerator.h"

namespace aamp {

struct CaptureStatus {
    bool clipped   = false;  // BR-022 / ER-020
    bool lowSignal = false;  // DOC-07 §5 / ER-023 (aviso nao bloqueante)
    bool partial   = false;  // ER-024 (nunca processar dados parciais)
    bool valid     = false;  // !clipped && !partial
};

struct CaptureResult {
    std::vector<double> samples;  // vazio se partial (nunca processado)
    CaptureStatus status;
    int syncOffsetSamples = 0;    // alinhamento medido (FR-018, ≤ 1 sample em loopback ideal)
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
    // noiseFloorRms: piso de ruido medido (ST-09) para deteccao de sinal baixo (DOC-07 §5).
    CaptureResult capture(const Stimulus& stim, IAudioDevice& dev,
                          double noiseFloorRms = 0.0) const;

    // Averaging coerente no tempo (FR-022, DOC-03 §4.3, BR-023): N capturas validas,
    // descartando invalidas, ate o limite de tentativas (DOC-07 §4 = 3 por etapa).
    AverageResult coherentAverage(const Stimulus& stim, IAudioDevice& dev,
                                  int targetValid, int maxRetries,
                                  double noiseFloorRms = 0.0) const;

    bool detectClipping(const std::vector<double>& x) const;
    bool detectLowSignal(const std::vector<double>& x, double noiseFloorRms) const;

    // Offset de sincronizacao via IR de ST-06 (pico) relativo a posicao emitida.
    static int measureSyncOffset(const std::vector<double>& captured, double fs);

private:
    double m_fs;
    double m_clipThreshold; // linear (BR-022)
};

} // namespace aamp

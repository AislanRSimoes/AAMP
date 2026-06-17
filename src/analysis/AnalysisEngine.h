// AAMP - src/analysis/AnalysisEngine.h
// M-05 (DOC-08 §3). computeAllMetrics() -> 15 familias (FR-024). Double-precision (AD-02),
// metodos exatos de DOC-03 (FR-025), determinístico (FR-026), flag de baixa confiab. (FR-027).
#pragma once
#include <map>
#include <vector>
#include "../common/ResultBundle.h"
#include "../stimulus/StimulusGenerator.h"

namespace aamp {

// Conjunto capturado por estimulo (entregue por M-04). Para loopback ideal: captured == stimulus.
struct CapturedSet {
    double fs = 48000.0;
    ChannelMode mode = ChannelMode::Mono;
    std::map<StimulusId, std::vector<double>> captured;     // canal principal
    std::map<StimulusId, std::vector<double>> capturedR;    // canal direito (estereo) p/ crosstalk
    double noiseDbfsHint = -120.0;                          // piso de ruido sintetico (loopback)
};

class AnalysisEngine {
public:
    explicit AnalysisEngine(double fs) : m_fs(fs) {}

    // Calcula as 15 familias e devolve um ResultBundle tipado (FR-024).
    ResultBundle computeAllMetrics(const CapturedSet& cap) const;

    // --- primitivas expostas para teste (TC-031 etc.) ---
    // Magnitude espectral em dB (referencia full-scale) e frequencia por bin.
    static int peakBin(const std::vector<double>& signal, int n);
    static double binFreq(int bin, int n, double fs);
    // THD em % para um tom puro de frequencia f0 (Blackman-Harris 4-termos).
    static double thdPercent(const std::vector<double>& signal, double f0, double fs, int n);
    // SNR em dB dado sinal e ruido (potencias RMS).
    static double snrDb(const std::vector<double>& signal, const std::vector<double>& noise);

private:
    double m_fs;
    MetricResult freqResponse(const CapturedSet&) const;     // MS-01
    MetricResult phaseResponse(const CapturedSet&) const;    // MS-02
    MetricResult groupDelay(const CapturedSet&) const;       // MS-03
    MetricResult impulseResp(const CapturedSet&) const;      // MS-04
    MetricResult thd(const CapturedSet&) const;              // MS-05
    MetricResult thdN(const CapturedSet&) const;             // MS-06
    MetricResult imd(const CapturedSet&) const;              // MS-07
    MetricResult noiseFloor(const CapturedSet&) const;       // MS-08
    MetricResult dynamicRange(const CapturedSet&) const;     // MS-09
    MetricResult snr(const CapturedSet&) const;              // MS-10
    MetricResult crosstalk(const CapturedSet&) const;        // MS-11
    MetricResult latency(const CapturedSet&) const;          // MS-12
    MetricResult dynamicBehaviour(const CapturedSet&) const; // MS-13
    MetricResult behavioural(const CapturedSet&) const;      // MS-14
    MetricResult musical(const CapturedSet&) const;          // MS-15
};

} // namespace aamp

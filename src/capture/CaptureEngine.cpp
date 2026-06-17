// AAMP - src/capture/CaptureEngine.cpp
#include "CaptureEngine.h"
#include "../common/Constants.h"
#include <cmath>
#include <algorithm>

namespace aamp {

static double rmsOf(const std::vector<double>& x) {
    if (x.empty()) return 0.0;
    double s = 0.0; for (double v : x) s += v * v;
    return std::sqrt(s / static_cast<double>(x.size()));
}

CaptureEngine::CaptureEngine(double fs, double clipThresholdDbfs)
    : m_fs(fs), m_clipThreshold(std::pow(10.0, clipThresholdDbfs / 20.0)) {}

bool CaptureEngine::detectClipping(const std::vector<double>& x) const {
    for (double v : x) if (std::fabs(v) > m_clipThreshold) return true;
    return false;
}

bool CaptureEngine::detectLowSignal(const std::vector<double>& x, double noiseFloorRms) const {
    if (noiseFloorRms <= 0.0) return false;            // sem referencia de ruido -> nao avalia
    const double r = rmsOf(x);
    if (r <= 0.0) return true;
    const double snrDb = 20.0 * std::log10(r / noiseFloorRms);
    return snrDb < k::MIN_SNR_OVER_FLOOR_DB;           // < 20 dB acima do piso (DOC-07 §5)
}

int CaptureEngine::measureSyncOffset(const std::vector<double>& captured, double fs) {
    // Para ST-06, o pico capturado relativo a posicao de emissao round(0.5*fs) e o offset.
    int pk = 0; double pv = 0.0;
    for (int i = 0; i < static_cast<int>(captured.size()); ++i)
        if (std::fabs(captured[static_cast<std::size_t>(i)]) > pv) { pv = std::fabs(captured[static_cast<std::size_t>(i)]); pk = i; }
    return pk - k::impulsePositionSamples(fs);
}

CaptureResult CaptureEngine::capture(const Stimulus& stim, IAudioDevice& dev,
                                     double noiseFloorRms) const {
    CaptureResult r;
    // Janela de captura = estimulo + post-roll (cauda); pre-roll modelado no inicio.
    const std::size_t post = static_cast<std::size_t>(k::POST_ROLL_SECONDS * m_fs + 0.5);
    const std::size_t captureLen = stim.samples.size() + post;
    std::vector<double> raw = dev.playAndCapture(stim.samples, captureLen);

    // ER-024: captura parcial (buffer incompleto) -> NUNCA processa dados parciais.
    if (raw.size() < captureLen) {
        r.status.partial = true;
        r.status.valid = false;
        return r;  // samples vazio
    }
    r.status.clipped   = detectClipping(raw);           // ER-020
    r.status.lowSignal = detectLowSignal(raw, noiseFloorRms); // ER-023 (nao bloqueante)
    r.status.valid     = !r.status.clipped && !r.status.partial;
    r.samples = std::move(raw);
    if (stim.id == StimulusId::ST06_Impulse)
        r.syncOffsetSamples = measureSyncOffset(r.samples, m_fs);
    return r;
}

AverageResult CaptureEngine::coherentAverage(const Stimulus& stim, IAudioDevice& dev,
                                             int targetValid, int maxRetries,
                                             double noiseFloorRms) const {
    AverageResult out;
    std::vector<std::vector<double>> valids;
    int attempts = 0;
    const int maxAttempts = targetValid + maxRetries;
    while (static_cast<int>(valids.size()) < targetValid && attempts < maxAttempts) {
        ++attempts;
        CaptureResult c = capture(stim, dev, noiseFloorRms);
        if (c.status.valid) valids.push_back(c.samples);
        else                out.invalidDiscarded++;     // descartada e substituida
    }
    if (static_cast<int>(valids.size()) < targetValid) { out.aborted = true; return out; }

    // Media coerente no dominio do tempo (DOC-03 §4.3).
    const std::size_t L = valids.front().size();
    out.samples.assign(L, 0.0);
    for (const auto& v : valids)
        for (std::size_t i = 0; i < L && i < v.size(); ++i) out.samples[i] += v[i];
    for (double& s : out.samples) s /= static_cast<double>(valids.size());
    out.validUsed = static_cast<int>(valids.size());
    return out;
}

} // namespace aamp

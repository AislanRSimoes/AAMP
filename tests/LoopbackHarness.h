// AAMP - tests/LoopbackHarness.h
// Loopback virtual (Plano de Testes §2.1): sistema sob teste sintetico H(f)=1, latencia conhecida.
// Permite verificar sincronizacao, determinismo e as 15 familias sem hardware.
#pragma once
#include <vector>
#include <map>
#include "../src/analysis/AnalysisEngine.h"
#include "../src/stimulus/StimulusGenerator.h"
#include "../src/common/Constants.h"
#include <cmath>
#include <random>
#include <cstdint>

namespace harness {

using namespace aamp;

// Desloca um sinal por D samples (latencia conhecida), preenchendo com zeros.
inline std::vector<double> shift(const std::vector<double>& x, int D) {
    if (D <= 0) return x;
    std::vector<double> y(x.size(), 0.0);
    for (std::size_t i = 0; i + static_cast<std::size_t>(D) < x.size(); ++i)
        y[i + static_cast<std::size_t>(D)] = x[i];
    return y;
}

// Ruido determinístico de nivel alvo (dBFS RMS) a partir de seed fixo (para ST-09 sintetico).
inline std::vector<double> deterministicNoise(std::size_t n, double targetDbfs, unsigned long seed = 12345UL) {
    std::mt19937 mt(static_cast<std::uint32_t>(seed));
    std::vector<double> y(n);
    for (std::size_t i = 0; i < n; ++i)
        y[i] = (static_cast<double>(mt()) / 4294967296.0) * 2.0 - 1.0;
    double s = 0.0; for (double v : y) s += v * v;
    const double r = std::sqrt(s / static_cast<double>(n));
    const double g = (r > 0.0) ? std::pow(10.0, targetDbfs / 20.0) / r : 0.0;
    for (double& v : y) v *= g;
    return y;
}

// Constroi um CapturedSet de loopback ideal a partir da suite.
// latencyD: latencia em samples aplicada a todos os capturados.
// stereo: se verdadeiro, preenche capturedR com vazamento atenuado (crosstalk).
inline CapturedSet makeLoopback(double fs, int latencyD = 0, bool stereo = false,
                                double noiseFloorDbfs = -95.0) {
    StimulusGenerator gen(fs);
    CapturedSet cap;
    cap.fs = fs;
    cap.mode = stereo ? ChannelMode::Stereo : ChannelMode::Mono;
    for (const Stimulus& s : gen.generateSuite()) {
        if (s.id == StimulusId::ST09_Silence) {
            // ST-09 capturado = ruido intrinseco sintetico (piso de ruido determinístico).
            cap.captured[s.id] = deterministicNoise(s.samples.size(), noiseFloorDbfs);
        } else {
            cap.captured[s.id] = shift(s.samples, latencyD);
        }
        if (stereo && s.id == StimulusId::ST01_ESS) {
            // canal inativo com vazamento de -90 dB (atenuacao 10^(-90/20)).
            std::vector<double> leak = s.samples;
            const double g = std::pow(10.0, -90.0 / 20.0);
            for (double& v : leak) v *= g;
            cap.capturedR[s.id] = shift(leak, latencyD);
        }
    }
    return cap;
}

} // namespace harness

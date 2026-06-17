// AAMP - src/capture/IAudioDevice.cpp
#include "IAudioDevice.h"
#include <random>
#include <cmath>
#include <cstdint>

namespace aamp {

std::vector<double> LoopbackDevice::playAndCapture(const std::vector<double>& stimulus,
                                                   std::size_t captureLen) {
    std::vector<double> cap(captureLen, 0.0);
    // H(f)=1 com latencia m_latency: captured[i+latency] = gain * stimulus[i].
    for (std::size_t i = 0; i < stimulus.size(); ++i) {
        const std::size_t j = i + static_cast<std::size_t>(m_latency);
        if (j < captureLen) cap[j] = m_gain * stimulus[i];
    }
    // piso de ruido determinístico (seed fixo) somado a captura.
    if (m_noiseDbfs > -200.0) {
        std::mt19937 mt(2024u);
        const double target = std::pow(10.0, m_noiseDbfs / 20.0);
        // normaliza um lote para RMS alvo
        std::vector<double> nz(captureLen);
        double s = 0.0;
        for (std::size_t i = 0; i < captureLen; ++i) {
            nz[i] = (static_cast<double>(mt()) / 4294967296.0) * 2.0 - 1.0;
            s += nz[i] * nz[i];
        }
        const double rms = (captureLen > 0) ? std::sqrt(s / static_cast<double>(captureLen)) : 0.0;
        const double g = (rms > 0.0) ? target / rms : 0.0;
        for (std::size_t i = 0; i < captureLen; ++i) cap[i] += nz[i] * g;
    }
    // captura parcial (falha de driver): devolve buffer truncado.
    if (m_truncate > 0 && m_truncate < cap.size())
        cap.resize(m_truncate);
    return cap;
}

} // namespace aamp

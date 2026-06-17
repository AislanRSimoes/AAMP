// AAMP - src/analysis/Hilbert.cpp
#include "Hilbert.h"
#include "Fft.h"
#include "../common/Constants.h"
#include <cmath>
#include <algorithm>

namespace aamp {

static int nextPow2(std::size_t n) {
    int p = 1;
    while (static_cast<std::size_t>(p) < n) p <<= 1;
    if (p < k::FFT_MIN) p = k::FFT_MIN;
    return p;
}

std::vector<double> analyticMagnitude(const std::vector<double>& signal) {
    const std::size_t L = signal.size();
    if (L == 0) return {};
    const int N = nextPow2(L);
    std::vector<Complex> X = Fft::forwardReal(signal, N);

    // Filtro analitico: h[0]=1, h[N/2]=1, h[1..N/2-1]=2, resto=0.
    for (int i = 0; i < N; ++i) {
        double h;
        if (i == 0 || i == N / 2)       h = 1.0;
        else if (i < N / 2)             h = 2.0;
        else                            h = 0.0;
        X[static_cast<std::size_t>(i)] *= h;
    }
    Fft::transform(X, true); // IFFT

    std::vector<double> env(L);
    for (std::size_t i = 0; i < L; ++i)
        env[i] = std::abs(X[i]);
    return env;
}

std::vector<double> extractEnvelope(const std::vector<double>& signal, double fs) {
    std::vector<double> mag = analyticMagnitude(signal);
    const std::size_t L = mag.size();
    if (L == 0) return {};

    // Media movel de round(0.001 * fs) samples (DOC-03 §4.4).
    int w = static_cast<int>(k::ENVELOPE_MA_SECONDS * fs + 0.5);
    if (w < 1) w = 1;
    std::vector<double> out(L, 0.0);
    double acc = 0.0;
    // Soma prefixada para janela causal-centrada simples (box filter).
    std::vector<double> prefix(L + 1, 0.0);
    for (std::size_t i = 0; i < L; ++i) prefix[i + 1] = prefix[i] + mag[i];
    const int half = w / 2;
    for (std::size_t i = 0; i < L; ++i) {
        long lo = static_cast<long>(i) - half;
        long hi = static_cast<long>(i) - half + w; // [lo, hi)
        if (lo < 0) lo = 0;
        if (hi > static_cast<long>(L)) hi = static_cast<long>(L);
        const double sum = prefix[static_cast<std::size_t>(hi)] - prefix[static_cast<std::size_t>(lo)];
        out[i] = sum / static_cast<double>(hi - lo);
    }
    (void)acc;
    return out;
}

} // namespace aamp

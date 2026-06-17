// AAMP - src/analysis/Deconvolution.cpp
#include "Deconvolution.h"
#include "Fft.h"
#include "../common/Constants.h"
#include <cmath>
#include <algorithm>

namespace aamp {

std::vector<Cx> deconvolveTikhonov(const std::vector<Cx>& Y,
                                   const std::vector<Cx>& X,
                                   const std::vector<double>& noise) {
    const std::size_t N = X.size();
    std::vector<Cx> H(N, Cx(0.0, 0.0));
    for (std::size_t i = 0; i < N; ++i) {
        const double S = std::norm(X[i]); // |X|^2 = energia de sinal no bin
        const double Nf = (i < noise.size()) ? std::max(0.0, noise[i]) : 0.0;
        // lambda adaptativo: cresce quando ruido domina o sinal; estavel quando S->0.
        const double ratio = (S > 0.0) ? (Nf / S) : 1e12;
        const double lambda = k::LAMBDA_BASE * (1.0 + ratio);
        const double denom = S + lambda; // sempre > 0 -> sem divisao por zero
        H[i] = (Y[i] * std::conj(X[i])) / denom;
    }
    return H;
}

} // namespace aamp

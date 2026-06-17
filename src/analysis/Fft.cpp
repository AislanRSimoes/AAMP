// AAMP - src/analysis/Fft.cpp
#include "Fft.h"
#include "../common/Constants.h"
#include <cmath>

namespace aamp {

bool Fft::isValidSize(int n) {
    if (n < k::FFT_MIN || n > k::FFT_MAX) return false;
    return (n & (n - 1)) == 0; // potencia de 2
}

// Cooley-Tukey iterativo radix-2 com bit-reversal (double-precision).
void Fft::transform(std::vector<Complex>& a, bool inverse) {
    const std::size_t n = a.size();
    if (n <= 1) return;

    // Bit-reversal permutation.
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        std::size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }

    const double PI = 3.14159265358979323846;
    for (std::size_t len = 2; len <= n; len <<= 1) {
        const double ang = 2.0 * PI / static_cast<double>(len) * (inverse ? 1.0 : -1.0);
        const Complex wlen(std::cos(ang), std::sin(ang));
        for (std::size_t i = 0; i < n; i += len) {
            Complex w(1.0, 0.0);
            for (std::size_t k = 0; k < len / 2; ++k) {
                Complex u = a[i + k];
                Complex v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wlen;
            }
        }
    }

    if (inverse) {
        const double invN = 1.0 / static_cast<double>(n);
        for (auto& x : a) x *= invN;
    }
}

std::vector<Complex> Fft::forwardReal(const std::vector<double>& signal, int n) {
    std::vector<Complex> buf(static_cast<std::size_t>(n), Complex(0.0, 0.0));
    const std::size_t m = std::min(signal.size(), static_cast<std::size_t>(n));
    for (std::size_t i = 0; i < m; ++i) buf[i] = Complex(signal[i], 0.0);
    transform(buf, false);
    return buf;
}

std::vector<double> Fft::inverseToReal(std::vector<Complex> spectrum) {
    transform(spectrum, true);
    std::vector<double> out(spectrum.size());
    for (std::size_t i = 0; i < spectrum.size(); ++i) out[i] = spectrum[i].real();
    return out;
}

} // namespace aamp

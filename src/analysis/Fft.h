// AAMP - src/analysis/Fft.h
// FFT Cooley-Tukey radix-2, double-precision (DOC-03 §4.1, AD-02 Double-precision).
// Tamanhos potencia de 2 de 256 a 65536, selecionados por metrica.
#pragma once

#include <vector>
#include <complex>
#include <cstddef>

namespace aamp {

using Complex = std::complex<double>;

class Fft {
public:
    // Verdadeiro se n e potencia de 2 dentro de [FFT_MIN, FFT_MAX].
    static bool isValidSize(int n);

    // FFT in-place sobre vetor complexo de tamanho potencia de 2.
    // inverse=false: transformada direta. inverse=true: inversa com escala 1/N.
    static void transform(std::vector<Complex>& data, bool inverse);

    // Conveniencia: FFT direta de sinal real -> espectro complexo (N bins).
    static std::vector<Complex> forwardReal(const std::vector<double>& signal, int n);

    // Conveniencia: IFFT -> parte real (descarta imaginario residual).
    static std::vector<double> inverseToReal(std::vector<Complex> spectrum);
};

} // namespace aamp

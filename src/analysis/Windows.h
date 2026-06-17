// AAMP - src/analysis/Windows.h
// Janelas (DOC-03 §4.1): Hann (padrao), Hamming, Blackman, Flat Top, Rectangular,
// e Blackman-Harris 4-termos (THD/THD+N/IMD: MS-05/06/07).
#pragma once
#include <vector>

namespace aamp {

enum class Window { Rectangular, Hann, Hamming, Blackman, FlatTop, BlackmanHarris4 };

// Gera os coeficientes da janela de comprimento n.
std::vector<double> makeWindow(Window w, int n);

// Ganho coerente da janela (soma dos coeficientes) para normalizacao de magnitude.
double windowCoherentGain(const std::vector<double>& win);

} // namespace aamp

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
// DOMINIO DE TOM: calibra a amplitude de linhas espectrais discretas (MS-05/06/07, peakBin, thdPercent).
double windowCoherentGain(const std::vector<double>& win);

// --- DOMINIO DE RUIDO (P1-001 / N1-R1 / AR-01) ---
// Abstracao criada exclusivamente para estimativas ENERGETICAS de ruido de banda larga.
// NAO substitui nem altera a semantica de windowCoherentGain (dominio de tom).

// Ganho de potencia da janela: S2 = sum(w[i]^2).
double windowPowerGain(const std::vector<double>& win);

// Largura de banda equivalente de ruido (ENBW) da janela, EM BINS (adimensional):
//   ENBW = n * S2 / S1^2,  com S1 = sum(w), S2 = sum(w^2).
// Depende apenas da FORMA da janela (n cancela): Hann = 1.5, Rectangular = 1.0.
double windowEnbwBins(const std::vector<double>& win);

} // namespace aamp

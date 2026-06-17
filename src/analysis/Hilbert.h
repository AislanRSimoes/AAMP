// AAMP - src/analysis/Hilbert.h
// Transformada de Hilbert e extracao de envelope FIXA (DOC-03 §4.4).
// Envelope = |sinal analitico| seguido de media movel round(0.001*fs).
// Nenhum metodo alternativo e permitido (determinismo entre implementacoes).
#pragma once
#include <vector>

namespace aamp {

// Magnitude do sinal analitico via FFT (zera bins negativos, dobra positivos).
std::vector<double> analyticMagnitude(const std::vector<double>& signal);

// Envelope determinístico: analyticMagnitude + media movel de round(0.001*fs) samples.
std::vector<double> extractEnvelope(const std::vector<double>& signal, double fs);

} // namespace aamp

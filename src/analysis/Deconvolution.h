// AAMP - src/analysis/Deconvolution.h
// Deconvolucao H(f)=Y(f)/X(f) com regularizacao de Tikhonov ADAPTATIVA (DOC-03 §4.2, AD-06).
// lambda(f) = LAMBDA_BASE * (1 + N(f)/S(f)); em bins S(f)->0, lambda domina (sem NaN/Inf, ER-031).
#pragma once
#include <vector>
#include <complex>

namespace aamp {

using Cx = std::complex<double>;

// H(f) = Y(f) * conj(X(f)) / (|X(f)|^2 + lambda(f)). noise: estimativa de N(f) por bin (>=0).
std::vector<Cx> deconvolveTikhonov(const std::vector<Cx>& Y,
                                   const std::vector<Cx>& X,
                                   const std::vector<double>& noise);

} // namespace aamp

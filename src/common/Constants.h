// AAMP - src/common/Constants.h
// Constantes nomeadas centralizadas (DOC-09 §2.1: sem numeros magicos).
// Todo valor abaixo deriva EXCLUSIVAMENTE da suite aprovada (DOC-03, DOC-07, DOC-09).
#pragma once

#include <cstddef>

namespace aamp {
namespace k {

// ---- Amostragem de referencia (DOC-09 §6) ----
constexpr double REF_SAMPLE_RATE = 48000.0;          // 48 kHz
constexpr double FULL_SCALE      = 1.0;              // 0 dBFS = 1.0 (DOC-03 MS-08)

// ---- FFT (DOC-03 §4.1) ----
constexpr int FFT_MIN  = 256;
constexpr int FFT_MAX  = 65536;                      // resolucao das metricas espectrais
constexpr int FFT_ANALYSIS = 65536;                  // MS-01,02,05,06,07,08 usam 65536 pontos

// ---- Banda reportada (DOC-03) ----
constexpr double BAND_LOW_HZ  = 20.0;
constexpr double BAND_HIGH_HZ = 20000.0;

// ---- Deconvolucao / regularizacao adaptativa de Tikhonov (DOC-03 §4.2, AD-06) ----
constexpr double LAMBDA_BASE = 1e-6;                 // lambda(f) = LAMBDA_BASE * (1 + N(f)/S(f))

// ---- Envelope comportamental (DOC-03 §4.4) ----
constexpr double ENVELOPE_MA_SECONDS = 0.001;        // media movel = round(0.001 * fs)

// ---- PRNG determinístico (DOC-03 ST-04/ST-05, FR-017) ----
constexpr unsigned long PRNG_SEED = 1UL;             // seed fixo = 1
constexpr int VOSS_STAGES = 16;                      // Voss-McCartney 16 estagios (Pink Noise)

// ---- Pre/post-roll (DOC-03 §5) ----
constexpr double PRE_ROLL_SECONDS  = 0.500;          // 500 ms
constexpr double POST_ROLL_SECONDS = 1.000;          // 1000 ms

// ---- Tom de calibracao pre-suite (DOC-03 §5 / BR-021) ----
constexpr double CALIB_FREQ_HZ   = 1000.0;
constexpr double CALIB_SECONDS   = 0.200;            // 200 ms
constexpr double CALIB_DBFS      = -12.0;

// ---- Clipping (BR-022, DOC-07 ER-020) ----
constexpr double CLIP_THRESHOLD_DBFS_DEFAULT = 0.0;  // padrao 0 dBFS (faixa -1..0)

// ---- Piso minimo de caracterizacao (DOC-07 §5) ----
constexpr double MIN_SNR_OVER_FLOOR_DB = 20.0;       // SNR de retorno < 20 dB acima do floor => ER-023

// ---- Limites de tentativa (DOC-07 §4) ----
constexpr int MAX_CAPTURE_RETRIES = 3;               // clipping / captura parcial
constexpr int MAX_METRIC_RETRIES  = 1;               // retry de metrica
constexpr int MAX_SUITE_RETRIES   = 1;               // retry da suite

// ---- THD: faixa de harmonicas (DOC-03 MS-05) ----
constexpr int THD_HARMONIC_FIRST = 2;
constexpr int THD_HARMONIC_LAST  = 10;

// ---- Noise floor: media de frames (DOC-03 MS-08) ----
constexpr int NOISE_AVG_FRAMES = 16;                 // exatamente 16 frames Hann 50% overlap

// ---- Numero de estimulos e familias ----
constexpr int NUM_STIMULI  = 16;                     // ST-01..ST-16
constexpr int NUM_METRICS  = 15;                     // MS-01..MS-15

// ---- ST-06 Impulse (DOC-03) ----
constexpr int IMPULSE_CAPTURE_SAMPLES = 65536;       // janela de captura
inline int impulsePositionSamples(double fs) {       // round(0.5 * fs)
    return static_cast<int>(0.5 * fs + 0.5);
}

} // namespace k
} // namespace aamp

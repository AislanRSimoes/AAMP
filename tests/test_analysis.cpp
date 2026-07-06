// AAMP - tests/test_analysis.cpp
// M-05 Analysis Engine + DSP: TC-030..TC-037 (DOC-11 §3, AC-M05-01..05).
#include "TestRunner.h"
#include "LoopbackHarness.h"
#include "../src/analysis/AnalysisEngine.h"
#include "../src/analysis/Fft.h"
#include "../src/analysis/Deconvolution.h"
#include "../src/common/Constants.h"
#include <cmath>

using namespace aamp;
static const double FS = k::REF_SAMPLE_RATE;

// FFT round-trip (base de confianca do nucleo DSP).
TEST_CASE("DSP FFT round-trip (forward+inverse == original)") {
    std::vector<double> sig(1024);
    for (int i = 0; i < 1024; ++i) sig[static_cast<std::size_t>(i)] = std::sin(2.0 * 3.14159265 * 5.0 * i / 1024.0);
    auto X = Fft::forwardReal(sig, 1024);
    auto back = Fft::inverseToReal(X);
    double maxErr = 0.0;
    for (int i = 0; i < 1024; ++i) maxErr = std::max(maxErr, std::fabs(back[static_cast<std::size_t>(i)] - sig[static_cast<std::size_t>(i)]));
    CHECK(maxErr < 1e-9);
}

// TC-031: FFT de seno puro -> pico no bin correto (±1 bin) e magnitude (±0.1 dB).
TEST_CASE("TC-031 FFT seno puro: pico no bin correto, mag +-0.1 dB") {
    const int N = k::FFT_ANALYSIS;
    const int targetBin = 1365;
    const double f = static_cast<double>(targetBin) * FS / N; // freq exata de bin
    const double A = dbfsToLinear(-6.0);
    std::vector<double> sig(static_cast<std::size_t>(N));
    for (int i = 0; i < N; ++i) sig[static_cast<std::size_t>(i)] = A * std::sin(2.0 * 3.14159265358979 * f * i / FS);
    const int pb = AnalysisEngine::peakBin(sig, N);
    CHECK(std::abs(pb - targetBin) <= 1);
    // magnitude no pico recuperada (via primitiva publica de THD nao; recomputa direto)
    // reusa peakBin + reconstrucao de magnitude por Hann.
    std::vector<double> win(static_cast<std::size_t>(N));
    double cg = 0.0;
    for (int i = 0; i < N; ++i) { win[static_cast<std::size_t>(i)] = 0.5 - 0.5 * std::cos(2.0*3.14159265358979*i/(N-1)); cg += win[static_cast<std::size_t>(i)]; }
    std::vector<double> buf(static_cast<std::size_t>(N));
    for (int i = 0; i < N; ++i) buf[static_cast<std::size_t>(i)] = sig[static_cast<std::size_t>(i)] * win[static_cast<std::size_t>(i)];
    auto X = Fft::forwardReal(buf, N);
    const double mag = std::abs(X[static_cast<std::size_t>(pb)]) / cg * 2.0;
    CHECK_NEAR(20.0 * std::log10(mag / A), 0.0, 0.1);
}

// TC-032: THD de seno puro loopback < 0.001%.
TEST_CASE("TC-032 THD seno puro loopback < 0.001 porcento") {
    const int N = k::FFT_ANALYSIS;
    const double f = 1000.0;
    const double A = dbfsToLinear(-6.0);
    std::vector<double> sig(static_cast<std::size_t>(N));
    for (int i = 0; i < N; ++i) sig[static_cast<std::size_t>(i)] = A * std::sin(2.0 * 3.14159265358979 * f * i / FS);
    const double thd = AnalysisEngine::thdPercent(sig, f, FS, N);
    CHECK(thd < 0.001);
}
TEST_CASE("EXP-B THD CAPTURADO")
{
    CapturedSet cap = harness::makeLoopback(FS, 0, false);

    AnalysisEngine eng(FS);

    ResultBundle b = eng.computeAllMetrics(cap);

    const MetricResult* r = b.find("MS-05");

    CHECK(r != nullptr);

    if (r)
    {
        printf("\n\n===== EXP-B ST03 CAPTURADO =====\n");

        for (const auto& sc : r->scalars)
        {
            if (sc.valuePresent)
            {
                printf("%s = %.12f %s\n",
                       sc.key.c_str(),
                       sc.value,
                       sc.unit.c_str());
            }
            else
            {
                printf("%s = N/A\n",
                       sc.key.c_str());
            }
        }

        printf("===============================\n\n");
    }

}
// TC-033: SNR de referencia -80 dBFS -> 80 dB ±0.5.
TEST_CASE("TC-033 SNR referencia 80 dB +-0.5") {
    const int N = 48000;
    std::vector<double> sig(static_cast<std::size_t>(N), 0.0), noise(static_cast<std::size_t>(N), 0.0);
    // sinal RMS = 1.0 (0 dBFS de referencia); ruido RMS = 1e-4 (-80 dBFS).
    for (int i = 0; i < N; ++i) sig[static_cast<std::size_t>(i)] = std::sqrt(2.0) * std::sin(2.0*3.14159265358979*1000.0*i/FS); // RMS 1.0
    harness::deterministicNoise(1, -80.0); // (warm-up determinístico irrelevante)
    std::vector<double> nz = harness::deterministicNoise(static_cast<std::size_t>(N), -80.0);
    const double snr = AnalysisEngine::snrDb(sig, nz);
    CHECK_NEAR(snr, 80.0, 0.5);
}

// TC-034: determinismo de metricas (mesma entrada -> idêntico).
TEST_CASE("TC-034 Determinismo de metricas") {
    CapturedSet cap = harness::makeLoopback(FS, 0, false);
    AnalysisEngine eng(FS);
    ResultBundle a = eng.computeAllMetrics(cap);
    ResultBundle b = eng.computeAllMetrics(cap);
    CHECK(a.metrics.size() == b.metrics.size());
    bool same = true;
    for (std::size_t i = 0; i < a.metrics.size(); ++i)
        for (std::size_t j = 0; j < a.metrics[i].scalars.size() && j < b.metrics[i].scalars.size(); ++j)
            if (a.metrics[i].scalars[j].value != b.metrics[i].scalars[j].value) same = false;
    CHECK(same);
}

// TC-035: deconvolucao com bin de energia zero -> regularizacao, sem NaN/Inf.
TEST_CASE("TC-035 Deconvolucao bin zero sem NaN/Inf") {
    const int N = 4096;
    // X = tom puro (a maioria dos bins tem energia ~0) -> divisao instavel sem regularizacao.
    std::vector<double> x(static_cast<std::size_t>(N)), y(static_cast<std::size_t>(N));
    for (int i = 0; i < N; ++i) { x[static_cast<std::size_t>(i)] = std::sin(2.0*3.14159265*50.0*i/N); y[static_cast<std::size_t>(i)] = x[static_cast<std::size_t>(i)]; }
    auto X = Fft::forwardReal(x, N);
    auto Y = Fft::forwardReal(y, N);
    auto H = deconvolveTikhonov(Y, X, {});
    bool allFinite = true;
    for (const auto& h : H) if (!std::isfinite(h.real()) || !std::isfinite(h.imag())) allFinite = false;
    CHECK(allFinite);
}

// TC-036: crosstalk calculado em estereo; N/A em mono.
TEST_CASE("TC-036 Crosstalk estereo calculado / mono N/A") {
    AnalysisEngine eng(FS);
    // mono
    ResultBundle mono = eng.computeAllMetrics(harness::makeLoopback(FS, 0, false));
    const MetricResult* m = mono.find("MS-11");
    CHECK(m != nullptr);
    CHECK(m->quality == Quality::NotApplicable);
    CHECK(!m->scalars.empty() && m->scalars[0].valuePresent == false); // -> 'N/A' na exportacao
    // estereo
    ResultBundle st = eng.computeAllMetrics(harness::makeLoopback(FS, 0, true));
    const MetricResult* s = st.find("MS-11");
    CHECK(s != nullptr);
    CHECK(s->quality == Quality::Normal);
    CHECK(!s->scalars.empty() && std::isfinite(s->scalars[0].value));
}

// TC-037: group delay constante para fase linear (atraso puro) ±2 samples.
TEST_CASE("TC-037 Group delay constante p/ fase linear +-2 samples") {
    const int D = 64; // atraso puro de 64 samples -> fase linear
    CapturedSet cap = harness::makeLoopback(FS, D, false);
    AnalysisEngine eng(FS);
    ResultBundle b = eng.computeAllMetrics(cap);
    const MetricResult* gd = b.find("MS-03");
    CHECK(gd != nullptr && !gd->curves.empty());
    // media de group delay na faixa 500 Hz - 5 kHz deve ~ D/fs.
    const Curve& c = gd->curves[0];
    double sum = 0.0; int cnt = 0;
    for (std::size_t i = 0; i < c.x.size(); ++i)
        if (c.x[i] >= 500.0 && c.x[i] <= 5000.0) { sum += c.y[i]; cnt++; }
    const double meanMs = (cnt > 0) ? sum / cnt : 0.0;
    const double expectedMs = static_cast<double>(D) / FS * 1000.0;
    const double tolMs = 2.0 / FS * 1000.0; // ±2 samples
    CHECK_NEAR(meanMs, expectedMs, tolMs);
}

// TC-030: as 15 familias calculadas (ou N/A justificado), nenhuma em Error.
TEST_CASE("TC-030 15 familias calculadas (ou N/A), nenhuma Error") {
    AnalysisEngine eng(FS);
    ResultBundle b = eng.computeAllMetrics(harness::makeLoopback(FS, 0, false));
    CHECK(b.metrics.size() == 15);
    for (int i = 1; i <= 15; ++i) {
        char id[8]; std::snprintf(id, sizeof(id), "MS-%02d", i);
        const MetricResult* m = b.find(id);
        CHECK(m != nullptr);
        if (m) CHECK(m->quality != Quality::Error);
    }
}

// AAMP - tests/test_stimulus.cpp
// M-03 Stimulus Generator: TC-010, TC-011, TC-012, TC-013 (DOC-11 §3, AC-M03-01..04).
#include "TestRunner.h"
#include "../src/stimulus/StimulusGenerator.h"
#include "../src/common/Constants.h"
#include <cmath>

using namespace aamp;
static const double FS = k::REF_SAMPLE_RATE;

// TC-010: a suite gera os 16 estimulos, presentes e em ordem A->B->C.
TEST_CASE("TC-010 Suite gera 16 estimulos em ordem A->B->C") {
    StimulusGenerator gen(FS);
    auto suite = gen.generateSuite();
    CHECK(suite.size() == 16);
    const char* expected[16] = {"ST-01","ST-02","ST-03","ST-04","ST-05","ST-06","ST-16",
                                "ST-07","ST-08","ST-09","ST-10","ST-11","ST-12","ST-13","ST-14","ST-15"};
    const char cats[16] =       {'A','A','A','A','A','A','A','B','B','B','C','C','C','C','C','C'};
    for (std::size_t i = 0; i < suite.size(); ++i) {
        CHECK(suite[i].code == expected[i]);
        CHECK(suite[i].category == cats[i]);
    }
}

// TC-011: geracao determinística byte-identica entre execucoes (FR-017).
TEST_CASE("TC-011 Determinismo byte-identico entre execucoes") {
    StimulusGenerator a(FS), b(FS);
    auto sa = a.generateSuite();
    auto sb = b.generateSuite();
    CHECK(sa.size() == sb.size());
    bool identical = true;
    for (std::size_t i = 0; i < sa.size(); ++i) {
        if (sa[i].samples.size() != sb[i].samples.size()) { identical = false; break; }
        for (std::size_t j = 0; j < sa[i].samples.size(); ++j)
            if (sa[i].samples[j] != sb[i].samples[j]) { identical = false; break; }
        if (!identical) break;
    }
    CHECK(identical);
    // Foco em ruido (onde determinismo e nao-trivial): ST-04 e ST-05.
    auto wn1 = a.getStimulus(StimulusId::ST04_WhiteNoise).samples;
    auto wn2 = b.getStimulus(StimulusId::ST04_WhiteNoise).samples;
    bool wnId = (wn1.size() == wn2.size());
    for (std::size_t j = 0; wnId && j < wn1.size(); ++j) if (wn1[j] != wn2[j]) wnId = false;
    CHECK(wnId);
}

// TC-012: normalizacao de compostos -> pico <= 0 dBFS (alvo -1 dBFS). ST-02/14/15 (FR-016).
TEST_CASE("TC-012 Compostos normalizados a -1 dBFS de pico (<= 0 dBFS)") {
    StimulusGenerator gen(FS);
    const StimulusId comps[3] = {StimulusId::ST02_MultiTone, StimulusId::ST14_HarmonicCluster,
                                 StimulusId::ST15_DynamicPerformance};
    const double target = dbfsToLinear(-1.0);
    for (StimulusId id : comps) {
        const double p = peak(gen.getStimulus(id).samples);
        CHECK(p <= 1.0);                          // nunca acima de 0 dBFS
        CHECK_NEAR(p, target, 1e-6);              // exatamente -1 dBFS de pico
    }
}

// TC-013: parametros conferem com DOC-03 (frequencias, duracoes, envelopes, amplitudes).
TEST_CASE("TC-013 Parametros conforme DOC-03") {
    StimulusGenerator gen(FS);
    // ST-01 ESS: duracao 10 s; fades nas extremidades (amplitude inicial ~0).
    auto ess = gen.getStimulus(StimulusId::ST01_ESS).samples;
    CHECK(static_cast<int>(ess.size()) == static_cast<int>(10.0 * FS + 0.5));
    CHECK(std::fabs(ess.front()) < 1e-6);         // fade-in: comeca em ~0
    CHECK(std::fabs(ess.back())  < 1e-6);         // fade-out: termina em ~0

    // ST-03 Pure Sine: 6 s (2 s x 3 freq); amplitude -6 dBFS.
    auto ps = gen.getStimulus(StimulusId::ST03_PureSine).samples;
    CHECK(static_cast<int>(ps.size()) == static_cast<int>(6.0 * FS + 0.5));
    CHECK_NEAR(peak(ps), dbfsToLinear(-6.0), 1e-3);

    // ST-04 White Noise: -12 dBFS RMS (FR-017 seed determinístico).
    auto wn = gen.getStimulus(StimulusId::ST04_WhiteNoise).samples;
    CHECK_NEAR(20.0 * std::log10(rms(wn)), -12.0, 0.01);

    // ST-06 Impulse: valor 1.0 (0 dBFS) em round(0.5*fs).
    auto imp = gen.getStimulus(StimulusId::ST06_Impulse).samples;
    const int pos = k::impulsePositionSamples(FS);
    CHECK_NEAR(imp[static_cast<std::size_t>(pos)], 1.0, 1e-12);

    // ST-08 Dynamic Step: 8 degraus de 100 ms (800 ms).
    auto step = gen.getStimulus(StimulusId::ST08_DynamicStep).samples;
    CHECK(static_cast<int>(step.size()) == static_cast<int>(0.800 * FS + 0.5));

    // ST-13 Sustained: 30 s.
    auto sus = gen.getStimulus(StimulusId::ST13_Sustained).samples;
    CHECK(static_cast<int>(sus.size()) == static_cast<int>(30.0 * FS + 0.5));
}

// AAMP - src/stimulus/StimulusGenerator.cpp
#include "StimulusGenerator.h"
#include "Prng.h"
#include "../common/Constants.h"
#include <cmath>
#include <algorithm>

namespace aamp {

static const double PI = 3.14159265358979323846;

double dbfsToLinear(double dbfs) { return std::pow(10.0, dbfs / 20.0); }

double peak(const std::vector<double>& x) {
    double p = 0.0;
    for (double v : x) p = std::max(p, std::fabs(v));
    return p;
}

double rms(const std::vector<double>& x) {
    if (x.empty()) return 0.0;
    double s = 0.0;
    for (double v : x) s += v * v;
    return std::sqrt(s / static_cast<double>(x.size()));
}

void applyHannFade(std::vector<double>& x, int fadeSamples) {
    const int n = static_cast<int>(x.size());
    if (fadeSamples <= 0 || n == 0) return;
    const int f = std::min(fadeSamples, n / 2);
    for (int i = 0; i < f; ++i) {
        // Hann ramp (0->1)
        const double g = 0.5 - 0.5 * std::cos(PI * static_cast<double>(i) / static_cast<double>(f));
        x[static_cast<std::size_t>(i)] *= g;
        x[static_cast<std::size_t>(n - 1 - i)] *= g;
    }
}

void normalizeCompositePeak(std::vector<double>& x, double targetDbfs) {
    const double p = peak(x);
    if (p <= 0.0) return;
    const double target = dbfsToLinear(targetDbfs);
    const double g = target / p;
    for (double& v : x) v *= g;
}

static int secToSamples(double sec, double fs) { return static_cast<int>(sec * fs + 0.5); }

StimulusGenerator::StimulusGenerator(double sampleRate) : m_fs(sampleRate) {}

// ---------------- ST-01 ESS ----------------
std::vector<double> StimulusGenerator::ess() const {
    const double T = 10.0;
    const int N = secToSamples(T, m_fs);
    const double f1 = 20.0, f2 = m_fs / 2.0;
    const double amp = dbfsToLinear(-6.0);
    const double L = std::log(f2 / f1);
    const double K = 2.0 * PI * f1 * T / L;
    std::vector<double> x(static_cast<std::size_t>(N), 0.0);
    for (int i = 0; i < N; ++i) {
        const double t = static_cast<double>(i) / m_fs;
        const double phase = K * (std::exp((t / T) * L) - 1.0);
        x[static_cast<std::size_t>(i)] = amp * std::sin(phase);
    }
    applyHannFade(x, secToSamples(0.010, m_fs)); // 10 ms cada extremidade
    return x;
}

// ---------------- ST-02 Multi-Tone ----------------
std::vector<double> StimulusGenerator::multiTone() const {
    const int N = secToSamples(5.0, m_fs);
    const double freqs[8] = {55, 110, 220, 440, 1320, 3960, 7920, 15840};
    const double toneAmp = dbfsToLinear(-20.0); // cada tom -20 dBFS
    std::vector<double> x(static_cast<std::size_t>(N), 0.0);
    for (int i = 0; i < N; ++i) {
        const double t = static_cast<double>(i) / m_fs;
        double s = 0.0;
        for (double f : freqs) s += toneAmp * std::sin(2.0 * PI * f * t); // fase inicial zero
        x[static_cast<std::size_t>(i)] = s;
    }
    normalizeCompositePeak(x, -1.0); // soma normalizada para -1 dBFS de pico
    return x;
}

// ---------------- ST-03 Pure Sine ----------------
std::vector<double> StimulusGenerator::pureSine() const {
    const double freqs[3] = {100.0, 1000.0, 10000.0};
    const int seg = secToSamples(2.0, m_fs);
    const double amp = dbfsToLinear(-6.0);
    std::vector<double> x;
    x.reserve(static_cast<std::size_t>(seg * 3));
    for (double f : freqs)
        for (int i = 0; i < seg; ++i)
            x.push_back(amp * std::sin(2.0 * PI * f * static_cast<double>(i) / m_fs));
    return x;
}

// ---------------- ST-04 White Noise ----------------
std::vector<double> StimulusGenerator::whiteNoise() const {
    const int N = secToSamples(5.0, m_fs);
    WhitePrng prng(k::PRNG_SEED);
    std::vector<double> x(static_cast<std::size_t>(N));
    for (int i = 0; i < N; ++i) x[static_cast<std::size_t>(i)] = prng.next();
    // Normaliza para -12 dBFS RMS.
    const double target = dbfsToLinear(-12.0);
    const double r = rms(x);
    if (r > 0.0) { const double g = target / r; for (double& v : x) v *= g; }
    // FR-016: garantir saida <= 0 dBFS (evitar clipping). So reescala se exceder.
    if (peak(x) > 1.0) normalizeCompositePeak(x, -1.0);
    return x;
}

// ---------------- ST-05 Pink Noise ----------------
std::vector<double> StimulusGenerator::pinkNoise() const {
    const int N = secToSamples(5.0, m_fs);
    PinkPrng prng(k::PRNG_SEED);
    std::vector<double> x(static_cast<std::size_t>(N));
    for (int i = 0; i < N; ++i) x[static_cast<std::size_t>(i)] = prng.next();
    const double target = dbfsToLinear(-12.0);
    const double r = rms(x);
    if (r > 0.0) { const double g = target / r; for (double& v : x) v *= g; }
    // FR-016: garantir saida <= 0 dBFS (evitar clipping). So reescala se exceder.
    if (peak(x) > 1.0) normalizeCompositePeak(x, -1.0);
    return x;
}

// ---------------- ST-06 Impulse ----------------
std::vector<double> StimulusGenerator::impulse() const {
    const int pos = k::impulsePositionSamples(m_fs);     // round(0.5*fs)
    const int N = pos + k::IMPULSE_CAPTURE_SAMPLES;
    std::vector<double> x(static_cast<std::size_t>(N), 0.0);
    x[static_cast<std::size_t>(pos)] = k::FULL_SCALE;    // 1.0 = 0 dBFS
    return x;
}

// ---------------- ST-07 Burst Tone ----------------
std::vector<double> StimulusGenerator::burstTone() const {
    const int burst = secToSamples(0.100, m_fs);
    const int silence = secToSamples(0.100, m_fs);
    const int fade = secToSamples(0.005, m_fs);
    const double amp = dbfsToLinear(-6.0);
    std::vector<double> x(static_cast<std::size_t>(burst + silence), 0.0);
    for (int i = 0; i < burst; ++i) {
        double g = 1.0;
        if (i < fade) g = static_cast<double>(i) / static_cast<double>(fade);
        else if (i >= burst - fade) g = static_cast<double>(burst - 1 - i) / static_cast<double>(fade);
        x[static_cast<std::size_t>(i)] = g * amp * std::sin(2.0 * PI * 1000.0 * static_cast<double>(i) / m_fs);
    }
    return x;
}

// ---------------- ST-08 Dynamic Step ----------------
std::vector<double> StimulusGenerator::dynamicStep() const {
    const double levels[8] = {-40, -34, -28, -22, -16, -10, -8, -6};
    const int seg = secToSamples(0.100, m_fs);
    std::vector<double> x;
    x.reserve(static_cast<std::size_t>(seg * 8));
    int n = 0;
    for (double lvl : levels) {
        const double amp = dbfsToLinear(lvl);
        for (int i = 0; i < seg; ++i, ++n)
            x.push_back(amp * std::sin(2.0 * PI * 1000.0 * static_cast<double>(n) / m_fs)); // fase continua
    }
    return x;
}

// ---------------- ST-09 Silence ----------------
std::vector<double> StimulusGenerator::silence() const {
    return std::vector<double>(static_cast<std::size_t>(secToSamples(2.0, m_fs)), 0.0);
}

// ---------------- ST-10 Hard Attack Pulse ----------------
std::vector<double> StimulusGenerator::hardAttack() const {
    const int N = secToSamples(0.010, m_fs);   // 10 ms
    const int atk = secToSamples(0.0005, m_fs); // 0.5 ms
    const double tau = 0.003;                   // 3 ms
    const double amp = dbfsToLinear(-6.0);
    std::vector<double> x(static_cast<std::size_t>(N), 0.0);
    for (int i = 0; i < N; ++i) {
        const double t = static_cast<double>(i) / m_fs;
        double env;
        if (i < atk) env = static_cast<double>(i) / static_cast<double>(std::max(1, atk)); // ataque linear
        else         env = std::exp(-(t - static_cast<double>(atk) / m_fs) / tau);          // decai exp
        x[static_cast<std::size_t>(i)] = env * amp * std::sin(2.0 * PI * 1000.0 * t);
    }
    return x;
}

// ---------------- ST-11 Soft Attack Pulse ----------------
std::vector<double> StimulusGenerator::softAttack() const {
    const int N = secToSamples(0.050, m_fs);   // 50 ms
    const int atk = secToSamples(0.020, m_fs); // 20 ms ataque linear
    const int dec = secToSamples(0.030, m_fs); // 30 ms decai linear
    const double amp = dbfsToLinear(-6.0);
    std::vector<double> x(static_cast<std::size_t>(N), 0.0);
    for (int i = 0; i < N; ++i) {
        const double t = static_cast<double>(i) / m_fs;
        double env;
        if (i < atk) env = static_cast<double>(i) / static_cast<double>(std::max(1, atk));
        else         env = std::max(0.0, 1.0 - static_cast<double>(i - atk) / static_cast<double>(std::max(1, dec)));
        x[static_cast<std::size_t>(i)] = env * amp * std::sin(2.0 * PI * 1000.0 * t);
    }
    return x;
}

// ---------------- ST-12 Pulse Train ----------------
std::vector<double> StimulusGenerator::pulseTrain() const {
    const int period = secToSamples(0.100, m_fs); // 100 ms inicio-a-inicio
    const int pdur = secToSamples(0.020, m_fs);    // 20 ms
    const int fade = secToSamples(0.002, m_fs);    // 2 ms
    const double amp = dbfsToLinear(-6.0);
    const int nPulses = 10;
    std::vector<double> x(static_cast<std::size_t>(period * nPulses), 0.0);
    for (int p = 0; p < nPulses; ++p) {
        const int base = p * period;
        for (int i = 0; i < pdur; ++i) {
            double g = 1.0;
            if (i < fade) g = static_cast<double>(i) / static_cast<double>(fade);
            else if (i >= pdur - fade) g = static_cast<double>(pdur - 1 - i) / static_cast<double>(fade);
            const int idx = base + i;
            x[static_cast<std::size_t>(idx)] =
                g * amp * std::sin(2.0 * PI * 1000.0 * static_cast<double>(i) / m_fs);
        }
    }
    return x;
}

// ---------------- ST-13 Sustained ----------------
std::vector<double> StimulusGenerator::sustained() const {
    const int N = secToSamples(30.0, m_fs);
    const double amp = dbfsToLinear(-6.0);
    std::vector<double> x(static_cast<std::size_t>(N));
    for (int i = 0; i < N; ++i)
        x[static_cast<std::size_t>(i)] = amp * std::sin(2.0 * PI * 1000.0 * static_cast<double>(i) / m_fs);
    return x;
}

// ---------------- ST-14 Harmonic Cluster ----------------
std::vector<double> StimulusGenerator::harmonicCluster() const {
    const int N = secToSamples(3.0, m_fs);
    const double fundamentals[3] = {220.0, 277.18, 329.63};
    std::vector<double> comps;
    for (double f : fundamentals) { comps.push_back(f); comps.push_back(2.0 * f); comps.push_back(3.0 * f); }
    std::vector<double> x(static_cast<std::size_t>(N), 0.0);
    for (int i = 0; i < N; ++i) {
        const double t = static_cast<double>(i) / m_fs;
        double s = 0.0;
        for (double f : comps) s += std::sin(2.0 * PI * f * t); // amplitudes iguais, fase 0
        x[static_cast<std::size_t>(i)] = s;
    }
    normalizeCompositePeak(x, -1.0);
    return x;
}

// ---------------- ST-15 Dynamic Performance Sequence ----------------
std::vector<double> StimulusGenerator::dynamicPerformance() const {
    const int seg = secToSamples(1.0, m_fs);
    std::vector<double> x;
    x.reserve(static_cast<std::size_t>(seg * 12));
    const double stepLevels[4] = {-34, -22, -10, -6};

    // 4 degraus (tom 1 kHz no nivel do degrau, 1 s cada)
    int phaseN = 0;
    for (double lvl : stepLevels) {
        const double amp = dbfsToLinear(lvl);
        for (int i = 0; i < seg; ++i, ++phaseN)
            x.push_back(amp * std::sin(2.0 * PI * 1000.0 * static_cast<double>(phaseN) / m_fs));
    }
    // 4 bursts (padrao ST-07 repetido para preencher 1 s)
    std::vector<double> burst = burstTone(); // 200 ms
    for (int b = 0; b < 4; ++b) {
        for (int i = 0; i < seg; ++i)
            x.push_back(burst[static_cast<std::size_t>(i % static_cast<int>(burst.size()))]);
    }
    // 4 clusters (padrao ST-14, truncado/repetido para 1 s)
    std::vector<double> cl = harmonicCluster(); // 3 s
    for (int c = 0; c < 4; ++c) {
        for (int i = 0; i < seg; ++i)
            x.push_back(cl[static_cast<std::size_t>(i % static_cast<int>(cl.size()))]);
    }
    normalizeCompositePeak(x, -1.0); // pico <= -1 dBFS
    return x;
}

// ---------------- ST-16 Latency Reference ----------------
std::vector<double> StimulusGenerator::latencyRef() const {
    const double freqs[3] = {100.0, 1000.0, 10000.0};
    const int seg = secToSamples(1.0, m_fs);
    const double amp = dbfsToLinear(-6.0);
    std::vector<double> x;
    x.reserve(static_cast<std::size_t>(seg * 3));
    for (double f : freqs)
        for (int i = 0; i < seg; ++i)
            x.push_back(amp * std::sin(2.0 * PI * f * static_cast<double>(i) / m_fs));
    return x;
}

Stimulus StimulusGenerator::getStimulus(StimulusId id) const {
    Stimulus s; s.id = id;
    switch (id) {
        case StimulusId::ST01_ESS:               s.code="ST-01"; s.name="Exponential Sine Sweep"; s.category='A'; s.samples=ess(); break;
        case StimulusId::ST02_MultiTone:         s.code="ST-02"; s.name="Multi-Tone"; s.category='A'; s.samples=multiTone(); break;
        case StimulusId::ST03_PureSine:          s.code="ST-03"; s.name="Pure Sine"; s.category='A'; s.samples=pureSine(); break;
        case StimulusId::ST04_WhiteNoise:        s.code="ST-04"; s.name="White Noise"; s.category='A'; s.samples=whiteNoise(); break;
        case StimulusId::ST05_PinkNoise:         s.code="ST-05"; s.name="Pink Noise"; s.category='A'; s.samples=pinkNoise(); break;
        case StimulusId::ST06_Impulse:           s.code="ST-06"; s.name="Impulse"; s.category='A'; s.samples=impulse(); break;
        case StimulusId::ST16_LatencyRef:        s.code="ST-16"; s.name="Pure Sine - Latency Reference"; s.category='A'; s.samples=latencyRef(); break;
        case StimulusId::ST07_BurstTone:         s.code="ST-07"; s.name="Burst Tone"; s.category='B'; s.samples=burstTone(); break;
        case StimulusId::ST08_DynamicStep:       s.code="ST-08"; s.name="Dynamic Step Excitation"; s.category='B'; s.samples=dynamicStep(); break;
        case StimulusId::ST09_Silence:           s.code="ST-09"; s.name="Silence Segments"; s.category='B'; s.samples=silence(); break;
        case StimulusId::ST10_HardAttack:        s.code="ST-10"; s.name="Hard Attack Pulse"; s.category='C'; s.samples=hardAttack(); break;
        case StimulusId::ST11_SoftAttack:        s.code="ST-11"; s.name="Soft Attack Pulse"; s.category='C'; s.samples=softAttack(); break;
        case StimulusId::ST12_PulseTrain:        s.code="ST-12"; s.name="Pulse Train"; s.category='C'; s.samples=pulseTrain(); break;
        case StimulusId::ST13_Sustained:         s.code="ST-13"; s.name="Sustained Excitation"; s.category='C'; s.samples=sustained(); break;
        case StimulusId::ST14_HarmonicCluster:   s.code="ST-14"; s.name="Harmonic Cluster"; s.category='C'; s.samples=harmonicCluster(); break;
        case StimulusId::ST15_DynamicPerformance:s.code="ST-15"; s.name="Dynamic Performance Sequence"; s.category='C'; s.samples=dynamicPerformance(); break;
    }
    return s;
}

std::vector<Stimulus> StimulusGenerator::generateSuite() {
    // Ordem A -> B -> C (DOC-03 §5): A = ST-01..06, 16; B = ST-07..09; C = ST-10..15.
    const StimulusId order[16] = {
        StimulusId::ST01_ESS, StimulusId::ST02_MultiTone, StimulusId::ST03_PureSine,
        StimulusId::ST04_WhiteNoise, StimulusId::ST05_PinkNoise, StimulusId::ST06_Impulse,
        StimulusId::ST16_LatencyRef,
        StimulusId::ST07_BurstTone, StimulusId::ST08_DynamicStep, StimulusId::ST09_Silence,
        StimulusId::ST10_HardAttack, StimulusId::ST11_SoftAttack, StimulusId::ST12_PulseTrain,
        StimulusId::ST13_Sustained, StimulusId::ST14_HarmonicCluster, StimulusId::ST15_DynamicPerformance
    };
    std::vector<Stimulus> out;
    out.reserve(16);
    for (StimulusId id : order) out.push_back(getStimulus(id));
    return out;
}

} // namespace aamp

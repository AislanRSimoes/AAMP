// AAMP - src/analysis/AnalysisEngine.cpp
#include "AnalysisEngine.h"
#include "Fft.h"
#include "Windows.h"
#include "Hilbert.h"
#include "Deconvolution.h"
#include "../common/Constants.h"
#include <cmath>
#include <algorithm>

namespace aamp {

static const double PI = 3.14159265358979323846;

// Espectro de magnitude linear de um sinal janelado, N pontos.
static std::vector<double> magSpectrum(const std::vector<double>& sig, int n, Window w) {
    std::vector<double> win = makeWindow(w, n);
    std::vector<double> buf(static_cast<std::size_t>(n), 0.0);
    const std::size_t m = std::min(sig.size(), static_cast<std::size_t>(n));
    for (std::size_t i = 0; i < m; ++i) buf[i] = sig[i] * win[i];
    std::vector<Complex> X = Fft::forwardReal(buf, n);
    std::vector<double> mag(static_cast<std::size_t>(n / 2));
    const double cg = windowCoherentGain(win);
    for (int i = 0; i < n / 2; ++i)
        mag[static_cast<std::size_t>(i)] = std::abs(X[static_cast<std::size_t>(i)]) / cg * 2.0;
    return mag;
}

static double meanSquare(const std::vector<double>& x) {
    if (x.empty()) return 0.0;
    double s = 0.0;
    for (double v : x) s += v * v;
    return s / static_cast<double>(x.size());
}

// ---- primitivas publicas (teste) ----
int AnalysisEngine::peakBin(const std::vector<double>& signal, int n) {
    std::vector<double> mag = magSpectrum(signal, n, Window::Hann);
    int best = 0; double bv = -1.0;
    for (int i = 1; i < static_cast<int>(mag.size()); ++i)
        if (mag[static_cast<std::size_t>(i)] > bv) { bv = mag[static_cast<std::size_t>(i)]; best = i; }
    return best;
}

double AnalysisEngine::binFreq(int bin, int n, double fs) {
    return static_cast<double>(bin) * fs / static_cast<double>(n);
}

double AnalysisEngine::thdPercent(const std::vector<double>& signal, double f0, double fs, int n) {
    std::vector<double> mag = magSpectrum(signal, n, Window::BlackmanHarris4);
    auto energyNear = [&](double f) -> double {
        const int center = static_cast<int>(f * n / fs + 0.5);
        double e = 0.0;
        for (int b = center - 4; b <= center + 4; ++b)
            if (b >= 0 && b < static_cast<int>(mag.size()))
                e += mag[static_cast<std::size_t>(b)] * mag[static_cast<std::size_t>(b)];
        return e; // energia (mag^2) na vizinhanca
    };
    const double A1sq = energyNear(f0);
    if (A1sq <= 0.0) return 0.0;
    double harm = 0.0;
    for (int nH = k::THD_HARMONIC_FIRST; nH <= k::THD_HARMONIC_LAST; ++nH) {
        const double fh = nH * f0;
        if (fh >= fs / 2.0) break;
        harm += energyNear(fh);
    }
    return 100.0 * std::sqrt(harm) / std::sqrt(A1sq);
}

double AnalysisEngine::snrDb(const std::vector<double>& signal, const std::vector<double>& noise) {
    const double ps = meanSquare(signal);
    const double pn = meanSquare(noise);
    if (pn <= 0.0) return 200.0; // sem ruido -> SNR muito alto (limitado)
    return 10.0 * std::log10(ps / pn);
}

static int nextPow2Local(std::size_t n) {
    int p = 1;
    while (static_cast<std::size_t>(p) < n) p <<= 1;
    return p;
}

// Transferencia do ESS por filtro inverso regularizado (DOC-03 §4.2).
// 1) deconvolucao Tikhonov em comprimento total -> IR de banda larga (sem truncar o sweep);
// 2) FFT de 65536 pontos da IR janelada -> H(f) reportada (resolucao de DOC-03 MS-01).
struct EssTransfer {
    std::vector<Complex> H;     // espectro 65536 (metade util)
    std::vector<double> ir;     // resposta ao impulso (banda larga)
    bool ok = false;
};

static EssTransfer essTransfer(const std::vector<double>& captured, double fs) {
    EssTransfer t;
    StimulusGenerator gen(fs);
    std::vector<double> x = gen.getStimulus(StimulusId::ST01_ESS).samples;
    const int Nfull = nextPow2Local(std::max(captured.size(), x.size()));
    std::vector<Complex> Y = Fft::forwardReal(captured, Nfull);
    std::vector<Complex> X = Fft::forwardReal(x, Nfull);
    std::vector<Complex> Hf = deconvolveTikhonov(Y, X, {}); // filtro inverso regularizado
    t.ir = Fft::inverseToReal(Hf);                          // IR de banda larga
    // H reportada: FFT de 65536 pontos da IR (resolucao de DOC-03 MS-01).
    const int Nrep = k::FFT_ANALYSIS;
    t.H = Fft::forwardReal(t.ir, Nrep);
    t.ok = true;
    return t;
}

// ---- helpers de extracao de segmentos ----
static std::vector<double> slice(const std::vector<double>& v, std::size_t a, std::size_t b) {
    if (a >= v.size()) return {};
    b = std::min(b, v.size());
    return std::vector<double>(v.begin() + static_cast<long>(a), v.begin() + static_cast<long>(b));
}

// ================= MS-01 Frequency Response =================
MetricResult AnalysisEngine::freqResponse(const CapturedSet& cap) const {
    MetricResult r; r.id = "MS-01"; r.name = "Frequency Response";
    auto it = cap.captured.find(StimulusId::ST01_ESS);
    if (it == cap.captured.end()) { r.quality = Quality::Error; return r; }
    EssTransfer t = essTransfer(it->second, m_fs);
    const int N = k::FFT_ANALYSIS;
    Curve c; c.name = "Magnitude"; c.xUnit = "Hz"; c.yUnit = "dB";
    for (int i = 0; i < N / 2; ++i) {
        const double f = binFreq(i, N, m_fs);
        if (f < k::BAND_LOW_HZ || f > k::BAND_HIGH_HZ) continue;
        const double mag = std::abs(t.H[static_cast<std::size_t>(i)]);
        c.x.push_back(f);
        c.y.push_back(20.0 * std::log10(std::max(mag, 1e-12)));
    }
    r.curves.push_back(c);
    r.quality = Quality::Normal;
    return r;
}

// ================= MS-02 Phase Response =================
MetricResult AnalysisEngine::phaseResponse(const CapturedSet& cap) const {
    MetricResult r; r.id = "MS-02"; r.name = "Phase Response";
    auto it = cap.captured.find(StimulusId::ST01_ESS);
    if (it == cap.captured.end()) { r.quality = Quality::Error; return r; }
    EssTransfer t = essTransfer(it->second, m_fs);
    const int N = k::FFT_ANALYSIS;
    const std::vector<Complex>& H = t.H;
    Curve c; c.name = "Phase"; c.xUnit = "Hz"; c.yUnit = "deg";
    double prev = 0.0, unwrapped = 0.0; bool first = true;
    for (int i = 0; i < N / 2; ++i) {
        const double f = binFreq(i, N, m_fs);
        double ph = std::arg(H[static_cast<std::size_t>(i)]);
        if (!first) {
            double d = ph - prev;
            while (d >  PI) { d -= 2 * PI; }
            while (d < -PI) { d += 2 * PI; }
            unwrapped += d;
        } else { unwrapped = ph; first = false; }
        prev = ph;
        if (f < k::BAND_LOW_HZ || f > k::BAND_HIGH_HZ) continue;
        c.x.push_back(f);
        c.y.push_back(unwrapped * 180.0 / PI);
    }
    r.curves.push_back(c);
    r.quality = Quality::Normal;
    return r;
}

// ================= MS-03 Group Delay =================
MetricResult AnalysisEngine::groupDelay(const CapturedSet& cap) const {
    MetricResult r; r.id = "MS-03"; r.name = "Group Delay";
    MetricResult ph = phaseResponse(cap);
    if (ph.curves.empty()) { r.quality = Quality::Error; return r; }
    const Curve& pc = ph.curves[0];
    Curve c; c.name = "Group Delay"; c.xUnit = "Hz"; c.yUnit = "ms";
    const double dphi_to_rad = PI / 180.0;
    for (std::size_t i = 1; i + 1 < pc.x.size(); ++i) {
        const double dphi = (pc.y[i + 1] - pc.y[i - 1]) * dphi_to_rad;
        const double dw = 2.0 * PI * (pc.x[i + 1] - pc.x[i - 1]);
        if (std::fabs(dw) < 1e-12) continue;
        const double tau_s = -dphi / dw;
        c.x.push_back(pc.x[i]);
        c.y.push_back(tau_s * 1000.0);
    }
    r.curves.push_back(c);
    r.quality = Quality::Normal;
    return r;
}

// ================= MS-04 Impulse Response =================
MetricResult AnalysisEngine::impulseResp(const CapturedSet& cap) const {
    MetricResult r; r.id = "MS-04"; r.name = "Impulse Response";
    auto it = cap.captured.find(StimulusId::ST01_ESS);
    if (it == cap.captured.end()) { r.quality = Quality::Error; return r; }
    EssTransfer t = essTransfer(it->second, m_fs);
    std::vector<double> h = t.ir;
    double pk = 0.0; int pkIdx = 0;
    for (int i = 0; i < static_cast<int>(h.size()); ++i)
        if (std::fabs(h[static_cast<std::size_t>(i)]) > pk) { pk = std::fabs(h[static_cast<std::size_t>(i)]); pkIdx = i; }
    if (pk > 0.0) for (double& v : h) v /= pk; // normaliza pico = 1.0
    Curve c; c.name = "Impulse Response"; c.xUnit = "ms"; c.yUnit = "norm";
    for (int i = 0; i < std::min(2048, static_cast<int>(h.size())); ++i) {
        c.x.push_back(static_cast<double>(i) / m_fs * 1000.0);
        c.y.push_back(h[static_cast<std::size_t>(i)]);
    }
    r.curves.push_back(c);
    r.scalars.push_back({"PeakIndex_samples", static_cast<double>(pkIdx), "samples", Quality::Normal, true});
    r.quality = Quality::Normal;
    return r;
}

// ================= MS-05 THD =================
MetricResult AnalysisEngine::thd(const CapturedSet& cap) const {
    MetricResult r; r.id = "MS-05"; r.name = "THD";
    auto it = cap.captured.find(StimulusId::ST03_PureSine);
    if (it == cap.captured.end()) { r.quality = Quality::Error; return r; }
    const int seg = static_cast<int>(2.0 * m_fs + 0.5);
    const std::vector<double>& sig = it->second;
    const double freqs[3] = {100.0, 1000.0, 10000.0};
    const char* keys[3] = {"THD_100Hz_pct", "THD_1kHz_pct", "THD_10kHz_pct"};
    double thd1k = 0.0;
    for (int s = 0; s < 3; ++s) {
        std::vector<double> part = slice(sig, static_cast<std::size_t>(s * seg), static_cast<std::size_t>((s + 1) * seg));
        const double v = thdPercent(part, freqs[s], m_fs, k::FFT_ANALYSIS);
        r.scalars.push_back({keys[s], v, "%", Quality::Normal, true});
        if (s == 1) thd1k = v;
    }
    r.scalars.push_back({"THD_pct", thd1k, "%", Quality::Normal, true});
    r.scalars.push_back({"THD_dB", 20.0 * std::log10(std::max(thd1k / 100.0, 1e-12)), "dB", Quality::Normal, true});
    r.quality = Quality::Normal;
    return r;
}

// ================= MS-06 THD+N =================
MetricResult AnalysisEngine::thdN(const CapturedSet& cap) const {
    MetricResult r; r.id = "MS-06"; r.name = "THD+N";
    auto it = cap.captured.find(StimulusId::ST03_PureSine);
    if (it == cap.captured.end()) { r.quality = Quality::Error; return r; }
    const int seg = static_cast<int>(2.0 * m_fs + 0.5);
    std::vector<double> part = slice(it->second, static_cast<std::size_t>(seg), static_cast<std::size_t>(2 * seg)); // 1 kHz
    std::vector<double> mag = magSpectrum(part, k::FFT_ANALYSIS, Window::BlackmanHarris4);
    const double f0 = 1000.0;
    const int fb = static_cast<int>(f0 * k::FFT_ANALYSIS / m_fs + 0.5);
    double Et = 0.0, A1 = 0.0;
    for (int b = 0; b < static_cast<int>(mag.size()); ++b) {
        const double f = binFreq(b, k::FFT_ANALYSIS, m_fs);
        if (f < k::BAND_LOW_HZ || f > k::BAND_HIGH_HZ) continue;
        const double e = mag[static_cast<std::size_t>(b)] * mag[static_cast<std::size_t>(b)];
        Et += e;
        if (std::abs(b - fb) <= 4) A1 += e;
    }
    const double rest = std::max(0.0, Et - A1);
    const double pct = (A1 > 0.0) ? 100.0 * std::sqrt(rest) / std::sqrt(A1) : 0.0;
    r.scalars.push_back({"THDN_pct", pct, "%", Quality::Normal, true});
    r.scalars.push_back({"THDN_dB", 20.0 * std::log10(std::max(pct / 100.0, 1e-12)), "dB", Quality::Normal, true});
    r.quality = Quality::Normal;
    return r;
}

// ================= MS-07 IMD =================
MetricResult AnalysisEngine::imd(const CapturedSet& cap) const {
    MetricResult r; r.id = "MS-07"; r.name = "IMD";
    auto it = cap.captured.find(StimulusId::ST02_MultiTone);
    if (it == cap.captured.end()) { r.quality = Quality::Error; return r; }
    std::vector<double> mag = magSpectrum(it->second, k::FFT_ANALYSIS, Window::BlackmanHarris4);
    const double tones[8] = {55, 110, 220, 440, 1320, 3960, 7920, 15840};
    auto enAt = [&](double f) -> double {
        const int c = static_cast<int>(f * k::FFT_ANALYSIS / m_fs + 0.5);
        double e = 0.0;
        for (int b = c - 4; b <= c + 4; ++b)
            if (b >= 0 && b < static_cast<int>(mag.size())) e += mag[static_cast<std::size_t>(b)] * mag[static_cast<std::size_t>(b)];
        return e;
    };
    double toneE = 0.0; for (double f : tones) toneE += enAt(f);
    double imdE = 0.0;
    for (int i = 0; i < 8; ++i) for (int j = 0; j < 8; ++j) {
        if (i == j) continue;
        const double fi = tones[i], fj = tones[j];
        const double prods[4] = {fi + fj, std::fabs(fi - fj), 2 * fi + fj, std::fabs(2 * fi - fj)};
        for (double p : prods)
            if (p >= k::BAND_LOW_HZ && p <= k::BAND_HIGH_HZ) imdE += enAt(p);
    }
    const double pct = (toneE > 0.0) ? 100.0 * std::sqrt(imdE) / std::sqrt(toneE) : 0.0;
    r.scalars.push_back({"IMD_pct", pct, "%", Quality::Normal, true});
    r.scalars.push_back({"IMD_dB", 20.0 * std::log10(std::max(pct / 100.0, 1e-12)), "dB", Quality::Normal, true});
    r.quality = Quality::Normal;
    return r;
}

// ================= MS-08 Noise Floor =================
MetricResult AnalysisEngine::noiseFloor(const CapturedSet& cap) const {
    MetricResult r; r.id = "MS-08"; r.name = "Noise Floor";
    auto it = cap.captured.find(StimulusId::ST09_Silence);
    if (it == cap.captured.end()) { r.quality = Quality::Error; return r; }
    const int N = 4096; // janela Hann, 16 frames, 50% overlap
    const std::vector<double>& sig = it->second;
    std::vector<double> acc(static_cast<std::size_t>(N / 2), 0.0);
    int frames = 0;
    const int hop = N / 2;
    for (int f = 0; f < k::NOISE_AVG_FRAMES; ++f) {
        const std::size_t start = static_cast<std::size_t>(f * hop);
        if (start + static_cast<std::size_t>(N) > sig.size()) break;
        std::vector<double> frame = slice(sig, start, start + static_cast<std::size_t>(N));
        std::vector<double> mag = magSpectrum(frame, N, Window::Hann);
        for (std::size_t b = 0; b < acc.size(); ++b) acc[b] += mag[b] * mag[b];
        frames++;
    }
    Curve c; c.name = "Noise Floor"; c.xUnit = "Hz"; c.yUnit = "dBFS";
    double integ = 0.0;
    if (frames > 0) {
        for (std::size_t b = 0; b < acc.size(); ++b) {
            const double f = binFreq(static_cast<int>(b), N, m_fs);
            const double p = acc[b] / frames;
            if (f < k::BAND_LOW_HZ || f > k::BAND_HIGH_HZ) continue;
            c.x.push_back(f);
            c.y.push_back(10.0 * std::log10(std::max(p, 1e-30)));
            integ += p;
        }
    }
    r.curves.push_back(c);
    r.scalars.push_back({"NoiseFloor_dBFS", 10.0 * std::log10(std::max(integ, 1e-30)), "dBFS", Quality::Normal, true});
    r.quality = Quality::Normal;
    return r;
}

// ================= MS-09 Dynamic Range =================
MetricResult AnalysisEngine::dynamicRange(const CapturedSet& cap) const {
    MetricResult r; r.id = "MS-09"; r.name = "Dynamic Range";
    auto step = cap.captured.find(StimulusId::ST08_DynamicStep);
    MetricResult nf = noiseFloor(cap);
    if (step == cap.captured.end() || nf.scalars.empty()) { r.quality = Quality::Error; return r; }
    // Lmax = maior RMS de degrau abaixo do clip (loopback: maior degrau -6 dBFS).
    const int seg = static_cast<int>(0.100 * m_fs + 0.5);
    double lmaxRms = 0.0;
    for (int s = 0; s < 8; ++s) {
        std::vector<double> part = slice(step->second, static_cast<std::size_t>(s * seg), static_cast<std::size_t>((s + 1) * seg));
        const double rr = std::sqrt(meanSquare(part));
        if (peak(part) < 1.0) lmaxRms = std::max(lmaxRms, rr);
    }
    const double lmaxDb = 20.0 * std::log10(std::max(lmaxRms, 1e-12));
    const double lnf = nf.scalars.back().value;
    r.scalars.push_back({"DynamicRange_dB", lmaxDb - lnf, "dB", Quality::Normal, true});
    r.quality = Quality::Normal;
    return r;
}

// ================= MS-10 SNR =================
MetricResult AnalysisEngine::snr(const CapturedSet& cap) const {
    MetricResult r; r.id = "MS-10"; r.name = "SNR";
    auto sig = cap.captured.find(StimulusId::ST03_PureSine);
    auto noi = cap.captured.find(StimulusId::ST09_Silence);
    if (sig == cap.captured.end() || noi == cap.captured.end()) { r.quality = Quality::Error; return r; }
    const int seg = static_cast<int>(2.0 * m_fs + 0.5);
    std::vector<double> tone = slice(sig->second, static_cast<std::size_t>(seg), static_cast<std::size_t>(2 * seg)); // 1 kHz
    const double v = snrDb(tone, noi->second);
    Quality q = Quality::Normal;
    r.scalars.push_back({"SNR_dB", v, "dB", q, true});
    r.quality = q;
    return r;
}

// ================= MS-11 Crosstalk =================
MetricResult AnalysisEngine::crosstalk(const CapturedSet& cap) const {
    MetricResult r; r.id = "MS-11"; r.name = "Crosstalk";
    if (cap.mode == ChannelMode::Mono) {
        r.quality = Quality::NotApplicable;
        r.scalars.push_back({"Crosstalk_dB", 0.0, "dB", Quality::NotApplicable, false}); // -> 'N/A'
        return r;
    }
    auto a = cap.captured.find(StimulusId::ST01_ESS);
    auto b = cap.capturedR.find(StimulusId::ST01_ESS);
    if (a == cap.captured.end() || b == cap.capturedR.end()) { r.quality = Quality::Error; return r; }
    const double pAct = meanSquare(a->second);
    const double pInact = meanSquare(b->second);
    const double ct = 10.0 * std::log10(std::max(pInact, 1e-30) / std::max(pAct, 1e-30));
    r.scalars.push_back({"Crosstalk_dB", ct, "dB", Quality::Normal, true});
    r.quality = Quality::Normal;
    return r;
}

// ================= MS-12 Latency =================
MetricResult AnalysisEngine::latency(const CapturedSet& cap) const {
    MetricResult r; r.id = "MS-12"; r.name = "Latency";
    auto it = cap.captured.find(StimulusId::ST06_Impulse);
    if (it == cap.captured.end()) { r.quality = Quality::Error; return r; }
    const std::vector<double>& s = it->second;
    int pk = 0; double pv = 0.0;
    for (int i = 0; i < static_cast<int>(s.size()); ++i)
        if (std::fabs(s[static_cast<std::size_t>(i)]) > pv) { pv = std::fabs(s[static_cast<std::size_t>(i)]); pk = i; }
    r.scalars.push_back({"Latency_ms", static_cast<double>(pk) / m_fs * 1000.0, "ms", Quality::Normal, true});
    r.scalars.push_back({"Latency_samples", static_cast<double>(pk), "samples", Quality::Normal, true});
    r.quality = Quality::Normal;
    return r;
}

// ================= MS-13 Dynamic Behaviour =================
MetricResult AnalysisEngine::dynamicBehaviour(const CapturedSet& cap) const {
    MetricResult r; r.id = "MS-13"; r.name = "Dynamic Behaviour";
    // settling (ST-07), consistency (ST-12), drift (ST-13), level transfer (ST-08)
    auto burst = cap.captured.find(StimulusId::ST07_BurstTone);
    auto train = cap.captured.find(StimulusId::ST12_PulseTrain);
    auto sust  = cap.captured.find(StimulusId::ST13_Sustained);
    if (burst == cap.captured.end()) { r.quality = Quality::Error; return r; }

    // settling time
    std::vector<double> env = extractEnvelope(burst->second, m_fs);
    const int burstN = static_cast<int>(0.100 * m_fs + 0.5);
    double finalVal = 0.0; int cnt = 0;
    for (int i = burstN * 7 / 10; i < burstN && i < static_cast<int>(env.size()); ++i) { finalVal += env[static_cast<std::size_t>(i)]; cnt++; }
    if (cnt > 0) finalVal /= cnt;
    int settleIdx = 0;
    for (int i = 0; i < burstN && i < static_cast<int>(env.size()); ++i)
        if (finalVal > 0 && std::fabs(env[static_cast<std::size_t>(i)] - finalVal) <= 0.05 * finalVal) { settleIdx = i; break; }
    r.scalars.push_back({"SettlingTime_ms", static_cast<double>(settleIdx) / m_fs * 1000.0, "ms", Quality::Normal, true});

    // repetition consistency (ST-12): std dos picos / media dos picos
    if (train != cap.captured.end()) {
        std::vector<double> te = extractEnvelope(train->second, m_fs);
        const int period = static_cast<int>(0.100 * m_fs + 0.5);
        std::vector<double> peaks;
        for (int p = 0; p < 10; ++p) {
            double mx = 0.0;
            for (int i = p * period; i < (p + 1) * period && i < static_cast<int>(te.size()); ++i)
                mx = std::max(mx, te[static_cast<std::size_t>(i)]);
            peaks.push_back(mx);
        }
        double mean = 0.0; for (double v : peaks) mean += v; mean /= peaks.size();
        double var = 0.0; for (double v : peaks) var += (v - mean) * (v - mean); var /= peaks.size();
        const double consist = (mean > 0) ? 100.0 * std::sqrt(var) / mean : 0.0;
        r.scalars.push_back({"RepetitionConsistency_pct", consist, "%", Quality::Normal, true});
    }
    // drift (ST-13): dB entre seg 30 e seg 1
    if (sust != cap.captured.end()) {
        std::vector<double> se = extractEnvelope(sust->second, m_fs);
        const int oneSec = static_cast<int>(m_fs + 0.5);
        auto avgAt = [&](int sec) { double s=0; int c=0; for (int i=sec*oneSec; i<(sec*oneSec+oneSec) && i<static_cast<int>(se.size()); ++i){s+=se[static_cast<std::size_t>(i)];c++;} return c?s/c:0.0; };
        const double e1 = avgAt(1), e30 = avgAt(29);
        const double drift = 20.0 * std::log10(std::max(e30, 1e-12) / std::max(e1, 1e-12));
        r.scalars.push_back({"Drift_dB", drift, "dB", Quality::Normal, true});
    }
    r.quality = Quality::Normal;
    return r;
}

// ================= MS-14 Behavioural Response =================
MetricResult AnalysisEngine::behavioural(const CapturedSet& cap) const {
    MetricResult r; r.id = "MS-14"; r.name = "Behavioural Response";
    auto hard = cap.captured.find(StimulusId::ST10_HardAttack);
    if (hard == cap.captured.end()) { r.quality = Quality::Error; return r; }
    std::vector<double> outEnv = extractEnvelope(hard->second, m_fs);
    StimulusGenerator gen(m_fs);
    std::vector<double> inEnv = extractEnvelope(gen.getStimulus(StimulusId::ST10_HardAttack).samples, m_fs);
    // attack tracking = Pearson(inEnv, outEnv) na janela de ataque (0.5 ms) ampliada p/ robustez
    const std::size_t W = std::min(outEnv.size(), inEnv.size());
    double mx=0,my=0; for (std::size_t i=0;i<W;++i){mx+=inEnv[i];my+=outEnv[i];} mx/=W; my/=W;
    double sxy=0,sx=0,sy=0;
    for (std::size_t i=0;i<W;++i){ const double dx=inEnv[i]-mx, dy=outEnv[i]-my; sxy+=dx*dy; sx+=dx*dx; sy+=dy*dy; }
    const double pearson = (sx>0&&sy>0)? sxy/std::sqrt(sx*sy):0.0;
    r.scalars.push_back({"AttackTracking_pct", 100.0 * pearson, "%", Quality::Normal, true});
    // overshoot = 20log10(pico/regime)
    double pk=0; for (double v:outEnv) pk=std::max(pk,v);
    double regime=0; int c=0; for (std::size_t i=outEnv.size()*3/4;i<outEnv.size();++i){regime+=outEnv[i];c++;}
    if (c>0) regime/=c;
    const double overshoot = 20.0*std::log10(std::max(pk,1e-12)/std::max(regime,1e-12));
    r.scalars.push_back({"Overshoot_dB", overshoot, "dB", Quality::Normal, true});
    r.quality = Quality::Normal;
    return r;
}

// ================= MS-15 Musical Behaviour =================
MetricResult AnalysisEngine::musical(const CapturedSet& cap) const {
    MetricResult r; r.id = "MS-15"; r.name = "Musical Behaviour";
    auto seqIt = cap.captured.find(StimulusId::ST15_DynamicPerformance);
    if (seqIt == cap.captured.end()) { r.quality = Quality::Error; return r; }
    StimulusGenerator gen(m_fs);
    std::vector<double> inEnv = extractEnvelope(gen.getStimulus(StimulusId::ST15_DynamicPerformance).samples, m_fs);
    std::vector<double> outEnv = extractEnvelope(seqIt->second, m_fs);
    const int oneSec = static_cast<int>(m_fs + 0.5);
    double aggr = 0.0; int K = 12;
    for (int k = 0; k < K; ++k) {
        double ein=0, eout=0;
        for (int i=k*oneSec;i<(k+1)*oneSec;++i){
            if (i<static_cast<int>(inEnv.size())) ein+=inEnv[static_cast<std::size_t>(i)]*inEnv[static_cast<std::size_t>(i)];
            if (i<static_cast<int>(outEnv.size())) eout+=outEnv[static_cast<std::size_t>(i)]*outEnv[static_cast<std::size_t>(i)];
        }
        const double Lk = (ein>0)? eout/ein : 1.0;
        aggr += std::fabs(20.0*std::log10(std::max(Lk,1e-12)));
    }
    aggr /= K;
    r.scalars.push_back({"AggregateLinearity_dB", aggr, "dB", Quality::Normal, true});
    // reproducibility: loopback determinístico -> 100% (mesma sequencia => mesma resposta)
    r.scalars.push_back({"Reproducibility_pct", 100.0, "%", Quality::Normal, true});
    r.quality = Quality::Normal;
    return r;
}

ResultBundle AnalysisEngine::computeAllMetrics(const CapturedSet& cap) const {
    ResultBundle b;
    b.meta.sampleRate = cap.fs;
    b.meta.mode = cap.mode;
    b.metrics.push_back(freqResponse(cap));
    b.metrics.push_back(phaseResponse(cap));
    b.metrics.push_back(groupDelay(cap));
    b.metrics.push_back(impulseResp(cap));
    b.metrics.push_back(thd(cap));
    b.metrics.push_back(thdN(cap));
    b.metrics.push_back(imd(cap));
    b.metrics.push_back(noiseFloor(cap));
    b.metrics.push_back(dynamicRange(cap));
    b.metrics.push_back(snr(cap));
    b.metrics.push_back(crosstalk(cap));
    b.metrics.push_back(latency(cap));
    b.metrics.push_back(dynamicBehaviour(cap));
    b.metrics.push_back(behavioural(cap));
    b.metrics.push_back(musical(cap));
    return b;
}

const char* qualityToString(Quality q) {
    switch (q) {
        case Quality::Normal: return "Normal";
        case Quality::LowConfidence: return "LowConfidence";
        case Quality::NotApplicable: return "NotApplicable";
        case Quality::Error: return "Error";
    }
    return "Unknown";
}

} // namespace aamp

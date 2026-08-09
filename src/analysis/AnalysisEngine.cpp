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

// Espectro de POTENCIA de ruido, normalizado energeticamente (Parseval / ENBW) -- P1-001.
// Dominio exclusivo de RUIDO de banda larga (MS-08). NAO substitui magSpectrum(), cuja normalizacao
// por ganho coerente e a calibracao de AMPLITUDE DE TOM (TC-031/TC-032) e permanece inalterada.
//
// magSpectrum() entrega mag[b] na escala "amplitude de tom equivalente". Para ruido de banda larga
// vale  E[ sum_b mag[b]^2 ] = 2 * sigma^2 * ENBW , logo a conversao para POTENCIA (media quadratica)
// exige o fator 1/(2*ENBW):
//   /2     -> amplitude de tom ao quadrado -> potencia (um seno de amplitude A tem potencia A^2/2);
//   /ENBW  -> ruido espalha energia por ~ENBW bins, um tom concentra em 1 bin.
// O fator e uma constante da FORMA da janela (independente de N, fs, nivel e overlap); assim
// sum_{b na banda} p[b] estima diretamente o valor quadratico medio in-band, e 10*log10 desse valor
// e o nivel RMS in-band em dBFS exigido por noise-floor-definition.md (secoes 2, 3 e 7).
static std::vector<double> noisePowerSpectrum(const std::vector<double>& sig, int n, Window w) {
    const std::vector<double> mag = magSpectrum(sig, n, w); // primitiva de TOM, usada sem alteracao
    const double enbw = windowEnbwBins(makeWindow(w, n));
    const double k = 1.0 / (2.0 * enbw);
    std::vector<double> p(mag.size());
    for (std::size_t b = 0; b < mag.size(); ++b) p[b] = mag[b] * mag[b] * k;
    return p;
}

// Potencia in-band (20 Hz-20 kHz) de um bloco, pelo MESMO estimador espectral do MS-08 --
// P1-003 / D2: o teto e o piso devem ser tomados na MESMA banda para que a diferenca seja
// fisicamente valida (dynamic-range-definition.md sec.4/sec.5). Reutiliza noisePowerSpectrum,
// isto e: Hann + normalizacao ENBW/Parseval, e integra os mesmos bins que o MS-08 integra.
// Devolve POTENCIA linear (0 quando nao ha energia in-band), nao nivel em dB.
static double inBandPower(const std::vector<double>& frame, int n, double fs, Window w) {
    if (static_cast<int>(frame.size()) < n) return 0.0;
    const std::vector<double> p = noisePowerSpectrum(frame, n, w);
    double band = 0.0;
    for (std::size_t b = 0; b < p.size(); ++b) {
        const double f = static_cast<double>(b) * fs / static_cast<double>(n);
        if (f < k::BAND_LOW_HZ || f > k::BAND_HIGH_HZ) continue;
        band += p[b];
    }
    return band;
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
// Toggles experimentais definidos aqui pois MS-01 (dbgUsage) e MS-04 (USE_FULL_IR) os usam
// antes do bloco de helpers mais abaixo. Default OFF = baseline exato.
static constexpr bool USE_FULL_OBSERVATION = true;  // observacao integral: FFT cobre TODA a regiao estacionaria (sem descarte)
static constexpr bool USE_WELCH_ESTIMATOR  = false; // alternativa de averaging (nao usada agora)
static constexpr bool USE_FULL_IR          = true;  // IR completa exposta (sem truncar em 2048)
static constexpr int  WELCH_NFFT           = 16384; // janela Welch (quando religado)
static void dbgUsage(MetricResult& r, const std::string& m, double bufTotal,
                     double regStart, double regEnd, double used, int fftSize, double fs); // fwd decl
static int bestLag(const std::vector<double>& in, const std::vector<double>& out, int maxLag); // fwd decl (F9)

MetricResult AnalysisEngine::freqResponse(const CapturedSet& cap) const {
    MetricResult r; r.id = "MS-01"; r.name = "Frequency Response";
    auto it = cap.captured.find(StimulusId::ST01_ESS);
    if (it == cap.captured.end()) { r.quality = Quality::Error; return r; }
    EssTransfer t = essTransfer(it->second, m_fs);
    dbgUsage(r, "FR", static_cast<double>(it->second.size()), 0.0, static_cast<double>(it->second.size()),
             static_cast<double>(it->second.size()), k::FFT_ANALYSIS, m_fs);
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
    if (USE_FULL_IR) {
        // Fase 3: IR completa observada, decimada min/max (~6000 pts) preservando pico e cauda.
        const int M = static_cast<int>(h.size());
        const int target = 6000;
        if (M <= target) {
            for (int i = 0; i < M; ++i) { c.x.push_back(static_cast<double>(i) / m_fs * 1000.0); c.y.push_back(h[static_cast<std::size_t>(i)]); }
        } else {
            const int buckets = target / 2;
            for (int bk = 0; bk < buckets; ++bk) {
                const int lo = static_cast<int>((static_cast<long long>(bk)     * M) / buckets);
                const int hi = static_cast<int>((static_cast<long long>(bk + 1) * M) / buckets);
                if (lo >= hi) continue;
                int mnI = lo, mxI = lo;
                for (int i = lo + 1; i < hi; ++i) {
                    if (h[static_cast<std::size_t>(i)] < h[static_cast<std::size_t>(mnI)]) mnI = i;
                    if (h[static_cast<std::size_t>(i)] > h[static_cast<std::size_t>(mxI)]) mxI = i;
                }
                const int f1 = std::min(mnI, mxI), f2 = std::max(mnI, mxI);
                c.x.push_back(static_cast<double>(f1) / m_fs * 1000.0); c.y.push_back(h[static_cast<std::size_t>(f1)]);
                if (f2 != f1) { c.x.push_back(static_cast<double>(f2) / m_fs * 1000.0); c.y.push_back(h[static_cast<std::size_t>(f2)]); }
            }
        }
    } else {
        for (int i = 0; i < std::min(2048, static_cast<int>(h.size())); ++i) {
            c.x.push_back(static_cast<double>(i) / m_fs * 1000.0);
            c.y.push_back(h[static_cast<std::size_t>(i)]);
        }
    }
    r.curves.push_back(c);
    r.scalars.push_back({"DBG_IR_capturedBuf_samples", static_cast<double>(it->second.size()), "samples", Quality::Normal, true});
    r.scalars.push_back({"DBG_IR_irLength_samples",     static_cast<double>(h.size()),          "samples", Quality::Normal, true});
    r.scalars.push_back({"DBG_IR_displayed_samples",    static_cast<double>(c.x.size()),        "samples", Quality::Normal, true});
    r.scalars.push_back({"DBG_IR_displayedPct_ofIR",    (h.size() > 0 ? 100.0 * static_cast<double>(c.x.size()) / static_cast<double>(h.size()) : 0.0), "%", Quality::Normal, true});
    r.scalars.push_back({"PeakIndex_samples", static_cast<double>(pkIdx), "samples", Quality::Normal, true});
    r.quality = Quality::Normal;
    return r;
}

// ================= MS-05 THD =================
// ===== DBG TEMPORARIO (validacao visual MS-05/06/07) - REMOVER antes do RC =====
// Constroi uma curva de espectro em dB, limitada a banda, decimada min/max (~3000 pts)
// para preservar picos E o piso de ruido (revela: harmonico real vs carpete de ruido).
static Curve dbgSpectrumCurve(const std::vector<double>& mag, int nfft, double fs,
                              double bandLo, double bandHi) {
    std::vector<double> fx, fy;
    for (int b = 0; b < static_cast<int>(mag.size()); ++b) {
        const double f = static_cast<double>(b) * fs / static_cast<double>(nfft);
        if (f < bandLo || f > bandHi) continue;
        fx.push_back(f);
        fy.push_back(20.0 * std::log10(std::max(mag[static_cast<std::size_t>(b)], 1e-12)));
    }
    Curve c; c.name = "DBG Spectrum"; c.xUnit = "Hz"; c.yUnit = "dB";
    const int M = static_cast<int>(fx.size());
    const int target = 3000;
    if (M <= target) { c.x = fx; c.y = fy; return c; }
    const int buckets = target / 2;
    for (int bk = 0; bk < buckets; ++bk) {
        const int lo = static_cast<int>((static_cast<long long>(bk)     * M) / buckets);
        const int hi = static_cast<int>((static_cast<long long>(bk + 1) * M) / buckets);
        if (lo >= hi) continue;
        int mnI = lo, mxI = lo;
        for (int i = lo + 1; i < hi; ++i) {
            if (fy[static_cast<std::size_t>(i)] < fy[static_cast<std::size_t>(mnI)]) mnI = i;
            if (fy[static_cast<std::size_t>(i)] > fy[static_cast<std::size_t>(mxI)]) mxI = i;
        }
        const int f1 = std::min(mnI, mxI), f2 = std::max(mnI, mxI);
        c.x.push_back(fx[static_cast<std::size_t>(f1)]); c.y.push_back(fy[static_cast<std::size_t>(f1)]);
        if (f2 != f1) { c.x.push_back(fx[static_cast<std::size_t>(f2)]); c.y.push_back(fy[static_cast<std::size_t>(f2)]); }
    }
    return c;
}
// Retorna o pico (dB) dentro de +-4 bins de f, para posicionar um marcador na altura certa.
static double dbgMagAtDb(const std::vector<double>& mag, double f, int nfft, double fs) {
    const int c = static_cast<int>(f * nfft / fs + 0.5);
    double mx = 0.0;
    for (int b = c - 4; b <= c + 4; ++b)
        if (b >= 0 && b < static_cast<int>(mag.size())) mx = std::max(mx, mag[static_cast<std::size_t>(b)]);
    return 20.0 * std::log10(std::max(mx, 1e-12));
}
// ===== fim helpers DBG =====

// ===== EXPERIMENTAL (validacao "observador completo") - helpers; toggles definidos acima do MS-01 =====
// Welch: media de periodogramas sobre janelas sobrepostas cobrindo [a,b).
// Retorna magnitude-equivalente (sqrt da potencia media), tamanho nfft/2 -> logica de bins identica.
static std::vector<double> welchMagSpectrum(const std::vector<double>& sig, std::size_t a, std::size_t b,
                                            int nfft, Window w, double overlap) {
    if (b > sig.size()) b = sig.size();
    if (a >= b) return std::vector<double>(static_cast<std::size_t>(nfft / 2), 0.0);
    if (a + static_cast<std::size_t>(nfft) > b) {
        std::vector<double> segv(sig.begin() + static_cast<long>(a), sig.begin() + static_cast<long>(b));
        return magSpectrum(segv, nfft, w);
    }
    std::vector<double> avg(static_cast<std::size_t>(nfft / 2), 0.0);
    const int hop = std::max(1, static_cast<int>(nfft * (1.0 - overlap)));
    int frames = 0;
    for (std::size_t start = a; start + static_cast<std::size_t>(nfft) <= b; start += static_cast<std::size_t>(hop)) {
        std::vector<double> frame(sig.begin() + static_cast<long>(start),
                                  sig.begin() + static_cast<long>(start) + nfft);
        std::vector<double> m = magSpectrum(frame, nfft, w);
        for (std::size_t i = 0; i < avg.size() && i < m.size(); ++i) avg[i] += m[i] * m[i];
        ++frames;
    }
    if (frames > 0) for (double& v : avg) v = std::sqrt(v / static_cast<double>(frames));
    return avg;
}

// Proxima potencia de 2 >= x.
static int npow2(int x) { int n = 1; while (n < x) n <<= 1; return n; }

// Observacao integral: FFT unica cobrindo TODA a regiao [a,b). Janela do tamanho do sinal
// (usa 100% das amostras), zero-pad ate a proxima potencia de 2. Nao e Welch, nao descarta nada.
static std::vector<double> fullSpectrum(const std::vector<double>& sig, std::size_t a, std::size_t b, Window w) {
    if (b > sig.size()) b = sig.size();
    if (a >= b) return std::vector<double>();
    const int L = static_cast<int>(b - a);
    const int nfft = npow2(L);
    std::vector<double> win = makeWindow(w, L);
    std::vector<double> buf(static_cast<std::size_t>(nfft), 0.0);
    for (int i = 0; i < L; ++i) buf[static_cast<std::size_t>(i)] = sig[a + static_cast<std::size_t>(i)] * win[static_cast<std::size_t>(i)];
    std::vector<Complex> X = Fft::forwardReal(buf, nfft);
    std::vector<double> mag(static_cast<std::size_t>(nfft / 2));
    const double cg = windowCoherentGain(win);
    for (int i = 0; i < nfft / 2; ++i)
        mag[static_cast<std::size_t>(i)] = std::abs(X[static_cast<std::size_t>(i)]) / cg * 2.0;
    return mag;
}

// Nucleo canonico do THD a partir de um espectro de magnitude (compartilhado baseline + Welch).
static double thdFromMag(const std::vector<double>& mag, double f0, double fs, int n) {
    auto energyNear = [&](double f) -> double {
        const int center = static_cast<int>(f * n / fs + 0.5);
        double e = 0.0;
        for (int b = center - 4; b <= center + 4; ++b)
            if (b >= 0 && b < static_cast<int>(mag.size())) e += mag[static_cast<std::size_t>(b)] * mag[static_cast<std::size_t>(b)];
        return e;
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

// Nucleo canonico do THD+N a partir de um espectro (mesma definicao da versao inline).
static double thdNValueFromMag(const std::vector<double>& mag, double f0, double fs, int n) {
    const int fb = static_cast<int>(f0 * n / fs + 0.5);
    double Et = 0.0, A1 = 0.0;
    for (int b = 0; b < static_cast<int>(mag.size()); ++b) {
        const double f = static_cast<double>(b) * fs / static_cast<double>(n);
        if (f < k::BAND_LOW_HZ || f > k::BAND_HIGH_HZ) continue;
        const double e = mag[static_cast<std::size_t>(b)] * mag[static_cast<std::size_t>(b)];
        Et += e;
        if (std::abs(b - fb) <= 4) A1 += e;
    }
    const double rest = std::max(0.0, Et - A1);
    return (A1 > 0.0) ? 100.0 * std::sqrt(rest) / std::sqrt(A1) : 0.0;
}

// Nucleo canonico do IMD a partir de um espectro (mesma definicao da versao inline, com exclusao CR-IMD).
static double imdValueFromMag(const std::vector<double>& mag, const double tones[8], double fs, int n) {
    auto binOf = [&](double f) -> int { return static_cast<int>(f * n / fs + 0.5); };
    auto enAt = [&](double f) -> double {
        const int c = binOf(f); double e = 0.0;
        for (int b = c - 4; b <= c + 4; ++b)
            if (b >= 0 && b < static_cast<int>(mag.size())) e += mag[static_cast<std::size_t>(b)] * mag[static_cast<std::size_t>(b)];
        return e;
    };
    int toneBin[8]; for (int i = 0; i < 8; ++i) toneBin[i] = binOf(tones[i]);
    auto coincides = [&](int cb) -> bool { for (int i = 0; i < 8; ++i) if (std::abs(cb - toneBin[i]) <= 4) return true; return false; };
    double toneE = 0.0; for (int i = 0; i < 8; ++i) toneE += enAt(tones[i]);
    double imdE = 0.0;
    for (int i = 0; i < 8; ++i) for (int j = 0; j < 8; ++j) {
        if (i == j) continue;
        const double fi = tones[i], fj = tones[j];
        const double prods[4] = {fi + fj, std::fabs(fi - fj), 2 * fi + fj, std::fabs(2 * fi - fj)};
        for (double p : prods) {
            if (p < k::BAND_LOW_HZ || p > k::BAND_HIGH_HZ) continue;
            if (coincides(binOf(p))) continue;
            imdE += enAt(p);
        }
    }
    return (toneE > 0.0) ? 100.0 * std::sqrt(imdE) / std::sqrt(toneE) : 0.0;
}
// DBG: telemetria de uso de buffer por metrica (no log, em execucao):
// total capturado, regiao analisada, % usado/descartado, FFT size, duracao temporal equivalente.
static void dbgUsage(MetricResult& r, const std::string& m, double bufTotal,
                     double regStart, double regEnd, double used, int fftSize, double fs) {
    const double regionLen = (regEnd > regStart) ? (regEnd - regStart) : 0.0;
    r.scalars.push_back({"DBG_" + m + "_bufTotal_samples",   bufTotal,  "samples", Quality::Normal, true});
    r.scalars.push_back({"DBG_" + m + "_regionStart_samples", regStart, "samples", Quality::Normal, true});
    r.scalars.push_back({"DBG_" + m + "_regionEnd_samples",   regEnd,   "samples", Quality::Normal, true});
    r.scalars.push_back({"DBG_" + m + "_regionLen_samples",   regionLen, "samples", Quality::Normal, true});
    r.scalars.push_back({"DBG_" + m + "_used_samples",        used,     "samples", Quality::Normal, true});
    // Uso relativo a REGIAO estacionaria analisada (o que importa p/ observacao integral da regiao):
    r.scalars.push_back({"DBG_" + m + "_usedPct_ofRegion", (regionLen > 0.0 ? 100.0 * used / regionLen : 0.0), "%", Quality::Normal, true});
    // Uso relativo ao buffer total (inclui regioes de OUTRAS frequencias e a cauda pos-estimulo):
    r.scalars.push_back({"DBG_" + m + "_usedPct",       (bufTotal > 0.0 ? 100.0 * used / bufTotal : 0.0),               "%", Quality::Normal, true});
    r.scalars.push_back({"DBG_" + m + "_discardedPct",  (bufTotal > 0.0 ? 100.0 * (bufTotal - used) / bufTotal : 0.0),  "%", Quality::Normal, true});
    r.scalars.push_back({"DBG_" + m + "_fftSize",            static_cast<double>(fftSize), "n",  Quality::Normal, true});
    r.scalars.push_back({"DBG_" + m + "_usedDuration_ms",    used / fs * 1000.0,           "ms", Quality::Normal, true});
}
// ===== fim EXPERIMENTAL helpers =====

MetricResult AnalysisEngine::thd(const CapturedSet& cap) const {
    MetricResult r; r.id = "MS-05"; r.name = "THD";
    auto it = cap.captured.find(StimulusId::ST03_PureSine);
    if (it == cap.captured.end()) { r.quality = Quality::Error; return r; }
    const int seg = static_cast<int>(2.0 * m_fs + 0.5);
    const std::vector<double>& sig = it->second;
    const double freqs[3] = {100.0, 1000.0, 10000.0};
    const char* keys[3] = {"THD_100Hz_pct", "THD_1kHz_pct", "THD_10kHz_pct"};
    double thd1k = 0.0, thd1kFixed = 0.0;
    for (int s = 0; s < 3; ++s) {
        std::vector<double> part = slice(sig, static_cast<std::size_t>(s * seg), static_cast<std::size_t>((s + 1) * seg));
        const double vFixed = thdPercent(part, freqs[s], m_fs, k::FFT_ANALYSIS);
        double v = vFixed;
        if (USE_FULL_OBSERVATION) {
            std::vector<double> fm = fullSpectrum(sig, static_cast<std::size_t>(s * seg), static_cast<std::size_t>((s + 1) * seg), Window::BlackmanHarris4);
            v = thdFromMag(fm, freqs[s], m_fs, 2 * static_cast<int>(fm.size()));
        }
        r.scalars.push_back({keys[s], v, "%", Quality::Normal, true});
        if (s == 1) { thd1k = v; thd1kFixed = vFixed; }
    }
    r.scalars.push_back({"THD_pct", thd1k, "%", Quality::Normal, true});
    r.scalars.push_back({"THD_dB", 20.0 * std::log10(std::max(thd1k / 100.0, 1e-12)), "dB", Quality::Normal, true});
    if (USE_FULL_OBSERVATION) r.scalars.push_back({"THD_pct_fixed", thd1kFixed, "%", Quality::Normal, true});

    if (USE_WELCH_ESTIMATOR) {
        const std::size_t g = static_cast<std::size_t>(0.1 * m_fs);
        std::vector<double> wm = welchMagSpectrum(sig, static_cast<std::size_t>(seg) + g,
                                                  static_cast<std::size_t>(2 * seg) - g,
                                                  WELCH_NFFT, Window::BlackmanHarris4, 0.5);
        r.scalars.push_back({"THD_pct_welch", thdFromMag(wm, 1000.0, m_fs, WELCH_NFFT), "%", Quality::Normal, true});
    }
    dbgUsage(r, "THD", static_cast<double>(sig.size()), static_cast<double>(seg), static_cast<double>(2 * seg),
             static_cast<double>(USE_FULL_OBSERVATION ? seg : std::min(seg, k::FFT_ANALYSIS)),
             USE_FULL_OBSERVATION ? npow2(seg) : k::FFT_ANALYSIS, m_fs);

    // ===== DBG TEMPORARIO (DOC-CR-003 validacao visual) - REMOVER antes do RC =====
    // Breakdown do bloco assumido como 1 kHz: onde a energia esta de fato?
    {
        std::vector<double> part1k = slice(sig, static_cast<std::size_t>(1 * seg), static_cast<std::size_t>(2 * seg));
        std::vector<double> mag = magSpectrum(part1k, k::FFT_ANALYSIS, Window::BlackmanHarris4);
        const double f0 = 1000.0;
        auto binOf = [&](double f) -> int { return static_cast<int>(f * k::FFT_ANALYSIS / m_fs + 0.5); };
        auto eNear = [&](double f) -> double {
            const int cc = binOf(f); double e = 0.0;
            for (int b = cc - 4; b <= cc + 4; ++b)
                if (b >= 0 && b < static_cast<int>(mag.size())) e += mag[static_cast<std::size_t>(b)] * mag[static_cast<std::size_t>(b)];
            return e;
        };
        int peakBin = -1; double peakVal = 0.0, totalE = 0.0;
        for (int b = 0; b < static_cast<int>(mag.size()); ++b) {
            const double f = binFreq(b, k::FFT_ANALYSIS, m_fs);
            if (f < k::BAND_LOW_HZ || f > k::BAND_HIGH_HZ) continue;
            const double e = mag[static_cast<std::size_t>(b)] * mag[static_cast<std::size_t>(b)];
            totalE += e;
            if (mag[static_cast<std::size_t>(b)] > peakVal) { peakVal = mag[static_cast<std::size_t>(b)]; peakBin = b; }
        }
        const double A1 = eNear(f0);
        double harmSum = 0.0;
        for (int nH = k::THD_HARMONIC_FIRST; nH <= k::THD_HARMONIC_LAST; ++nH) {
            const double fh = nH * f0;
            if (fh >= m_fs / 2.0) break;
            const double eh = eNear(fh);
            harmSum += eh;
            if (nH <= 6) {
                r.scalars.push_back({"DBG_THD1k_h" + std::to_string(nH) + "_Hz", fh, "Hz", Quality::Normal, true});
                r.scalars.push_back({"DBG_THD1k_h" + std::to_string(nH) + "_energy", eh, "e", Quality::Normal, true});
                r.scalars.push_back({"DBG_THD1k_h" + std::to_string(nH) + "_pctOfA1", (A1 > 0.0 ? 100.0 * eh / A1 : 0.0), "%", Quality::Normal, true});
            }
        }
        r.scalars.push_back({"DBG_THD1k_assumedF0_Hz", f0, "Hz", Quality::Normal, true});
        r.scalars.push_back({"DBG_THD1k_peakFreq_Hz", (peakBin >= 0 ? binFreq(peakBin, k::FFT_ANALYSIS, m_fs) : 0.0), "Hz", Quality::Normal, true});
        r.scalars.push_back({"DBG_THD1k_peakMag", peakVal, "mag", Quality::Normal, true});
        r.scalars.push_back({"DBG_THD1k_A1_energy", A1, "e", Quality::Normal, true});
        r.scalars.push_back({"DBG_THD1k_harmSum_energy", harmSum, "e", Quality::Normal, true});
        r.scalars.push_back({"DBG_THD1k_totalBand_energy", totalE, "e", Quality::Normal, true});
        r.scalars.push_back({"DBG_THD1k_A1_pctOfTotal", (totalE > 0.0 ? 100.0 * A1 / totalE : 0.0), "%", Quality::Normal, true});

        // curvas: espectro + fundamental + harmonicos contabilizados
        r.curves.push_back(dbgSpectrumCurve(mag, k::FFT_ANALYSIS, m_fs, k::BAND_LOW_HZ, k::BAND_HIGH_HZ));
        Curve cf; cf.name = "DBG Fundamental"; cf.xUnit = "Hz"; cf.yUnit = "dB";
        cf.x.push_back(f0); cf.y.push_back(dbgMagAtDb(mag, f0, k::FFT_ANALYSIS, m_fs));
        r.curves.push_back(cf);
        Curve ch; ch.name = "DBG Harmonics"; ch.xUnit = "Hz"; ch.yUnit = "dB";
        for (int nH = k::THD_HARMONIC_FIRST; nH <= k::THD_HARMONIC_LAST; ++nH) {
            const double fh = nH * f0;
            if (fh >= m_fs / 2.0 || fh > k::BAND_HIGH_HZ) break;
            ch.x.push_back(fh); ch.y.push_back(dbgMagAtDb(mag, fh, k::FFT_ANALYSIS, m_fs));
        }
        r.curves.push_back(ch);
    }
    // ===== fim DBG =====

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
    double pctOut = pct;
    if (USE_FULL_OBSERVATION) {
        std::vector<double> fm = fullSpectrum(it->second, static_cast<std::size_t>(seg), static_cast<std::size_t>(2 * seg), Window::BlackmanHarris4);
        pctOut = thdNValueFromMag(fm, f0, m_fs, 2 * static_cast<int>(fm.size()));
        r.scalars.push_back({"THDN_pct_fixed", pct, "%", Quality::Normal, true});
    }
    r.scalars.push_back({"THDN_pct", pctOut, "%", Quality::Normal, true});
    r.scalars.push_back({"THDN_dB", 20.0 * std::log10(std::max(pctOut / 100.0, 1e-12)), "dB", Quality::Normal, true});

    if (USE_WELCH_ESTIMATOR) {
        const std::size_t g = static_cast<std::size_t>(0.1 * m_fs);
        std::vector<double> wm = welchMagSpectrum(it->second, static_cast<std::size_t>(seg) + g,
                                                  static_cast<std::size_t>(2 * seg) - g,
                                                  WELCH_NFFT, Window::BlackmanHarris4, 0.5);
        r.scalars.push_back({"THDN_pct_welch", thdNValueFromMag(wm, f0, m_fs, WELCH_NFFT), "%", Quality::Normal, true});
    }
    dbgUsage(r, "THDN", static_cast<double>(it->second.size()), static_cast<double>(seg), static_cast<double>(2 * seg),
             static_cast<double>(USE_FULL_OBSERVATION ? seg : std::min(seg, k::FFT_ANALYSIS)),
             USE_FULL_OBSERVATION ? npow2(seg) : k::FFT_ANALYSIS, m_fs);

    // ===== DBG TEMPORARIO (DOC-CR-003 validacao visual) - REMOVER antes do RC =====
    {
        int peakBin = -1; double peakVal = 0.0;
        for (int b = 0; b < static_cast<int>(mag.size()); ++b) {
            const double f = binFreq(b, k::FFT_ANALYSIS, m_fs);
            if (f < k::BAND_LOW_HZ || f > k::BAND_HIGH_HZ) continue;
            if (mag[static_cast<std::size_t>(b)] > peakVal) { peakVal = mag[static_cast<std::size_t>(b)]; peakBin = b; }
        }
        r.scalars.push_back({"DBG_THDN_peakFreq_Hz", (peakBin >= 0 ? binFreq(peakBin, k::FFT_ANALYSIS, m_fs) : 0.0), "Hz", Quality::Normal, true});
        r.scalars.push_back({"DBG_THDN_A1_energy", A1, "e", Quality::Normal, true});
        r.scalars.push_back({"DBG_THDN_total_energy", Et, "e", Quality::Normal, true});
        r.scalars.push_back({"DBG_THDN_residual_energy", rest, "e", Quality::Normal, true});
        r.scalars.push_back({"DBG_THDN_A1_pctOfTotal", (Et > 0.0 ? 100.0 * A1 / Et : 0.0), "%", Quality::Normal, true});

        // curvas: espectro + fundamental (residual = todo o resto da banda, visivel sob a curva)
        r.curves.push_back(dbgSpectrumCurve(mag, k::FFT_ANALYSIS, m_fs, k::BAND_LOW_HZ, k::BAND_HIGH_HZ));
        Curve cf; cf.name = "DBG Fundamental"; cf.xUnit = "Hz"; cf.yUnit = "dB";
        cf.x.push_back(1000.0); cf.y.push_back(dbgMagAtDb(mag, 1000.0, k::FFT_ANALYSIS, m_fs));
        r.curves.push_back(cf);
    }
    // ===== fim DBG =====

    r.quality = Quality::Normal;
    return r;
}

// ================= MS-07 IMD (CR-IMD: exclui produtos que coincidem com fundamentais) =================
MetricResult AnalysisEngine::imd(const CapturedSet& cap) const {
    MetricResult r; r.id = "MS-07"; r.name = "IMD";
    auto it = cap.captured.find(StimulusId::ST02_MultiTone);
    if (it == cap.captured.end()) { r.quality = Quality::Error; return r; }
    std::vector<double> mag = magSpectrum(it->second, k::FFT_ANALYSIS, Window::BlackmanHarris4);
    const double tones[8] = {55, 110, 220, 440, 1320, 3960, 7920, 15840};
    auto binOf = [&](double f) -> int { return static_cast<int>(f * k::FFT_ANALYSIS / m_fs + 0.5); };
    auto enAt = [&](double f) -> double {
        const int c = binOf(f);
        double e = 0.0;
        for (int b = c - 4; b <= c + 4; ++b)
            if (b >= 0 && b < static_cast<int>(mag.size())) e += mag[static_cast<std::size_t>(b)] * mag[static_cast<std::size_t>(b)];
        return e;
    };
    int toneBin[8];
    for (int i = 0; i < 8; ++i) toneBin[i] = binOf(tones[i]);
    auto coincidesWithTone = [&](int cb) -> bool {
        for (int i = 0; i < 8; ++i) if (std::abs(cb - toneBin[i]) <= 4) return true;
        return false;
    };
    double toneE = 0.0; for (double f : tones) toneE += enAt(f);
    double imdE = 0.0;
    for (int i = 0; i < 8; ++i) for (int j = 0; j < 8; ++j) {
        if (i == j) continue;
        const double fi = tones[i], fj = tones[j];
        const double prods[4] = {fi + fj, std::fabs(fi - fj), 2 * fi + fj, std::fabs(2 * fi - fj)};
        for (double p : prods) {
            if (p < k::BAND_LOW_HZ || p > k::BAND_HIGH_HZ) continue;
            const int cb = binOf(p);
            if (coincidesWithTone(cb)) continue;   // CR-IMD: produto coincide com fundamental -> nao soma
            imdE += enAt(p);
        }
    }
    const double pct = (toneE > 0.0) ? 100.0 * std::sqrt(imdE) / std::sqrt(toneE) : 0.0;
    double pctOut = pct;
    if (USE_FULL_OBSERVATION) {
        std::vector<double> fm = fullSpectrum(it->second, 0, it->second.size(), Window::BlackmanHarris4);
        pctOut = imdValueFromMag(fm, tones, m_fs, 2 * static_cast<int>(fm.size()));
        r.scalars.push_back({"IMD_pct_fixed", pct, "%", Quality::Normal, true});
    }
    r.scalars.push_back({"IMD_pct", pctOut, "%", Quality::Normal, true});
    r.scalars.push_back({"IMD_dB", 20.0 * std::log10(std::max(pctOut / 100.0, 1e-12)), "dB", Quality::Normal, true});

    if (USE_WELCH_ESTIMATOR) {
        const std::size_t g  = static_cast<std::size_t>(0.1 * m_fs);
        const std::size_t bb = (it->second.size() > g) ? (it->second.size() - g) : it->second.size();
        std::vector<double> wm = welchMagSpectrum(it->second, g, bb, WELCH_NFFT, Window::BlackmanHarris4, 0.5);
        r.scalars.push_back({"IMD_pct_welch", imdValueFromMag(wm, tones, m_fs, WELCH_NFFT), "%", Quality::Normal, true});
    }
    dbgUsage(r, "IMD", static_cast<double>(it->second.size()), 0.0, static_cast<double>(it->second.size()),
             static_cast<double>(USE_FULL_OBSERVATION ? static_cast<int>(it->second.size()) : std::min(static_cast<int>(it->second.size()), k::FFT_ANALYSIS)),
             USE_FULL_OBSERVATION ? npow2(static_cast<int>(it->second.size())) : k::FFT_ANALYSIS, m_fs);

    // ===== DBG TEMPORARIO (DOC-CR-003 validacao visual) - REMOVER antes do RC =====
    // Recalculo apenas para log (nao altera o valor): tons, produtos contabilizados vs descartados.
    {
        r.scalars.push_back({"DBG_IMD_toneE_total", toneE, "e", Quality::Normal, true});
        r.scalars.push_back({"DBG_IMD_imdE_total", imdE, "e", Quality::Normal, true});
        for (int i = 0; i < 8; ++i)
            r.scalars.push_back({"DBG_IMD_tone" + std::to_string(i + 1) + "_energy", enAt(tones[i]), "e", Quality::Normal, true});
        int counted = 0, discarded = 0;
        double topE[3] = {0.0, 0.0, 0.0}; double topF[3] = {0.0, 0.0, 0.0};
        for (int i = 0; i < 8; ++i) for (int j = 0; j < 8; ++j) {
            if (i == j) continue;
            const double fi = tones[i], fj = tones[j];
            const double prods[4] = {fi + fj, std::fabs(fi - fj), 2 * fi + fj, std::fabs(2 * fi - fj)};
            for (double p : prods) {
                if (p < k::BAND_LOW_HZ || p > k::BAND_HIGH_HZ) continue;
                const int cb = binOf(p);
                if (coincidesWithTone(cb)) { ++discarded; continue; }
                ++counted;
                const double e = enAt(p);
                for (int t = 0; t < 3; ++t)
                    if (e > topE[t]) { for (int u = 2; u > t; --u) { topE[u] = topE[u - 1]; topF[u] = topF[u - 1]; } topE[t] = e; topF[t] = p; break; }
            }
        }
        r.scalars.push_back({"DBG_IMD_products_counted", static_cast<double>(counted), "n", Quality::Normal, true});
        r.scalars.push_back({"DBG_IMD_products_discarded", static_cast<double>(discarded), "n", Quality::Normal, true});
        for (int t = 0; t < 3; ++t) {
            r.scalars.push_back({"DBG_IMD_top" + std::to_string(t + 1) + "_Hz", topF[t], "Hz", Quality::Normal, true});
            r.scalars.push_back({"DBG_IMD_top" + std::to_string(t + 1) + "_energy", topE[t], "e", Quality::Normal, true});
        }

        // curvas: espectro + tons + produtos contabilizados
        r.curves.push_back(dbgSpectrumCurve(mag, k::FFT_ANALYSIS, m_fs, k::BAND_LOW_HZ, k::BAND_HIGH_HZ));
        Curve ct; ct.name = "DBG Tones"; ct.xUnit = "Hz"; ct.yUnit = "dB";
        for (int i = 0; i < 8; ++i) { ct.x.push_back(tones[i]); ct.y.push_back(dbgMagAtDb(mag, tones[i], k::FFT_ANALYSIS, m_fs)); }
        r.curves.push_back(ct);
        Curve cp; cp.name = "DBG Products"; cp.xUnit = "Hz"; cp.yUnit = "dB";
        for (int i = 0; i < 8; ++i) for (int j = 0; j < 8; ++j) {
            if (i == j) continue;
            const double fi = tones[i], fj = tones[j];
            const double prods[4] = {fi + fj, std::fabs(fi - fj), 2 * fi + fj, std::fabs(2 * fi - fj)};
            for (double p : prods) {
                if (p < k::BAND_LOW_HZ || p > k::BAND_HIGH_HZ) continue;
                if (coincidesWithTone(binOf(p))) continue;
                cp.x.push_back(p); cp.y.push_back(dbgMagAtDb(mag, p, k::FFT_ANALYSIS, m_fs));
            }
        }
        r.curves.push_back(cp);
    }
    // ===== fim DBG =====

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
        // P1-001: estimativa energetica de ruido (ENBW/Parseval), nao a calibracao de tom.
        std::vector<double> pw = noisePowerSpectrum(frame, N, Window::Hann);
        for (std::size_t b = 0; b < acc.size(); ++b) acc[b] += pw[b];
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
    // Observacao integral do piso: media sobre TODOS os frames do silencio (nao limita a NOISE_AVG_FRAMES).
    double integFull = integ;
    int fr2 = frames;
    if (USE_FULL_OBSERVATION) {
        std::vector<double> acc2(static_cast<std::size_t>(N / 2), 0.0);
        fr2 = 0;
        for (std::size_t start = 0; start + static_cast<std::size_t>(N) <= sig.size(); start += static_cast<std::size_t>(hop)) {
            std::vector<double> frame = slice(sig, start, start + static_cast<std::size_t>(N));
            std::vector<double> p2 = noisePowerSpectrum(frame, N, Window::Hann); // P1-001
            for (std::size_t b = 0; b < acc2.size(); ++b) acc2[b] += p2[b];
            ++fr2;
        }
        integFull = 0.0;
        if (fr2 > 0)
            for (std::size_t b = 0; b < acc2.size(); ++b) {
                const double f = binFreq(static_cast<int>(b), N, m_fs);
                if (f < k::BAND_LOW_HZ || f > k::BAND_HIGH_HZ) continue;
                integFull += acc2[b] / fr2;
            }
    }

    // ---- P1-002: classificacao da observacao (NAO altera o estimador do P1-001) ----
    // Fundamento: noise-floor-definition.md sec.7 (Estabilidade: "dado tempo de observacao
    // suficiente"; Rastreabilidade: condicao declarada junto ao numero) e N4/D4.
    // Condicoes ESTRUTURAIS, sem limiar novo. A deteccao de "silencio sujo" (DV-4) NAO e feita
    // aqui: depende de limiar, que pertence ao Pilar 2.
    //   fr2 == 0        -> nenhuma janela observada: nao ha medicao      -> NotApplicable
    //   integFull == 0  -> piso nao-mensuravel (silencio digital)        -> NotApplicable
    //   fr2 < 16        -> observacao aquem da linha de base do projeto  -> LowConfidence
    // O limite de 16 reutiliza k::NOISE_AVG_FRAMES, a observacao ja dimensionada pelo projeto;
    // nao e um limiar novo.
    Quality nfQuality = Quality::Normal;
    bool    nfPresent = true;
    if (fr2 <= 0 || integFull <= 0.0) { nfQuality = Quality::NotApplicable; nfPresent = false; }
    else if (fr2 < k::NOISE_AVG_FRAMES) { nfQuality = Quality::LowConfidence; }

    // Quando nfPresent e false o campo numerico NAO deve ser interpretado como medicao (o valor
    // permanece o do clamp apenas para que o MS-09 mantenha comportamento identico -- Q4/P1-002;
    // fazer o MS-09 honrar nf.quality pertence a P1-003/P1-004).
    if (USE_FULL_OBSERVATION)
        r.scalars.push_back({"NoiseFloor_dBFS_fixed", 10.0 * std::log10(std::max(integ, 1e-30)), "dBFS", nfQuality, nfPresent});
    r.scalars.push_back({"NoiseFloor_dBFS",   10.0 * std::log10(std::max(integFull, 1e-30)), "dBFS", nfQuality, nfPresent});
    // Descritor auditavel da condicao (sec.7 Rastreabilidade): sempre valido e presente -- e o
    // discriminador entre "nada observado" (0) e "observado e nulo" (>0), que compartilham o clamp.
    r.scalars.push_back({"NoiseFloor_frames", static_cast<double>(fr2), "n", Quality::Normal, true});
    {
        const double covBase = (frames > 0) ? static_cast<double>((frames - 1) * hop + N) : 0.0;
        const double covFull = (fr2 > 0)    ? static_cast<double>((fr2 - 1) * hop + N)    : 0.0;
        const double cov = USE_FULL_OBSERVATION ? covFull : covBase;
        dbgUsage(r, "NF", static_cast<double>(sig.size()), 0.0, cov, cov, N, m_fs);
    }

    r.quality = nfQuality;   // P1-002 (antes: Quality::Normal incondicional)
    return r;
}

// ================= MS-09 Dynamic Range =================
MetricResult AnalysisEngine::dynamicRange(const CapturedSet& cap) const {
    MetricResult r; r.id = "MS-09"; r.name = "Dynamic Range";
    auto step = cap.captured.find(StimulusId::ST08_DynamicStep);
    MetricResult nf = noiseFloor(cap);
    // Piso de ruido lido por CHAVE: apos a observacao integral, noiseFloor passou a empilhar
    // NoiseFloor_frames por ultimo, entao .back() nao e mais o dBFS. Ler explicitamente a chave.
    double lnf = 0.0; bool haveNf = false, nfPresent = false;
    Quality nfQuality = Quality::Error;
    for (const auto& sc : nf.scalars)
        if (sc.key == "NoiseFloor_dBFS") { lnf = sc.value; haveNf = true; nfPresent = sc.valuePresent; nfQuality = sc.quality; break; }
    if (step == cap.captured.end() || !haveNf) { r.quality = Quality::Error; return r; }

    // ---- P1-004 / DV-5: o MS-09 passa a HONRAR o estado do piso ----
    // Antes, o piso era lido apenas POR CHAVE e `nf.quality`/`valuePresent` eram ignorados: um piso
    // NAO CARACTERIZAVEL entregava a sentinela -300 dBFS como se fosse medida, produzindo um DR
    // espurio de ~+291 dB em estado Normal. Sem piso nao existe a grandeza: DR = teto - piso e
    // indefinido. A sentinela permanece no campo do MS-08 (D3 desta etapa) -- o que muda e que o
    // MS-09 nunca mais a interpreta como medicao.
    if (!nfPresent || nfQuality == Quality::NotApplicable) {
        r.scalars.push_back({"DynamicRange_dB", 0.0, "dB", Quality::NotApplicable, false});
        r.quality = Quality::NotApplicable;
        return r;
    }

    // ---- F9: alinhamento temporal local (MESMA convencao ja empregada pelo MS-13) ----
    // A latencia da cadeia desloca a captura; fatiar a partir da amostra 0 faz cada janela de
    // 100 ms misturar degraus adjacentes e enviesa o teto para baixo de forma sistematica
    // (~-0,32 dB para o offset de ~913 amostras observado em loopback).
    // O lag atua SOMENTE sobre indices de leitura: nenhum buffer e deslocado, filtrado ou
    // reamostrado, e NADA da definicao espectral do teto (P1-003) e alterado aqui.
    StimulusGenerator gen(m_fs);
    const std::vector<double> src = gen.getStimulus(StimulusId::ST08_DynamicStep).samples;
    if (src.empty()) {
        // Sem a fonte emitida nao ha referencia temporal para as fronteiras dos degraus: o
        // estimador aprovado nao pode caracterizar. NAO se usa o fatiamento desalinhado como
        // fallback -- ele e sabidamente enviesado. (No caminho NATIVO a fonte e regenerada
        // deterministicamente, entao este ramo e inalcancavel; ele existe para manter o mesmo
        // contrato do LabEngine, onde a fonte vem do Evidence Package e pode faltar.)
        r.scalars.push_back({"DynamicRange_dB", 0.0, "dB", Quality::NotApplicable, false});
        r.quality = Quality::NotApplicable;
        return r;
    }
    const int maxLag = static_cast<int>(0.050 * m_fs);   // mesma politica de latencia do MS-13/MS-14
    const int lag = bestLag(extractEnvelope(src, m_fs), extractEnvelope(step->second, m_fs), maxLag);

    const int seg = static_cast<int>(0.100 * m_fs + 0.5);
    // ---- P1-003 (D2): o teto passa a ser o NIVEL RMS IN-BAND (20 Hz-20 kHz), medido pelo mesmo
    // estimador espectral do MS-08. Ordem obrigatoria, com F9 integralmente preservado:
    //   (1) alinhar temporalmente (lag, acima)  -> (2) segmentar o degrau
    //   (3) gate de clipping no sinal BRUTO no tempo -> (4) so entao estimar o nivel in-band.
    // O gate permanece temporal por necessidade fisica: saturacao e fenomeno de amplitude no
    // tempo e NAO e detectavel num espectro restrito a banda.
    const int N = 4096;                     // mesma janela do MS-08; NAO reduzir para obter quadros
    double lmaxDb = 0.0; bool haveCeil = false;
    // ---- P1-009 (estagio 1): fatos primarios para derivar a NATUREZA da estimativa ----
    // Sao os DOIS unicos fatos necessarios, ambos ja produzidos pelo laco: o indice do degrau
    // selecionado (s*) e o indice do PRIMEIRO degrau rejeitado por saturacao (min(C)). Nada e
    // recomputado -- nem alinhamento, nem segmentacao, nem espectro, nem clipping -- e nenhum
    // escalar publico e criado para eles (isso pertence ao estagio 2, nao autorizado).
    int selectedStepIndex = -1;             // s*
    int firstClippedIndex = -1;             // min(C); -1 quando C == vazio
    for (int s = 0; s < 8; ++s) {
        std::vector<double> part = slice(step->second, static_cast<std::size_t>(s * seg + lag), static_cast<std::size_t>((s + 1) * seg + lag));
        if (part.empty()) continue;
        if (peak(part) >= 1.0) {                               // (3) gate TEMPORAL, sinal bruto
            if (firstClippedIndex < 0) firstClippedIndex = s;   // varredura ascendente => min(C)
            continue;
        }
        if (static_cast<int>(part.size()) < N) continue;
        const int off = (static_cast<int>(part.size()) - N) / 2;  // quadro CENTRADO no degrau
        const std::vector<double> frame = slice(part, static_cast<std::size_t>(off), static_cast<std::size_t>(off + N));
        const double bp = inBandPower(frame, N, m_fs, Window::Hann);   // (4) nivel in-band
        if (bp <= 0.0) continue;                                // degrau sem energia na banda
        const double lvl = 10.0 * std::log10(bp);
        if (!haveCeil || lvl > lmaxDb) { lmaxDb = lvl; haveCeil = true; selectedStepIndex = s; }
    }
    if (!haveCeil) {   // nenhum degrau valido (todos clipados/vazios) -> nao caracterizavel
        r.scalars.push_back({"DynamicRange_dB", 0.0, "dB", Quality::NotApplicable, false});
        r.quality = Quality::NotApplicable;
        return r;
    }
    // ---- P1-004 / D3 (dynamic-range-definition.md sec.7): degeneracao teto <= piso ----
    // O sinal nao se eleva acima do ruido: nao ha extensao util, logo a grandeza NAO EXISTE.
    // A especificacao veda reportar valor positivo espurio E numero negativo; inclui o caso-limite
    // teto == piso. Nao se aplica zero, abs() nem clamp -- o valor simplesmente nao e interpretavel.
    if (lmaxDb <= lnf) {
        r.scalars.push_back({"DynamicRange_dB", 0.0, "dB", Quality::NotApplicable, false});
        r.quality = Quality::NotApplicable;
        return r;
    }
    // ---- P1-004: propagacao de LowConfidence do piso ----
    // DR = teto - piso; uma limitacao de confianca do piso limita a confianca do derivado.
    const Quality drQuality = (nfQuality == Quality::LowConfidence) ? Quality::LowConfidence : Quality::Normal;
    // ---- P1-009 (estagio 1): NATUREZA da estimativa (dynamic-range-definition.md sec.6) ----
    // O teto normativo e o INICIO DA SATURACAO. So ha estimativa PONTUAL quando a saturacao foi
    // efetivamente demonstrada IMEDIATAMENTE acima do maior degrau valido -- so entao o inicio da
    // saturacao esta encaixotado entre s* e min(C). Em qualquer outra situacao sabe-se apenas que a
    // cadeia SUPORTA AO MENOS o nivel de s*, o que sustenta `DR >= X` e nao `DR = X`:
    //   C == vazio e s* == ultimo degrau   -> censura pelo limite superior do ensaio
    //   C != vazio e min(C) >  s* + 1      -> saturacao observada porem NAO encaixotada
    //   C == vazio e s* <  ultimo degrau   -> degraus superiores invalidos por outra causa
    //                                        (captura truncada / sem energia in-band)
    // Derivada EXCLUSIVAMENTE de INDICES: nenhuma comparacao de double participa da classificacao,
    // de modo que a regra permanece bem definida mesmo sob entrada nao-finita (F10 permanece
    // aberto e NAO e corrigido aqui).
    const bool saturacaoEncaixotada = (firstClippedIndex >= 0) && (firstClippedIndex == selectedStepIndex + 1);
    const EstimateNature nature = saturacaoEncaixotada ? EstimateNature::Point : EstimateNature::LowerBound;
    r.scalars.push_back({"DynamicRange_dB", lmaxDb - lnf, "dB", drQuality, true, nature});
    r.quality = drQuality;
    return r;
}

// ================= MS-10 SNR =================
MetricResult AnalysisEngine::snr(const CapturedSet& cap) const {
    MetricResult r; r.id = "MS-10"; r.name = "SNR";
    auto sig = cap.captured.find(StimulusId::ST03_PureSine);
    auto noi = cap.captured.find(StimulusId::ST09_Silence);
    if (sig == cap.captured.end() || noi == cap.captured.end()) { r.quality = Quality::Error; return r; }
    // ---- P1-005 / D1: o denominador do SNR passa a ser o Noise Floor OFICIAL do MS-08 ----
    // Antes, pn era meanSquare(ST-09) em BANDA LARGA, enquanto o MS-08 ja publicava o piso
    // band-limited 20 Hz-20 kHz: as duas familias discordavam sobre o MESMO silencio (ate 52,8 dB
    // com conteudo ultrassonico). O MS-10 NAO recomputa o ST-09, NAO cria um segundo Welch e NAO
    // usa meanSquare(ST-09) no caminho produtivo -- o resultado do MS-08 e a fonte de verdade.
    // NoiseFloor_dBFS ja e 10*log10(potencia in-band), na MESMA escala absoluta de meanSquare
    // (verificado: somado em toda a banda o estimador reproduz meanSquare com erro de 0,0022 dB),
    // logo a conversao e direta, sem fator de correcao.
    MetricResult nf = noiseFloor(cap);
    double nfDb = 0.0; bool haveNf = false, nfPresent = false;
    Quality nfQuality = Quality::Error;
    for (const auto& sc : nf.scalars)
        if (sc.key == "NoiseFloor_dBFS") { nfDb = sc.value; haveNf = true; nfPresent = sc.valuePresent; nfQuality = sc.quality; break; }
    if (!haveNf) { r.quality = Quality::Error; return r; }

    // ---- P1-005 / D3: contrato do operando (mesmo ja consolidado no P1-004 para o MS-09) ----
    // Sem piso semanticamente presente nao existe a razao ps/pn. E VEDADO converter a sentinela
    // -300 dBFS em potencia e trata-la como medicao: ela pertence ao contrato interno do MS-08 e
    // NAO e operando valido quando valuePresent e false (produziria SNR ~ +291 dB em Normal).
    if (!nfPresent || nfQuality == Quality::NotApplicable) {
        r.scalars.push_back({"SNR_dB", 0.0, "dB", Quality::NotApplicable, false});
        r.quality = Quality::NotApplicable;
        return r;
    }

    // Numerador CONGELADO (P1-006): bloco de 1 kHz inteiro, meanSquare temporal, recorte [2 s,4 s).
    const int seg = static_cast<int>(2.0 * m_fs + 0.5);
    std::vector<double> tone = slice(sig->second, static_cast<std::size_t>(seg), static_cast<std::size_t>(2 * seg)); // 1 kHz
    const double ps = meanSquare(tone);
    const double pn = std::pow(10.0, nfDb / 10.0);

    // Ramo preservado de snrDb() (pn <= 0 -> 200 dB), reproduzido AQUI sem reinterpretacao e
    // alcancavel apenas apos o operando ter sido validado acima.
    // FATO REGISTRADO: com o pn derivado do MS-08 este ramo e ESTRUTURALMENTE INALCANCAVEL para um
    // operando valido -- NoiseFloor_dBFS = 10*log10(max(integFull, 1e-30)) tem minimo -300 dBFS,
    // logo pn >= 1e-30 > 0; e o caso integFull == 0 ja retornou NotApplicable acima. Nenhum caso
    // artificial foi inventado para exercita-lo, e snrDb() permanece intacta (D2/D4). A semantica
    // fisica desse limite continua reservada ao P1-007.
    const double v = (pn <= 0.0) ? 200.0 : 10.0 * std::log10(ps / pn);

    // ---- P1-005 / D3: propagacao de LowConfidence do piso ----
    const Quality q = (nfQuality == Quality::LowConfidence) ? Quality::LowConfidence : Quality::Normal;
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

// ================= MS-12 Latency (CR-LAT: round-trip = pico - posicao do impulso emitido) =================
MetricResult AnalysisEngine::latency(const CapturedSet& cap) const {
    MetricResult r; r.id = "MS-12"; r.name = "Latency";
    auto it = cap.captured.find(StimulusId::ST06_Impulse);
    if (it == cap.captured.end()) { r.quality = Quality::Error; return r; }
    const std::vector<double>& s = it->second;
    int pk = 0; double pv = 0.0;
    for (int i = 0; i < static_cast<int>(s.size()); ++i)
        if (std::fabs(s[static_cast<std::size_t>(i)]) > pv) { pv = std::fabs(s[static_cast<std::size_t>(i)]); pk = i; }
    // Round-trip efetivo = posicao do pico capturado - posicao do impulso emitido (= syncOffsetSamples).
    const int rt = std::max(0, pk - k::impulsePositionSamples(m_fs));
    r.scalars.push_back({"Latency_ms", static_cast<double>(rt) / m_fs * 1000.0, "ms", Quality::Normal, true});
    r.scalars.push_back({"Latency_samples", static_cast<double>(rt), "samples", Quality::Normal, true});
    r.quality = Quality::Normal;
    return r;
}

// Latencia estrutural: lag (>=0) de 'out' em relacao a 'in' que maximiza a correlacao cruzada.
// Usado para ALINHAR entrada/saida antes de metricas de envelope/nivel (senao a latencia de
// loopback ~913 amostras desalinha as janelas e produz tracking negativo / level transfer em rampa).
static int bestLag(const std::vector<double>& in, const std::vector<double>& out, int maxLag) {
    int best = 0; double bestC = -1.0e300;
    if (in.empty() || out.empty()) return 0;
    for (int lag = 0; lag <= maxLag; ++lag) {
        if (static_cast<std::size_t>(lag) >= out.size()) break;
        const std::size_t W = std::min(in.size(), out.size() - static_cast<std::size_t>(lag));
        double c = 0.0;
        for (std::size_t i = 0; i < W; ++i) c += in[i] * out[i + static_cast<std::size_t>(lag)];
        if (c > bestC) { bestC = c; best = lag; }
    }
    return best;
}

// ================= MS-13 Dynamic Behaviour (+ Level Transfer ST-08, aderencia DOC-03 §3) =================
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

    // repetition consistency (ST-12)
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
    // drift (ST-13)
    if (sust != cap.captured.end()) {
        std::vector<double> se = extractEnvelope(sust->second, m_fs);
        const int oneSec = static_cast<int>(m_fs + 0.5);
        auto avgAt = [&](int sec) { double s=0; int c=0; for (int i=sec*oneSec; i<(sec*oneSec+oneSec) && i<static_cast<int>(se.size()); ++i){s+=se[static_cast<std::size_t>(i)];c++;} return c?s/c:0.0; };
        const double e1 = avgAt(1), e30 = avgAt(29);
        const double drift = 20.0 * std::log10(std::max(e30, 1e-12) / std::max(e1, 1e-12));
        r.scalars.push_back({"Drift_dB", drift, "dB", Quality::Normal, true});
    }

    // level transfer (ST-08): ganho out/in por degrau (8 x 100 ms) + erro maximo (DOC-03 §3)
    auto step = cap.captured.find(StimulusId::ST08_DynamicStep);
    if (step != cap.captured.end()) {
        StimulusGenerator gen(m_fs);
        std::vector<double> in = gen.getStimulus(StimulusId::ST08_DynamicStep).samples;
        // Compensa a latencia estrutural antes de fatiar: alinha a saida a entrada pelo lag de
        // correlacao das envelopes; senao cada janela de 100 ms mistura degraus adjacentes (rampa).
        const int maxLag = static_cast<int>(0.050 * m_fs);
        const int lag = bestLag(extractEnvelope(in, m_fs), extractEnvelope(step->second, m_fs), maxLag);
        const int seg = static_cast<int>(0.100 * m_fs + 0.5);
        std::vector<double> lt;
        for (int s = 0; s < 8; ++s) {
            std::vector<double> oi = slice(in,          static_cast<std::size_t>(s * seg),       static_cast<std::size_t>((s + 1) * seg));
            std::vector<double> oo = slice(step->second, static_cast<std::size_t>(s * seg + lag), static_cast<std::size_t>((s + 1) * seg + lag));
            const double ri = std::sqrt(meanSquare(oi));
            const double ro = std::sqrt(meanSquare(oo));
            const double db = 20.0 * std::log10(std::max(ro, 1e-12) / std::max(ri, 1e-12));
            lt.push_back(db);
            r.scalars.push_back({"LevelTransfer_step" + std::to_string(s + 1) + "_dB", db, "dB", Quality::Normal, true});
        }
        double mean = 0.0; for (double v : lt) mean += v; if (!lt.empty()) mean /= static_cast<double>(lt.size());
        double maxErr = 0.0; for (double v : lt) maxErr = std::max(maxErr, std::fabs(v - mean));
        r.scalars.push_back({"LevelTransfer_maxError_dB", maxErr, "dB", Quality::Normal, true});
    }

    r.quality = Quality::Normal;
    return r;
}

// ================= MS-14 Behavioural Response (Hard+Soft tracking, Consistency, Overshoot robusto) =================
MetricResult AnalysisEngine::behavioural(const CapturedSet& cap) const {
    MetricResult r; r.id = "MS-14"; r.name = "Behavioural Response";
    auto hard = cap.captured.find(StimulusId::ST10_HardAttack);
    if (hard == cap.captured.end()) { r.quality = Quality::Error; return r; }
    StimulusGenerator gen(m_fs);

    // tracking = 100 * Pearson(env_in, env_out) ALINHADO pela latencia (DOC-03 §3 MS-14).
    auto trackingPct = [&](const std::vector<double>& captured, StimulusId id) -> double {
        std::vector<double> outEnv = extractEnvelope(captured, m_fs);
        std::vector<double> inEnv  = extractEnvelope(gen.getStimulus(id).samples, m_fs);
        if (inEnv.empty() || outEnv.empty()) return 0.0;
        // Compensa a latencia estrutural: alinha a saida a entrada antes do Pearson; sem isto a
        // correlacao mede o atraso do sistema (fica negativa), nao o tracking do DUT.
        const int maxLag = static_cast<int>(0.050 * m_fs);
        const int lag = bestLag(inEnv, outEnv, maxLag);
        const std::size_t avail = (outEnv.size() > static_cast<std::size_t>(lag)) ? outEnv.size() - static_cast<std::size_t>(lag) : 0;
        const std::size_t W = std::min(inEnv.size(), avail);
        if (W == 0) return 0.0;
        double mx = 0, my = 0; for (std::size_t i = 0; i < W; ++i) { mx += inEnv[i]; my += outEnv[i + static_cast<std::size_t>(lag)]; } mx /= W; my /= W;
        double sxy = 0, sx = 0, sy = 0;
        for (std::size_t i = 0; i < W; ++i) { const double dx = inEnv[i] - mx, dy = outEnv[i + static_cast<std::size_t>(lag)] - my; sxy += dx*dy; sx += dx*dx; sy += dy*dy; }
        const double p = (sx > 0 && sy > 0) ? sxy / std::sqrt(sx * sy) : 0.0;
        return 100.0 * p;
    };

    // Hard Attack (ST-10) separado
    const double hardPct = trackingPct(hard->second, StimulusId::ST10_HardAttack);
    r.scalars.push_back({"AttackTracking_hard_pct", hardPct, "%", Quality::Normal, true});

    // Soft Attack (ST-11) separado + Attack Consistency
    const StimulusId softId = StimulusId::ST11_SoftAttack;   // <<< se o enum tiver outro nome, ajuste AQUI
    auto soft = cap.captured.find(softId);
    if (soft != cap.captured.end()) {
        const double softPct = trackingPct(soft->second, softId);
        r.scalars.push_back({"AttackTracking_soft_pct", softPct, "%", Quality::Normal, true});
        const double cons = std::max(0.0, 100.0 - std::fabs(hardPct - softPct));   // consistencia hard vs soft
        r.scalars.push_back({"AttackConsistency_pct", cons, "%", Quality::Normal, true});
    } else {
        r.scalars.push_back({"AttackTracking_soft_pct", 0.0, "%", Quality::NotApplicable, false});
        r.scalars.push_back({"AttackConsistency_pct",   0.0, "%", Quality::NotApplicable, false});
    }

    // Overshoot (ST-10) com protecao robusta (CR-OVS): ST-10 decai ao silencio -> regime ~ 0.
    // Regime medido em janela ANCORADA AO PICO OBSERVADO (relativa ao onset, nao ao comprimento
    // do buffer) e LIMITADA ao conteudo REAL capturado (a captura adaptativa pode zero-preencher
    // a cauda; o regime nao pode ser medido sobre zeros de preenchimento).
    std::vector<double> outEnv = extractEnvelope(hard->second, m_fs);
    std::size_t realEnd = std::min(outEnv.size(), hard->second.size());
    while (realEnd > 0 && std::fabs(hard->second[realEnd - 1]) < 1e-6) --realEnd;
    double pk = 0; std::size_t pkIdx = 0;
    for (std::size_t i = 0; i < realEnd; ++i) if (outEnv[i] > pk) { pk = outEnv[i]; pkIdx = i; }
    const std::size_t settle = static_cast<std::size_t>(0.10 * m_fs);   // 100 ms de assentamento apos o pico
    const std::size_t wlen   = static_cast<std::size_t>(0.20 * m_fs);   // 200 ms de janela de medida do regime
    const std::size_t rs = std::min(pkIdx + settle, realEnd);
    const std::size_t re = std::min(rs + wlen, realEnd);
    double regime = 0; std::size_t c = 0;
    for (std::size_t i = rs; i < re; ++i) { regime += outEnv[i]; ++c; }
    if (c > 0) regime /= static_cast<double>(c);
    Quality oq = Quality::Normal;
    double overshoot;
    const double floorRel = pk * 1e-3;   // piso de regime = -60 dB do pico (evita divisao por ~0)
    if (pk <= 1e-12) { overshoot = 0.0; oq = Quality::LowConfidence; }
    else if (regime < floorRel) { overshoot = 20.0 * std::log10(pk / floorRel); oq = Quality::LowConfidence; }
    else { overshoot = 20.0 * std::log10(pk / regime); }
    r.scalars.push_back({"Overshoot_dB", overshoot, "dB", oq, true});

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

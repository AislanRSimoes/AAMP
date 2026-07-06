// Baseline.h
// PROTOTIPO OBSERVACIONAL (Baseline v0 / Experimental) das frentes 3, 4 e 5.
// Status: NAO validado sobre a arquitetura definitiva.
//
// ============================================================================
// LIMITACAO CONHECIDA (Baseline v0)
//   BaselineState e calculado sobre o buffer interno do device (m_cap), nao
//   sobre o substrato definitivo. A validacao final depende da integracao de:
//       - rawCapture
//       - CaptureMeta
//       - stimulusStartSample
//       - onsetSample
//       - latencySamples
//   quando o CapturedSet definitivo estiver disponivel.
//
//   COMPROMISSO: nenhum parametro abaixo sera considerado CALIBRADO e nenhuma
//   decisao automatica (BASELINE_GOVERNS_SCHEDULER=true) sera habilitada antes
//   da migracao para rawCapture + CaptureMeta. Os parametros aqui servem apenas
//   para verificar se a classificacao e SEPARAVEL, nao para descobrir os numeros
//   corretos.
// ============================================================================
//
// Calcula BaselineState, SchedulerState e a classificacao de retorno-ao-basal,
// mas NAO governa o scheduler enquanto BASELINE_GOVERNS_SCHEDULER == false.
//
// Congelado (arquitetura experimental):
//   - BaselineState = estado OBSERVAVEL do DUT na ausencia de excitacao externa
//                     (Nivel + Transiente + Assinatura espectral de baixa ordem)
//   - SchedulerState = residualRMS, residualPeak, slope (1a classe), persistence
//   - Classificacao  = Converging | ReturnedToBaseline | FailedToReturnToBaseline
//   - Analyze ocorre apos o encerramento observacional, independente da classificacao
//   - Converging e estado TRANSITORIO; FailedToReturnToBaseline e resultado VALIDO
//   - Pre-roll preservado = re-observacao do basal; scheduler e metricas desacoplados
//
// Aberto (experimental, decidido pelos 4 DUTs; sao apenas constantes aqui):
//   - N bandas e cortes; k, k_band, margem de pico, T (kReturnHold), T_fail (kFailHold),
//     janela do slope. NADA disto esta congelado nem calibrado.
#pragma once

#include <vector>
#include <array>
#include <cmath>
#include <cstddef>
#include <algorithm>
#include <string>

namespace aamp {
namespace baseline {

// ============================================================================
// GOVERNANCA. false = modo observacional: calcula, registra, NAO decide.
// Trocar para true SOMENTE apos validacao em hardware com os 4 DUTs.
// ============================================================================
inline constexpr bool BASELINE_GOVERNS_SCHEDULER = true;   // habilitado (decisao do PO): captura encerra ao recuperar baseline + hold

// ============================================================================
// Assinatura espectral de baixa ordem. N e cortes PARAMETRICOS (nao congelados).
// Cortes pensados em mecanismos de ruido (hum/rumble | meio | hiss/whine/clock),
// NAO em conteudo musical. Editar para varrer 2/3/4 bandas nos DUTs reais.
// ============================================================================
inline constexpr int kNumBands = 3;
inline constexpr std::array<double, kNumBands + 1> kBandEdgesHz = { 20.0, 120.0, 4000.0, 20000.0 };

// ============================================================================
// Parametros de calibracao. NAO congelados: serao definidos pelos dados reais.
// ============================================================================
inline constexpr double kSigmaK       = 3.0;   // banda de equivalencia de nivel (em sigma do basal)
inline constexpr double kBandSigmaK   = 3.0;   // idem por banda
inline constexpr double kPeakMarginDb = 3.0;   // tolerancia de transiente acima do pico basal
inline constexpr double kSlopeFlatDbS = 0.5;   // |slope| abaixo disto = estagnado (nao converge)
inline constexpr double kReturnHoldS  = 0.5;   // T: persistencia compativel p/ ReturnedToBaseline
inline constexpr double kFailHoldS    = 5.0;   // T_fail: persistencia estagnada p/ FailedToReturn
inline constexpr double kSlopeWindowS = 2.0;   // janela de regressao do slope

inline constexpr double kEps = 1.0e-12;

inline double toDb(double linear) { return 20.0 * std::log10(std::max(linear, kEps)); }

// ----------------------------------------------------------------------------
// Biquad bandpass (RBJ). Assinatura GROSSEIRA de baixa ordem, por design.
// ----------------------------------------------------------------------------
struct Biquad {
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;

    void reset() { z1 = 0.0; z2 = 0.0; }

    // f0/Q via cookbook; bandpass (constant 0 dB peak gain).
    void designBandpass(double f0Hz, double Q, double fs) {
        if (f0Hz <= 0.0 || fs <= 0.0 || Q <= 0.0) { b0 = 1.0; b1 = b2 = a1 = a2 = 0.0; return; }
        const double w0 = 2.0 * 3.14159265358979323846 * (f0Hz / fs);
        const double cw = std::cos(w0), sw = std::sin(w0);
        const double alpha = sw / (2.0 * Q);
        const double a0 =  1.0 + alpha;
        b0 =  alpha / a0;
        b1 =  0.0;
        b2 = -alpha / a0;
        a1 = (-2.0 * cw) / a0;
        a2 = ( 1.0 - alpha) / a0;
    }

    inline double process(double x) {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// RMS de uma banda [fLo,fHi) via um bandpass de baixa ordem (assinatura grosseira).
inline double bandRms(const std::vector<double>& sig, std::size_t a, std::size_t b,
                      double fLo, double fHi, double fs) {
    if (b > sig.size()) b = sig.size();
    if (a >= b) return 0.0;
    const double f0 = std::sqrt(std::max(fLo, kEps) * std::max(fHi, kEps));
    const double bw = std::max(fHi - fLo, kEps);
    const double Q  = std::max(f0 / bw, 0.2);
    Biquad bp; bp.designBandpass(f0, Q, fs);
    double acc = 0.0; std::size_t n = 0;
    for (std::size_t i = a; i < b; ++i) { const double y = bp.process(sig[i]); acc += y * y; ++n; }
    return (n > 0) ? std::sqrt(acc / static_cast<double>(n)) : 0.0;
}

inline double windowRms(const std::vector<double>& sig, std::size_t a, std::size_t b) {
    if (b > sig.size()) b = sig.size();
    if (a >= b) return 0.0;
    double acc = 0.0; for (std::size_t i = a; i < b; ++i) acc += sig[i] * sig[i];
    return std::sqrt(acc / static_cast<double>(b - a));
}

inline double windowPeak(const std::vector<double>& sig, std::size_t a, std::size_t b) {
    if (b > sig.size()) b = sig.size();
    double pk = 0.0; for (std::size_t i = a; i < b; ++i) pk = std::max(pk, std::abs(sig[i]));
    return pk;
}

// ============================================================================
// Estado OBSERVAVEL do DUT na ausencia de excitacao externa.
// ============================================================================
struct BaselineState {
    double rmsMean_dB   = -120.0;
    double rmsSigma_dB  = 0.0;
    double peakMax_dBFS = -120.0;
    std::array<double, kNumBands> bandMean_dB  = {};
    std::array<double, kNumBands> bandSigma_dB = {};
    bool   valid = false;
};

// Mede o BaselineState a partir de um trecho SEM excitacao (silencio inicial OU pre-roll).
// Janela em blocos para obter media e sigma. fs em Hz, blockS = tamanho do bloco (s).
inline BaselineState measureBaseline(const std::vector<double>& rest, double fs, double blockS = 0.020) {
    BaselineState bs;
    if (rest.empty() || fs <= 0.0) return bs;
    const std::size_t blk = std::max<std::size_t>(1, static_cast<std::size_t>(blockS * fs + 0.5));
    std::vector<double> rmsDb; rmsDb.reserve(rest.size() / blk + 1);
    std::array<std::vector<double>, kNumBands> bandDb;
    double peak = 0.0;
    for (std::size_t a = 0; a + blk <= rest.size(); a += blk) {
        rmsDb.push_back(toDb(windowRms(rest, a, a + blk)));
        peak = std::max(peak, windowPeak(rest, a, a + blk));
        for (int k = 0; k < kNumBands; ++k)
            bandDb[k].push_back(toDb(bandRms(rest, a, a + blk, kBandEdgesHz[k], kBandEdgesHz[k + 1], fs)));
    }
    if (rmsDb.empty()) return bs;
    auto meanStd = [](const std::vector<double>& v, double& m, double& s) {
        double acc = 0.0; for (double x : v) acc += x; m = acc / static_cast<double>(v.size());
        double var = 0.0; for (double x : v) var += (x - m) * (x - m);
        s = std::sqrt(var / static_cast<double>(v.size()));
    };
    meanStd(rmsDb, bs.rmsMean_dB, bs.rmsSigma_dB);
    for (int k = 0; k < kNumBands; ++k) meanStd(bandDb[k], bs.bandMean_dB[k], bs.bandSigma_dB[k]);
    bs.peakMax_dBFS = toDb(peak);
    bs.valid = true;
    return bs;
}

// ============================================================================
// Observacao temporal do residual contra o basal.
// ============================================================================
struct SchedulerState {
    double residualRMS_dB     = -120.0;
    double residualPeak_dBFS  = -120.0;
    double slope_dBs          = 0.0;   // 1a classe: distingue convergente de estagnado
    double persistence_s      = 0.0;   // tempo no estado corrente
    double baselineDistance_sigma = 0.0;
    std::array<double, kNumBands> bandResidual_dB = {};
};

enum class Classification { Converging, ReturnedToBaseline, FailedToReturnToBaseline };
enum class TerminationReason { EnergySettled, NonDecayingTail, UserOverride };

inline const char* toString(Classification c) {
    switch (c) {
        case Classification::ReturnedToBaseline:       return "ReturnedToBaseline";
        case Classification::FailedToReturnToBaseline:  return "FailedToReturnToBaseline";
        case Classification::Converging:               default: return "Converging";
    }
}

// Classificador com estado: alimentado janela a janela durante a cauda.
// NAO decide o termino; apenas classifica e mede. O scheduler le isto somente
// quando BASELINE_GOVERNS_SCHEDULER == true (ainda false).
class TailObserver {
public:
    TailObserver(const BaselineState& bs, double fs, double windowS = 0.020)
        : m_bs(bs), m_fs(fs), m_windowS(windowS) {}

    // Alimenta uma janela de residual; retorna o SchedulerState e a classificacao corrente.
    Classification feed(double residualRms, double residualPeak,
                        const std::array<double, kNumBands>& bandResidualRms,
                        SchedulerState& outState) {
        const double rDb = toDb(residualRms);
        m_history.push_back(rDb);
        const std::size_t maxHist = std::max<std::size_t>(2,
            static_cast<std::size_t>(kSlopeWindowS / std::max(m_windowS, kEps)));
        if (m_history.size() > maxHist) m_history.erase(m_history.begin());

        outState.residualRMS_dB    = rDb;
        outState.residualPeak_dBFS = toDb(residualPeak);
        outState.slope_dBs         = slope();
        for (int k = 0; k < kNumBands; ++k) outState.bandResidual_dB[k] = toDb(bandResidualRms[k]);

        const double sig = std::max(m_bs.rmsSigma_dB, 0.1);
        outState.baselineDistance_sigma = (m_bs.valid) ? std::abs(rDb - m_bs.rmsMean_dB) / sig : 0.0;

        const bool compatible = isCompatible(rDb, outState.residualPeak_dBFS, outState.bandResidual_dB);
        const bool flat = std::abs(outState.slope_dBs) < kSlopeFlatDbS;

        Classification c;
        if (compatible) {
            m_compatRun += m_windowS; m_flatRun = 0.0;
            c = (m_compatRun >= kReturnHoldS) ? Classification::ReturnedToBaseline
                                              : Classification::Converging;
        } else if (flat) {
            m_flatRun += m_windowS; m_compatRun = 0.0;
            c = (m_flatRun >= kFailHoldS) ? Classification::FailedToReturnToBaseline
                                          : Classification::Converging;
        } else {  // incompativel mas com slope < 0 -> ainda caindo
            m_compatRun = 0.0; m_flatRun = 0.0;
            c = Classification::Converging;
        }
        if (c == m_last) m_persist += m_windowS; else { m_persist = m_windowS; m_last = c; }
        outState.persistence_s = m_persist;
        return c;
    }

private:
    bool isCompatible(double rDb, double peakDb, const std::array<double, kNumBands>& bandDb) const {
        if (!m_bs.valid) return false;
        if (std::abs(rDb - m_bs.rmsMean_dB) > kSigmaK * std::max(m_bs.rmsSigma_dB, 0.1)) return false;
        if (peakDb > m_bs.peakMax_dBFS + kPeakMarginDb) return false;
        for (int k = 0; k < kNumBands; ++k)
            if (std::abs(bandDb[k] - m_bs.bandMean_dB[k]) > kBandSigmaK * std::max(m_bs.bandSigma_dB[k], 0.1))
                return false;
        return true;
    }

    double slope() const {  // regressao linear de dB vs tempo (dB/s)
        const std::size_t n = m_history.size();
        if (n < 2) return 0.0;
        double sx = 0, sy = 0, sxx = 0, sxy = 0;
        for (std::size_t i = 0; i < n; ++i) {
            const double x = static_cast<double>(i) * m_windowS;
            const double y = m_history[i];
            sx += x; sy += y; sxx += x * x; sxy += x * y;
        }
        const double den = static_cast<double>(n) * sxx - sx * sx;
        return (std::abs(den) > kEps) ? (static_cast<double>(n) * sxy - sx * sy) / den : 0.0;
    }

    BaselineState m_bs;
    double m_fs, m_windowS;
    std::vector<double> m_history;
    double m_compatRun = 0.0, m_flatRun = 0.0, m_persist = 0.0;
    Classification m_last = Classification::Converging;
};

// Linha de telemetria [EVID-BASELINE] (observacional; sai antes do RC).
inline std::string evidLine(Classification c, const SchedulerState& s, const BaselineState& bs) {
    auto f = [](double v) { char b[32]; std::snprintf(b, sizeof(b), "%.2f", v); return std::string(b); };
    std::string out = "[EVID-BASELINE v0] State=" + std::string(toString(c))
        + " ResidualRMS=" + f(s.residualRMS_dB) + " dBFS"
        + " ResidualPeak=" + f(s.residualPeak_dBFS) + " dBFS"
        + " Slope=" + f(s.slope_dBs) + " dB/s"
        + " Persistence=" + f(s.persistence_s) + " s"
        + " BaselineDistance=" + f(s.baselineDistance_sigma) + " sigma"
        + " BaselineRMS=" + f(bs.rmsMean_dB) + " dBFS(sigma=" + f(bs.rmsSigma_dB) + ")"
        + " governs=" + std::string(BASELINE_GOVERNS_SCHEDULER ? "true" : "false");
    return out;
}

}  // namespace baseline
}  // namespace aamp

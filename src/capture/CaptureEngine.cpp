// AAMP - src/capture/CaptureEngine.cpp
#include "../export/ExportEngine.h"
#include "CaptureEngine.h"
#include "../common/Constants.h"
#include <cmath>
#include <algorithm>
#include <fstream>   // DIAG TEMPORARIO
#include <cstdlib>   // DIAG TEMPORARIO
#include <string>    // DIAG TEMPORARIO
namespace aamp {

// FR-018: habilita o alinhamento temporal (deslocamento do buffer capturado pelo offset medido).
// FASE DE MEDICAO: mantido FALSE -> NENHUM buffer e alterado. Primeiro provamos que o atraso
// e estrutural (constante entre estimulos) via correlacao cruzada; so depois ligar para true.
static constexpr bool ENABLE_SYNC_ALIGNMENT = false;

// Aquisicao completa (arquitetura final, nao e toggle): captura sempre ate um teto de seguranca
// e MEDE a duracao real da resposta (decaimento da energia residual), sem truncar a observacao.
static constexpr double MAX_TAIL_SECONDS     = 8.0;    // teto p/ sistemas nao-decaentes (freeze/oscilacao)
static constexpr double DECAY_FLOOR_FACTOR   = 3.0;    // limiar = 3x o piso de ruido (~ +9.5 dB)

// ---- DIAG TEMPORARIO: append no mesmo diag_log.txt do device (plain C++, sem JUCE) ----
static void m04log(const std::string& s) {
#ifdef _WIN32
    char* base = nullptr; std::size_t len = 0;
    if (_dupenv_s(&base, &len, "APPDATA") != 0 || base == nullptr) return;
    const std::string path = std::string(base) + "\\AAMP\\diag_log.txt";
    std::free(base);
#else
    const char* base = std::getenv("APPDATA");
    if (base == nullptr) return;
    const std::string path = std::string(base) + "/AAMP/diag_log.txt";
#endif
    std::ofstream f(path, std::ios::app);
    if (f) f << s << "\n";
}

static double rmsOf(const std::vector<double>& x) {
    if (x.empty()) return 0.0;
    double s = 0.0; for (double v : x) s += v * v;
    return std::sqrt(s / static_cast<double>(x.size()));
}
CaptureEngine::CaptureEngine(double fs, double clipThresholdDbfs)
    : m_fs(fs), m_clipThreshold(std::pow(10.0, clipThresholdDbfs / 20.0)) {}
bool CaptureEngine::detectClipping(const std::vector<double>& x) const {
    for (double v : x) if (std::fabs(v) > m_clipThreshold) return true;
    return false;
}
bool CaptureEngine::detectLowSignal(const std::vector<double>& x, double noiseFloorRms) const {
    if (noiseFloorRms <= 0.0) return false;            // sem referencia de ruido -> nao avalia
    const double r = rmsOf(x);
    if (r <= 0.0) return true;
    const double snrDb = 20.0 * std::log10(r / noiseFloorRms);
    return snrDb < k::MIN_SNR_OVER_FLOOR_DB;           // < 20 dB acima do piso (DOC-07 sec.5)
}
int CaptureEngine::measureSyncOffset(const std::vector<double>& captured, double fs) {
    // Para ST-06, o pico capturado relativo a posicao de emissao round(0.5*fs) e o offset.
    int pk = 0; double pv = 0.0;
    for (int i = 0; i < static_cast<int>(captured.size()); ++i)
        if (std::fabs(captured[static_cast<std::size_t>(i)]) > pv) { pv = std::fabs(captured[static_cast<std::size_t>(i)]); pk = i; }
    return pk - k::impulsePositionSamples(fs);
}
CaptureResult CaptureEngine::capture(const Stimulus& stim, IAudioDevice& dev,
                                     double noiseFloorRms) const {
    CaptureResult r;
    // Janela de captura: estimulo + teto de seguranca, observando a RESPOSTA COMPLETA do DUT.
    // Arquitetura final (nao e toggle): sempre observa a janela inteira; a duracao real e apenas
    // MEDIDA apos a captura (decaimento da energia residual), nunca truncada.
    const std::size_t post = static_cast<std::size_t>(MAX_TAIL_SECONDS * m_fs + 0.5);
    const std::size_t captureLen = stim.samples.size() + post;
    std::vector<double> raw = dev.playAndCapture(stim.samples, captureLen);
	
	// ===== EXP-RAW =====
if (stim.id == StimulusId::ST02_MultiTone)
{
    ExportEngine ex("DEBUG");

#ifdef _WIN32
    char* base = nullptr;
    std::size_t len = 0;

    if (_dupenv_s(&base, &len, "APPDATA") == 0 && base != nullptr)
    {
        std::string path =
            std::string(base) +
            "\\AAMP\\raw_capture_st02.wav";

        ex.exportWAV(raw, m_fs, 1, path);

        std::free(base);
    }
#endif
}
// ===== fim EXP-RAW =====
	
    // Captura governada por energia: o device retorna estimulo + decaimento real (<= captureLen),
    // sem a cauda vazia. So e PARCIAL (ER-024) se nem o estimulo foi coberto (abort/underrun real).
    if (raw.size() < stim.samples.size()) {
        m04log("M04 stimId=" + std::to_string(static_cast<int>(stim.id))
               + " stimSize=" + std::to_string(stim.samples.size())
               + " capLen=" + std::to_string(captureLen)
               + " rawSize=" + std::to_string(raw.size())
               + " *** PARCIAL (nao processado) ***");
        r.status.partial = true;
        r.status.valid = false;
        return r;  // samples vazio
    }
    r.status.clipped   = detectClipping(raw);           // ER-020
    r.status.lowSignal = detectLowSignal(raw, noiseFloorRms); // ER-023 (nao bloqueante)
    r.status.valid     = !r.status.clipped && !r.status.partial;

    // --- FR-018: medir o offset de sincronizacao no ST-06 (impulso) e guardar p/ os demais ---
    if (stim.id == StimulusId::ST06_Impulse) {
        const int measured = measureSyncOffset(raw, m_fs);   // pico - posicao do impulso emitido
        r.syncOffsetSamples = measured;
        m_syncOffset   = std::max(0, measured);              // offset do sistema (round-trip)
        m_syncMeasured = true;
    }

    r.samples = std::move(raw);

    // --- FR-018: aplicar o deslocamento ao buffer capturado ANTES de entregar a M-05 ---
    // Exclusoes: ST-06 (referencia de latencia de MS-12) e ST-09 (silencio: alinhar nao ajuda
    // e protege Noise Floor / SNR). Os 5 espectrais (ST-01..05) sao capturados ANTES do ST-06,
    // entao m_syncMeasured ainda e false -> nao sao deslocados (THD/IMD/FR intactos).
    int applied = 0;
    const bool excluded = (stim.id == StimulusId::ST06_Impulse) || (stim.id == StimulusId::ST09_Silence);
    if (ENABLE_SYNC_ALIGNMENT && m_syncMeasured && m_syncOffset > 0 && !excluded) {
        applied = m_syncOffset;
        const std::size_t n   = r.samples.size();
        const std::size_t off = static_cast<std::size_t>(applied);
        std::vector<double> aligned(n, 0.0);
        if (off < n)
            std::copy(r.samples.begin() + static_cast<long>(off), r.samples.end(), aligned.begin());
        r.samples.swap(aligned);   // shift-left por 'applied'; zero-pad no fim; tamanho preservado
    }

    // --- Medir a duracao REAL da resposta observada (decaimento da energia residual). ---
    // OBSERVACAO apenas: registra observedDurationSamples/nonDecayed/decay*; NUNCA trunca r.samples.
    {
        const int blk = std::max(1, static_cast<int>(0.020 * m_fs));   // blocos de 20 ms
        const std::size_t stimEnd = std::min(stim.samples.size(), r.samples.size());
        double tailPeak = 0.0;
        for (std::size_t i = stimEnd; i < r.samples.size(); ++i) tailPeak = std::max(tailPeak, std::fabs(r.samples[i]));
        double thr = (noiseFloorRms > 0.0) ? noiseFloorRms * DECAY_FLOOR_FACTOR : tailPeak * 1.0e-3;
        std::vector<double> blkRms;
        int lastAbove = -1, bi = 0;
        for (std::size_t s = stimEnd; s + static_cast<std::size_t>(blk) <= r.samples.size(); s += static_cast<std::size_t>(blk), ++bi) {
            double e = 0.0;
            for (int i = 0; i < blk; ++i) { const double v = r.samples[s + static_cast<std::size_t>(i)]; e += v * v; }
            const double rms = std::sqrt(e / static_cast<double>(blk));
            blkRms.push_back(rms);
            if (rms > thr) lastAbove = bi;   // ultimo bloco acima do limiar: robusto a nulls (tremolo/gating)
        }
        const int observedTail = (lastAbove >= 0) ? (lastAbove + 1) * blk : 0;
        r.observedDurationSamples = static_cast<int>(stimEnd) + observedTail;
        r.nonDecayed = (lastAbove >= 0) && (static_cast<std::size_t>(lastAbove + 1) >= blkRms.size());
        auto decayMark = [&](double dB) -> int {
            const double lvl = tailPeak * std::pow(10.0, dB / 20.0);
            for (std::size_t z = 0; z < blkRms.size(); ++z) if (blkRms[z] <= lvl) return static_cast<int>(z) * blk;
            return -1;
        };
        r.decay20 = decayMark(-20.0); r.decay40 = decayMark(-40.0); r.decay60 = decayMark(-60.0);
        m04log("M04-ADAPT stimId=" + std::to_string(static_cast<int>(stim.id))
               + " observedTail=" + std::to_string(observedTail)
               + " observedTotal=" + std::to_string(r.observedDurationSamples)
               + " nonDecayed=" + std::to_string(r.nonDecayed ? 1 : 0)
               + " d20=" + std::to_string(r.decay20)
               + " d40=" + std::to_string(r.decay40)
               + " d60=" + std::to_string(r.decay60));
    }

    // ---- DIAG TEMPORARIO: fronteira M-04 + FR-018 (offset medido / aplicado) ----
    {
        int onset = -1;
        for (std::size_t i = 0; i < r.samples.size(); ++i)
            if (std::fabs(r.samples[i]) > 1.0e-3) { onset = static_cast<int>(i); break; }
        const int measuredShown = m_syncMeasured ? m_syncOffset : 0;
        m04log("M04 stimId=" + std::to_string(static_cast<int>(stim.id))
               + " stimSize=" + std::to_string(stim.samples.size())
               + " capLen=" + std::to_string(captureLen)
               + " rawSize=" + std::to_string(r.samples.size())
               + " onset=" + std::to_string(onset)
               + " | Offset medido = " + std::to_string(measuredShown) + " samples"
               + " | Offset aplicado = " + std::to_string(applied) + " samples"
               + (excluded ? "  (excluido do alinhamento)" : "")
               + (r.status.clipped ? "  CLIP" : ""));
    }
    return r;
}
AverageResult CaptureEngine::coherentAverage(const Stimulus& stim, IAudioDevice& dev,
                                             int targetValid, int maxRetries,
                                             double noiseFloorRms) const {
    AverageResult out;
    std::vector<std::vector<double>> valids;
    int attempts = 0;
    const int maxAttempts = targetValid + maxRetries;
    while (static_cast<int>(valids.size()) < targetValid && attempts < maxAttempts) {
        ++attempts;
        CaptureResult c = capture(stim, dev, noiseFloorRms);
        if (c.status.valid) valids.push_back(c.samples);
        else                out.invalidDiscarded++;     // descartada e substituida
    }
    if (static_cast<int>(valids.size()) < targetValid) { out.aborted = true; return out; }
    // Media coerente no dominio do tempo (DOC-03 sec.4.3).
    const std::size_t L = valids.front().size();
    out.samples.assign(L, 0.0);
    for (const auto& v : valids)
        for (std::size_t i = 0; i < L && i < v.size(); ++i) out.samples[i] += v[i];
    for (double& s : out.samples) s /= static_cast<double>(valids.size());
    out.validUsed = static_cast<int>(valids.size());
    return out;
}
} // namespace aamp

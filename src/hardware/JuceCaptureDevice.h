// AAMP - src/hardware/JuceCaptureDevice.h
// SCAFFOLDING DE MAQUINA-ALVO (nao compilado/verificado neste ambiente headless: requer JUCE).
// RELOCACAO do bloco REAL de play+capture do Gerador de Estimulos (Main.cpp: JuceCaptureDevice),
// tornado reutilizavel para o Laboratorio via AampWebBridge. Codigo identico ao ja validado — apenas
// movido para namespace aamp e para um header, para ser consumido tambem pela ponte (M-08).
// Reproduz e captura cada estimulo gerado por StimulusGenerator (M-03) atraves do AudioDeviceManager
// configurado (dispositivo/modo/canais) pela mesma logica do AudioConfig.
#pragma once
#if defined(AAMP_WITH_JUCE)
#include <juce_audio_devices/juce_audio_devices.h>
#include "../capture/IAudioDevice.h"
#include "../capture/Baseline.h"
#include "../common/Constants.h"
#include <atomic>
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>

namespace aamp {

// diag_log.txt: mesmo destino/comportamento do Gerador (static => linkage interno; sem colisao ODR).
namespace capture_detail {
inline juce::File diagFile() {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("AAMP").getChildFile("diag_log.txt");
}
inline void diagLog(const juce::String& line) { diagFile().appendText(line + "\n"); }
} // namespace capture_detail
using capture_detail::diagLog;

// ---------- IAudioDevice real (com flag de aborto) — VERBATIM de Main.cpp ----------
class JuceCaptureDevice : public IAudioDevice, private juce::AudioIODeviceCallback {
public:
    JuceCaptureDevice(juce::AudioDeviceManager& dm, double fs, const std::atomic<bool>* abort)
        : m_dm(dm), m_fs(fs), m_abort(abort) {}
    double sampleRate() const override { return m_fs; }
    int inputChannels() const override { return 1; }
    int outputChannels() const override { return 1; }
    std::vector<double> playAndCapture(const std::vector<double>& stimulus, std::size_t captureLen) override {
        m_stim.resize(stimulus.size());
        for (size_t i = 0; i < stimulus.size(); ++i) m_stim[i] = (float) stimulus[i];
        m_preRoll = (int) (kPreRollSeconds * (m_fs > 0.0 ? m_fs : 48000.0));
        m_total = m_preRoll + (int) captureLen;   // pre-roll + estimulo + teto de cauda
        m_cap.assign((size_t) juce::jmax(1, m_total), 0.0f);
        m_playPos.store(0); m_capPos.store(0); m_done.store(false);
        m_floor = 0.0; m_floorDone = false; m_belowRun = 0; m_capturedEnd = m_total; m_endByEnergy = false;
        m_dm.addAudioCallback(this);
        const double secs = (double) m_total / (m_fs > 0.0 ? m_fs : 48000.0);
        const int timeoutMs = (int) (secs * 1000.0) + 2000;
        const auto t0 = juce::Time::getMillisecondCounter();
        while (!m_done.load()) {
            if (m_abort != nullptr && m_abort->load()) break;
            juce::Thread::sleep(5);
            if ((int) (juce::Time::getMillisecondCounter() - t0) > timeoutMs) break;
        }
        m_dm.removeAudioCallback(this);
        m_capturedEnd = juce::jlimit(m_preRoll, m_total, m_capPos.load());
        const int outLen = juce::jmax(0, m_capturedEnd - m_preRoll);   // strip do pre-roll (alinhamento da analise intacto)

        // DIAG + evidencia de governanca por energia.
        float cpk = 0.0f; double cs = 0.0; int peakIdx = -1, onsetIdx = -1;
        for (int i = 0; i < outLen; ++i) {
            const float a = std::abs(m_cap[(size_t)(m_preRoll + i)]);
            if (a > cpk) { cpk = a; peakIdx = i; }
            cs += (double) a * a;
            if (onsetIdx < 0 && a > 0.01f) onsetIdx = i;
        }
        const double cRms = (outLen > 0) ? std::sqrt(cs / (double) outLen) : 0.0;
        diagLog("stim#" + juce::String(m_callIndex)
                + " preRoll=" + juce::String(m_preRoll)
                + " stim=" + juce::String((int) m_stim.size())
                + " tetoCaptura=" + juce::String(m_total - m_preRoll)
                + " capturadoEfetivo=" + juce::String(outLen)
                + " piso=" + juce::String(m_floor, 6)
                + " capPk=" + juce::String(cpk, 4) + " capRMS=" + juce::String(cRms, 4)
                + " peakIdx=" + juce::String(peakIdx) + " onsetIdx=" + juce::String(onsetIdx)
                + " FIM=" + juce::String(m_endByEnergy ? "ENERGIA(decaimento+hold) -> transicao autorizada"
                                                       : "TIMEOUT(nao-decaente) -> teto de seguranca")
                + (cpk > 0.99f ? "  *** CLIP ***" : ""));
        ++m_callIndex;

        // ===== Observacao do basal (MODO OBSERVACIONAL: mede, classifica, registra; NAO decide) =====
        {
            using namespace aamp::baseline;
            // Basal = estado OBSERVAVEL do DUT na ausencia de excitacao -> medido no pre-roll desta captura.
            std::vector<double> rest((size_t) juce::jmax(0, m_preRoll), 0.0);
            for (int i = 0; i < m_preRoll; ++i) rest[(size_t) i] = (double) m_cap[(size_t) i];
            BaselineState bs = measureBaseline(rest, m_fs);

            // Filtros de banda persistentes (estado corre ao longo da cauda; assinatura grosseira).
            Biquad bank[kNumBands];
            for (int k = 0; k < kNumBands; ++k) {
                const double f0 = std::sqrt(kBandEdgesHz[(size_t) k] * kBandEdgesHz[(size_t) k + 1]);
                const double bw = std::max(kBandEdgesHz[(size_t) k + 1] - kBandEdgesHz[(size_t) k], 1.0e-12);
                bank[k].designBandpass(f0, std::max(f0 / bw, 0.2), m_fs);
            }
            // Varre a CAUDA (apos a reproducao do estimulo) em janelas de 20 ms.
            const int stimEnd = m_preRoll + (int) m_stim.size();
            const int win = juce::jmax(1, (int) (0.020 * m_fs));
            TailObserver obs(bs, m_fs, 0.020);
            Classification last = Classification::Converging;
            SchedulerState st;
            std::array<double, kNumBands> bands{};
            int emitted = 0;
            for (int a = stimEnd; a + win <= m_capturedEnd; a += win) {
                double sq = 0.0, pk = 0.0;
                std::array<double, kNumBands> bsq{};
                for (int i = a; i < a + win; ++i) {
                    const double v = (double) m_cap[(size_t) i];
                    sq += v * v; pk = std::max(pk, std::abs(v));
                    for (int k = 0; k < kNumBands; ++k) { const double y = bank[k].process(v); bsq[(size_t) k] += y * y; }
                }
                const double rms = std::sqrt(sq / (double) win);
                for (int k = 0; k < kNumBands; ++k) bands[(size_t) k] = std::sqrt(bsq[(size_t) k] / (double) win);
                last = obs.feed(rms, pk, bands, st);
                if ((emitted % 12) == 0) diagLog(juce::String(evidLine(last, st, bs).c_str()));  // ~a cada 250 ms
                ++emitted;
            }
            diagLog(juce::String(evidLine(last, st, bs).c_str()) + " (final)");
        }

        std::vector<double> out((size_t) outLen, 0.0);
        for (int i = 0; i < outLen; ++i) out[(size_t) i] = (double) m_cap[(size_t)(m_preRoll + i)];
        return out;
    }
private:
    void audioDeviceIOCallbackWithContext(const float* const* in, int numIn,
                                          float* const* out, int numOut, int numSamples,
                                          const juce::AudioIODeviceCallbackContext&) override {
        for (int ch = 0; ch < numOut; ++ch) if (out[ch]) juce::FloatVectorOperations::clear(out[ch], numSamples);
        if (m_done.load()) return;
        const int pp = m_playPos.load();
        // Saida: silencio no pre-roll, depois o estimulo (offset por m_preRoll), depois silencio (cauda).
        for (int ch = 0; ch < numOut; ++ch)
            if (out[ch])
                for (int n = 0; n < numSamples; ++n) {
                    const int s = (pp + n) - m_preRoll;   // posicao dentro do estimulo
                    out[ch][n] = (s >= 0 && s < (int) m_stim.size()) ? m_stim[(size_t) s] : 0.0f;
                }
        m_playPos.store(pp + numSamples);

        const int cp = m_capPos.load();
        int inCh = -1; for (int ch = 0; ch < numIn; ++ch) if (in[ch]) { inCh = ch; break; }
        double blkSq = 0.0; int blkN = 0;
        if (inCh >= 0)
            for (int n = 0; n < numSamples; ++n) {
                const int idx = cp + n;
                if (idx < m_total) {
                    const float v = in[inCh][n];
                    m_cap[(size_t) idx] = v;
                    blkSq += (double) v * v; ++blkN;
                }
            }
        m_capPos.store(cp + numSamples);

        // Piso de ruido medido no PRE-ROLL (captura antes do estimulo): RMS de [0, preRoll).
        if (!m_floorDone && cp + numSamples >= m_preRoll) {
            double sq = 0.0; int c = 0;
            for (int i = 0; i < m_preRoll && i < m_total; ++i) { const double v = m_cap[(size_t) i]; sq += v * v; ++c; }
            m_floor = (c > 0) ? std::sqrt(sq / (double) c) : 0.0;
            m_floorDone = true;
        }
        // Fim por ENERGIA (governanca da transicao): apos a reproducao do estimulo, encerra quando a
        // energia residual fica abaixo de k*PISO pelo hold. Relativo ao PISO -> preserva TODO o
        // decaimento acima do piso (sem amputacao). O proximo estimulo so e autorizado apos isto.
        const bool playbackDone = (pp + numSamples) >= (m_preRoll + (int) m_stim.size());
        if (m_floorDone && playbackDone && blkN > 0) {
            const double blkRms = std::sqrt(blkSq / (double) blkN);
            const double thr = std::max(m_floor * kDecayK, 1.0e-6);
            if (blkRms < thr) m_belowRun += blkN; else m_belowRun = 0;
            // MODO OBSERVACIONAL (BASELINE_GOVERNS_SCHEDULER=false): a energia NAO governa o termino.
            // A captura corre ate o teto -> a cauda inteira e observada p/ o estudo do basal.
            if (aamp::baseline::BASELINE_GOVERNS_SCHEDULER
                && m_belowRun >= (int) (kHoldSeconds * m_fs)) { m_endByEnergy = true; m_done.store(true); }
        }
        if (cp + numSamples >= m_total) m_done.store(true);   // teto de seguranca
    }
    void audioDeviceAboutToStart(juce::AudioIODevice*) override {}
    void audioDeviceStopped() override {}

    juce::AudioDeviceManager& m_dm;
    double m_fs;
    const std::atomic<bool>* m_abort = nullptr;
    std::vector<float> m_stim, m_cap;
    int m_total = 0, m_callIndex = 0;
    std::atomic<int> m_playPos { 0 }, m_capPos { 0 };
    std::atomic<bool> m_done { false };
    int    m_preRoll = 0;          // amostras capturadas ANTES do estimulo (onset + medida do piso)
    double m_floor = 0.0;          // piso de ruido medido no pre-roll
    bool   m_floorDone = false;
    int    m_belowRun = 0;         // amostras consecutivas abaixo de k*piso (hold)
    int    m_capturedEnd = 0;      // posicao final efetiva (decaimento por energia ou timeout)
    bool   m_endByEnergy = false;  // true = encerrou por decaimento; false = timeout (nao-decaente)
    static constexpr double kPreRollSeconds = 0.15; // captura antes do estimulo
    static constexpr double kDecayK         = 3.0;  // limiar = 3x o PISO (preserva todo decaimento acima do piso)
    static constexpr double kHoldSeconds    = 0.20; // hold abaixo do limiar p/ autorizar a transicao
};

} // namespace aamp
#endif // AAMP_WITH_JUCE

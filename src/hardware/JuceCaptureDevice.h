// AAMP - src/hardware/JuceCaptureDevice.h
// SCAFFOLDING DE MAQUINA-ALVO (nao compilado/verificado neste ambiente headless: requer JUCE).
// RELOCACAO do bloco REAL de play+capture do Gerador de Estimulos (Main.cpp: JuceCaptureDevice),
// tornado reutilizavel para o Laboratorio via AampWebBridge. Codigo identico ao ja validado — apenas
// movido para namespace aamp e para um header, para ser consumido tambem pela ponte (M-08).
// Reproduz e captura cada estimulo gerado por StimulusGenerator (M-03) atraves do AudioDeviceManager
// configurado (dispositivo/modo/canais) pela mesma logica do AudioConfig.
//
// ===================== FASE 1 — INSTRUMENTACAO PASSIVA (2026-07) =====================
// Objetivo: reconstruir integralmente o ciclo de vida de cada captura APENAS pelo diag_log.txt.
// PASSIVA: nao altera logica, temporizacao nem sincronizacao. Apenas adiciona logs e contadores.
//   - No AUDIO CALLBACK: somente atomicos + escrita em array pre-alocado (SEM I/O, SEM alocacao).
//   - Todo I/O (diagLog) ocorre na THREAD DE CONTROLE (playAndCapture) ou nos eventos de stream
//     (audioDeviceAboutToStart/Stopped), que sao transicoes raras — nunca no caminho por-bloco.
// A logica original (m_playPos/m_capPos/energia/teto/m_done) permanece VERBATIM.
// ====================================================================================
#pragma once
#if defined(AAMP_WITH_JUCE)
#include <juce_audio_devices/juce_audio_devices.h>
#include "../capture/IAudioDevice.h"
#include "../capture/Baseline.h"
#include "../common/Constants.h"
#include "../common/DiagLog.h"
#include <atomic>
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace aamp {

// diag_log.txt: encaminha ao canal unico (src/common/DiagLog.h), que carimba data/hora e
// tempo relativo e serializa as escritas. Assinatura preservada -- nenhuma chamada mudou.
namespace capture_detail {
inline void diagLog(const juce::String& line) { diag::write(line.toStdString()); }
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

        // ---- Fase 1: reset dos coletores de instrumentacao (log-only) ----
        m_cbCount.store(0); m_lastCbMs.store(0); m_firstSeen.store(false); m_snapN.store(0);
        m_aboutToStartInCap.store(0); m_stoppedInCap.store(0);
        m_firstMs = 0; m_firstNumSamples = 0; m_firstNumIn = 0; m_firstNumOut = 0; m_firstInCh = -1; m_firstCapPos = 0; m_firstDone = false;
        const std::uint32_t tCapStart = juce::Time::getMillisecondCounter();

        // ---- Fase 1: CAPTURE BEGIN + estado do device ANTES de addAudioCallback ----
        diagLog("========== CAPTURE BEGIN ==========");
        diagLog(juce::String("callIndex=") + juce::String(m_callIndex)
                + " preRoll=" + juce::String(m_preRoll)
                + " stimSize=" + juce::String((int) m_stim.size())
                + " total=" + juce::String(m_total));
        diagLog("STATE Preparing");
        {
            juce::AudioIODevice* d = m_dm.getCurrentAudioDevice();
            if (d != nullptr)
                diagLog(juce::String("AudioDevice: device!=null=1 name='") + d->getName() + "'"
                        + " open=" + (d->isOpen() ? "1" : "0")
                        + " playing=" + (d->isPlaying() ? "1" : "0")
                        + " sampleRate=" + juce::String(d->getCurrentSampleRate(), 1)
                        + " bufferSize=" + juce::String(d->getCurrentBufferSizeSamples())
                        + " activeInputChannels=" + d->getActiveInputChannels().toString(2)
                        + " activeOutputChannels=" + d->getActiveOutputChannels().toString(2));
            else
                diagLog("AudioDevice: device!=null=0 (getCurrentAudioDevice()==nullptr)");
        }

        m_dm.addAudioCallback(this);
        diagLog("addAudioCallback()  STATE CallbackAttached");

        const double secs = (double) m_total / (m_fs > 0.0 ? m_fs : 48000.0);
        const int timeoutMs = (int) (secs * 1000.0) + 2000;
        const auto t0 = juce::Time::getMillisecondCounter();
        diagLog("STATE WaitingForCompletion");
        bool exAbort = false, exTimeout = false;   // Fase 1: registra o motivo de saida (NAO altera os breaks)
        while (!m_done.load()) {
            if (m_abort != nullptr && m_abort->load()) { exAbort = true; break; }
            juce::Thread::sleep(5);
            if ((int) (juce::Time::getMillisecondCounter() - t0) > timeoutMs) { exTimeout = true; break; }
        }
        const std::uint32_t elapsedMs = juce::Time::getMillisecondCounter() - t0;
        diagLog("STATE Completed");
        m_dm.removeAudioCallback(this);
        diagLog("removeAudioCallback()  STATE RemovingCallback");
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

        // ---- Fase 1: TIMELINE do callback (flush na thread de controle; nada disto rodou no callback) ----
        {
            const long cbTotal = m_cbCount.load();
            if (cbTotal == 0) {
                diagLog("FIRST CALLBACK: <NUNCA OCORREU>  (callback nao foi invocado durante a janela)");
            } else {
                diagLog(juce::String("FIRST CALLBACK: n=1  t+") + juce::String((juce::int64)(std::uint32_t)(m_firstMs - tCapStart)) + "ms"
                        + " numSamples=" + juce::String(m_firstNumSamples)
                        + " numInputs=" + juce::String(m_firstNumIn)
                        + " numOutputs=" + juce::String(m_firstNumOut)
                        + " firstValidInputChannel=" + juce::String(m_firstInCh)
                        + " m_capPos=" + juce::String(m_firstCapPos)
                        + " m_done=" + (m_firstDone ? "1" : "0"));
                const int ns = m_snapN.load();
                const int lim = juce::jmin(ns, (int) kMaxSnaps);
                for (int i = 0; i < lim; ++i) {
                    const CbSnap& s = m_snaps[(size_t) i];
                    diagLog(juce::String("callback #") + juce::String((juce::int64) s.n)
                            + "  m_capPos=" + juce::String(s.capPos)
                            + " numSamples=" + juce::String(s.numSamples)
                            + " t+" + juce::String((juce::int64)(std::uint32_t)(s.tMs - tCapStart)) + "ms");
                }
                if (ns > (int) kMaxSnaps) diagLog(juce::String("(marcos truncados: +") + juce::String(ns - (int) kMaxSnaps) + " nao registrados)");
                diagLog(juce::String("callback FINAL: n=") + juce::String((juce::int64) cbTotal)
                        + "  m_capPos=" + juce::String(m_capPos.load())
                        + " lastCb t+" + juce::String((juce::int64)(std::uint32_t)(m_lastCbMs.load() - tCapStart)) + "ms");
            }
            // Motivo da saida: ABORT > ENERGY > TOTAL_LIMIT (teto) > TIMEOUT > UNKNOWN
            const char* reason = exAbort ? "ABORT"
                               : (m_endByEnergy ? "ENERGY"
                               : (m_done.load() ? "TOTAL_LIMIT"
                               : (exTimeout ? "TIMEOUT" : "UNKNOWN")));
            diagLog(juce::String("reason: ") + reason
                    + "  callbackCount=" + juce::String((juce::int64) cbTotal)
                    + " m_capPos=" + juce::String(m_capPos.load())
                    + " m_total=" + juce::String(m_total)
                    + " capturadoEfetivo=" + juce::String(outLen)
                    + " elapsed_ms=" + juce::String((int) elapsedMs)
                    + " aboutToStartInCap=" + juce::String(m_aboutToStartInCap.load())
                    + " stoppedInCap=" + juce::String(m_stoppedInCap.load()));
            diagLog("STATE Finished");
            diagLog("========== CAPTURE END ==========");
        }

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
        // ---- Fase 1: contagem/marcos PASSIVOS (somente atomicos + array pre-alocado; SEM I/O) ----
        const long n = m_cbCount.fetch_add(1) + 1;
        const std::uint32_t nowMs = juce::Time::getMillisecondCounter();
        m_lastCbMs.store(nowMs);
        if (!m_firstSeen.exchange(true)) {
            int inCh0 = -1; for (int ch = 0; ch < numIn; ++ch) if (in[ch]) { inCh0 = ch; break; }
            m_firstMs = nowMs; m_firstNumSamples = numSamples; m_firstNumIn = numIn; m_firstNumOut = numOut;
            m_firstInCh = inCh0; m_firstCapPos = m_capPos.load(); m_firstDone = m_done.load();
        }
        if (n == 1 || n == 10 || (n % 100) == 0) {
            const int idx = m_snapN.fetch_add(1);
            if (idx < (int) kMaxSnaps)
                m_snaps[(size_t) idx] = CbSnap{ n, m_capPos.load(), m_playPos.load(), numSamples, nowMs };
        }
        // ---- (LOGICA ORIGINAL, INALTERADA a partir daqui) ----
        for (int ch = 0; ch < numOut; ++ch) if (out[ch]) juce::FloatVectorOperations::clear(out[ch], numSamples);
        if (m_done.load()) return;
        const int pp = m_playPos.load();
        // Saida: silencio no pre-roll, depois o estimulo (offset por m_preRoll), depois silencio (cauda).
        for (int ch = 0; ch < numOut; ++ch)
            if (out[ch])
                for (int nn = 0; nn < numSamples; ++nn) {
                    const int s = (pp + nn) - m_preRoll;   // posicao dentro do estimulo
                    out[ch][nn] = (s >= 0 && s < (int) m_stim.size()) ? m_stim[(size_t) s] : 0.0f;
                }
        m_playPos.store(pp + numSamples);

        const int cp = m_capPos.load();
        int inCh = -1; for (int ch = 0; ch < numIn; ++ch) if (in[ch]) { inCh = ch; break; }
        double blkSq = 0.0; int blkN = 0;
        if (inCh >= 0)
            for (int nn = 0; nn < numSamples; ++nn) {
                const int idx = cp + nn;
                if (idx < m_total) {
                    const float v = in[inCh][nn];
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
    // Fase 1: eventos de STREAM (transicoes raras — start/stop). Logam direto p/ flagrar reinicializacoes.
    void audioDeviceAboutToStart(juce::AudioIODevice* d) override {
        m_aboutToStartInCap.fetch_add(1);
        diagLog(juce::String(">>> audioDeviceAboutToStart()  t=") + juce::String((juce::int64) juce::Time::getMillisecondCounter())
                + " name='" + (d != nullptr ? d->getName() : juce::String("<null>")) + "'"
                + " sampleRate=" + juce::String(d != nullptr ? d->getCurrentSampleRate() : 0.0, 1)
                + " bufferSize=" + juce::String(d != nullptr ? d->getCurrentBufferSizeSamples() : 0));
    }
    void audioDeviceStopped() override {
        m_stoppedInCap.fetch_add(1);
        diagLog(juce::String(">>> audioDeviceStopped()  t=") + juce::String((juce::int64) juce::Time::getMillisecondCounter()));
    }

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

    // ---- Fase 1: coletores de instrumentacao (log-only). Escritos no callback SEM I/O; lidos no flush. ----
    struct CbSnap { long n; int capPos; int playPos; int numSamples; std::uint32_t tMs; };
    static constexpr int kMaxSnaps = 128;               // marcos por captura (#1,#10, e a cada 100)
    std::atomic<long> m_cbCount { 0 };                  // total de invocacoes do callback nesta captura
    std::atomic<std::uint32_t> m_lastCbMs { 0 };        // timestamp do ultimo callback
    std::atomic<bool> m_firstSeen { false };            // guarda de "primeiro callback registrado"
    std::atomic<int>  m_snapN { 0 };                    // indice atomico (limitado) dos marcos
    std::array<CbSnap, (size_t) kMaxSnaps> m_snaps {};  // buffer pre-alocado dos marcos
    // Detalhes do PRIMEIRO callback (gravados uma unica vez, sem I/O):
    std::uint32_t m_firstMs = 0; int m_firstNumSamples = 0, m_firstNumIn = 0, m_firstNumOut = 0, m_firstInCh = -1, m_firstCapPos = 0; bool m_firstDone = false;
    // Eventos de stream ocorridos DURANTE a captura corrente:
    std::atomic<int> m_aboutToStartInCap { 0 };
    std::atomic<int> m_stoppedInCap { 0 };
};

} // namespace aamp
#endif // AAMP_WITH_JUCE

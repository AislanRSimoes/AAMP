// AAMP - src/orchestration/MeasurementOrchestrator.cpp
#include "MeasurementOrchestrator.h"
#include "../common/Constants.h"
#include "../capture/Baseline.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <string>

namespace aamp {

// ===== EXPERIMENTAL: observacao do BaselineState com ORIGEM EXPLICITA no ST-09 =====
// Modo observacional puro: NAO governa a transicao (BASELINE_GOVERNS_SCHEDULER permanece false).
// So mede e registra. A telemetria leva "Source=ST09" para distinguir do basal oportunista (v0)
// emitido pelo device.
namespace {

void evidBaselineLog(const std::string& s) {   // append no mesmo diag_log.txt (plain C++, igual m04log)
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

std::string fmt2(double v) { char b[32]; std::snprintf(b, sizeof(b), "%.2f", v); return std::string(b); }

// Varre a cauda do buffer (apos o estimulo) classificando contra o basal do ST-09. So registra.
void observeTailAgainstBaseline(const std::vector<double>& buf, std::size_t stimSamples,
                                const baseline::BaselineState& bs, double fs) {
    using namespace baseline;
    if (!bs.valid || buf.empty()) return;
    Biquad bank[kNumBands];
    for (int k = 0; k < kNumBands; ++k) {
        const double f0 = std::sqrt(kBandEdgesHz[(std::size_t) k] * kBandEdgesHz[(std::size_t) k + 1]);
        const double bw = std::max(kBandEdgesHz[(std::size_t) k + 1] - kBandEdgesHz[(std::size_t) k], 1.0e-12);
        bank[k].designBandpass(f0, std::max(f0 / bw, 0.2), fs);
    }
    const std::size_t stimEnd = std::min(stimSamples, buf.size());
    const int win = std::max(1, (int) (0.020 * fs));
    TailObserver obs(bs, fs, 0.020);
    Classification last = Classification::Converging;
    SchedulerState st;
    std::array<double, kNumBands> bands{};
    int emitted = 0;
    for (std::size_t a = stimEnd; a + (std::size_t) win <= buf.size(); a += (std::size_t) win) {
        double sq = 0.0, pk = 0.0;
        std::array<double, kNumBands> bsq{};
        for (std::size_t i = a; i < a + (std::size_t) win; ++i) {
            const double v = buf[i];
            sq += v * v; pk = std::max(pk, std::abs(v));
            for (int k = 0; k < kNumBands; ++k) { const double y = bank[k].process(v); bsq[(std::size_t) k] += y * y; }
        }
        const double rms = std::sqrt(sq / (double) win);
        for (int k = 0; k < kNumBands; ++k) bands[(std::size_t) k] = std::sqrt(bsq[(std::size_t) k] / (double) win);
        last = obs.feed(rms, pk, bands, st);
        if ((emitted % 12) == 0)
            evidBaselineLog("[EVID-BASELINE] Source=ST09 State=" + std::string(toString(last))
                + " ResidualRMS=" + fmt2(st.residualRMS_dB) + " dBFS"
                + " ResidualPeak=" + fmt2(st.residualPeak_dBFS) + " dBFS"
                + " Slope=" + fmt2(st.slope_dBs) + " dB/s"
                + " Persistence=" + fmt2(st.persistence_s) + " s"
                + " BaselineDistance=" + fmt2(st.baselineDistance_sigma) + " sigma"
                + " governs=false");
        ++emitted;
    }
    evidBaselineLog("[EVID-BASELINE] Source=ST09 State=" + std::string(toString(last))
        + " ResidualRMS=" + fmt2(st.residualRMS_dB) + " dBFS Slope=" + fmt2(st.slope_dBs) + " dB/s (final)");
}

}  // namespace

MeasurementOrchestrator::MeasurementOrchestrator(double fs, ChannelMode mode)
    : m_fs(fs), m_mode(mode),
      m_capture(fs, k::CLIP_THRESHOLD_DBFS_DEFAULT),
      m_analysis(fs) {}

void MeasurementOrchestrator::report(const std::string& label, int completed, int total) {
    if (!m_progress) return;
    Progress p;
    p.label = label;
    p.completed = completed;
    p.total = total;
    p.percent = (total > 0) ? static_cast<int>(100.0 * completed / total + 0.5) : 0;
    m_progress(p);
}

std::size_t MeasurementOrchestrator::estimateSuiteMemoryBytes() const {
    // Soma dos buffers dos 16 estimulos + janela de captura (post-roll) + estruturas.
    StimulusGenerator gen(m_fs);
    std::size_t total = 0;
    for (const Stimulus& s : gen.generateSuite()) {
        const std::size_t cap = s.samples.size() + static_cast<std::size_t>(k::POST_ROLL_SECONDS * m_fs);
        total += (s.samples.size() + cap) * sizeof(double); // estimulo + captura
    }
    total += 4ull * 1024 * 1024; // folga p/ FFTs e ResultBundle
    return total;
}

bool MeasurementOrchestrator::prepare(IAudioDevice& dev) {
    if (dev.inputChannels() <= 0 || dev.outputChannels() <= 0) {
        m_sm.forceError();               // ER-010: sem canais utilizaveis
        return false;
    }
    m_cancel = false;
    return m_sm.transition(State::Ready); // BR-003
}


bool MeasurementOrchestrator::levelPreCheck(IAudioDevice& dev, std::string& abortMsg)
{
    (void)dev;
    (void)abortMsg;
    return true;
}

//bool MeasurementOrchestrator::levelPreCheck(IAudioDevice& dev, std::string& abortMsg) {
    // Tom de calibracao 1 kHz / 200 ms / -12 dBFS (DOC-03 sec.5, BR-021).
 //   const int N = static_cast<int>(k::CALIB_SECONDS * m_fs + 0.5);
 //   const double amp = std::pow(10.0, k::CALIB_DBFS / 20.0);
 //   std::vector<double> tone(static_cast<std::size_t>(N));
 //   for (int i = 0; i < N; ++i)
 //       tone[static_cast<std::size_t>(i)] = amp * std::sin(2.0 * 3.14159265358979 * k::CALIB_FREQ_HZ * i / m_fs);
 //   std::vector<double> ret = dev.playAndCapture(tone, tone.size());
	  
    // Verificacao de roteamento: precisa receber sinal (ER-022).
 //   double pk = 0.0; for (double v : ret) pk = std::max(pk, std::fabs(v));
 //   if (pk < 1e-6) { abortMsg = "Nenhum sinal recebido no canal de entrada. Verifique as conexoes fisicas."; return false; }
 //   return true;
//}

bool MeasurementOrchestrator::captureWithRetry(const Stimulus& stim, IAudioDevice& dev,
                                               CaptureResult& outResult, std::string& abortMsg) {
    for (int attempt = 0; attempt < k::MAX_CAPTURE_RETRIES; ++attempt) {
        CaptureResult c = m_capture.capture(stim, dev, m_noiseFloorRms);
        if (c.status.valid) { outResult = std::move(c); return true; }
        // ER-020 (clipping) / ER-024 (parcial): repete ate o limite, depois aborta.
        if (attempt == k::MAX_CAPTURE_RETRIES - 1) {
            if (c.status.clipped)
                abortMsg = "Sinal acima do limite durante a medicao. Reduza o ganho de saida.";
            else
                abortMsg = "A captura foi interrompida repetidamente. Verifique o dispositivo.";
            return false;
        }
    }
    return false;
}

StimulusReport MeasurementOrchestrator::makeStimulusReport(const Stimulus& stim, const CaptureResult& c,
                                                           int idx, int total) const {
    StimulusReport rep;
    rep.index = idx;
    rep.total = total;
    rep.stimSamples     = static_cast<int>(stim.samples.size());
    rep.capturedSamples = static_cast<int>(c.samples.size());
    double pk = 0.0, energy = 0.0;
    for (double v : c.samples) { pk = std::max(pk, std::fabs(v)); energy += v * v; }
    rep.peak = pk;
    rep.rms  = c.samples.empty() ? 0.0 : std::sqrt(energy / static_cast<double>(c.samples.size()));
    // Energia residual: RMS da cauda APOS o fim do estimulo (comportamento que persiste no DUT).
    const std::size_t stimEnd = std::min(stim.samples.size(), c.samples.size());
    double et = 0.0; std::size_t nt = 0;
    for (std::size_t i = stimEnd; i < c.samples.size(); ++i) { et += c.samples[i] * c.samples[i]; ++nt; }
    rep.residual = (nt > 0) ? std::sqrt(et / static_cast<double>(nt)) : 0.0;
    rep.clipped    = c.status.clipped;
    rep.nonDecayed = c.nonDecayed;
    rep.observedDurationSamples = c.observedDurationSamples;
    return rep;
}

RunResult MeasurementOrchestrator::runSuite(IAudioDevice& dev) {
    RunResult rr;

    // Estimativa de memoria pre-suite (DOC-09 sec.4.1).
    if (estimateSuiteMemoryBytes() > MEMORY_LIMIT_BYTES) {
        rr.outcome = RunOutcome::InsufficientMemory;
        rr.userMessage = "Memoria insuficiente para iniciar a medicao.";
        return rr;
    }

    if (m_sm.state() != State::Ready) { rr.userMessage = "Dispositivo nao esta pronto."; return rr; }
    m_sm.transition(State::Measuring);

    std::string abortMsg;
    if (!levelPreCheck(dev, abortMsg)) {
        m_sm.forceError();
        rr.outcome = RunOutcome::Aborted; rr.userMessage = abortMsg; return rr;
    }

    StimulusGenerator gen(m_fs);
    std::vector<Stimulus> suite = gen.generateSuite();
    // EXPERIMENTAL (validacao do basal explicito): ST-09 (Silence) como PRIMEIRA etapa, para construir
    // o BaselineState ANTES da primeira excitacao. Apenas reordenacao + origem do basal; nao governa nada.
    {
        auto it09 = std::find_if(suite.begin(), suite.end(),
                                 [](const Stimulus& s) { return s.id == StimulusId::ST09_Silence; });
        if (it09 != suite.end() && it09 != suite.begin()) std::rotate(suite.begin(), it09, it09 + 1);
    }
    CapturedSet cap; cap.fs = m_fs; cap.mode = m_mode;
    m_noiseFloorRms = 0.0;

    // ---- Mapa de dependencias estimulo -> metrica ----
    // Cada metrica e EMITIDA uma unica vez, assim que TODOS os seus estimulos requeridos foram
    // capturados. A ordem da tabela e a ordem canonica do ResultBundle (= computeAllMetrics).
    // A computacao usa a interface PUBLICA computeAllMetrics (os metodos por metrica sao privados):
    // metricas sem seus estimulos retornam quality=Error e sao ignoradas ate ficarem completas.
    struct MetricDep {
        const char* id;
        std::vector<StimulusId> deps;
        const char* depLabel;
    };
    const std::vector<MetricDep> table = {
        { "MS-01", { StimulusId::ST01_ESS }, "ST01" },
        { "MS-02", { StimulusId::ST01_ESS }, "ST01" },
        { "MS-03", { StimulusId::ST01_ESS }, "ST01" },
        { "MS-04", { StimulusId::ST01_ESS }, "ST01" },
        { "MS-05", { StimulusId::ST03_PureSine }, "ST03" },
        { "MS-06", { StimulusId::ST03_PureSine }, "ST03" },
        { "MS-07", { StimulusId::ST02_MultiTone }, "ST02" },
        { "MS-08", { StimulusId::ST09_Silence }, "ST09" },
        { "MS-09", { StimulusId::ST08_DynamicStep, StimulusId::ST09_Silence }, "ST08+ST09" },
        { "MS-10", { StimulusId::ST03_PureSine,   StimulusId::ST09_Silence }, "ST03+ST09" },
        { "MS-11", { StimulusId::ST01_ESS }, "ST01" },
        { "MS-12", { StimulusId::ST06_Impulse }, "ST06" },
        { "MS-13", { StimulusId::ST07_BurstTone, StimulusId::ST08_DynamicStep,
                     StimulusId::ST12_PulseTrain, StimulusId::ST13_Sustained }, "ST07+ST08+ST12+ST13" },
        { "MS-14", { StimulusId::ST10_HardAttack, StimulusId::ST11_SoftAttack }, "ST10+ST11" },
        { "MS-15", { StimulusId::ST15_DynamicPerformance }, "ST15" },
    };
    // Restringe as dependencias aos estimulos que REALMENTE existem na suite (nao trava se a suite mudar).
    std::set<StimulusId> present;
    for (const Stimulus& s : suite) present.insert(s.id);
    auto depsReady = [&](const MetricDep& md) -> bool {
        for (StimulusId d : md.deps)
            if (present.count(d) && !cap.captured.count(d)) return false;
        return true;
    };

    baseline::BaselineState suiteBaseline;           // EXPERIMENTAL: basal explicito do ST-09 (observacional)
    bool baselineReady = false;

    report("Preparando medicao", 0, static_cast<int>(suite.size()));
    int done = 0;
    const int total = static_cast<int>(suite.size());

    for (const Stimulus& s : suite) {
        if (m_cancel) {                       // FR-039: cancelamento seguro
            m_sm.transition(State::Idle);
            rr.outcome = RunOutcome::Cancelled;
            rr.userMessage = "Medicao cancelada.";
            return rr;                         // sem processamento parcial
        }

        // 1) Gerar (ja gerado em suite) -> 2) Capturar a resposta COMPLETA (captura adaptativa).
        CaptureResult c;
        if (!captureWithRetry(s, dev, c, abortMsg)) {
            m_sm.forceError();
            rr.outcome = RunOutcome::Aborted; rr.userMessage = abortMsg; return rr;
        }

        // 3) Validar integridade + monitoramento continuo (RMS/peak/residual/clipping/decaimento).
        StimulusReport sreport = makeStimulusReport(s, c, done + 1, total);
        if (m_stimcb) m_stimcb(sreport);
        if (s.id == StimulusId::ST09_Silence) m_noiseFloorRms = sreport.rms; // piso p/ limiar adaptativo

        cap.captured[s.id] = std::move(c.samples);
        ++done;
        report("Medindo", done, total);

        // EXPERIMENTAL: origem do basal = ST-09 (primeira etapa). Observacional; governanca = false.
        if (s.id == StimulusId::ST09_Silence) {
            suiteBaseline = baseline::measureBaseline(cap.captured[s.id], m_fs);
            baselineReady = suiteBaseline.valid;
            evidBaselineLog(std::string("[EVID-BASELINE] Source=ST09 BaselineCaptured=")
                + (baselineReady ? "true" : "false")
                + " BaselineRMS=" + fmt2(suiteBaseline.rmsMean_dB) + " dBFS"
                + " sigma=" + fmt2(suiteBaseline.rmsSigma_dB)
                + " PeakMax=" + fmt2(suiteBaseline.peakMax_dBFS) + " dBFS governs=false");
        } else if (baselineReady) {
            observeTailAgainstBaseline(cap.captured[s.id], s.samples.size(), suiteBaseline, m_fs);
        }

        // FASE 1 (Aquisicao): este laco APENAS captura buffers + observa baseline. NENHUMA analise
        // aqui — a analise ocorre UMA vez apos a suite completa (Fase 3, abaixo). Isso elimina o
        // reprocessamento repetido (computeAllMetrics por estimulo) que inflava o tempo total.
    }

    // FASE 3 (Processamento) — apos capturar TODA a suite, o AnalysisEngine processa o conjunto
    // COMPLETO uma UNICA vez. Sem reprocessamento por estimulo. Ordem canonica = table.
    if (!m_sm.transition(State::Processing)) { m_sm.forceError(); rr.userMessage = "Falha de transicao."; return rr; }
    report("Processando resultados", total, total);
    rr.bundle.meta.sampleRate = cap.fs;
    rr.bundle.meta.mode = cap.mode;

    ResultBundle full = m_analysis.computeAllMetrics(cap);   // UNICA passagem de analise (todos os buffers)
    int errors = 0, emitted = 0;
    for (const MetricDep& md : table) {
        if (!depsReady(md)) continue;                        // metrica sem seus estimulos: ignorada
        const MetricResult* mr = nullptr;
        for (const MetricResult& m : full.metrics) if (m.id == md.id) { mr = &m; break; }
        if (mr == nullptr) continue;
        rr.bundle.metrics.push_back(*mr);
        if (mr->quality == Quality::Error) ++errors;
        ++emitted;
        if (m_result) {                                      // emite as metricas prontas (apos a suite)
            MetricEmissionInfo info;
            info.deps = md.depLabel;
            info.stimuliCompleted = total;
            info.totalStimuli = total;
            info.emittedIndex = emitted;
            info.totalMetrics = static_cast<int>(table.size());
            m_result(*mr, info);
        }
    }
    rr.errorMetrics = errors;
    rr.emittedIncremental = 0;                               // analise em fase unica (nao incremental)
    rr.emittedAfterSuite  = emitted;
    // Falha de TODAS as metricas -> suite falha (ER-033); caso contrario, prossegue (degradacao FR-040).
    if (!rr.bundle.metrics.empty() && errors >= static_cast<int>(rr.bundle.metrics.size())) {
        m_sm.forceError();
        rr.outcome = RunOutcome::Aborted;
        rr.userMessage = "Nao foi possivel processar os resultados. Repita a medicao.";
        return rr;
    }

    m_sm.transition(State::Results);
    report("Concluido", total, total);
    rr.outcome = RunOutcome::Completed;
    return rr;
}

bool MeasurementOrchestrator::startNewMeasurement(bool confirmDiscard, bool resultsExported) {
    if (m_sm.state() != State::Results) return false;
    // FR-038/BR-032: se ha resultados nao exportados, exige confirmacao explicita.
    if (!resultsExported && !confirmDiscard) return false;
    // Descarte confirmado -> volta para READY para nova medicao.
    m_sm.transition(State::Idle);
    return m_sm.transition(State::Ready);
}

} // namespace aamp

// AAMP - src/orchestration/MeasurementOrchestrator.cpp
#include "MeasurementOrchestrator.h"
#include "../common/Constants.h"
#include <cmath>

namespace aamp {

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

bool MeasurementOrchestrator::levelPreCheck(IAudioDevice& dev, std::string& abortMsg) {
    // Tom de calibracao 1 kHz / 200 ms / -12 dBFS (DOC-03 §5, BR-021).
    const int N = static_cast<int>(k::CALIB_SECONDS * m_fs + 0.5);
    const double amp = std::pow(10.0, k::CALIB_DBFS / 20.0);
    std::vector<double> tone(static_cast<std::size_t>(N));
    for (int i = 0; i < N; ++i)
        tone[static_cast<std::size_t>(i)] = amp * std::sin(2.0 * 3.14159265358979 * k::CALIB_FREQ_HZ * i / m_fs);
    std::vector<double> ret = dev.playAndCapture(tone, tone.size());
    // Verificacao de roteamento: precisa receber sinal (ER-022).
    double pk = 0.0; for (double v : ret) pk = std::max(pk, std::fabs(v));
    if (pk < 1e-6) { abortMsg = "Nenhum sinal recebido no canal de entrada. Verifique as conexoes fisicas."; return false; }
    return true;
}

bool MeasurementOrchestrator::captureWithRetry(const Stimulus& stim, IAudioDevice& dev,
                                               std::vector<double>& outSamples,
                                               std::string& abortMsg) {
    for (int attempt = 0; attempt < k::MAX_CAPTURE_RETRIES; ++attempt) {
        CaptureResult c = m_capture.capture(stim, dev);
        if (c.status.valid) { outSamples = std::move(c.samples); return true; }
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

RunResult MeasurementOrchestrator::runSuite(IAudioDevice& dev) {
    RunResult rr;

    // Estimativa de memoria pre-suite (DOC-09 §4.1).
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
    CapturedSet cap; cap.fs = m_fs; cap.mode = m_mode;

    report("Preparando medicao", 0, static_cast<int>(suite.size()));
    int done = 0;
    for (const Stimulus& s : suite) {
        if (m_cancel) {                       // FR-039: cancelamento seguro
            m_sm.transition(State::Idle);
            rr.outcome = RunOutcome::Cancelled;
            rr.userMessage = "Medicao cancelada.";
            return rr;                         // sem processamento parcial
        }
        std::vector<double> samples;
        if (!captureWithRetry(s, dev, samples, abortMsg)) {
            m_sm.forceError();
            rr.outcome = RunOutcome::Aborted; rr.userMessage = abortMsg; return rr;
        }
        cap.captured[s.id] = std::move(samples);
        ++done;
        report("Medindo", done, static_cast<int>(suite.size()));
    }

    // PROCESSING (M-05) com degradacao graciosa (FR-040).
    if (!m_sm.transition(State::Processing)) { m_sm.forceError(); rr.userMessage = "Falha de transicao."; return rr; }
    report("Processando resultados", suite.size(), static_cast<int>(suite.size()));
    rr.bundle = m_analysis.computeAllMetrics(cap);

    int errors = 0;
    for (const auto& m : rr.bundle.metrics) if (m.quality == Quality::Error) errors++;
    rr.errorMetrics = errors;
    // Falha de TODAS as metricas -> suite falha (ER-033); caso contrario, prossegue (degradacao).
    if (errors >= static_cast<int>(rr.bundle.metrics.size())) {
        m_sm.forceError();
        rr.outcome = RunOutcome::Aborted;
        rr.userMessage = "Nao foi possivel processar os resultados. Repita a medicao.";
        return rr;
    }

    m_sm.transition(State::Results);
    report("Concluido", suite.size(), static_cast<int>(suite.size()));
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

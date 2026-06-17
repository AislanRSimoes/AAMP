// AAMP - tests/test_orchestrator.cpp
// M-08 Orchestrator (loopback virtual + mocks): TC-060..TC-063 (AC-M08-01..04),
// + modelo de progresso sem detalhes tecnicos (TC-044, parte unit).
#include "TestRunner.h"
#include "../src/orchestration/MeasurementOrchestrator.h"
#include "../src/orchestration/StateMachine.h"
#include "../src/capture/IAudioDevice.h"
#include "../src/common/Constants.h"
#include <string>
#include <vector>

using namespace aamp;
static const double FS = k::REF_SAMPLE_RATE;

// Maquina de estados: transicoes validas/invalidas (DOC-04 §5).
TEST_CASE("StateMachine transicoes (DOC-04 §5)") {
    StateMachine sm;
    CHECK(sm.state() == State::Idle);
    CHECK(sm.transition(State::Ready));
    CHECK(!sm.transition(State::Results));        // READY->RESULTS invalido
    CHECK(sm.transition(State::Measuring));
    CHECK(sm.transition(State::Processing));
    CHECK(sm.transition(State::Results));
    CHECK(sm.transition(State::Exporting));
    CHECK(sm.transition(State::Results));
}

// Maquina de estados: caminhos de erro e reset.
TEST_CASE("StateMachine erro e reset") {
    StateMachine sm;
    sm.transition(State::Ready);
    sm.transition(State::Measuring);
    sm.forceError();                              // aborto seguro (ER-002/021)
    CHECK(sm.state() == State::Error);
    CHECK(!sm.transition(State::Results));        // ERROR->RESULTS invalido
    CHECK(sm.transition(State::Idle));            // ERROR->IDLE permitido (reset)
    CHECK(sm.state() == State::Idle);
    // transicoes invalidas a partir de PROCESSING
    StateMachine sm2;
    sm2.transition(State::Ready); sm2.transition(State::Measuring); sm2.transition(State::Processing);
    CHECK(!sm2.transition(State::Exporting));     // PROCESSING->EXPORTING invalido
    CHECK(sm2.transition(State::Results));
}

// TC-060: fluxo automatico sem selecao -> suite completa, 15 familias, estado RESULTS.
TEST_CASE("TC-060 Fluxo automatico: 1 comando, suite completa") {
    LoopbackDevice dev(FS, 0);
    MeasurementOrchestrator orch(FS, ChannelMode::Mono);
    CHECK(orch.prepare(dev));
    CHECK(orch.state() == State::Ready);
    RunResult rr = orch.runSuite(dev);
    CHECK(rr.outcome == RunOutcome::Completed);
    CHECK(orch.state() == State::Results);
    CHECK(rr.bundle.metrics.size() == 15);
}

// TC-061: nova medicao a partir de RESULTS exige confirmacao de descarte.
TEST_CASE("TC-061 Nova medicao confirma descarte (FR-038)") {
    LoopbackDevice dev(FS, 0);
    MeasurementOrchestrator orch(FS, ChannelMode::Mono);
    orch.prepare(dev);
    orch.runSuite(dev);
    CHECK(orch.state() == State::Results);
    // sem confirmar e com resultados nao exportados -> bloqueia.
    CHECK(!orch.startNewMeasurement(/*confirm*/false, /*exported*/false));
    CHECK(orch.state() == State::Results);
    // confirmando -> prossegue para READY.
    CHECK(orch.startNewMeasurement(/*confirm*/true, /*exported*/false));
    CHECK(orch.state() == State::Ready);
}

// TC-062: cancelamento seguro durante MEASURING (aborto sem processamento parcial).
TEST_CASE("TC-062 Cancelamento seguro durante a suite (FR-039)") {
    LoopbackDevice dev(FS, 0);
    MeasurementOrchestrator orch(FS, ChannelMode::Mono);
    orch.prepare(dev);
    // cancela imediatamente; runSuite checa o flag antes de cada estimulo.
    orch.requestCancel();
    RunResult rr = orch.runSuite(dev);
    CHECK(rr.outcome == RunOutcome::Cancelled);
    CHECK(orch.state() == State::Idle);
    CHECK(rr.bundle.metrics.empty());   // nenhum resultado parcial
}

// TC-063: degradacao graciosa -> metrica com erro isolada; demais presentes.
TEST_CASE("TC-063 Degradacao graciosa (FR-040)") {
    // Simula falha de uma metrica removendo um estimulo do conjunto capturado.
    AnalysisEngine eng(FS);
    // monta um CapturedSet completo e depois remove ST-02 (alimenta MS-07 IMD).
    StimulusGenerator gen(FS);
    CapturedSet cap; cap.fs = FS; cap.mode = ChannelMode::Mono;
    for (const Stimulus& s : gen.generateSuite())
        if (s.id != StimulusId::ST02_MultiTone)         // ST-02 ausente -> MS-07 Error
            cap.captured[s.id] = s.samples;
    ResultBundle b = eng.computeAllMetrics(cap);
    CHECK(b.metrics.size() == 15);
    const MetricResult* imd = b.find("MS-07");
    CHECK(imd != nullptr && imd->quality == Quality::Error);   // isolada
    // demais metricas seguem normalmente (ex.: MS-05 THD continua valida).
    const MetricResult* thd = b.find("MS-05");
    CHECK(thd != nullptr && thd->quality != Quality::Error);
    int errors = 0; for (const auto& m : b.metrics) if (m.quality == Quality::Error) ++errors;
    CHECK(errors < static_cast<int>(b.metrics.size()));        // nao bloqueia o dashboard
}

// TC-044 (parte unit): progresso sem detalhes tecnicos (DOC-04 §6).
TEST_CASE("TC-044 Progresso amigavel sem detalhes tecnicos") {
    LoopbackDevice dev(FS, 0);
    MeasurementOrchestrator orch(FS, ChannelMode::Mono);
    std::vector<std::string> labels;
    orch.setProgressCallback([&](const Progress& p){ labels.push_back(p.label); });
    orch.prepare(dev);
    orch.runSuite(dev);
    CHECK(!labels.empty());
    for (const std::string& l : labels) {
        // sem nomes de modulo (M-0x), sem ids tecnicos (ST-/MS-), sem ":" de timestamp/log.
        CHECK(l.find("M-0") == std::string::npos);
        CHECK(l.find("ST-") == std::string::npos);
        CHECK(l.find("MS-") == std::string::npos);
        CHECK(l.find(":") == std::string::npos);
    }
}

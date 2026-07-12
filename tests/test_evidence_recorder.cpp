// AAMP - tests/test_evidence_recorder.cpp
// Etapa 2: o Acquisition Engine PRODUZ um EvidencePackage a partir de uma caracterizacao real,
// via observador (EvidenceRecorder) acoplado ao callback somente-leitura do MeasurementOrchestrator.
// Verifica producao/completude/fidelidade, ausencia de ciencia e as duas diretrizes de observacao.
#include "TestRunner.h"
#include "../src/orchestration/MeasurementOrchestrator.h"
#include "../src/capture/IAudioDevice.h"
#include "../src/evidence/EvidenceRecorder.h"
#include "../src/stimulus/StimulusGenerator.h"
#include "../src/common/Constants.h"
#include <stdexcept>
#include <string>

using namespace aamp;
using namespace aamp::evidence;
static const double FS = k::REF_SAMPLE_RATE;

// Semeia o recorder com a configuracao de aquisicao (conhecida antes do processo).
static void seedConfig(EvidenceRecorder& rec, ChannelMode mode) {
    SessionEvidence se; se.sessionId = "SESSION-TEST"; se.appVersion = "test";
    HardwareEvidence hw; hw.sampleRate = FS;
    hw.channels = (mode == ChannelMode::Stereo) ? Channels::Stereo : Channels::Mono;
    AcquisitionEvidence ac;
    ac.clipThresholdDbfs = k::CLIP_THRESHOLD_DBFS_DEFAULT;
    ac.prngSeed = k::PRNG_SEED;
    ac.preRollSeconds = k::PRE_ROLL_SECONDS;
    ac.postRollSeconds = k::POST_ROLL_SECONDS;
    rec.setSession(se); rec.setHardware(hw); rec.setAcquisition(ac);
}

// Producao real: uma caracterizacao completa gera um pacote com origem + captura por estimulo,
// vinculados por codigo, sem qualquer campo cientifico (garantido estruturalmente pelo modelo).
TEST_CASE("Evidence Recorder: producao a partir de aquisicao real (loopback)") {
    LoopbackDevice dev(FS, 0);
    MeasurementOrchestrator orch(FS, ChannelMode::Mono);
    EvidenceRecorder rec; seedConfig(rec, ChannelMode::Mono);
    orch.setObservationCallback([&rec](const Stimulus& s, const CaptureResult& c) { rec.observe(s, c); });

    CHECK(orch.prepare(dev));
    const RunResult rr = orch.runSuite(dev);
    CHECK(rr.outcome == RunOutcome::Completed);

    const EvidencePackage& pkg = rec.package();
    CHECK(static_cast<int>(pkg.stimuli.size())  == k::NUM_STIMULI);
    CHECK(static_cast<int>(pkg.captures.size()) == k::NUM_STIMULI);

    bool linked = true, srcNonEmpty = true, capNonEmpty = true, secEmpty = true, noPartial = true;
    for (std::size_t i = 0; i < pkg.captures.size(); ++i) {
        if (pkg.stimuli[i].code != pkg.captures[i].code) linked = false;
        if (pkg.stimuli[i].source.empty())              srcNonEmpty = false;
        if (pkg.captures[i].primary.empty())            capNonEmpty = false;
        if (!pkg.captures[i].secondary.empty())         secEmpty = false;      // captura mono
        if (pkg.captures[i].partial)                    noPartial = false;     // run completo
    }
    CHECK(linked);
    CHECK(srcNonEmpty);
    CHECK(capNonEmpty);
    CHECK(secEmpty);
    CHECK(noPartial);
}

// Fidelidade: a origem registrada e identica a amostra produzida pelo StimulusGenerator (sem
// transformacao) — base para o Laboratory reproduzir a ciencia a partir da evidencia.
TEST_CASE("Evidence Recorder: fidelidade da origem observada") {
    LoopbackDevice dev(FS, 0);
    MeasurementOrchestrator orch(FS, ChannelMode::Mono);
    EvidenceRecorder rec; seedConfig(rec, ChannelMode::Mono);
    orch.setObservationCallback([&rec](const Stimulus& s, const CaptureResult& c) { rec.observe(s, c); });
    orch.prepare(dev);
    orch.runSuite(dev);

    StimulusGenerator gen(FS);
    const Stimulus st03 = gen.getStimulus(StimulusId::ST03_PureSine);
    const StimulusEvidence* found = nullptr;
    for (const auto& se : rec.package().stimuli) if (se.code == st03.code) { found = &se; break; }
    CHECK(found != nullptr);
    if (found) CHECK(found->source == st03.samples);   // sem qualquer transformacao
}

// Diretriz 2: falha do observador NUNCA interrompe nem invalida a aquisicao. Um observador que
// lanca excecao nao impede a conclusao da caracterizacao nem a producao das 15 familias (ciencia).
TEST_CASE("Evidence Recorder: falha do observador nao interrompe a aquisicao") {
    LoopbackDevice dev(FS, 0);
    MeasurementOrchestrator orch(FS, ChannelMode::Mono);
    orch.setObservationCallback([](const Stimulus&, const CaptureResult&) {
        throw std::runtime_error("falha simulada do observador");
    });
    CHECK(orch.prepare(dev));
    const RunResult rr = orch.runSuite(dev);
    CHECK(rr.outcome == RunOutcome::Completed);        // aquisicao concluida apesar da falha
    CHECK(rr.bundle.metrics.size() == 15);             // ciencia intacta
    CHECK(orch.state() == State::Results);
}

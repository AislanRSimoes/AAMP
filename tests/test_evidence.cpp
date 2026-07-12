// AAMP - tests/test_evidence.cpp
// Evidence Package (Etapa 1): modelo de domínio da sessão de aquisição. Verifica que o modelo
// representa integralmente uma sessão contendo APENAS fatos observados + configuração + identidade,
// e que é construído por inicialização direta a partir de saídas reais do StimulusGenerator (M-03).
#include "TestRunner.h"
#include "../src/evidence/EvidencePackage.h"
#include "../src/stimulus/StimulusGenerator.h"
#include "../src/common/Constants.h"

using namespace aamp;
using namespace aamp::evidence;

// Constrói um EvidencePackage a partir da suíte real de estímulos. As capturas são um loopback
// ideal (primary == source): representa "o que o Laboratory consumiria" para reproduzir a ciência.
// `stereo` popula o canal secundário; caso contrário permanece vazio (mono).
static EvidencePackage buildIdealPackage(bool stereo) {
    StimulusGenerator gen(k::REF_SAMPLE_RATE);
    const std::vector<Stimulus> suite = gen.generateSuite();

    EvidencePackage pkg;
    pkg.session.sessionId  = "SESSION-TEST";
    pkg.session.appVersion = "test";
    pkg.session.timestamp  = "2026-07-12 00:00:00";
    pkg.hardware.deviceName = "Loopback Virtual";
    pkg.hardware.sampleRate = k::REF_SAMPLE_RATE;
    pkg.hardware.channels   = stereo ? Channels::Stereo : Channels::Mono;
    pkg.acquisition.clipThresholdDbfs = k::CLIP_THRESHOLD_DBFS_DEFAULT;
    pkg.acquisition.prngSeed          = k::PRNG_SEED;
    pkg.acquisition.preRollSeconds    = k::PRE_ROLL_SECONDS;
    pkg.acquisition.postRollSeconds   = k::POST_ROLL_SECONDS;

    for (const Stimulus& s : suite) {
        StimulusEvidence se;
        se.code = s.code; se.name = s.name; se.category = s.category; se.source = s.samples;
        pkg.stimuli.push_back(se);

        CaptureEvidence ce;
        ce.code = s.code;
        ce.primary = s.samples;                    // loopback ideal
        if (stereo) ce.secondary = s.samples;      // canal direito presente em estéreo
        pkg.captures.push_back(ce);
    }
    return pkg;
}

// A sessão é representada integralmente: um estímulo e uma captura por ST-01..ST-16, vinculados
// por código, com origem e captura não-vazias.
TEST_CASE("Evidence: completude da sessão (origem + captura por estímulo)") {
    const EvidencePackage pkg = buildIdealPackage(false);
    CHECK(static_cast<int>(pkg.stimuli.size())  == k::NUM_STIMULI);
    CHECK(static_cast<int>(pkg.captures.size()) == k::NUM_STIMULI);
    CHECK(pkg.stimuli.size() == pkg.captures.size());

    bool allLinked = true, allSourceNonEmpty = true, allCaptureNonEmpty = true;
    for (std::size_t i = 0; i < pkg.stimuli.size(); ++i) {
        if (pkg.stimuli[i].code != pkg.captures[i].code) allLinked = false;
        if (pkg.stimuli[i].source.empty())               allSourceNonEmpty = false;
        if (pkg.captures[i].primary.empty())             allCaptureNonEmpty = false;
    }
    CHECK(allLinked);
    CHECK(allSourceNonEmpty);
    CHECK(allCaptureNonEmpty);
}

// Reprodutibilidade: o áudio bruto é preservado bit-a-bit (o Laboratory recomputa toda a ciência
// a partir dele — nenhuma métrica é armazenada na evidência).
TEST_CASE("Evidence: áudio bruto preservado (base de reprodução científica)") {
    StimulusGenerator gen(k::REF_SAMPLE_RATE);
    const Stimulus st = gen.getStimulus(StimulusId::ST03_PureSine);
    const EvidencePackage pkg = buildIdealPackage(false);

    const StimulusEvidence* src = nullptr;
    const CaptureEvidence*  cap = nullptr;
    for (const auto& s : pkg.stimuli)  if (s.code == st.code) { src = &s; break; }
    for (const auto& c : pkg.captures) if (c.code == st.code) { cap = &c; break; }
    CHECK(src != nullptr);
    CHECK(cap != nullptr);
    if (src && cap) {
        CHECK(src->source == st.samples);   // origem idêntica ao emitido
        CHECK(cap->primary == st.samples);  // captura bruta idêntica (loopback ideal)
    }
}

// Estéreo carrega o canal secundário; mono não. (Necessário para futura análise de crosstalk.)
TEST_CASE("Evidence: canal secundário presente somente em estéreo") {
    const EvidencePackage mono   = buildIdealPackage(false);
    const EvidencePackage stereo = buildIdealPackage(true);

    bool monoSecondaryEmpty = true;
    for (const auto& c : mono.captures) if (!c.secondary.empty()) monoSecondaryEmpty = false;
    CHECK(monoSecondaryEmpty);
    CHECK(mono.hardware.channels == Channels::Mono);

    bool stereoSecondaryMatches = true;
    for (const auto& c : stereo.captures)
        if (c.secondary.size() != c.primary.size()) stereoSecondaryMatches = false;
    CHECK(stereoSecondaryMatches);
    CHECK(stereo.hardware.channels == Channels::Stereo);
}

// Telemetria: uma captura incompleta (underrun/abort) é representável — samples vazio + partial.
TEST_CASE("Evidence: telemetria de captura parcial (evento de aquisição)") {
    CaptureEvidence ce;
    ce.code = "ST-01";
    ce.partial = true;      // underrun/abort observado durante a aquisição
    CHECK(ce.partial);
    CHECK(ce.primary.empty());     // dado parcial nunca é processado (samples vazio)
    CHECK(ce.secondary.empty());
}

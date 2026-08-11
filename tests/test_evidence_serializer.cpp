// AAMP - tests/test_evidence_serializer.cpp
// Passo 2: serialização/empacotamento/persistência do Evidence Package. Valida determinismo (P-1),
// não interpretação/fidelidade do áudio (P-2), estrutura do container (B-2) e persistência (B-3).
#include "TestRunner.h"
#include "../src/evidence/EvidenceSerializer.h"
#include "../src/evidence/EvidencePackage.h"
#include "../src/common/RiffWaveWriter.h"
#include "../src/stimulus/StimulusGenerator.h"
#include "../src/common/Constants.h"
#include <fstream>
#include <string>

using namespace aamp;
using namespace aamp::evidence;
static const double FS = k::REF_SAMPLE_RATE;

// Constrói um pacote imutável a partir da suíte real (loopback ideal: primary == source).
static EvidencePackage buildPackage() {
    StimulusGenerator gen(FS);
    EvidencePackage pkg;
    pkg.session.sessionId = "SESSION-TEST"; pkg.session.appVersion = "test";
    pkg.session.timestamp = "2026-07-13 00:00:00";
    pkg.hardware.deviceName = "Loopback Virtual";
    pkg.hardware.sampleRate = FS; pkg.hardware.channels = Channels::Mono;
    pkg.acquisition.clipThresholdDbfs = k::CLIP_THRESHOLD_DBFS_DEFAULT;
    pkg.acquisition.prngSeed = k::PRNG_SEED;
    pkg.acquisition.preRollSeconds = k::PRE_ROLL_SECONDS;
    pkg.acquisition.postRollSeconds = k::POST_ROLL_SECONDS;
    for (const Stimulus& s : gen.generateSuite()) {
        StimulusEvidence se; se.code = s.code; se.name = s.name; se.category = s.category; se.source = s.samples;
        pkg.stimuli.push_back(se);
        CaptureEvidence ce; ce.code = s.code; ce.primary = s.samples;
        pkg.captures.push_back(ce);
    }
    return pkg;
}

// P-1: o mesmo EvidencePackage produz sempre exatamente os mesmos bytes.
TEST_CASE("Serializer: determinismo (mesmo pacote -> mesmos bytes)") {
    const EvidencePackage pkg = buildPackage();
    const std::string a = serialize(pkg);
    const std::string b = serialize(pkg);
    CHECK(!a.empty());
    CHECK(a == b);
    CHECK(a.compare(0, 4, "PK\x03\x04") == 0);          // assinatura ZIP (local file header)
    CHECK(a.find("PK\x05\x06") != std::string::npos);   // EOCD presente
}

// B-2: o container contém os metadados e um WAV de origem e de captura por estímulo.
TEST_CASE("Serializer: estrutura do container (.aamp)") {
    const std::string z = serialize(buildPackage());
    CHECK(z.find("manifest.json") != std::string::npos);
    CHECK(z.find("session.json")  != std::string::npos);
    CHECK(z.find("hardware.json") != std::string::npos);
    CHECK(z.find("metadata.json") != std::string::npos);
    CHECK(z.find("stimuli/ST01_Source.wav")  != std::string::npos);
    CHECK(z.find("captures/ST01_Capture.wav") != std::string::npos);
}

// P-2: o áudio bruto é embutido VERBATIM (STORE), sem transformação/interpretação.
TEST_CASE("Serializer: fidelidade do áudio (sem interpretação)") {
    const std::string z = serialize(buildPackage());
    StimulusGenerator gen(FS);
    const Stimulus st01 = gen.getStimulus(StimulusId::ST01_ESS);
    const std::string expectedWav = riff::writeFloat32(st01.samples, FS, 1);  // mesma capacidade neutra
    CHECK(z.find(expectedWav) != std::string::npos);
    // Nenhuma ciência vaza para a evidência.
    CHECK(z.find("MS-01") == std::string::npos);
    CHECK(z.find("analysis.json") == std::string::npos);
    CHECK(z.find("metrics") == std::string::npos);
}

// B-3: persistência grava em disco exatamente os bytes serializados.
TEST_CASE("Serializer: persistência em disco (B-3)") {
    const EvidencePackage pkg = buildPackage();
    const std::string expected = serialize(pkg);
    const std::string path = "aamp_evidence_selfcheck.aamp";
    CHECK(writeToFile(pkg, path));
    std::ifstream f(path, std::ios::binary);
    CHECK(f.good());
    const std::string onDisk((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();
    CHECK(onDisk == expected);
    std::remove(path.c_str());
}

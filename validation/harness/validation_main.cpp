// AAMP - validation/harness/validation_main.cpp
// Scientific Validation Harness — Etapa 0 (infraestrutura mínima permanente).
//
// Objetivo desta etapa: estabelecer o ponto de entrada permanente da validação científica,
// integrado ao CTest, reutilizando a infraestrutura já existente do projeto — SEM criar
// abstrações, parser JSON próprio, gerador ou escritor WAV novos, e SEM casos científicos.
//
// Reuso (nada duplicado):
//   - StimulusGenerator (aamp_core)  -> geração determinística dos sinais do dataset.
//   - ExportEngine::exportWAV (aamp_core) -> materialização de áudio sob demanda.
//   - tests/TestRunner.h             -> relato do self-test (mesmo framework dos testes).
//
// O manifest.json é apenas o PRIMEIRO formato operacional do Scientific Validation Dataset;
// sua estrutura poderá evoluir nas próximas etapas. Por isso, nesta etapa ele NÃO é parseado:
// o harness apenas confirma sua presença como âncora do dataset. Os casos científicos e a
// leitura estruturada do manifest pertencem às etapas seguintes.
#include "TestRunner.h"
#include "stimulus/StimulusGenerator.h"
#include "export/ExportEngine.h"

#include <fstream>
#include <string>
#include <vector>
#include <cstdio>

// Diretório do dataset injetado pelo CMake (compile definition). Fallback relativo permite
// compilar/rodar fora do CMake a partir da raiz do projeto.
#ifndef AAMP_VALIDATION_DATASET_DIR
#define AAMP_VALIDATION_DATASET_DIR "validation/dataset"
#endif

namespace {

// Estímulo determinístico de referência para o self-test (seno puro, sem PRNG).
constexpr double kSelfTestFs = 48000.0;
constexpr aamp::StimulusId kSelfTestStimulus = aamp::StimulusId::ST03_PureSine;

} // namespace

// A infraestrutura precisa localizar o dataset. O manifest é a âncora do formato operacional.
TEST_CASE("infra: manifest do dataset presente e não-vazio") {
    const std::string manifestPath = std::string(AAMP_VALIDATION_DATASET_DIR) + "/manifest.json";
    std::ifstream f(manifestPath, std::ios::binary);
    CHECK(f.good());
    const std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    CHECK(!content.empty());
}

// O dataset será gerado por geradores determinísticos: a reprodutibilidade é pré-requisito
// da validação científica. Reutiliza StimulusGenerator (nenhum gerador novo é criado).
TEST_CASE("infra: geração determinística via StimulusGenerator") {
    aamp::StimulusGenerator genA(kSelfTestFs);
    aamp::StimulusGenerator genB(kSelfTestFs);
    const aamp::Stimulus a = genA.getStimulus(kSelfTestStimulus);
    const aamp::Stimulus b = genB.getStimulus(kSelfTestStimulus);
    CHECK(!a.samples.empty());
    CHECK(a.samples.size() == b.samples.size());
    CHECK(a.samples == b.samples);   // idêntico bit-a-bit entre execuções
}

// Os arquivos de áudio serão produzidos quando necessários durante a validação. Reutiliza a
// exportação WAV headless existente (ExportEngine::exportWAV) — nenhum escritor novo é criado.
TEST_CASE("infra: materialização WAV via ExportEngine::exportWAV") {
    aamp::StimulusGenerator gen(kSelfTestFs);
    const aamp::Stimulus s = gen.getStimulus(kSelfTestStimulus);

    const std::string outPath = "aamp_validation_selfcheck.wav";
    aamp::ExportEngine exporter;
    const aamp::ExportStatus st = exporter.exportWAV(s.samples, kSelfTestFs, 1, outPath);
    CHECK(st.ok);

    std::ifstream produced(outPath, std::ios::binary | std::ios::ate);
    CHECK(produced.good());
    CHECK(produced.tellg() > 0);
    produced.close();
    std::remove(outPath.c_str());
}

int main() {
    return test::runAll();
}

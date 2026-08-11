// AAMP - tests/test_dashboard.cpp
// M-06 view-model (camada de dados, headless): TC-040 (6 secoes), TC-042 (estado vazio),
// estados por item FR-031 (N/A, baixa-confiabilidade). Render JUCE na maquina-alvo.
#include "TestRunner.h"
#include "LoopbackHarness.h"
#include "../src/dashboard/DashboardModel.h"
#include "../src/analysis/AnalysisEngine.h"

using namespace aamp;
static const double FS = k::REF_SAMPLE_RATE;

// TC-040: dashboard apresenta exatamente 6 secoes (DOC-05 §3).
TEST_CASE("TC-040 Dashboard com 6 secoes (DOC-05 §3)") {
    AnalysisEngine eng(FS);
    ResultBundle b = eng.computeAllMetrics(harness::makeLoopback(FS, 0, false));
    DashboardModel dm = buildDashboardModel(b);
    CHECK(dm.sections.size() == 6);
    const char* titles[6] = {"Summary","Technical Metrics","Dynamic Behaviour",
                             "Musical Behaviour","Noise Analysis","Stereo Analysis"};
    for (int i = 0; i < 6; ++i) CHECK(dm.sections[static_cast<std::size_t>(i)].title == titles[i]);
    CHECK(!dm.isEmpty);
}

// TC-042: estado vazio antes de qualquer medicao.
TEST_CASE("TC-042 Estado vazio sem medicao (FR-031)") {
    ResultBundle empty;
    DashboardModel dm = buildDashboardModel(empty);
    CHECK(dm.isEmpty);
    CHECK(dm.sections.size() == 6);                 // estrutura presente
    for (const auto& sec : dm.sections)
        for (const auto& it : sec.items)
            CHECK(it.state == ItemState::Empty);    // itens vazios
}

// FR-031 / BR-043: crosstalk em mono -> item N/A.
TEST_CASE("Dashboard: crosstalk N/A em mono (FR-031)") {
    AnalysisEngine eng(FS);
    DashboardModel mono = buildDashboardModel(eng.computeAllMetrics(harness::makeLoopback(FS, 0, false)));
    const auto& stereoSec = mono.sections[5];
    CHECK(!stereoSec.items.empty());
    CHECK(stereoSec.items[0].state == ItemState::NotApplicable);
    CHECK(stereoSec.items[0].value == "N/A");
    // estereo -> crosstalk Normal com valor.
    DashboardModel st = buildDashboardModel(eng.computeAllMetrics(harness::makeLoopback(FS, 0, true)));
    CHECK(st.sections[5].items[0].state == ItemState::Normal);
    CHECK(st.sections[5].items[0].value != "N/A");
}

// FR-027/FR-031: baixa confiabilidade -> badge (ItemState::LowConfidence).
TEST_CASE("Dashboard: baixa confiabilidade vira badge (FR-027)") {
    ResultBundle b;
    MetricResult m; m.id = "MS-10"; m.name = "SNR";
    m.scalars.push_back({"SNR_dB", 18.0, "dB", Quality::LowConfidence, true});
    b.metrics.push_back(m);
    DashboardModel dm = buildDashboardModel(b);
    // SNR aparece em Summary e Noise; ambos com estado LowConfidence.
    bool found = false;
    for (const auto& sec : dm.sections)
        for (const auto& it : sec.items)
            if (it.label == "SNR" && it.state == ItemState::LowConfidence) found = true;
    CHECK(found);
}

// TC-066 -- V3 (AAMP-HG-20260811-001): o dashboard e superficie de APRESENTACAO.
// O escalar e um coeficiente de variacao (0% = perfeito); o rotulo "Repetition Consistency"
// afirmava o oposto do que a grandeza mede. Rotulo neutralizado; chave e valor intactos.
TEST_CASE("TC-066 Dashboard rotula MS-13 como Repetition Variation (AAMP-HG-20260811-001)") {
    AnalysisEngine eng(FS);
    ResultBundle b = eng.computeAllMetrics(harness::makeLoopback(FS, 0, false));
    DashboardModel dm = buildDashboardModel(b);
    bool novo = false, antigo = false;
    for (const auto& sec : dm.sections)
        for (const auto& it : sec.items) {
            if (it.label == "Repetition Variation")   novo = true;
            if (it.label == "Repetition Consistency") antigo = true;
        }
    CHECK(novo);
    CHECK(!antigo);
}

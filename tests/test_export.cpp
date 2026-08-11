// AAMP - tests/test_export.cpp
// M-07 Export Engine (serializacao headless): TC-050/051/052/053(parte)/055 (AC-M07-01..03).
#include "TestRunner.h"
#include "LoopbackHarness.h"
#include "../src/export/ExportEngine.h"
#include "../src/analysis/AnalysisEngine.h"
#include <fstream>
#include <sstream>
#include <cstdint>
#include <cstring>

using namespace aamp;
static const double FS = k::REF_SAMPLE_RATE;

static std::string readFile(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    std::stringstream ss; ss << f.rdbuf(); return ss.str();
}

static ResultBundle loopbackBundle(ChannelMode mode) {
    AnalysisEngine eng(FS);
    ResultBundle b = eng.computeAllMetrics(harness::makeLoopback(FS, 0, mode == ChannelMode::Stereo));
    b.meta.deviceName = "Loopback Virtual";
    b.meta.sampleRate = FS;
    b.meta.mode = mode;
    b.meta.timestamp = "2026-06-16 14:30:22";
    return b;
}

// TC-050: CSV de curvas valido (metadados + colunas + dados; UTF-8; ponto decimal).
TEST_CASE("TC-050 CSV de curvas valido (DOC-10 §3.1)") {
    ResultBundle b = loopbackBundle(ChannelMode::Mono);
    ExportEngine ex;
    const std::string path = "/tmp/aamp_curves.csv";
    ExportStatus st = ex.exportCurvesCSV(b, path);
    CHECK(st.ok);
    std::string content = readFile(path);
    CHECK(!content.empty());
    std::istringstream is(content);
    std::string l1, l2; std::getline(is, l1); std::getline(is, l2);
    CHECK(l1.rfind("# AAMP Export", 0) == 0);                 // cabecalho de metadados
    CHECK(l1.find("SR: 48000 Hz") != std::string::npos);
    CHECK(l2 == "Frequency_Hz,Magnitude_dB,Phase_deg,GroupDelay_ms"); // linha de colunas
    std::string row; std::getline(is, row);
    int commas = 0; for (char c : row) if (c == ',') commas++;
    CHECK(commas == 3);                                       // 4 campos
    CHECK(row.find('.') != std::string::npos);               // separador decimal ponto
    CHECK(content.find(',') != std::string::npos);
}

// TC-051: N/A para metrica indisponivel (crosstalk em mono).
TEST_CASE("TC-051 N/A para metrica indisponivel (BR-043)") {
    ResultBundle b = loopbackBundle(ChannelMode::Mono);
    ExportEngine ex;
    const std::string path = "/tmp/aamp_metrics.csv";
    ExportStatus st = ex.exportScalarsCSV(b, path);
    CHECK(st.ok);
    std::string content = readFile(path);
    CHECK(content.find("Crosstalk_dB=N/A") != std::string::npos);  // mono -> N/A literal
    CHECK(content.find("SNR_dB=") != std::string::npos);
}

// Baixa confiabilidade: sufixo '*' e nota explicativa (DOC-10 §3.2).
TEST_CASE("Export: baixa confiabilidade marcada com * e nota") {
    ResultBundle b;
    b.meta.timestamp = "2026-06-16 14:30:22";
    MetricResult m; m.id = "MS-10"; m.name = "SNR";
    m.scalars.push_back({"SNR_dB", 42.1, "dB", Quality::LowConfidence, true});
    b.metrics.push_back(m);
    ExportEngine ex;
    CHECK(ExportEngine::formatScalar(m.scalars[0]) == "42.100*");
    const std::string path = "/tmp/aamp_lowconf.csv";
    ex.exportScalarsCSV(b, path);
    std::string content = readFile(path);
    CHECK(content.find("SNR_dB=42.100*") != std::string::npos);
    CHECK(content.find("# * = baixa confiabilidade") != std::string::npos);
}

// TC-052: WAV IR valido (32-bit float; SR correto).
TEST_CASE("TC-052 WAV IR valido: 32-bit float, SR correto (DOC-10 §4)") {
    std::vector<double> ir(256, 0.0); ir[10] = 1.0; ir[11] = -0.5;
    ExportEngine ex;
    const std::string path = "/tmp/aamp_ir.wav";
    ExportStatus st = ex.exportWAV(ir, FS, 1, path);
    CHECK(st.ok);
    std::string w = readFile(path);
    CHECK(w.size() > 44);
    CHECK(w.compare(0, 4, "RIFF") == 0);
    CHECK(w.compare(8, 4, "WAVE") == 0);
    CHECK(w.compare(12, 4, "fmt ") == 0);
    // audioFormat (offset 20) = 3 (IEEE float)
    std::uint16_t fmtCode = static_cast<unsigned char>(w[20]) | (static_cast<unsigned char>(w[21]) << 8);
    CHECK(fmtCode == 3);
    // sampleRate (offset 24)
    std::uint32_t sr = static_cast<unsigned char>(w[24]) | (static_cast<unsigned char>(w[25]) << 8)
                     | (static_cast<unsigned char>(w[26]) << 16) | (static_cast<unsigned char>(w[27]) << 24);
    CHECK(sr == 48000u);
    // bitsPerSample (offset 34)
    std::uint16_t bits = static_cast<unsigned char>(w[34]) | (static_cast<unsigned char>(w[35]) << 8);
    CHECK(bits == 32);
    CHECK(w.find("ISFT") != std::string::npos); // INFO metadata
}

// TC-055: falha de destino preserva dados (ER-040).
TEST_CASE("TC-055 Falha de destino preserva dados (ER-040)") {
    ResultBundle b = loopbackBundle(ChannelMode::Mono);
    const std::size_t before = b.metrics.size();
    ExportEngine ex;
    ExportStatus st = ex.exportScalarsCSV(b, "/diretorio_inexistente_aamp/x.csv");
    CHECK(!st.ok);                          // exportacao aborta
    CHECK(!st.message.empty());             // mensagem amigavel
    CHECK(b.metrics.size() == before);      // dados preservados em memoria
}

// TC-053 (parte): modelo do relatorio com as 6 secoes (render PDF na maquina-alvo).
TEST_CASE("TC-053 Modelo de relatorio com 6 secoes (DOC-10 §5.1)") {
    ResultBundle b = loopbackBundle(ChannelMode::Stereo);
    ExportEngine ex;
    ReportModel rm = ex.buildReportModel(b);
    CHECK(rm.sections.size() == 6);
    const char* expected[6] = {"Summary","Technical Metrics","Dynamic Behaviour",
                               "Musical Behaviour","Noise Analysis","Stereo Analysis"};
    for (int i = 0; i < 6; ++i) CHECK(rm.sections[static_cast<std::size_t>(i)].title == expected[i]);
    // estereo -> secao Stereo tem crosstalk; mono teria 'N/A - medicao mono'.
    CHECK(rm.sections[5].lines[0].find("Crosstalk") != std::string::npos);

    // mesma estrutura via IPdfWriter falso (captura o modelo).
    struct FakePdf : IPdfWriter {
        ReportModel captured; bool called = false;
        bool write(const ReportModel& m, const std::string&) override { captured = m; called = true; return true; }
    } fake;
    ExportStatus st = ex.exportPDF(b, fake, "/tmp/aamp_report.pdf");
    CHECK(st.ok);
    CHECK(fake.called);
    CHECK(fake.captured.sections.size() == 6);
}

// Nomenclatura DOC-10 §7.
TEST_CASE("Export: nomenclatura aamp_{tipo}_{data}_{hora}.{ext}") {
    CHECK(ExportEngine::makeFilename("freqresponse", "csv", "2026-06-15 14:30:22")
          == "aamp_freqresponse_20260615_143022.csv");
    CHECK(ExportEngine::makeFilename("ir", "wav", "2026-06-15 14:30:22")
          == "aamp_ir_20260615_143022.wav");
}

// TC-067 -- V4 (AAMP-HG-20260811-001): um placeholder nao pode ser EXPORTADO como medicao.
// Antes, o CSV escalar publicava "Reproducibility_pct=100.000" -- um numero constante, jamais
// medido, indistinguivel de um resultado. Com valuePresent=false o contrato ja existente
// (BR-043) produz o literal N/A, sem inventar nenhuma representacao nova.
TEST_CASE("TC-067 CSV escalar exporta Reproducibility_pct=N/A (AAMP-HG-20260811-001)") {
    ResultBundle b = loopbackBundle(ChannelMode::Mono);
    ExportEngine ex;
    const std::string path = "/tmp/aamp_metrics_v4.csv";
    ExportStatus st = ex.exportScalarsCSV(b, path);
    CHECK(st.ok);
    std::string content = readFile(path);
    CHECK(content.find("Reproducibility_pct=N/A") != std::string::npos);
    CHECK(content.find("Reproducibility_pct=100.000") == std::string::npos);
    // Controle negativo: o escalar vizinho da MESMA familia continua exportando numero.
    CHECK(content.find("AggregateLinearity_dB=N/A") == std::string::npos);
}

// TC-067 -- V3: o relatorio tambem e superficie de apresentacao.
TEST_CASE("TC-067 Relatorio rotula MS-13 como Repetition Variation (AAMP-HG-20260811-001)") {
    ResultBundle b = loopbackBundle(ChannelMode::Mono);
    ExportEngine ex;
    ReportModel rm = ex.buildReportModel(b);
    bool novo = false, antigo = false;
    for (const auto& sec : rm.sections)
        for (const auto& line : sec.lines) {
            if (line.rfind("Repetition Variation:", 0) == 0)   novo = true;
            if (line.rfind("Repetition Consistency:", 0) == 0) antigo = true;
        }
    CHECK(novo);
    CHECK(!antigo);
}

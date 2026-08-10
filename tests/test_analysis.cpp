// AAMP - tests/test_analysis.cpp
// M-05 Analysis Engine + DSP: TC-030..TC-037 (DOC-11 §3, AC-M05-01..05).
#include "TestRunner.h"
#include "LoopbackHarness.h"
#include "../src/analysis/AnalysisEngine.h"
#include "../src/analysis/Fft.h"
#include "../src/analysis/Deconvolution.h"
#include "../src/common/Constants.h"
#include "../src/export/ExportEngine.h"          // TC-047: prova de invariancia da exportacao
#include "../src/dashboard/DashboardModel.h"     // TC-047: prova de invariancia do view-model
#include <cmath>
#include <fstream>
#include <sstream>

using namespace aamp;
static const double FS = k::REF_SAMPLE_RATE;

// FFT round-trip (base de confianca do nucleo DSP).
TEST_CASE("DSP FFT round-trip (forward+inverse == original)") {
    std::vector<double> sig(1024);
    for (int i = 0; i < 1024; ++i) sig[static_cast<std::size_t>(i)] = std::sin(2.0 * 3.14159265 * 5.0 * i / 1024.0);
    auto X = Fft::forwardReal(sig, 1024);
    auto back = Fft::inverseToReal(X);
    double maxErr = 0.0;
    for (int i = 0; i < 1024; ++i) maxErr = std::max(maxErr, std::fabs(back[static_cast<std::size_t>(i)] - sig[static_cast<std::size_t>(i)]));
    CHECK(maxErr < 1e-9);
}

// TC-031: FFT de seno puro -> pico no bin correto (±1 bin) e magnitude (±0.1 dB).
TEST_CASE("TC-031 FFT seno puro: pico no bin correto, mag +-0.1 dB") {
    const int N = k::FFT_ANALYSIS;
    const int targetBin = 1365;
    const double f = static_cast<double>(targetBin) * FS / N; // freq exata de bin
    const double A = dbfsToLinear(-6.0);
    std::vector<double> sig(static_cast<std::size_t>(N));
    for (int i = 0; i < N; ++i) sig[static_cast<std::size_t>(i)] = A * std::sin(2.0 * 3.14159265358979 * f * i / FS);
    const int pb = AnalysisEngine::peakBin(sig, N);
    CHECK(std::abs(pb - targetBin) <= 1);
    // magnitude no pico recuperada (via primitiva publica de THD nao; recomputa direto)
    // reusa peakBin + reconstrucao de magnitude por Hann.
    std::vector<double> win(static_cast<std::size_t>(N));
    double cg = 0.0;
    for (int i = 0; i < N; ++i) { win[static_cast<std::size_t>(i)] = 0.5 - 0.5 * std::cos(2.0*3.14159265358979*i/(N-1)); cg += win[static_cast<std::size_t>(i)]; }
    std::vector<double> buf(static_cast<std::size_t>(N));
    for (int i = 0; i < N; ++i) buf[static_cast<std::size_t>(i)] = sig[static_cast<std::size_t>(i)] * win[static_cast<std::size_t>(i)];
    auto X = Fft::forwardReal(buf, N);
    const double mag = std::abs(X[static_cast<std::size_t>(pb)]) / cg * 2.0;
    CHECK_NEAR(20.0 * std::log10(mag / A), 0.0, 0.1);
}

// TC-032: THD de seno puro loopback < 0.001%.
TEST_CASE("TC-032 THD seno puro loopback < 0.001 porcento") {
    const int N = k::FFT_ANALYSIS;
    const double f = 1000.0;
    const double A = dbfsToLinear(-6.0);
    std::vector<double> sig(static_cast<std::size_t>(N));
    for (int i = 0; i < N; ++i) sig[static_cast<std::size_t>(i)] = A * std::sin(2.0 * 3.14159265358979 * f * i / FS);
    const double thd = AnalysisEngine::thdPercent(sig, f, FS, N);
    CHECK(thd < 0.001);
}
TEST_CASE("EXP-B THD CAPTURADO")
{
    CapturedSet cap = harness::makeLoopback(FS, 0, false);

    AnalysisEngine eng(FS);

    ResultBundle b = eng.computeAllMetrics(cap);

    const MetricResult* r = b.find("MS-05");

    CHECK(r != nullptr);

    if (r)
    {
        printf("\n\n===== EXP-B ST03 CAPTURADO =====\n");

        for (const auto& sc : r->scalars)
        {
            if (sc.valuePresent)
            {
                printf("%s = %.12f %s\n",
                       sc.key.c_str(),
                       sc.value,
                       sc.unit.c_str());
            }
            else
            {
                printf("%s = N/A\n",
                       sc.key.c_str());
            }
        }

        printf("===============================\n\n");
    }

}
// TC-033: SNR de referencia -80 dBFS -> 80 dB ±0.5.
TEST_CASE("TC-033 SNR referencia 80 dB +-0.5") {
    const int N = 48000;
    std::vector<double> sig(static_cast<std::size_t>(N), 0.0), noise(static_cast<std::size_t>(N), 0.0);
    // sinal RMS = 1.0 (0 dBFS de referencia); ruido RMS = 1e-4 (-80 dBFS).
    for (int i = 0; i < N; ++i) sig[static_cast<std::size_t>(i)] = std::sqrt(2.0) * std::sin(2.0*3.14159265358979*1000.0*i/FS); // RMS 1.0
    harness::deterministicNoise(1, -80.0); // (warm-up determinístico irrelevante)
    std::vector<double> nz = harness::deterministicNoise(static_cast<std::size_t>(N), -80.0);
    const double snr = AnalysisEngine::snrDb(sig, nz);
    CHECK_NEAR(snr, 80.0, 0.5);
}

// TC-034: determinismo de metricas (mesma entrada -> idêntico).
TEST_CASE("TC-034 Determinismo de metricas") {
    CapturedSet cap = harness::makeLoopback(FS, 0, false);
    AnalysisEngine eng(FS);
    ResultBundle a = eng.computeAllMetrics(cap);
    ResultBundle b = eng.computeAllMetrics(cap);
    CHECK(a.metrics.size() == b.metrics.size());
    bool same = true;
    for (std::size_t i = 0; i < a.metrics.size(); ++i)
        for (std::size_t j = 0; j < a.metrics[i].scalars.size() && j < b.metrics[i].scalars.size(); ++j)
            if (a.metrics[i].scalars[j].value != b.metrics[i].scalars[j].value) same = false;
    CHECK(same);
}

// TC-035: deconvolucao com bin de energia zero -> regularizacao, sem NaN/Inf.
TEST_CASE("TC-035 Deconvolucao bin zero sem NaN/Inf") {
    const int N = 4096;
    // X = tom puro (a maioria dos bins tem energia ~0) -> divisao instavel sem regularizacao.
    std::vector<double> x(static_cast<std::size_t>(N)), y(static_cast<std::size_t>(N));
    for (int i = 0; i < N; ++i) { x[static_cast<std::size_t>(i)] = std::sin(2.0*3.14159265*50.0*i/N); y[static_cast<std::size_t>(i)] = x[static_cast<std::size_t>(i)]; }
    auto X = Fft::forwardReal(x, N);
    auto Y = Fft::forwardReal(y, N);
    auto H = deconvolveTikhonov(Y, X, {});
    bool allFinite = true;
    for (const auto& h : H) if (!std::isfinite(h.real()) || !std::isfinite(h.imag())) allFinite = false;
    CHECK(allFinite);
}

// TC-036: crosstalk calculado em estereo; N/A em mono.
TEST_CASE("TC-036 Crosstalk estereo calculado / mono N/A") {
    AnalysisEngine eng(FS);
    // mono
    ResultBundle mono = eng.computeAllMetrics(harness::makeLoopback(FS, 0, false));
    const MetricResult* m = mono.find("MS-11");
    CHECK(m != nullptr);
    CHECK(m->quality == Quality::NotApplicable);
    CHECK(!m->scalars.empty() && m->scalars[0].valuePresent == false); // -> 'N/A' na exportacao
    // estereo
    ResultBundle st = eng.computeAllMetrics(harness::makeLoopback(FS, 0, true));
    const MetricResult* s = st.find("MS-11");
    CHECK(s != nullptr);
    CHECK(s->quality == Quality::Normal);
    CHECK(!s->scalars.empty() && std::isfinite(s->scalars[0].value));
}

// TC-037: group delay constante para fase linear (atraso puro) ±2 samples.
TEST_CASE("TC-037 Group delay constante p/ fase linear +-2 samples") {
    const int D = 64; // atraso puro de 64 samples -> fase linear
    CapturedSet cap = harness::makeLoopback(FS, D, false);
    AnalysisEngine eng(FS);
    ResultBundle b = eng.computeAllMetrics(cap);
    const MetricResult* gd = b.find("MS-03");
    CHECK(gd != nullptr && !gd->curves.empty());
    // media de group delay na faixa 500 Hz - 5 kHz deve ~ D/fs.
    const Curve& c = gd->curves[0];
    double sum = 0.0; int cnt = 0;
    for (std::size_t i = 0; i < c.x.size(); ++i)
        if (c.x[i] >= 500.0 && c.x[i] <= 5000.0) { sum += c.y[i]; cnt++; }
    const double meanMs = (cnt > 0) ? sum / cnt : 0.0;
    const double expectedMs = static_cast<double>(D) / FS * 1000.0;
    const double tolMs = 2.0 / FS * 1000.0; // ±2 samples
    CHECK_NEAR(meanMs, expectedMs, tolMs);
}

// TC-038 (P1-001): a estimativa de piso de ruido do MS-08 deve ser ENERGETICAMENTE correta
// para ruido de banda larga (normalizacao ENBW / Parseval), e nao a calibracao de amplitude
// de TOM por ganho coerente. Ver docs/laboratory/noise-floor-definition.md (secoes 2, 3 e 7),
// achado N1-R1 e restricao arquitetural AR-01.
//
// Oraculo INDEPENDENTE do codigo de producao: o nivel esperado e derivado aqui, do nivel RMS
// total do ruido determinístico sintetizado, restrito a banda normativa:
//
//     L_esperado(dBFS) = 20*log10(RMS_total) + 10*log10( bins_na_banda / (N/2) )
//
// (ruido branco distribui potencia uniformemente entre os bins; a fracao de bins dentro de
// 20 Hz-20 kHz e a fracao de potencia que o Noise Floor normativo deve reter).
//
// O teste FALHA se a normalizacao por ganho coerente for reintroduzida: aquela escala produz
// um valor 10*log10(2*ENBW) = +4.77 dB acima do nivel RMS (Hann), muito alem da tolerancia.
TEST_CASE("TC-038 MS-08 piso de ruido energeticamente correto (ENBW/Parseval) +-0.1 dB") {
    const int NFFT = 4096;              // espelha a janela de analise de AnalysisEngine::noiseFloor
    const double TOL_DB = 0.1;          // mesma tolerancia de calibracao espectral de TC-031

    // Fracao de potencia branca retida pela banda normativa 20 Hz - 20 kHz.
    int inBand = 0;
    for (int b = 0; b < NFFT / 2; ++b) {
        const double f = static_cast<double>(b) * FS / NFFT;
        if (f >= k::BAND_LOW_HZ && f <= k::BAND_HIGH_HZ) ++inBand;
    }
    CHECK(inBand > 0 && inBand < NFFT / 2);
    const double bandDb = 10.0 * std::log10(static_cast<double>(inBand) / static_cast<double>(NFFT / 2));

    // ENBW da janela Hann, calculada AQUI (oraculo independente) apenas para caracterizar o
    // viés da normalizacao antiga, que este teste deve rejeitar.
    double s1 = 0.0, s2 = 0.0;
    for (int i = 0; i < NFFT; ++i) {
        const double w = 0.5 - 0.5 * std::cos(2.0 * 3.14159265358979 * i / (NFFT - 1));
        s1 += w; s2 += w * w;
    }
    const double enbwBins = static_cast<double>(NFFT) * s2 / (s1 * s1);   // Hann ~ 1.5
    const double viesAntigoDb = 10.0 * std::log10(2.0 * enbwBins);        // ~ +4.77 dB
    CHECK_NEAR(enbwBins, 1.5, 0.01);

    const double niveis[3] = { -60.0, -95.0, -120.0 };   // nivel RMS TOTAL do ruido sintetico
    double medidos[3] = { 0.0, 0.0, 0.0 };
    double rmsDeclarados[3] = { 0.0, 0.0, 0.0 };
    double piorErro = 0.0;

    for (int i = 0; i < 3; ++i) {
        CapturedSet cap = harness::makeLoopback(FS, 0, false, niveis[i]);
        // Nivel RMS total efetivamente sintetizado (declarado explicitamente pelo teste).
        auto it = cap.captured.find(StimulusId::ST09_Silence);
        CHECK(it != cap.captured.end() && !it->second.empty());
        double acc = 0.0;
        for (double v : it->second) acc += v * v;
        const double rmsTotal = std::sqrt(acc / static_cast<double>(it->second.size()));
        rmsDeclarados[i] = 20.0 * std::log10(rmsTotal);
        CHECK_NEAR(rmsDeclarados[i], niveis[i], 0.01);   // o harness gera o nivel pedido

        const double esperado = rmsDeclarados[i] + bandDb;

        // Caminho OFICIAL do MS-08 (computeAllMetrics -> noiseFloor).
        AnalysisEngine eng(FS);
        ResultBundle b = eng.computeAllMetrics(cap);
        const MetricResult* nf = b.find("MS-08");
        CHECK(nf != nullptr);
        bool achou = false;
        if (nf != nullptr)
            for (const Scalar& s : nf->scalars)
                if (s.key == "NoiseFloor_dBFS") { medidos[i] = s.value; achou = true; break; }
        CHECK(achou);

        CHECK_NEAR(medidos[i], esperado, TOL_DB);
        piorErro = std::max(piorErro, std::fabs(medidos[i] - esperado));

        // Anti-regressao explicita: rejeita a normalizacao por ganho coerente (viés +4.77 dB).
        CHECK(std::fabs(medidos[i] - (esperado + viesAntigoDb)) > TOL_DB);
    }
    CHECK(piorErro <= TOL_DB);

    // Invariancia de escala: a diferenca entre os niveis estimados deve acompanhar a diferenca
    // entre os niveis de entrada (o estimador nao pode ter termo dependente do nivel).
    for (int i = 0; i + 1 < 3; ++i) {
        const double dEntrada = rmsDeclarados[i] - rmsDeclarados[i + 1];
        const double dEstimado = medidos[i] - medidos[i + 1];
        CHECK_NEAR(dEstimado, dEntrada, 0.01);
    }
}

// TC-039 (P1-002): estados de qualidade do MS-08 em silencio degenerado.
// Fundamento: noise-floor-definition.md sec.7 (Estabilidade / Rastreabilidade), N4 e o principio D4.
// Mapeamento aprovado: ST-09 ausente -> Error; nao caracterizavel -> NotApplicable; observacao
// insuficiente -> LowConfidence. "Silencio sujo" (DV-4) NAO faz parte deste item -> Pilar 2.
//
// O teste tambem fixa duas nao-regressoes exigidas pela autorizacao:
//   (a) captura valida permanece Normal e com o MESMO valor do P1-001 (TC-038 cobre o numero);
//   (b) o comportamento do MS-09 NAO muda (Q4), inclusive no caso de silencio digital.
namespace {
// Localiza um escalar do MS-08 no bundle. Devolve nullptr se ausente.
const Scalar* nfScalar(const ResultBundle& b, const char* key) {
    const MetricResult* m = b.find("MS-08");
    if (m == nullptr) return nullptr;
    for (const Scalar& s : m->scalars) if (s.key == key) return &s;
    return nullptr;
}
} // namespace

TEST_CASE("TC-039 MS-08 estados de qualidade em silencio degenerado") {
    AnalysisEngine eng(FS);
    const int NFFT = 4096, HOP = 2048;

    // --- Referencia: captura valida -> Normal, presente, com quadros suficientes ---
    {
        ResultBundle b = eng.computeAllMetrics(harness::makeLoopback(FS, 0, false));
        const MetricResult* m = b.find("MS-08");
        CHECK(m != nullptr && m->quality == Quality::Normal);
        const Scalar* v = nfScalar(b, "NoiseFloor_dBFS");
        const Scalar* f = nfScalar(b, "NoiseFloor_frames");
        CHECK(v != nullptr && v->quality == Quality::Normal && v->valuePresent);
        CHECK(f != nullptr && f->value >= static_cast<double>(k::NOISE_AVG_FRAMES));
        CHECK(v != nullptr && v->value < -1.0);   // valor real, nunca o clamp
    }

    // --- C0: ST-09 ausente -> Error (comportamento preservado) ---
    {
        CapturedSet cap = harness::makeLoopback(FS, 0, false);
        cap.captured.erase(StimulusId::ST09_Silence);
        ResultBundle b = eng.computeAllMetrics(cap);
        const MetricResult* m = b.find("MS-08");
        CHECK(m != nullptr && m->quality == Quality::Error);
        CHECK(nfScalar(b, "NoiseFloor_dBFS") == nullptr);   // Error nao emite escalar
    }

    // --- C1: ST-09 presente porem VAZIO -> nada observado -> NotApplicable ---
    {
        CapturedSet cap = harness::makeLoopback(FS, 0, false);
        cap.captured[StimulusId::ST09_Silence].clear();
        ResultBundle b = eng.computeAllMetrics(cap);
        const MetricResult* m = b.find("MS-08");
        const Scalar* v = nfScalar(b, "NoiseFloor_dBFS");
        const Scalar* f = nfScalar(b, "NoiseFloor_frames");
        CHECK(m != nullptr && m->quality == Quality::NotApplicable);
        CHECK(v != nullptr && v->quality == Quality::NotApplicable && !v->valuePresent);
        CHECK(f != nullptr && f->value == 0.0 && f->quality == Quality::Normal && f->valuePresent);
    }

    // --- C2: ST-09 mais curto que uma janela -> nada observado -> NotApplicable ---
    {
        CapturedSet cap = harness::makeLoopback(FS, 0, false);
        cap.captured[StimulusId::ST09_Silence] =
            harness::deterministicNoise(static_cast<std::size_t>(NFFT - 1), -95.0);
        ResultBundle b = eng.computeAllMetrics(cap);
        const MetricResult* m = b.find("MS-08");
        const Scalar* f = nfScalar(b, "NoiseFloor_frames");
        CHECK(m != nullptr && m->quality == Quality::NotApplicable);
        CHECK(f != nullptr && f->value == 0.0);
    }

    // --- C3: poucos quadros (5 < 16) -> observacao insuficiente -> LowConfidence ---
    {
        CapturedSet cap = harness::makeLoopback(FS, 0, false);
        const std::size_t n5 = static_cast<std::size_t>(NFFT + 4 * HOP);   // exatamente 5 janelas
        cap.captured[StimulusId::ST09_Silence] = harness::deterministicNoise(n5, -95.0);
        ResultBundle b = eng.computeAllMetrics(cap);
        const MetricResult* m = b.find("MS-08");
        const Scalar* v = nfScalar(b, "NoiseFloor_dBFS");
        const Scalar* f = nfScalar(b, "NoiseFloor_frames");
        CHECK(f != nullptr && f->value == 5.0);
        CHECK(m != nullptr && m->quality == Quality::LowConfidence);
        // observacao insuficiente reduz a CONFIANCA, mas o valor continua sendo uma medicao
        CHECK(v != nullptr && v->quality == Quality::LowConfidence && v->valuePresent);
    }

    // --- C4: silencio digital bit-exato -> piso nao-mensuravel -> NotApplicable ---
    // (condicao efetivamente observada em hardware: Helix em preset vazio)
    {
        CapturedSet cap = harness::makeLoopback(FS, 0, false);
        std::vector<double>& st09 = cap.captured[StimulusId::ST09_Silence];
        std::fill(st09.begin(), st09.end(), 0.0);
        ResultBundle b = eng.computeAllMetrics(cap);
        const MetricResult* m = b.find("MS-08");
        const Scalar* v = nfScalar(b, "NoiseFloor_dBFS");
        const Scalar* f = nfScalar(b, "NoiseFloor_frames");
        CHECK(m != nullptr && m->quality == Quality::NotApplicable);
        CHECK(v != nullptr && !v->valuePresent);
        // discrimina C4 de C1/C2: houve observacao (quadros > 0), o piso e que e nulo
        CHECK(f != nullptr && f->value > 0.0);

        // D3 (P1-004): a sentinela -300 dBFS PERMANECE no campo do MS-08 -- nao foi removida.
        CHECK(v != nullptr && std::fabs(v->value - (-300.0)) < 1e-9);

        // Guarda Q4 do P1-002 -- RETIRADA pelo P1-004, que e o item autorizado a alterar o MS-09.
        // Ela travava o Dynamic Range no comportamento anterior (Normal, > 200 dB) apenas para
        // impedir que o P1-002 o alterasse por efeito colateral; era uma restricao de ESCOPO
        // TEMPORAL daquele item, nao uma propriedade do MS-08. O contrato vigente do MS-09 para
        // esta mesma condicao esta verificado em TC-046 caso (5). O objeto do TC-039 -- os estados
        // de qualidade do MS-08 -- permanece integralmente verificado acima.
    }
}

// TC-041 (F9): o teto do MS-09 deve ser INVARIANTE a latencia da cadeia.
// A captura chega deslocada (loopback tipico ~913 amostras). Fatiar em janelas fixas de 100 ms a
// partir da amostra 0 faz cada janela misturar degraus adjacentes; como a progressao do ST-08 e
// monotonica, o vies e sistematico e sempre REDUZ o teto. O MS-09 passa a alinhar por correlacao
// de envelopes contra a fonte emitida -- mesma convencao ja empregada pelo MS-13.
//
// O caso prova DUAS coisas separadamente:
//   (a) controle POSITIVO -- com o alinhamento, o teto nao depende do offset;
//   (b) controle NEGATIVO -- o fatiamento SEM lag, reimplementado aqui de forma independente do
//       codigo de producao, reproduz -0,3157 dB (913) e -0,8859 dB (2400).
// (b) nao usa nenhuma funcao do produto: e um oraculo proprio do teste.
namespace {

// Oraculo independente: replica o fatiamento ANTIGO (sem lag) apenas para o controle negativo.
double tetoSemAlinhamento(const std::vector<double>& cap, double fs) {
    const int seg = static_cast<int>(0.100 * fs + 0.5);
    double best = 0.0;
    for (int s = 0; s < 8; ++s) {
        const std::size_t a = static_cast<std::size_t>(s * seg);
        std::size_t b = static_cast<std::size_t>((s + 1) * seg);
        if (a >= cap.size()) continue;
        if (b > cap.size()) b = cap.size();
        double sq = 0.0, pkv = 0.0;
        for (std::size_t i = a; i < b; ++i) { sq += cap[i] * cap[i]; pkv = std::max(pkv, std::fabs(cap[i])); }
        const double rms = std::sqrt(sq / static_cast<double>(b - a));
        if (pkv < 1.0) best = std::max(best, rms);
    }
    return 20.0 * std::log10(std::max(best, 1e-30));
}

// Captura simulada: cadeia de ganho unitario, apenas atrasada de 'off' amostras.
std::vector<double> atrasar(const std::vector<double>& x, int off) {
    std::vector<double> y(x.size() + static_cast<std::size_t>(off), 0.0);
    for (std::size_t i = 0; i < x.size(); ++i) y[i + static_cast<std::size_t>(off)] = x[i];
    return y;
}

double drDe(const ResultBundle& b) {
    const MetricResult* m = b.find("MS-09");
    if (m == nullptr) return 0.0;
    for (const Scalar& s : m->scalars) if (s.key == "DynamicRange_dB") return s.value;
    return 0.0;
}

} // namespace

TEST_CASE("TC-041 MS-09 teto invariante a latencia (F9: alinhamento temporal)") {
    AnalysisEngine eng(FS);
    StimulusGenerator gen(FS);
    const std::vector<double> src = gen.getStimulus(StimulusId::ST08_DynamicStep).samples;
    // Teto ideal: degrau mais alto = seno de 1 kHz a -6 dBFS de pico -> RMS -9,0103 dBFS.
    const double tetoIdeal = 20.0 * std::log10(std::sqrt(0.5) * dbfsToLinear(-6.0));

    const int offsets[3] = { 0, 913, 2400 };
    // Vies esperado do fatiamento SEM alinhamento, derivado analiticamente da contaminacao:
    //   f = off/seg;  vies = 10*log10[ f*10^(-0.8) + (1-f)*10^(-0.6) ] - 10*log10(10^(-0.6))
    const double viesEsperado[3] = { 0.0, -0.3157, -0.8859 };

    double drRef = 0.0;
    for (int k = 0; k < 3; ++k) {
        CapturedSet cap = harness::makeLoopback(FS, 0, false);
        cap.captured[StimulusId::ST08_DynamicStep] = atrasar(src, offsets[k]);
        ResultBundle b = eng.computeAllMetrics(cap);

        // --- (a) controle POSITIVO: o MS-09 nao depende do offset ---
        const MetricResult* m = b.find("MS-09");
        CHECK(m != nullptr && m->quality == Quality::Normal);
        const double dr = drDe(b);
        if (k == 0) drRef = dr; else CHECK_NEAR(dr, drRef, 0.01);

        // --- (b) controle NEGATIVO: o fatiamento antigo reproduz o vies previsto ---
        const double tetoAntigo = tetoSemAlinhamento(cap.captured[StimulusId::ST08_DynamicStep], FS);
        CHECK_NEAR(tetoAntigo - tetoIdeal, viesEsperado[k], 0.01);

        // e o vies do algoritmo ANTIGO e efetivamente diferente de zero quando ha atraso
        if (offsets[k] > 0) CHECK(std::fabs(tetoAntigo - tetoIdeal) > 0.1);

        // --- MS-13 permanece invariante (ja alinhava): nao-regressao ---
        const MetricResult* m13 = b.find("MS-13");
        CHECK(m13 != nullptr);
        if (m13 != nullptr)
            for (const Scalar& s : m13->scalars)
                if (s.key == "LevelTransfer_step8_dB") CHECK_NEAR(s.value, 0.0, 0.01);
    }
}

// TC-043 (P1-003): o teto do MS-09 deve ser o nivel RMS IN-BAND (20 Hz - 20 kHz), medido pelo
// MESMO estimador espectral do MS-08 (Hann N=4096 + ENBW/Parseval), e NAO o RMS de banda larga
// no tempo. `dynamic-range-definition.md` sec.4 exige teto e piso na MESMA banda; sec.5 exclui
// explicitamente DC, subsonicos e ultrassonicos TAMBEM no teto.
//
// Prova tres coisas separadamente:
//   (a) cadeia limpa -> teto correto e INVARIANTE ao atraso (F9 permanece preservado);
//   (b) energia FORA da banda nao eleva o teto -- com controle NEGATIVO independente que
//       reproduz o vies do estimador de banda larga (oraculo proprio do teste);
//   (c) o gate de clipping continua TEMPORAL e ainda rejeita o degrau saturado.
TEST_CASE("TC-043 MS-09 teto restrito a banda 20 Hz-20 kHz (P1-003)") {
    AnalysisEngine eng(FS);
    StimulusGenerator gen(FS);
    const std::vector<double> src = gen.getStimulus(StimulusId::ST08_DynamicStep).samples;
    // Degrau mais alto: seno de 1 kHz a -6 dBFS de pico -> -9,0103 dBFS RMS.
    const double tetoIdeal = 20.0 * std::log10(std::sqrt(0.5) * dbfsToLinear(-6.0));

    // teto reconstruido do bundle: DynamicRange_dB + NoiseFloor_dBFS (mesmo piso nos dois)
    auto tetoDe = [](const ResultBundle& b) -> double {
        double dr = 0.0, nf = 0.0;
        const MetricResult* m9 = b.find("MS-09");
        const MetricResult* m8 = b.find("MS-08");
        if (m9 != nullptr) for (const Scalar& s : m9->scalars) if (s.key == "DynamicRange_dB")   dr = s.value;
        if (m8 != nullptr) for (const Scalar& s : m8->scalars) if (s.key == "NoiseFloor_dBFS")   nf = s.value;
        return dr + nf;
    };
    auto bundleCom = [&](const std::vector<double>& st08) {
        CapturedSet cap = harness::makeLoopback(FS, 0, false);
        cap.captured[StimulusId::ST08_DynamicStep] = st08;
        return eng.computeAllMetrics(cap);
    };

    // --- (a) cadeia limpa: teto correto e invariante ao atraso (nao-regressao de F9) ---
    {
        const int offsets[3] = { 0, 913, 2400 };
        for (int k = 0; k < 3; ++k) {
            ResultBundle b = bundleCom(atrasar(src, offsets[k]));
            const MetricResult* m = b.find("MS-09");
            CHECK(m != nullptr && m->quality == Quality::Normal);
            CHECK_NEAR(tetoDe(b), tetoIdeal, 0.01);
            // MS-13 nao regride na cadeia limpa
            const MetricResult* m13 = b.find("MS-13");
            if (m13 != nullptr)
                for (const Scalar& s : m13->scalars)
                    if (s.key == "LevelTransfer_step8_dB") CHECK_NEAR(s.value, 0.0, 0.01);
        }
    }

    // --- (b) energia fora da banda NAO eleva o teto; controle negativo independente ---
    // O oraculo tetoSemAlinhamento() replica o estimador ANTIGO (RMS de banda larga no tempo);
    // com offset 0 o alinhamento e trivial, entao ele isola exclusivamente o efeito de BANDA.
    {
        // DC de 0,02 (componente continua: fora da banda por definicao)
        std::vector<double> cDc = src;
        for (double& v : cDc) v += 0.02;
        CHECK_NEAR(tetoDe(bundleCom(cDc)), tetoIdeal, 0.01);                 // rejeitado
        CHECK_NEAR(tetoSemAlinhamento(cDc, FS), -8.9965, 0.01);              // oraculo: inflado

        // subsonico de 5 Hz a -20 dBFS
        std::vector<double> cSub = src;
        for (std::size_t i = 0; i < cSub.size(); ++i)
            cSub[i] += 0.1 * std::sin(2.0 * 3.14159265358979 * 5.0 * static_cast<double>(i) / FS);
        CHECK_NEAR(tetoDe(bundleCom(cSub)), tetoIdeal, 0.01);                // rejeitado
        CHECK_NEAR(tetoSemAlinhamento(cSub, FS), -8.8408, 0.01);             // oraculo: inflado

        // ultrassonico de 22 kHz a -20 dBFS
        std::vector<double> cUlt = src;
        for (std::size_t i = 0; i < cUlt.size(); ++i)
            cUlt[i] += 0.1 * std::sin(2.0 * 3.14159265358979 * 22000.0 * static_cast<double>(i) / FS);
        CHECK_NEAR(tetoDe(bundleCom(cUlt)), tetoIdeal, 0.01);                // rejeitado
        CHECK_NEAR(tetoSemAlinhamento(cUlt, FS), -8.8408, 0.01);             // oraculo: inflado

        // o vies do estimador antigo e efetivamente NAO-NULO (impede aprovacao por acaso)
        CHECK(tetoSemAlinhamento(cSub, FS) - tetoIdeal > 0.1);
        CHECK(tetoSemAlinhamento(cUlt, FS) - tetoIdeal > 0.1);
    }

    // --- (c) o gate de clipping continua TEMPORAL e rejeita o degrau saturado ---
    {
        std::vector<double> cSat = src;
        for (double& v : cSat) v = std::max(-1.0, std::min(1.0, v * 4.0));
        ResultBundle b = bundleCom(cSat);
        const MetricResult* m = b.find("MS-09");
        CHECK(m != nullptr && m->quality == Quality::Normal);
        // degrau 6 (-10 dBFS) x 4 = +2 dBFS satura; o maior valido e o degrau 5 (-16 dBFS x 4)
        const double tetoEsperado = 20.0 * std::log10(std::sqrt(0.5) * 4.0 * dbfsToLinear(-16.0));
        CHECK_NEAR(tetoDe(b), tetoEsperado, 0.01);
        CHECK(tetoDe(b) < tetoIdeal + 6.0);   // nao pegou um degrau saturado
    }
}

// ---------------------------------------------------------------------------------------------
// TC-046 -- MS-09: condicoes NAO CARACTERIZAVEIS de Dynamic Range (P1-004).
//
// IDENTIFICADOR: o TC-045 autorizado ja estava OCUPADO (tests/e2e/E2E_Specs.cpp, "TC-045 Tooltip
// multi-curva"). A reconfirmacao exigida antes da edicao detectou a colisao; adotou-se o proximo
// identificador LIVRE da mesma faixa, TC-046 (mesmo procedimento aplicado em TC-042 -> TC-043).
//
// Contrato verificado (decisao D1, Alternativa II) -- estado do PISO -> estado do DR:
//     piso Error            -> MS-09 Error,           sem escalar
//     piso NotApplicable    -> MS-09 NotApplicable,   valuePresent = false
//     piso LowConfidence    -> MS-09 LowConfidence,   valuePresent = TRUE  (medicao valida)
//     nenhum degrau valido  -> MS-09 NotApplicable,   valuePresent = false
//     teto <= piso (D3/D6)  -> MS-09 NotApplicable,   valuePresent = false
//     medicao normal        -> MS-09 Normal,          valuePresent = true
//
// O que este teste IMPEDE de reintroduzir: (i) interpretar a sentinela -300 dBFS do piso como
// medicao (produzia DR ~ +291 dB em estado Normal); (ii) reportar DR NEGATIVO quando o teto nao
// supera o piso; (iii) descartar o valor de uma medicao LowConfidence; (iv) substituir a
// degeneracao por zero, abs() ou clamp.
TEST_CASE("TC-046 MS-09 condicoes nao caracterizaveis de Dynamic Range (P1-004)") {
    AnalysisEngine eng(FS);

    // Acesso ao escalar do MS-09 (nullptr quando a familia nao emite escalar -- caso Error).
    auto drSc = [](const ResultBundle& b) -> const Scalar* {
        const MetricResult* m = b.find("MS-09");
        if (m == nullptr) return nullptr;
        for (const Scalar& s : m->scalars) if (s.key == "DynamicRange_dB") return &s;
        return nullptr;
    };
    auto q09 = [](const ResultBundle& b) {
        const MetricResult* m = b.find("MS-09");
        return m == nullptr ? Quality::Error : m->quality;
    };
    // Coerencia estrutural exigida em TODOS os casos: o estado da familia e o estado do escalar
    // nao podem divergir, e um valor ausente jamais pode vir rotulado como Normal.
    auto coerente = [&](const ResultBundle& b) {
        const Scalar* s = drSc(b);
        if (s == nullptr) return q09(b) == Quality::Error;          // Error nao emite escalar
        if (s->quality != q09(b)) return false;
        if (!s->valuePresent && s->quality == Quality::Normal) return false;
        return true;
    };

    // Cada caso avaliado abaixo e ACUMULADO aqui e reaproveitado pelos controles negativos,
    // que varrem o conjunto inteiro com criterios independentes da classificacao esperada.
    std::vector<ResultBundle> todos;

    // ---- (1) REFERENCIA: medicao normal -- valor NAO pode mudar por causa do P1-004 ----
    ResultBundle bRef = eng.computeAllMetrics(harness::makeLoopback(FS, 0, false));
    todos.push_back(bRef);
    {
        const Scalar* s = drSc(bRef);
        CHECK(q09(bRef) == Quality::Normal);
        CHECK(s != nullptr && s->quality == Quality::Normal && s->valuePresent);
        // valor medido ANTES do P1-004 na mesma cadeia limpa (86,789489 dB): nao-regressao numerica
        CHECK_NEAR(s->value, 86.789489, 0.001);
        CHECK(coerente(bRef));
    }
    // teto e piso de referencia, usados adiante para posicionar o piso com precisao
    double T0 = 0.0, P0 = 0.0;
    {
        const Scalar* nfv = nfScalar(bRef, "NoiseFloor_dBFS");
        P0 = nfv->value; T0 = P0 + drSc(bRef)->value;
        CHECK_NEAR(T0, 20.0 * std::log10(std::sqrt(0.5) * dbfsToLinear(-6.0)), 0.01);
    }

    // ---- (2) ST-08 ausente -> nao ha o que caracterizar -> Error (comportamento preservado) ----
    {
        CapturedSet cap = harness::makeLoopback(FS, 0, false);
        cap.captured.erase(StimulusId::ST08_DynamicStep);
        ResultBundle b = eng.computeAllMetrics(cap);
        CHECK(q09(b) == Quality::Error);
        CHECK(drSc(b) == nullptr);
        CHECK(coerente(b));
        todos.push_back(b);
    }

    // ---- (3) piso em Error (ST-09 ausente) -> DR em Error ----
    {
        CapturedSet cap = harness::makeLoopback(FS, 0, false);
        cap.captured.erase(StimulusId::ST09_Silence);
        ResultBundle b = eng.computeAllMetrics(cap);
        CHECK(b.find("MS-08")->quality == Quality::Error);
        CHECK(q09(b) == Quality::Error);
        CHECK(drSc(b) == nullptr);
        CHECK(coerente(b));
        todos.push_back(b);
    }

    // ---- (4) piso NotApplicable por AUSENCIA DE OBSERVACAO (ST-09 vazio) -> DR NotApplicable ----
    {
        CapturedSet cap = harness::makeLoopback(FS, 0, false);
        cap.captured[StimulusId::ST09_Silence].clear();
        ResultBundle b = eng.computeAllMetrics(cap);
        const Scalar* s = drSc(b);
        CHECK(b.find("MS-08")->quality == Quality::NotApplicable);
        CHECK(q09(b) == Quality::NotApplicable);
        CHECK(s != nullptr && s->quality == Quality::NotApplicable && !s->valuePresent);
        CHECK(coerente(b));
        todos.push_back(b);
    }

    // ---- (5) piso NotApplicable por SILENCIO DIGITAL bit-exato -> DR NotApplicable ----
    // Condicao efetivamente observada em hardware (Helix em preset vazio). Este e o caso que,
    // antes do P1-004, produzia DR = +290,99 dB em estado Normal (DV-5).
    {
        CapturedSet cap = harness::makeLoopback(FS, 0, false);
        std::vector<double>& st09 = cap.captured[StimulusId::ST09_Silence];
        std::fill(st09.begin(), st09.end(), 0.0);
        ResultBundle b = eng.computeAllMetrics(cap);
        const Scalar* s = drSc(b);
        const Scalar* nfv = nfScalar(b, "NoiseFloor_dBFS");
        CHECK(q09(b) == Quality::NotApplicable);
        CHECK(s != nullptr && s->quality == Quality::NotApplicable && !s->valuePresent);
        // D3: a sentinela -300 dBFS PERMANECE no campo do MS-08 -- o que mudou e que o MS-09
        // nunca mais a interpreta como medicao.
        CHECK(nfv != nullptr && std::fabs(nfv->value - (-300.0)) < 1e-9 && !nfv->valuePresent);
        CHECK(coerente(b));
        todos.push_back(b);
    }

    // ---- (6) piso LowConfidence -> DR LowConfidence, com o VALOR PRESERVADO ----
    // Observacao aquem da linha de base reduz a CONFIANCA; nao invalida a medicao.
    {
        CapturedSet cap = harness::makeLoopback(FS, 0, false);
        cap.captured[StimulusId::ST09_Silence] =
            harness::deterministicNoise(static_cast<std::size_t>(4096 + 4 * 2048), -95.0);   // 5 quadros
        ResultBundle b = eng.computeAllMetrics(cap);
        const Scalar* s = drSc(b);
        const Scalar* nfv = nfScalar(b, "NoiseFloor_dBFS");
        CHECK(b.find("MS-08")->quality == Quality::LowConfidence);
        CHECK(q09(b) == Quality::LowConfidence);
        CHECK(s != nullptr && s->quality == Quality::LowConfidence && s->valuePresent);
        // o valor continua sendo teto - piso, com o MESMO teto da cadeia de referencia
        CHECK(nfv != nullptr);
        if (nfv != nullptr && s != nullptr) CHECK_NEAR(s->value, T0 - nfv->value, 1e-9);
        CHECK(coerente(b));
        todos.push_back(b);
    }

    // ---- (7) nenhum degrau valido (todos saturados) -> NotApplicable ----
    {
        CapturedSet cap = harness::makeLoopback(FS, 0, false);
        std::vector<double>& st08 = cap.captured[StimulusId::ST08_DynamicStep];
        for (double& v : st08) v = std::max(-1.0, std::min(1.0, v * 100.0));   // ate o degrau -40 dBFS satura
        ResultBundle b = eng.computeAllMetrics(cap);
        const Scalar* s = drSc(b);
        CHECK(q09(b) == Quality::NotApplicable);
        CHECK(s != nullptr && s->quality == Quality::NotApplicable && !s->valuePresent);
        CHECK(coerente(b));
        todos.push_back(b);
    }

    // ---- (8)(9)(10) DEGENERACAO teto <= piso (D3/D6) ----
    // O piso e posicionado com precisao explorando a HOMOGENEIDADE EXATA do estimador: multiplicar
    // o ST-09 por g escala a potencia por g^2, logo desloca o piso em exatamente 20.log10(g) dB.
    // Nao ha aproximacao envolvida -- as verificacoes abaixo conferem o piso obtido.
    auto comPiso = [&](double alvoDb) {
        CapturedSet cap = harness::makeLoopback(FS, 0, false);
        const double g = std::pow(10.0, (alvoDb - P0) / 20.0);
        for (double& v : cap.captured[StimulusId::ST09_Silence]) v *= g;
        return eng.computeAllMetrics(cap);
    };
    {
        // (8) degeneracao franca: piso 40 dB ACIMA do teto
        ResultBundle b = comPiso(T0 + 40.0);
        const Scalar* s = drSc(b);
        CHECK(b.find("MS-08")->quality == Quality::Normal);                  // o piso em si e valido
        CHECK_NEAR(nfScalar(b, "NoiseFloor_dBFS")->value, T0 + 40.0, 1e-6);  // posicionamento conferido
        CHECK(q09(b) == Quality::NotApplicable);
        CHECK(s != nullptr && s->quality == Quality::NotApplicable && !s->valuePresent);
        CHECK(coerente(b));
        todos.push_back(b);
    }
    {
        // (9) CASO-LIMITE, lado nao caracterizavel: piso apenas 1e-6 dB acima do teto.
        // O ponto teto == piso exato nao e construivel em ponto flutuante (a igualdade e um
        // conjunto de medida nula: qualquer realizacao cai de um dos lados por ~1e-15 dB). O
        // operador implementado e "<=", e este par de casos o delimita com folga de 1e-6 dB --
        // nove ordens de grandeza acima do ruido numerico e muito abaixo de qualquer degeneracao
        // fisicamente distinguivel.
        ResultBundle b = comPiso(T0 + 1e-6);
        const Scalar* s = drSc(b);
        CHECK(q09(b) == Quality::NotApplicable);
        CHECK(s != nullptr && s->quality == Quality::NotApplicable && !s->valuePresent);
        CHECK(coerente(b));
        todos.push_back(b);
    }
    {
        // (10) CASO-LIMITE, lado caracterizavel: piso 1e-6 dB ABAIXO do teto -> faixa util
        // minuscula porem REAL. Prova que a rejeicao nao e larga demais e que uma faixa de
        // 1e-6 dB e reportada como medida, sem clamp e sem arredondamento para zero.
        ResultBundle b = comPiso(T0 - 1e-6);
        const Scalar* s = drSc(b);
        CHECK(q09(b) == Quality::Normal);
        CHECK(s != nullptr && s->quality == Quality::Normal && s->valuePresent);
        CHECK(s->value > 0.0);
        CHECK_NEAR(s->value, 1e-6, 1e-9);
        CHECK(coerente(b));
        todos.push_back(b);
    }

    // ---- CONTROLES NEGATIVOS INDEPENDENTES ----
    // Varrem TODOS os casos acima de uma so vez, com criterios que NAO dependem da classificacao
    // esperada de cada caso: falham se a logica defeituosa for reintroduzida por outro caminho.
    {
        CHECK(todos.size() == 10);   // referencia + 9 condicoes; guarda contra caso perdido
        for (const ResultBundle& b : todos) {
            const Scalar* s = drSc(b);
            // CN-1: nenhum Dynamic Range NEGATIVO e jamais apresentado como valor (D6).
            CHECK(!(s != nullptr && s->valuePresent && s->value < 0.0));
            // CN-2: nenhum DR absurdo -- a sentinela -300 dBFS nunca volta a virar medicao (DV-5).
            CHECK(!(s != nullptr && s->valuePresent && s->value > 200.0));
            // CN-3: valor ausente nunca vem rotulado como medicao normal, e familia/escalar
            //       nunca divergem de estado.
            CHECK(coerente(b));
        }
    }
}

// ---------------------------------------------------------------------------------------------
// TC-047 -- P1-009 (estagio 1): NATUREZA DA ESTIMATIVA do MS-09.
//
// IDENTIFICADOR: reconfirmado livre imediatamente antes da edicao (TC-044/045/046 ocupados;
// 047/048/049 livres).
//
// O contrato passa a ter TRES eixos INDEPENDENTES:
//     quality        -> validade / confianca da medicao
//     valuePresent   -> existencia de um valor numericamente interpretavel
//     estimate       -> o que o valor AFIRMA sobre a grandeza (Point / LowerBound / UpperBound)
//
// Regra do MS-09 (dynamic-range-definition.md sec.6). Com V = degraus de estimativa valida,
// C = degraus rejeitados por saturacao, s* = selecionado e min(C) = primeiro saturado:
//     C != vazio e min(C) == s*+1  -> Point       (saturacao demonstrada e ENCAIXOTADA)
//     C == vazio e s* == ultimo    -> LowerBound  (censura pelo limite superior do ensaio)
//     C != vazio e min(C) >  s*+1  -> LowerBound  (saturacao observada, NAO encaixotada)
//     V == vazio                   -> NotApplicable / valuePresent=false (natureza sem significado)
//
// O que este teste IMPEDE de reintroduzir: (i) publicar uma estimativa unilateral como se fosse
// pontual; (ii) representar unilateralidade via Quality ou valuePresent; (iii) deixar o novo campo
// escapar para a exportacao, o relatorio ou o dashboard; (iv) alterar o valor numerico do MS-09.
TEST_CASE("TC-047 MS-09 natureza da estimativa: Point vs LowerBound (P1-009)") {
    AnalysisEngine eng(FS);
    const int seg = static_cast<int>(0.100 * FS + 0.5);

    auto drScalar = [](const ResultBundle& b) -> const Scalar* {
        const MetricResult* m = b.find("MS-09");
        if (m == nullptr) return nullptr;
        for (const Scalar& s : m->scalars) if (s.key == "DynamicRange_dB") return &s;
        return nullptr;
    };
    // Captura de ST-08 com ganho aplicado e saturacao no fundo de escala (cadeia real satura).
    auto comGanho = [&](double g) {
        CapturedSet cap = harness::makeLoopback(FS, 0, false);
        for (double& v : cap.captured[StimulusId::ST08_DynamicStep])
            v = std::max(-1.0, std::min(1.0, v * g));
        return cap;
    };

    // ---- (1) ganho unitario: nenhum degrau satura -> teto CENSURADO pelo limite do ensaio ----
    // Este e o REGIME NORMAL: o ST-08 tem pico maximo de -6 dBFS, logo uma cadeia de ganho unitario
    // nunca demonstra saturacao. O numero e valido e confiavel; apenas afirma DR >= X, nao DR = X.
    ResultBundle bUnit = eng.computeAllMetrics(harness::makeLoopback(FS, 0, false));
    {
        const Scalar* s = drScalar(bUnit);
        CHECK(s != nullptr);
        CHECK(s->estimate == EstimateNature::LowerBound);
        // ORTOGONALIDADE: unilateral NAO rebaixa confianca nem remove o valor.
        CHECK(s->quality == Quality::Normal);
        CHECK(s->valuePresent);
        // INVARIANCIA NUMERICA: identico ao estado anterior ao P1-009.
        CHECK_NEAR(s->value, 86.789489, 0.001);
    }

    // ---- (1b) nenhum clipping, mas s* ANTES do ultimo degrau -> LowerBound ----
    // Mantem somente os degraus 0..3 do loopback unitario. Como todas as amostras presentes tem
    // pico < 1.0, C e vazio; como os degraus 4..7 foram truncados, eles nao podem integrar V e o
    // maior degrau valido selecionado e o 3. Logo C == vazio e s*=3 < 7, ramo distinto do caso (1).
    {
        CapturedSet cap = harness::makeLoopback(FS, 0, false);
        std::vector<double>& st08 = cap.captured[StimulusId::ST08_DynamicStep];
        CHECK(st08.size() == static_cast<std::size_t>(8 * seg));
        st08.resize(static_cast<std::size_t>(4 * seg));

        double picoDisponivel = 0.0;
        for (double v : st08) picoDisponivel = std::max(picoDisponivel, std::fabs(v));
        CHECK(picoDisponivel < 1.0);                              // nenhum indice pertence a C
        CHECK(st08.size() == static_cast<std::size_t>(4 * seg)); // indices 4..7 estao ausentes

        ResultBundle b = eng.computeAllMetrics(cap);
        const Scalar* s = drScalar(b);
        const Scalar* sUltimo = drScalar(bUnit);
        CHECK(s != nullptr);
        CHECK(s->quality == Quality::Normal);
        CHECK(s->valuePresent);
        CHECK(s->estimate == EstimateNature::LowerBound);
        CHECK(sUltimo != nullptr);
        CHECK_NEAR(sUltimo->value - s->value, 16.0, 0.1);        // s*=3, nao o ultimo indice 7
    }

    // ---- (2) ganho x2: satura o degrau 7, selecionado e o 6 -> saturacao ENCAIXOTADA -> Point ----
    // Controle negativo do caso (1b): C nao vazio e min(C)=s*+1 devem produzir Point.
    {
        ResultBundle b = eng.computeAllMetrics(comGanho(2.0));
        const Scalar* s = drScalar(b);
        CHECK(s != nullptr && s->estimate == EstimateNature::Point);
        CHECK(s->quality == Quality::Normal && s->valuePresent);
    }

    // ---- (3) ganho x4: satura a partir do degrau 5, selecionado e o 4 -> Point ----
    {
        ResultBundle b = eng.computeAllMetrics(comGanho(4.0));
        const Scalar* s = drScalar(b);
        CHECK(s != nullptr && s->estimate == EstimateNature::Point);
        CHECK(s->quality == Quality::Normal && s->valuePresent);
    }

    // ---- (4) todos os degraus saturados -> sem valor; a natureza nao tem significado ----
    {
        ResultBundle b = eng.computeAllMetrics(comGanho(100.0));
        const Scalar* s = drScalar(b);
        CHECK(s != nullptr);
        CHECK(s->quality == Quality::NotApplicable && !s->valuePresent);
    }

    // ---- (5) saturacao observada porem NAO encaixotada -> LowerBound ----
    // Constroi-se a partir do caso x4 (min(C)=5, s*=4) zerando o degrau 4: ele passa a ser invalido
    // por AUSENCIA DE ENERGIA IN-BAND, nao por saturacao. Resulta s*=3 e min(C)=5 > s*+1: sabe-se
    // que houve saturacao, mas o inicio dela NAO esta encaixotado acima do degrau selecionado.
    {
        CapturedSet cap = comGanho(4.0);
        std::vector<double>& st08 = cap.captured[StimulusId::ST08_DynamicStep];
        for (int i = 4 * seg; i < 5 * seg && i < static_cast<int>(st08.size()); ++i)
            st08[static_cast<std::size_t>(i)] = 0.0;
        ResultBundle b = eng.computeAllMetrics(cap);
        const Scalar* s = drScalar(b);
        CHECK(s != nullptr && s->estimate == EstimateNature::LowerBound);
        CHECK(s->quality == Quality::Normal && s->valuePresent);
        // discrimina do caso (3): o teto caiu um degrau (-6 dB), provando que s* mudou de 4 p/ 3
        ResultBundle b4 = eng.computeAllMetrics(comGanho(4.0));
        const Scalar* s4 = drScalar(b4);
        CHECK(s4 != nullptr && s->value > 0.0 && s4->value - s->value > 5.0);
    }

    // ---- (6) ORTOGONALIDADE PLENA: LowerBound + LowConfidence + valuePresent=true ----
    // Prova que os tres eixos sao realmente independentes: um teto censurado (unilateral) sobre um
    // piso de observacao insuficiente (confianca reduzida) mantem o valor presente.
    {
        CapturedSet cap = harness::makeLoopback(FS, 0, false);
        cap.captured[StimulusId::ST09_Silence] =
            harness::deterministicNoise(static_cast<std::size_t>(4096 + 4 * 2048), -95.0);  // 5 quadros
        ResultBundle b = eng.computeAllMetrics(cap);
        const Scalar* s = drScalar(b);
        CHECK(s != nullptr);
        CHECK(s->estimate == EstimateNature::LowerBound);
        CHECK(s->quality == Quality::LowConfidence);
        CHECK(s->valuePresent);
    }

    // ---- (7) PADRAO: toda métrica que nao se pronuncia continua sendo estimativa PONTUAL ----
    // Nenhum escalar de nenhuma outra familia foi tocado; o inicializador padrao do membro
    // acrescentado ao final de Scalar garante Point sem edicao alguma nas outras 14 familias.
    {
        int verificados = 0;
        for (const MetricResult& m : bUnit.metrics) {
            for (const Scalar& s : m.scalars) {
                if (m.id == "MS-09" && s.key == "DynamicRange_dB") continue;   // o unico que se pronuncia
                CHECK(s.estimate == EstimateNature::Point);
                ++verificados;
            }
        }
        CHECK(verificados > 100);   // guarda: a varredura precisa ter sido efetiva
    }

    // ---- (8) INVARIANCIA DE EXPORTACAO: o novo campo NAO escapa para nenhuma saida ----
    // Dois bundles identicos exceto pela natureza (LowerBound x Point) devem produzir:
    //   CSV escalar BYTE-IDENTICO, ReportModel identico e DashboardModel identico.
    {
        ResultBundle bLower = bUnit;                      // MS-09 em LowerBound (caso 1)
        ResultBundle bPoint = bUnit;
        bool trocou = false;
        for (MetricResult& m : bPoint.metrics)
            if (m.id == "MS-09")
                for (Scalar& s : m.scalars)
                    if (s.key == "DynamicRange_dB") { s.estimate = EstimateNature::Point; trocou = true; }
        CHECK(trocou);   // a unica diferenca entre os dois bundles

        ExportEngine ex;
        const std::string pL = "/tmp/aamp_p1009_lower.csv";
        const std::string pP = "/tmp/aamp_p1009_point.csv";
        CHECK(ex.exportScalarsCSV(bLower, pL).ok);
        CHECK(ex.exportScalarsCSV(bPoint, pP).ok);
        auto slurp = [](const std::string& p) {
            std::ifstream f(p, std::ios::binary); std::stringstream ss; ss << f.rdbuf(); return ss.str();
        };
        const std::string csvL = slurp(pL), csvP = slurp(pP);
        CHECK(!csvL.empty());
        CHECK(csvL == csvP);                                        // BYTE-IDENTICO
        CHECK(csvL.find("LowerBound") == std::string::npos);        // nenhuma linha/campo novo
        CHECK(csvL.find("nature")     == std::string::npos);
        CHECK(csvL.find("estimate")   == std::string::npos);
        CHECK(csvL.find("DynamicRange_dB=86.789") != std::string::npos);  // numero puro, sem ">="
        CHECK(csvL.find(">=") == std::string::npos);

        // ReportModel (PDF) identico
        const ReportModel rL = ex.buildReportModel(bLower), rP = ex.buildReportModel(bPoint);
        CHECK(rL.sections.size() == rP.sections.size());
        bool relIgual = true;
        for (std::size_t i = 0; i < rL.sections.size() && i < rP.sections.size(); ++i) {
            if (rL.sections[i].title != rP.sections[i].title) relIgual = false;
            if (rL.sections[i].lines != rP.sections[i].lines) relIgual = false;
        }
        CHECK(relIgual);

        // DashboardModel identico
        const DashboardModel dL = buildDashboardModel(bLower), dP = buildDashboardModel(bPoint);
        CHECK(dL.sections.size() == dP.sections.size());
        bool dashIgual = true;
        for (std::size_t i = 0; i < dL.sections.size() && i < dP.sections.size(); ++i) {
            if (dL.sections[i].items.size() != dP.sections[i].items.size()) { dashIgual = false; continue; }
            for (std::size_t j = 0; j < dL.sections[i].items.size(); ++j) {
                const DashboardItem& a = dL.sections[i].items[j];
                const DashboardItem& c = dP.sections[i].items[j];
                if (a.label != c.label || a.value != c.value || a.unit != c.unit || a.state != c.state)
                    dashIgual = false;
            }
        }
        CHECK(dashIgual);
    }

    // ---- (9) CONTROLE: a classificacao NAO depende de comparacao de double ----
    // A natureza e funcao exclusiva de s* e min(C), ambos INTEIROS. Prova-se pela via observavel:
    // duas capturas com o MESMO padrao de saturacao mas niveis in-band bem diferentes produzem a
    // MESMA natureza -- o valor numerico do teto nao participa da decisao.
    // (F10/NaN permanece ABERTO e NAO e corrigido aqui; este caso nao sanitiza entrada alguma.)
    {
        ResultBundle b2 = eng.computeAllMetrics(comGanho(2.0));
        const Scalar* s2 = drScalar(b2);
        CHECK(s2 != nullptr && s2->estimate == EstimateNature::Point);

        // (a) MESMO padrao de saturacao, TETO DESLOCADO: atenua em 6 dB apenas os degraus NAO
        //     saturados. O degrau 7 continua saturando e o 6 continua sendo o selecionado -- os
        //     indices s* e min(C) NAO mudam --, mas o NIVEL in-band do teto cai ~6 dB. Se a
        //     classificacao dependesse do valor, a natureza mudaria. Ela NAO pode mudar.
        CapturedSet capAten = comGanho(2.0);
        std::vector<double>& z = capAten.captured[StimulusId::ST08_DynamicStep];
        for (int k = 0; k < 7; ++k)
            for (int i = k * seg; i < (k + 1) * seg && i < static_cast<int>(z.size()); ++i)
                z[static_cast<std::size_t>(i)] *= 0.5;
        ResultBundle bA = eng.computeAllMetrics(capAten);
        const Scalar* sA = drScalar(bA);
        CHECK(sA != nullptr);
        CHECK(sA->estimate == s2->estimate);                       // natureza INVARIANTE ao nivel
        CHECK(s2->value - sA->value > 5.0);                        // ...com o teto ~6 dB MENOR

        // (b) padroes de saturacao em degraus DIFERENTES (x2 e x4), ambos encaixotados: tetos
        //     distintos, MESMA natureza.
        ResultBundle b4 = eng.computeAllMetrics(comGanho(4.0));
        const Scalar* s4 = drScalar(b4);
        CHECK(s4 != nullptr && s4->estimate == s2->estimate);
        CHECK(std::fabs(s4->value - s2->value) > 1.0);             // valores distintos, natureza igual
    }
}

// ---------------------------------------------------------------------------------------------
// TC-048 -- P1-005: o denominador do SNR passa a ser o Noise Floor BAND-LIMITED oficial do MS-08.
//
// IDENTIFICADOR: reconfirmado livre mecanicamente imediatamente antes da edicao (TC-045/046/047
// ocupados; 048 livre).
//
// Antes, pn era meanSquare(ST-09) em BANDA LARGA enquanto o MS-08 ja publicava o piso in-band
// 20 Hz-20 kHz: as duas familias discordavam sobre o MESMO silencio -- ate 52,8 dB de diferenca
// com conteudo ultrassonico. O MS-10 passa a CONSUMIR o resultado oficial do MS-08 (fonte unica),
// convertendo NoiseFloor_dBFS em potencia linear; nao recomputa o ST-09 nem cria um segundo Welch.
//
// O que este teste IMPEDE de reintroduzir: (i) voltar ao meanSquare de banda larga; (ii) rejeitar
// ruido INDISCRIMINADAMENTE (o ruido in-band tem de continuar afetando o SNR); (iii) converter a
// sentinela -300 dBFS em potencia e publicar ~+291 dB; (iv) perder a propagacao de estado do piso.
//
// FORA DE ESCOPO, deliberadamente: a condicao pn~0 (P1-007).
//
// ATUALIZADO PELO P1-006: este caso continua sendo o teste do DENOMINADOR, mas o numerador deixou de
// ser meanSquare do bloco inteiro e passou a ser a potencia da FUNDAMENTAL (TC-049). Os valores
// esperados que dependem do numerador caem 0,001221 dB -- o vies +-2 bins ratificado -- e a
// identidade de coerencia (j) passa a ser expressa com o numerador estrito. Nenhuma protecao foi
// removida: os controles negativos, a vedacao da sentinela e a propagacao de estado seguem intactos.
TEST_CASE("TC-048 MS-10 pn = Noise Floor band-limited do MS-08 (P1-005)") {
    AnalysisEngine eng(FS);
    const double PI048 = 3.14159265358979;

    auto snrSc = [](const ResultBundle& b) -> const Scalar* {
        const MetricResult* m = b.find("MS-10");
        if (m == nullptr) return nullptr;
        for (const Scalar& s : m->scalars) if (s.key == "SNR_dB") return &s;
        return nullptr;
    };
    auto nfDe = [](const ResultBundle& b) -> double {
        const Scalar* s = nfScalar(b, "NoiseFloor_dBFS");
        return s != nullptr ? s->value : 0.0;
    };
    auto q10 = [](const ResultBundle& b) {
        const MetricResult* m = b.find("MS-10");
        return m == nullptr ? Quality::Error : m->quality;
    };
    // ORACULO INDEPENDENTE do teste: reproduz o metodo ANTIGO (meanSquare de banda larga do ST-09).
    // Nao usa nenhuma funcao do produto -- e o controle negativo exigido.
    auto snrAntigo = [](const CapturedSet& cap) {
        auto ms = [](const std::vector<double>& x) {
            if (x.empty()) return 0.0;
            double a = 0.0; for (double v : x) a += v * v; return a / static_cast<double>(x.size());
        };
        const int seg = static_cast<int>(2.0 * FS + 0.5);
        const std::vector<double>& t = cap.captured.at(StimulusId::ST03_PureSine);
        std::vector<double> tone(t.begin() + seg, t.begin() + 2 * seg);
        const double ps = ms(tone);
        const double pn = ms(cap.captured.at(StimulusId::ST09_Silence));
        return (pn <= 0.0) ? 200.0 : 10.0 * std::log10(ps / pn);
    };
    // ST-09 do loopback + um tom aditivo de -40 dBFS na frequencia indicada.
    auto comTom = [&](double hz) {
        CapturedSet cap = harness::makeLoopback(FS, 0, false);
        std::vector<double>& z = cap.captured[StimulusId::ST09_Silence];
        for (std::size_t i = 0; i < z.size(); ++i)
            z[i] += 0.01 * std::sin(2.0 * 3.14159265358979 * hz * static_cast<double>(i) / FS);
        return cap;
    };

    // ---- (a) ruido branco padrao: Normal, presente, e o valor NORMATIVO ----
    // O piso in-band e ~0,80 dB menor que o de banda larga (fracao (20k-20)/24k = 0,8325), logo o
    // SNR normativo e ~0,80 dB MAIOR que o antigo. A diferenca NAO e ruido: e a banda.
    CapturedSet capRef = harness::makeLoopback(FS, 0, false);
    ResultBundle bRef = eng.computeAllMetrics(capRef);
    {
        const Scalar* s = snrSc(bRef);
        CHECK(s != nullptr);
        CHECK(q10(bRef) == Quality::Normal && s->quality == Quality::Normal && s->valuePresent);
        CHECK_NEAR(s->value, 86.7883, 0.001);                   // P1-006: -0,001221 dB (vies +-2 bins)
        CHECK_NEAR(snrAntigo(capRef), 85.9897, 0.001);          // oraculo: metodo antigo
        CHECK(s->value - snrAntigo(capRef) > 0.5);              // a correcao e efetivamente nao-nula
    }

    // ---- (b) 22 kHz a -40 dBFS: FORA da banda -> SNR praticamente invariante ----
    // (c) esta embutido: o oraculo independente demonstra que o metodo ANTIGO desabaria para ~34 dB.
    {
        CapturedSet cap = comTom(22000.0);
        ResultBundle b = eng.computeAllMetrics(cap);
        const Scalar* s = snrSc(b);
        CHECK(s != nullptr && s->quality == Quality::Normal && s->valuePresent);
        CHECK_NEAR(s->value, 86.7883, 0.001);                   // invariante em relacao a (a)
        CHECK_NEAR(snrAntigo(cap), 34.0, 0.1);                  // CONTROLE NEGATIVO: o antigo desaba
        CHECK(s->value - snrAntigo(cap) > 50.0);                // vies do metodo antigo: >50 dB
    }

    // ---- (d) 5 Hz a -40 dBFS: subsonico -> rejeicao PARCIAL, nao total ----
    // O componente esta a 0,43 bin da resolucao de 11,72 Hz e o lobulo principal de Hann atravessa
    // a borda de 20 Hz. A atenuacao medida e ~18,9 dB. Isto NAO e defeito do P1-005: e propriedade
    // de resolucao do estimador oficial do MS-08, herdada por construcao (D5). O teste assere a
    // rejeicao parcial EXPLICITAMENTE, sem exigir eliminacao completa.
    {
        CapturedSet cap = comTom(5.0);
        ResultBundle b = eng.computeAllMetrics(cap);
        const Scalar* s = snrSc(b);
        CHECK(s != nullptr && s->quality == Quality::Normal && s->valuePresent);
        CHECK_NEAR(s->value, 52.9384, 0.01);
        CHECK(s->value - snrAntigo(cap) > 18.0);                // melhora substancial...
        CHECK(s->value < 86.0);                                 // ...porem NAO ha eliminacao total
    }

    // ---- (e) 1 kHz a -40 dBFS: IN-BAND -> deve continuar afetando o SNR ----
    // Controle contra rejeicao indiscriminada: se o novo metodo simplesmente ignorasse ruido, este
    // caso passaria despercebido. Os dois metodos devem coincidir aqui.
    {
        CapturedSet cap = comTom(1000.0);
        ResultBundle b = eng.computeAllMetrics(cap);
        const Scalar* s = snrSc(b);
        CHECK(s != nullptr && s->quality == Quality::Normal && s->valuePresent);
        CHECK_NEAR(s->value, 34.0, 0.01);                       // degradado, como deve ser
        CHECK_NEAR(s->value, snrAntigo(cap), 0.01);             // antigo e novo COINCIDEM in-band
        CHECK(snrSc(bRef)->value - s->value > 50.0);            // muito abaixo do caso limpo
    }

    // ---- (f) piso LowConfidence -> SNR LowConfidence, com o valor PRESERVADO ----
    {
        CapturedSet cap = harness::makeLoopback(FS, 0, false);
        cap.captured[StimulusId::ST09_Silence] =
            harness::deterministicNoise(static_cast<std::size_t>(4096 + 4 * 2048), -95.0);   // 5 quadros
        ResultBundle b = eng.computeAllMetrics(cap);
        const Scalar* s = snrSc(b);
        CHECK(b.find("MS-08")->quality == Quality::LowConfidence);
        CHECK(q10(b) == Quality::LowConfidence);
        CHECK(s != nullptr && s->quality == Quality::LowConfidence && s->valuePresent);
        CHECK_NEAR(s->value, 86.8238, 0.001);                   // P1-006: -0,001221 dB
    }

    // ---- (g)(h) piso NotApplicable -> SNR NotApplicable, sem valor; a sentinela NAO volta ----
    // Antes do P1-005 este caso produzia 200,0 em estado Normal. Ler NoiseFloor_dBFS por chave sem
    // honrar o estado produziria +290,99 dB -- a DV-5 reencarnada no MS-10.
    {
        CapturedSet cap = harness::makeLoopback(FS, 0, false);
        std::vector<double>& z = cap.captured[StimulusId::ST09_Silence];
        std::fill(z.begin(), z.end(), 0.0);                     // silencio digital bit-exato
        ResultBundle b = eng.computeAllMetrics(cap);
        const Scalar* s = snrSc(b);
        CHECK(b.find("MS-08")->quality == Quality::NotApplicable);
        CHECK(q10(b) == Quality::NotApplicable);
        CHECK(s != nullptr && s->quality == Quality::NotApplicable && !s->valuePresent);
        CHECK(!(s->valuePresent && s->value > 200.0));          // nunca ~+291 dB
        CHECK(std::fabs(nfDe(b) - (-300.0)) < 1e-9);            // a sentinela segue no campo do MS-08
    }

    // ---- (i) PRECEDENCIA ESTRUTURAL: ausencia de QUALQUER entrada requerida -> Error ----
    // O Core testa AMBAS as entradas ANTES de calcular e a familia SEMPRE EXISTE no bundle. Esta e a
    // referencia normativa que o LabEngine tem de reproduzir (CC-A): a divergencia detectada na
    // validacao independente do P1-005 era o JS OMITIR a familia quando o ST-09 faltava, em vez de
    // publica-la em Error. As tres combinacoes sao verificadas para que a regra fique fixada como
    // PRECEDENCIA, e nao como um caso particular do ST-09.
    {
        struct Caso { const char* nome; bool tirarSt03; bool tirarSt09; };
        const Caso casos[3] = {
            { "ST-09 ausente",  false, true  },
            { "ST-03 ausente",  true,  false },
            { "ambos ausentes", true,  true  },
        };
        for (const Caso& k : casos) {
            CapturedSet cap = harness::makeLoopback(FS, 0, false);
            if (k.tirarSt03) cap.captured.erase(StimulusId::ST03_PureSine);
            if (k.tirarSt09) cap.captured.erase(StimulusId::ST09_Silence);
            ResultBundle b = eng.computeAllMetrics(cap);
            CHECK(b.find("MS-10") != nullptr);          // a familia EXISTE mesmo sem as entradas
            CHECK(q10(b) == Quality::Error);
            CHECK(snrSc(b) == nullptr);                 // Error nao emite escalar no Core
            CHECK(b.metrics.size() == 15);              // o bundle continua completo
        }
    }

    // ---- (j) COERENCIA no mesmo bundle: fonte unica de verdade ----
    // SNR_dB deve ser exatamente 10*log10(ps) - NoiseFloor_dBFS lidos do MESMO bundle. Se o MS-10
    // recomputasse o piso por outro caminho, esta identidade quebraria. O que esta sendo testado
    // AQUI e o DENOMINADOR; por isso o ps do oraculo acompanha o numerador vigente (P1-006: potencia
    // da fundamental, quadro centrado de 4096, bins k0+-2), reconstruido independentemente.
    {
        const int N = 4096;
        const int seg = static_cast<int>(2.0 * FS + 0.5);
        const std::vector<double>& t = capRef.captured.at(StimulusId::ST03_PureSine);
        const std::vector<double> tone(t.begin() + seg, t.begin() + 2 * seg);
        const int off = (static_cast<int>(tone.size()) - N) / 2;
        std::vector<double> win(static_cast<std::size_t>(N));
        double s1 = 0.0, s2 = 0.0;
        for (int i = 0; i < N; ++i) {
            const double w = 0.5 - 0.5 * std::cos(2.0 * PI048 * i / (N - 1));
            win[static_cast<std::size_t>(i)] = w; s1 += w; s2 += w * w;
        }
        std::vector<double> buf(static_cast<std::size_t>(N));
        for (int i = 0; i < N; ++i)
            buf[static_cast<std::size_t>(i)] = tone[static_cast<std::size_t>(off + i)] * win[static_cast<std::size_t>(i)];
        std::vector<Complex> X = Fft::forwardReal(buf, N);
        const double kPow = 1.0 / (2.0 * (static_cast<double>(N) * s2 / (s1 * s1)));
        const int k0 = static_cast<int>(1000.0 * N / FS + 0.5);
        double ps = 0.0;
        for (int b = k0 - 2; b <= k0 + 2; ++b) {
            const double mag = std::abs(X[static_cast<std::size_t>(b)]) / s1 * 2.0;
            ps += mag * mag * kPow;
        }
        CHECK_NEAR(snrSc(bRef)->value, 10.0 * std::log10(ps) - nfDe(bRef), 1e-9);
    }

    // ---- (k) NAO-REGRESSAO das familias vizinhas ----
    {
        CHECK_NEAR(nfDe(bRef), -95.799789257, 1e-6);            // MS-08 numericamente inalterado
        const MetricResult* m9 = bRef.find("MS-09");
        CHECK(m9 != nullptr);
        for (const Scalar& s : m9->scalars)
            if (s.key == "DynamicRange_dB") {
                CHECK_NEAR(s.value, 86.789489300, 1e-6);        // MS-09 inalterado
                CHECK(s.estimate == EstimateNature::LowerBound);// EstimateNature intocado
            }
        const Scalar* s10 = snrSc(bRef);
        CHECK(s10 != nullptr && s10->estimate == EstimateNature::Point);  // P1-005 nao cria LowerBound
    }
}

// ---------------------------------------------------------------------------------------------
// TC-049 -- P1-006: o NUMERADOR do SNR passa a ser a POTENCIA DA FUNDAMENTAL RETORNADA (S/N estrito).
//
// IDENTIFICADOR: reconfirmado livre mecanicamente imediatamente antes da edicao (nenhuma ocorrencia
// de "TC-049" em arquivos rastreados nem em todo o historico; a numeracao sob tests/ saltava de
// TC-048 para TC-050).
//
// Antes, ps era o meanSquare do bloco de 1 kHz INTEIRO: o SNR incorporava distorcao, ruido, DC e
// energia fora de banda, de modo que ps_total/ps_fund = 1 + (THD+N/100)^2 EXATAMENTE -- num DUT muito
// distorcido o "SNR" MELHORAVA. A distorcao pertence a MS-05/MS-06, que NAO sao alteradas.
//
// ESTIMADOR: a MESMA maquina espectral absoluta do piso (Hann + ENBW/Parseval, N=4096), num quadro
// unico CENTRADO no bloco. Frequencia nominal FIXA em 1000 Hz, k0 = round(1000*N/fs) = 85 em
// 48 kHz/N=4096, integracao nos bins k0+-2 = {83,84,85,86,87}.
//
// DECISAO DE GATE REGISTRADA (A): o alinhamento temporal do ST-03 e DISPENSADO neste item. A
// invariancia NAO vem de correlacao/bestLag -- vem do ENQUADRAMENTO temporal escolhido: um quadro de
// 4096 amostras centrado num bloco de 2 s tolera deslocamentos muito superiores ao dominio
// operacional (politica de latencia <= 50 ms). O caso (h) prova exatamente isso. A decisao vale para
// a GEOMETRIA ATUAL -- bloco de 1 kHz = 2 s, quadro = 4096, quadro centralizado, latencia <= 50 ms --
// e deve ser reavaliada se qualquer uma dessas premissas mudar.
//
// VIES ACEITO: a integracao em k0+-2 introduz um vies determinístico de -0,001221 dB para 1 kHz em
// 48 kHz/N=4096 (o tom cai no bin 85,333). Este teste fixa o VALOR ESPERADO DO NOVO CONTRATO, nao o
// valor ideal analitico de um seno infinitamente resolvido: a largura +-2 bins ja foi ratificada e
// NAO deve ser ampliada para +-3/+-4 so para reduzir esse erro.
//
// FORA DE ESCOPO, deliberadamente: pn~0 / limite inferior (P1-007) e Trust Ladder (P1-008).
TEST_CASE("TC-049 MS-10 ps = potencia da fundamental, S/N estrito (P1-006)") {
    AnalysisEngine eng(FS);
    const double PI = 3.14159265358979;

    auto snrSc = [](const ResultBundle& b) -> const Scalar* {
        const MetricResult* m = b.find("MS-10");
        if (m == nullptr) return nullptr;
        for (const Scalar& s : m->scalars) if (s.key == "SNR_dB") return &s;
        return nullptr;
    };
    auto nfDe = [](const ResultBundle& b) -> double {
        const Scalar* s = nfScalar(b, "NoiseFloor_dBFS");
        return s != nullptr ? s->value : 0.0;
    };
    auto q10 = [](const ResultBundle& b) {
        const MetricResult* m = b.find("MS-10");
        return m == nullptr ? Quality::Error : m->quality;
    };
    // Bloco de 1 kHz efetivamente analisado pelo Core: recorte [2 s, 4 s) do ST-03 CAPTURADO.
    auto bloco1k = [](const CapturedSet& cap) {
        const int seg = static_cast<int>(2.0 * FS + 0.5);
        const std::vector<double>& t = cap.captured.at(StimulusId::ST03_PureSine);
        return std::vector<double>(t.begin() + seg, t.begin() + 2 * seg);
    };
    // ORACULO INDEPENDENTE (i): numerador ESTRITO reconstruido no proprio teste -- Hann simetrico,
    // ENBW/Parseval, N=4096, quadro CENTRADO, bins k0+-2. Nao chama inBandPower nem snr(): usa
    // apenas a primitiva Fft, exatamente como TC-031 ja faz para a calibracao de amplitude.
    auto psFund = [&](const std::vector<double>& tone) {
        const int N = 4096;
        if (static_cast<int>(tone.size()) < N) return 0.0;
        const int off = (static_cast<int>(tone.size()) - N) / 2;
        std::vector<double> win(static_cast<std::size_t>(N));
        double s1 = 0.0, s2 = 0.0;
        for (int i = 0; i < N; ++i) {
            const double w = 0.5 - 0.5 * std::cos(2.0 * PI * i / (N - 1));
            win[static_cast<std::size_t>(i)] = w; s1 += w; s2 += w * w;
        }
        const double enbw = static_cast<double>(N) * s2 / (s1 * s1);   // Hann = 1.5
        std::vector<double> buf(static_cast<std::size_t>(N));
        for (int i = 0; i < N; ++i)
            buf[static_cast<std::size_t>(i)] = tone[static_cast<std::size_t>(off + i)] * win[static_cast<std::size_t>(i)];
        std::vector<Complex> X = Fft::forwardReal(buf, N);
        const double kPow = 1.0 / (2.0 * enbw);
        const int k0 = static_cast<int>(1000.0 * N / FS + 0.5);
        double p = 0.0;
        for (int b = k0 - 2; b <= k0 + 2; ++b) {
            const double mag = std::abs(X[static_cast<std::size_t>(b)]) / s1 * 2.0;
            p += mag * mag * kPow;
        }
        return p;
    };
    // ORACULO INDEPENDENTE (ii): o metodo ANTIGO (meanSquare do bloco inteiro). E o CONTROLE NEGATIVO
    // exigido -- ele NAO discrimina frequencia, por isso incorpora harmonicas, DC e 22 kHz.
    // Observacao registrada: nao se exige dele "falha por latencia", justamente porque a insensibilidade
    // a frequencia MASCARAVA o problema em vez de expo-lo.
    auto psTotal = [](const std::vector<double>& x) {
        if (x.empty()) return 0.0;
        double a = 0.0; for (double v : x) a += v * v; return a / static_cast<double>(x.size());
    };
    // ST-03 do loopback contaminado por um componente aditivo.
    auto comContaminacao = [&](int tipo) {
        CapturedSet cap = harness::makeLoopback(FS, 0, false);
        std::vector<double>& t = cap.captured[StimulusId::ST03_PureSine];
        const std::vector<double> nz = harness::deterministicNoise(t.size(), -60.0, 999);
        for (std::size_t i = 0; i < t.size(); ++i) {
            const double n = static_cast<double>(i);
            if (tipo == 0) t[i] += 0.05 * std::sin(2.0 * PI * 2000.0 * n / FS)      // 2ª e 3ª harmonicas
                                 + 0.03 * std::sin(2.0 * PI * 3000.0 * n / FS);
            else if (tipo == 1) t[i] += 0.05;                                        // DC
            else if (tipo == 2) t[i] += 0.05 * std::sin(2.0 * PI * 22000.0 * n / FS);// fora de banda
            else                t[i] += nz[i];                                       // ruido de banda larga
        }
        return cap;
    };

    CapturedSet capRef = harness::makeLoopback(FS, 0, false);
    ResultBundle bRef = eng.computeAllMetrics(capRef);
    const double SNR_REF = 86.788268656;   // valor NORMATIVO do novo contrato

    // ---- (a) SENO PURO: o numerador e a potencia da FUNDAMENTAL, com o vies +-2 bins ACEITO ----
    {
        const Scalar* s = snrSc(bRef);
        CHECK(s != nullptr);
        CHECK(q10(bRef) == Quality::Normal && s->quality == Quality::Normal && s->valuePresent);
        CHECK_NEAR(s->value, SNR_REF, 1e-6);
        CHECK_NEAR(s->value, 10.0 * std::log10(psFund(bloco1k(capRef))) - nfDe(bRef), 1e-9);

        // Vies determinístico da largura ratificada, medido contra um seno sintetico EXATO de -6 dBFS
        // (potencia analitica A^2/2). Fixado como CONTRATO, nao como erro a eliminar.
        CapturedSet capSin = harness::makeLoopback(FS, 0, false);
        std::vector<double>& t = capSin.captured[StimulusId::ST03_PureSine];
        const double A = dbfsToLinear(-6.0);
        for (std::size_t i = 0; i < t.size(); ++i)
            t[i] = A * std::sin(2.0 * PI * 1000.0 * static_cast<double>(i) / FS);
        const double vies = 10.0 * std::log10(psFund(bloco1k(capSin)) / (A * A / 2.0));
        CHECK_NEAR(vies, -0.001221, 1e-5);                       // o vies ACEITO, nao um defeito
        CHECK(vies < 0.0);                                       // truncamento so pode SUBESTIMAR
    }

    // ---- (b)(d)(e) HARMONICAS, DC e 22 kHz: o numerador estrito e INVARIANTE; o antigo INFLA ----
    // Este e o coracao do P1-006: o controle negativo prova que o meanSquare antigo incorpora as tres
    // contaminacoes e, portanto, se comporta de forma DIFERENTE do numerador estrito.
    {
        struct Caso { const char* nome; int tipo; double minInflacaoAntigo; };
        const Caso casos[3] = {
            { "harmonicas 2k+3k", 0, 0.05 },   // +0,0596 dB medidos
            { "DC",               1, 0.08 },   // +0,0866 dB medidos
            { "22 kHz",           2, 0.04 },   // +0,0443 dB medidos
        };
        for (const Caso& k : casos) {
            CapturedSet cap = comContaminacao(k.tipo);
            ResultBundle b = eng.computeAllMetrics(cap);
            const Scalar* s = snrSc(b);
            const std::vector<double> tone = bloco1k(cap);
            CHECK(s != nullptr && s->quality == Quality::Normal && s->valuePresent);
            // o numerador ESTRITO nao ve a contaminacao
            CHECK_NEAR(s->value, SNR_REF, 1e-6);
            // o metodo ANTIGO ve, e para MAIS -- "SNR" que melhora com distorcao
            const double antigo = 10.0 * std::log10(psTotal(tone)) - nfDe(b);
            CHECK(antigo - s->value > k.minInflacaoAntigo);
        }
    }

    // ---- (c) RUIDO de banda larga somado ao TOM: afeta pouco o estrito, e os metodos DIVERGEM ----
    {
        CapturedSet cap = comContaminacao(3);
        ResultBundle b = eng.computeAllMetrics(cap);
        const Scalar* s = snrSc(b);
        const double antigo = 10.0 * std::log10(psTotal(bloco1k(cap))) - nfDe(b);
        CHECK(s != nullptr && s->quality == Quality::Normal && s->valuePresent);
        CHECK_NEAR(s->value, 86.787547890, 1e-6);
        CHECK(s->value < SNR_REF);                               // ruido in-band entra na vizinhanca
        CHECK(antigo > s->value);                                // o antigo soma TODO o ruido do bloco
    }

    // ---- (f) GANHO x1,5 SEM clipping: o numerador estrito escala EXATAMENTE 20*log10(1,5) ----
    {
        CapturedSet cap = harness::makeLoopback(FS, 0, false);
        std::vector<double>& t = cap.captured[StimulusId::ST03_PureSine];
        double pico = 0.0;
        for (std::size_t i = 0; i < t.size(); ++i) { t[i] *= 1.5; pico = std::max(pico, std::fabs(t[i])); }
        CHECK(pico < 1.0);                                       // pre-condicao: NAO ha clipping
        ResultBundle b = eng.computeAllMetrics(cap);
        const Scalar* s = snrSc(b);
        CHECK(s != nullptr && s->quality == Quality::Normal && s->valuePresent);
        CHECK_NEAR(s->value - SNR_REF, 20.0 * std::log10(1.5), 1e-6);
    }

    // ---- (g) CLIPPING x3: a fundamental e COMPRIMIDA e o antigo fica ACIMA do estrito ----
    // O clipping cria harmonicas: o meanSquare as CONTA, o numerador estrito NAO. O ganho aparente
    // fica muito abaixo dos 20*log10(3) ideais -- que e exatamente a informacao fisica correta.
    {
        CapturedSet cap = harness::makeLoopback(FS, 0, false);
        std::vector<double>& t = cap.captured[StimulusId::ST03_PureSine];
        double pico = 0.0;
        for (std::size_t i = 0; i < t.size(); ++i) {
            t[i] *= 3.0;
            if (t[i] >  1.0) t[i] =  1.0;
            if (t[i] < -1.0) t[i] = -1.0;
            pico = std::max(pico, std::fabs(t[i]));
        }
        CHECK_NEAR(pico, 1.0, 1e-12);                            // pre-condicao: HA clipping
        ResultBundle b = eng.computeAllMetrics(cap);
        const Scalar* s = snrSc(b);
        const double antigo = 10.0 * std::log10(psTotal(bloco1k(cap))) - nfDe(b);
        CHECK(s != nullptr && s->quality == Quality::Normal && s->valuePresent);
        CHECK_NEAR(s->value, 94.161086680, 1e-6);
        CHECK(antigo > s->value);                                // o antigo conta as harmonicas de clipping
        CHECK(s->value < SNR_REF + 20.0 * std::log10(3.0) - 2.0);// muito abaixo do ganho ideal
    }

    // ---- (h) INVARIANCIA TEMPORAL DO QUADRO CENTRADO (gate A dispensado) ----
    // A invariancia decorre do ENQUADRAMENTO -- quadro de 4096 centrado num bloco de 2 s --, NAO de
    // correlacao ou alinhamento: nenhum bestLag participa do caminho. 2400 amostras = 50 ms, o limite
    // exato da politica de latencia do projeto; o comportamento se mantem MUITO alem disso.
    {
        const int offsets[3] = { 0, 913, 2400 };
        for (int d : offsets) {
            CapturedSet cap = harness::makeLoopback(FS, 0, false);
            cap.captured[StimulusId::ST03_PureSine] =
                harness::shift(cap.captured[StimulusId::ST03_PureSine], d);
            ResultBundle b = eng.computeAllMetrics(cap);
            const Scalar* s = snrSc(b);
            CHECK(s != nullptr && s->quality == Quality::Normal && s->valuePresent);
            CHECK_NEAR(s->value, SNR_REF, 1e-6);                 // identico ao caso sem deslocamento
        }
        // margem: mesmo a 4800 amostras (100 ms, o DOBRO da politica) nada muda.
        CapturedSet capM = harness::makeLoopback(FS, 0, false);
        capM.captured[StimulusId::ST03_PureSine] =
            harness::shift(capM.captured[StimulusId::ST03_PureSine], 4800);
        CHECK_NEAR(snrSc(eng.computeAllMetrics(capM))->value, SNR_REF, 1e-6);
    }

    // ---- (i) ST-03 AUSENTE -> Error (precedencia estrutural preservada pelo P1-006) ----
    {
        CapturedSet cap = harness::makeLoopback(FS, 0, false);
        cap.captured.erase(StimulusId::ST03_PureSine);
        ResultBundle b = eng.computeAllMetrics(cap);
        CHECK(b.find("MS-10") != nullptr);                       // a familia EXISTE
        CHECK(q10(b) == Quality::Error);
        CHECK(snrSc(b) == nullptr);                              // Error nao emite escalar no Core
        CHECK(b.metrics.size() == 15);
    }

    // ---- (j) NAO-REGRESSAO DO DENOMINADOR (P1-005): pn continua sendo o piso oficial do MS-08 ----
    // A identidade so fecha se o denominador continuar vindo do MS-08 pela fonte unica de verdade.
    // Trocar o numerador NAO pode ter reaberto o denominador.
    {
        CHECK_NEAR(snrSc(bRef)->value,
                   10.0 * std::log10(psFund(bloco1k(capRef))) - nfDe(bRef), 1e-9);
        // piso LowConfidence -> propaga, com valor presente
        CapturedSet capLc = harness::makeLoopback(FS, 0, false);
        capLc.captured[StimulusId::ST09_Silence] =
            harness::deterministicNoise(static_cast<std::size_t>(4096 + 4 * 2048), -95.0);
        ResultBundle bLc = eng.computeAllMetrics(capLc);
        CHECK(q10(bLc) == Quality::LowConfidence);
        CHECK(snrSc(bLc) != nullptr && snrSc(bLc)->valuePresent);
        CHECK_NEAR(snrSc(bLc)->value, 86.823790210, 1e-6);
        // piso NotApplicable -> sem valor, e a sentinela -300 dBFS NAO volta como ~+291 dB
        CapturedSet capNa = harness::makeLoopback(FS, 0, false);
        std::vector<double>& z = capNa.captured[StimulusId::ST09_Silence];
        std::fill(z.begin(), z.end(), 0.0);
        ResultBundle bNa = eng.computeAllMetrics(capNa);
        CHECK(q10(bNa) == Quality::NotApplicable);
        CHECK(snrSc(bNa) != nullptr && !snrSc(bNa)->valuePresent);
        CHECK(!(snrSc(bNa)->valuePresent && snrSc(bNa)->value > 200.0));
        CHECK_NEAR(nfDe(bNa), -300.0, 1e-9);
    }

    // ---- (k) CC-A: contrato EXATO que o LabEngine tem de reproduzir ----
    // A convergencia C++ <-> JS e verificada fora do harness C++ (o LabEngine e JavaScript). O que
    // este bloco fixa sao os pontos de referencia normativos comparados la: a selecao de bins e os
    // valores publicados. Se qualquer um mudar, a comparacao externa deixa de fechar.
    {
        const int N = 4096;
        const int k0 = static_cast<int>(1000.0 * N / FS + 0.5);
        CHECK(k0 == 85);                                         // Math.round(1000*4096/48000) = 85
        CHECK_NEAR(static_cast<double>(k0 - 2) * FS / N, 972.65625, 1e-9);   // loHz
        CHECK_NEAR(static_cast<double>(k0 + 2) * FS / N, 1019.53125, 1e-9);  // hiHz
        // o quadro CENTRADO tem de ser a mesma expressao inteira dos dois lados
        const int len = static_cast<int>(bloco1k(capRef).size());
        CHECK((len - N) / 2 == ((len - N) >> 1));
        CHECK_NEAR(snrSc(bRef)->value, SNR_REF, 1e-6);
    }

    // ---- (l) GATES ANTERIORES intactos ----
    {
        CHECK_NEAR(nfDe(bRef), -95.799789257, 1e-6);             // MS-08 (P1-002/P1-003)
        const MetricResult* m9 = bRef.find("MS-09");
        CHECK(m9 != nullptr);
        for (const Scalar& s : m9->scalars)
            if (s.key == "DynamicRange_dB") {
                CHECK_NEAR(s.value, 86.789489300, 1e-6);         // MS-09 (P1-004)
                CHECK(s.estimate == EstimateNature::LowerBound); // P1-009 estagio 1
            }
        const Scalar* s10 = snrSc(bRef);
        CHECK(s10 != nullptr && s10->estimate == EstimateNature::Point);  // P1-007 NAO antecipado
        CHECK(bRef.metrics.size() == 15);
    }
}

// TC-030: as 15 familias calculadas (ou N/A justificado), nenhuma em Error.
TEST_CASE("TC-030 15 familias calculadas (ou N/A), nenhuma Error") {
    AnalysisEngine eng(FS);
    ResultBundle b = eng.computeAllMetrics(harness::makeLoopback(FS, 0, false));
    CHECK(b.metrics.size() == 15);
    for (int i = 1; i <= 15; ++i) {
        char id[8]; std::snprintf(id, sizeof(id), "MS-%02d", i);
        const MetricResult* m = b.find(id);
        CHECK(m != nullptr);
        if (m) CHECK(m->quality != Quality::Error);
    }
}

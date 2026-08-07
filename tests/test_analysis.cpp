// AAMP - tests/test_analysis.cpp
// M-05 Analysis Engine + DSP: TC-030..TC-037 (DOC-11 §3, AC-M05-01..05).
#include "TestRunner.h"
#include "LoopbackHarness.h"
#include "../src/analysis/AnalysisEngine.h"
#include "../src/analysis/Fft.h"
#include "../src/analysis/Deconvolution.h"
#include "../src/common/Constants.h"
#include <cmath>

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

        // Q4 -- NAO-REGRESSAO DO MS-09: o DV-2 nao pode alterar o Dynamic Range neste item.
        // O MS-09 le NoiseFloor_dBFS por chave e ignora nf.quality; deve continuar produzindo
        // exatamente o mesmo resultado de antes do P1-002 (clamp -300 dBFS preservado no campo).
        const MetricResult* dr = b.find("MS-09");
        CHECK(dr != nullptr && dr->quality == Quality::Normal);
        CHECK(v != nullptr && std::fabs(v->value - (-300.0)) < 1e-9);
        for (const Scalar& s : dr->scalars)
            if (s.key == "DynamicRange_dB") CHECK(s.value > 200.0);   // teto - (-300)
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

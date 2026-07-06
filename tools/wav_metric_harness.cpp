// AAMP - tools/wav_metric_harness.cpp
// Ferramenta de REGRESSAO (fora do produto): roda as metricas do AnalysisEngine sobre um WAV externo.
// Permite reconciliar o AAMP com ferramentas de referencia (Ozone, etc.) sobre o MESMO arquivo,
// sem captura/driver/latencia/StimulusGenerator. Util para reamps (Helix/Kemper/QC/Tonex/plugins).
//
// Build: integrado ao CMake como target wav_metric_harness (linka aamp_core).
//
// Uso:
//   wav_metric_harness reamp_ac30.wav thd        -> THD  (mapeia o WAV como ST-03 PureSine)
//   wav_metric_harness reamp_st02_ac30.wav imd    -> IMD  (mapeia o WAV como ST-02 MultiTone)
//
// IMPORTANTE: thd() espera um SENO reampeado; imd() espera o MULTITOM reampeado. Rodar imd() sobre
// um seno (ou thd() sobre o multitom) executa, mas o numero NAO tem significado fisico da metrica.
// O ST-03 e SEQUENCIAL (100Hz/1kHz/10kHz, 2s cada): o thd() fatia o bloco de 1 kHz [2s,4s)
// automaticamente. Para comparar com Ozone, analise a MESMA janela 2s-4s no Ozone.

#include "analysis/AnalysisEngine.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

// ---- leitor WAV minimo: RIFF/WAVE, PCM16 ou IEEE-float32, mono (usa canal 0 se multicanal) ----
static bool loadWav(const std::string& path, std::vector<double>& out, double& fs) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { std::fprintf(stderr, "ERRO: nao abriu %s\n", path.c_str()); return false; }
    std::vector<char> all((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (all.size() < 44) { std::fprintf(stderr, "ERRO: arquivo curto demais\n"); return false; }
    auto rd16 = [&](std::size_t p) { return static_cast<uint16_t>((uint8_t)all[p] | ((uint8_t)all[p+1] << 8)); };
    auto rd32 = [&](std::size_t p) { return static_cast<uint32_t>((uint8_t)all[p] | ((uint8_t)all[p+1] << 8) | ((uint8_t)all[p+2] << 16) | ((uint8_t)all[p+3] << 24)); };
    if (std::memcmp(all.data(), "RIFF", 4) != 0 || std::memcmp(all.data() + 8, "WAVE", 4) != 0) {
        std::fprintf(stderr, "ERRO: nao e RIFF/WAVE\n"); return false;
    }
    uint16_t fmtTag = 0, channels = 0, bits = 0; uint32_t rate = 0;
    std::size_t dataOff = 0, dataLen = 0;
    std::size_t p = 12;
    while (p + 8 <= all.size()) {
        const std::size_t id = p;
        const uint32_t sz = rd32(p + 4);
        const std::size_t body = p + 8;
        if (std::memcmp(all.data() + id, "fmt ", 4) == 0) {
            fmtTag = rd16(body); channels = rd16(body + 2); rate = rd32(body + 4); bits = rd16(body + 14);
        } else if (std::memcmp(all.data() + id, "data", 4) == 0) {
            dataOff = body; dataLen = sz; break;
        }
        p = body + sz + (sz & 1); // chunks sao word-aligned
    }
    if (dataOff == 0 || channels == 0) { std::fprintf(stderr, "ERRO: sem fmt/data validos\n"); return false; }
    if (dataOff + dataLen > all.size()) dataLen = all.size() - dataOff;
    fs = static_cast<double>(rate);
    const int ch = channels;
    out.clear();
    if (fmtTag == 3 && bits == 32) {            // IEEE float32
        const std::size_t n = dataLen / 4;
        out.reserve(n / static_cast<std::size_t>(ch));
        for (std::size_t i = 0; i + static_cast<std::size_t>(ch) <= n; i += static_cast<std::size_t>(ch)) {
            float v; std::memcpy(&v, all.data() + dataOff + i * 4, 4); // canal 0
            out.push_back(static_cast<double>(v));
        }
    } else if (fmtTag == 1 && bits == 16) {     // PCM16
        const std::size_t n = dataLen / 2;
        out.reserve(n / static_cast<std::size_t>(ch));
        for (std::size_t i = 0; i + static_cast<std::size_t>(ch) <= n; i += static_cast<std::size_t>(ch)) {
            int16_t v = static_cast<int16_t>(rd16(dataOff + i * 2)); // canal 0
            out.push_back(static_cast<double>(v) / 32768.0);
        }
    } else {
        std::fprintf(stderr, "ERRO: formato nao suportado (fmtTag=%u bits=%u). Use float32 ou PCM16.\n", fmtTag, bits);
        return false;
    }
    std::fprintf(stderr, "WAV: fs=%.0f ch=%u bits=%u fmt=%u samples=%zu (%.3fs)\n",
                 fs, channels, bits, fmtTag, out.size(), out.size() / (fs > 0.0 ? fs : 1.0));
    return true;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "uso: %s <arquivo.wav> <thd|imd>\n", argv[0]);
        return 2;
    }
    const std::string path = argv[1];
    const std::string metric = argv[2];

    std::vector<double> samples; double fs = 0.0;
    if (!loadWav(path, samples, fs)) return 1;
    if (fs <= 0.0) fs = 48000.0;

    aamp::CapturedSet cap;
    cap.fs = fs;
    aamp::AnalysisEngine eng(fs);

    // computeAllMetrics e o ponto de entrada PUBLICO; thd()/imd() sao private.
    // Mapeamos o WAV no estimulo certo e extraimos a metrica desejada por id.
    const char* wantId = nullptr;
    if (metric == "thd") {
        cap.captured[aamp::StimulusId::ST03_PureSine] = samples;   // WAV como ST-03 (seno reampeado)
        wantId = "MS-05";
    } else if (metric == "imd") {
        cap.captured[aamp::StimulusId::ST02_MultiTone] = samples;  // WAV como ST-02 (multitom reampeado)
        wantId = "MS-07";
    } else {
        std::fprintf(stderr, "metrica desconhecida: %s (use thd ou imd)\n", metric.c_str());
        return 2;
    }

    aamp::ResultBundle bundle = eng.computeAllMetrics(cap);
    const aamp::MetricResult* found = nullptr;
    for (const auto& mr : bundle.metrics)
        if (mr.id == wantId) { found = &mr; break; }
    if (found == nullptr) {
        std::fprintf(stderr, "ERRO: metrica %s nao encontrada no bundle\n", wantId);
        return 1;
    }
    const aamp::MetricResult& r = *found;

    std::printf("=== AAMP %s sobre %s ===\n", metric.c_str(), path.c_str());
    std::printf("quality_enum=%d\n", static_cast<int>(r.quality));
    for (const auto& sc : r.scalars)
        std::printf("%s = %.8g%s\n", sc.key.c_str(), sc.value, sc.valuePresent ? "" : "  (present=0)");
    return 0;
}
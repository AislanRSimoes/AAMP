// AAMP - src/export/ExportEngine.cpp
#include "ExportEngine.h"
#include "../common/RiffWaveWriter.h"   // capacidade neutra de escrita RIFF/WAVE (fmt+data)
#include <fstream>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <map>
#include <string>

namespace aamp {

ExportEngine::ExportEngine(const std::string& appVersion) : m_version(appVersion) {}

// Formata com 3 casas e ponto decimal, independente de locale (DOC-10 §3.1).
static std::string fmt3(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.3f", v);
    // garante ponto (caso o locale C tenha sido alterado)
    for (char& c : buf) if (c == ',') c = '.';
    return std::string(buf);
}

std::string ExportEngine::formatScalar(const Scalar& s) {
    if (!s.valuePresent || s.quality == Quality::NotApplicable) return "N/A";
    std::string v = fmt3(s.value);
    if (s.quality == Quality::LowConfidence) v += "*";   // DOC-10: sufixo de baixa confiab.
    return v;
}

std::string ExportEngine::metadataHeader(const MeasurementMeta& m) const {
    std::string mode = (m.mode == ChannelMode::Stereo) ? "Stereo" : "Mono";
    char sr[32]; std::snprintf(sr, sizeof(sr), "%.0f", m.sampleRate);
    std::string ts = m.timestamp.empty() ? "1970-01-01 00:00:00" : m.timestamp;
    return "# AAMP Export | " + m_version + " | " + ts +
           " | Device: " + m.deviceName + " | SR: " + sr + " Hz | Mode: " + mode;
}

std::string ExportEngine::makeFilename(const std::string& tipo, const std::string& ext,
                                       const std::string& timestamp) {
    // "YYYY-MM-DD HH:MM:SS" -> "YYYYMMDD_HHMMSS"
    std::string digits;
    for (char c : timestamp) if (c >= '0' && c <= '9') digits += c;
    std::string date = digits.size() >= 8 ? digits.substr(0, 8) : "00000000";
    std::string time = digits.size() >= 14 ? digits.substr(8, 6) : "000000";
    return "aamp_" + tipo + "_" + date + "_" + time + "." + ext;
}

ExportStatus ExportEngine::exportCurvesCSV(const ResultBundle& b, const std::string& path) const {
    ExportStatus st; st.path = path;
    std::ofstream f(path, std::ios::binary);
    if (!f) { st.message = "Nao foi possivel salvar " + path + ": destino nao gravavel."; return st; }

    f << metadataHeader(b.meta) << "\n";
    f << "Frequency_Hz,Magnitude_dB,Phase_deg,GroupDelay_ms\n";

    const MetricResult* mag = b.find("MS-01");
    const MetricResult* pha = b.find("MS-02");
    const MetricResult* gd  = b.find("MS-03");
    const Curve* cMag = (mag && !mag->curves.empty()) ? &mag->curves[0] : nullptr;
    const Curve* cPha = (pha && !pha->curves.empty()) ? &pha->curves[0] : nullptr;
    const Curve* cGd  = (gd  && !gd->curves.empty())  ? &gd->curves[0]  : nullptr;

    // lookup de group delay por frequencia (arredondada a 0.1 Hz).
    std::map<long, double> gdMap;
    if (cGd) for (std::size_t i = 0; i < cGd->x.size(); ++i)
        gdMap[static_cast<long>(cGd->x[i] * 10.0 + 0.5)] = cGd->y[i];

    if (cMag) {
        for (std::size_t i = 0; i < cMag->x.size(); ++i) {
            const double freq = cMag->x[i];
            f << fmt3(freq) << ",";
            f << fmt3(cMag->y[i]) << ",";
            // fase (mesma grade que MS-01)
            if (cPha && i < cPha->y.size()) f << fmt3(cPha->y[i]); else f << "N/A";
            f << ",";
            // group delay (pode faltar nas extremidades -> N/A)
            auto it = gdMap.find(static_cast<long>(freq * 10.0 + 0.5));
            if (it != gdMap.end()) f << fmt3(it->second); else f << "N/A";
            f << "\n";
        }
    }
    f.flush();
    if (!f) { st.message = "Falha de escrita em " + path; return st; }
    st.ok = true;
    return st;
}

ExportStatus ExportEngine::exportScalarsCSV(const ResultBundle& b, const std::string& path) const {
    ExportStatus st; st.path = path;
    std::ofstream f(path, std::ios::binary);
    if (!f) { st.message = "Nao foi possivel salvar " + path + ": destino nao gravavel."; return st; }

    f << metadataHeader(b.meta) << "\n";
    bool anyLowConf = false;
    for (const MetricResult& m : b.metrics) {
        for (const Scalar& s : m.scalars) {
            if (s.key.empty()) continue;
            f << s.key << "=" << formatScalar(s) << "\n";
            if (s.quality == Quality::LowConfidence) anyLowConf = true;
        }
    }
    if (anyLowConf) f << "# * = baixa confiabilidade (SNR insuficiente)\n";
    f.flush();
    if (!f) { st.message = "Falha de escrita em " + path; return st; }
    st.ok = true;
    return st;
}

// --- WAV (RIFF/WAVE, IEEE float 32-bit, DOC-10 §4) ---
// O núcleo RIFF/WAVE (fmt+data) é a capacidade NEUTRA RiffWaveWriter, consumida de forma
// independente. Aqui o ExportEngine compõe, por conta própria, seu chunk LIST/INFO específico de
// exportação e o anexa como trailing chunk — a capacidade neutra não conhece esse INFO.
ExportStatus ExportEngine::exportWAV(const std::vector<double>& ir, double fs, int channels,
                                     const std::string& path) const {
    ExportStatus st; st.path = path;
    if (channels < 1) channels = 1;
    std::ofstream f(path, std::ios::binary);
    if (!f) { st.message = "Nao foi possivel salvar " + path + ": destino nao gravavel."; return st; }

    // INFO (LIST) chunk — metadados do domínio de EXPORTAÇÃO (fora da capacidade neutra).
    auto pad = [](std::string s){ if (s.size() % 2) s.push_back('\0'); return s; };
    std::string isft = pad(std::string("AAMP ") + m_version + '\0');
    std::string icrd = pad("2026-06-16\0");
    std::string icmt = pad("AAMP measurement IR\0");
    const std::uint32_t listBytes = 4 // "INFO"
        + 8 + static_cast<std::uint32_t>(isft.size())
        + 8 + static_cast<std::uint32_t>(icrd.size())
        + 8 + static_cast<std::uint32_t>(icmt.size());
    std::string list;
    list.append("LIST", 4); riff::putU32(list, listBytes); list.append("INFO", 4);
    auto writeInfo = [&](const char* id, const std::string& s){
        list.append(id, 4); riff::putU32(list, static_cast<std::uint32_t>(s.size())); list.append(s);
    };
    writeInfo("ISFT", isft); writeInfo("ICRD", icrd); writeInfo("ICMT", icmt);

    // fmt+data pela capacidade neutra; LIST/INFO anexado como trailing chunk (bytes idênticos).
    const std::string bytes = riff::writeFloat32(ir, fs, channels, list);
    f.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));

    f.flush();
    if (!f) { st.message = "Falha de escrita em " + path; return st; }
    st.ok = true;
    return st;
}

ReportModel ExportEngine::buildReportModel(const ResultBundle& b) const {
    ReportModel rm; rm.meta = b.meta;
    auto sc = [&](const char* id, const char* key) -> std::string {
        const MetricResult* m = b.find(id);
        if (!m) return "N/A";
        for (const Scalar& s : m->scalars) if (s.key == key) return formatScalar(s) + " " + s.unit;
        return "N/A";
    };
    // Espelha as 6 secoes do dashboard (DOC-10 §5.1). Sem julgamento qualitativo (§5.2).
    ReportSection summary{ "Summary", {
        "THD: " + sc("MS-05","THD_pct") , "SNR: " + sc("MS-10","SNR_dB"),
        "Dynamic Range: " + sc("MS-09","DynamicRange_dB"), "Latency: " + sc("MS-12","Latency_ms") } };
    ReportSection tech{ "Technical Metrics", {
        "Frequency Response (curva)", "Phase (curva)", "Group Delay (curva)",
        "Impulse Response (curva)", "THD: " + sc("MS-05","THD_pct"),
        "THD+N: " + sc("MS-06","THDN_pct"), "IMD: " + sc("MS-07","IMD_pct") } };
    ReportSection dyn{ "Dynamic Behaviour", {
        "Settling: " + sc("MS-13","SettlingTime_ms"),
        "Repetition Consistency: " + sc("MS-13","RepetitionConsistency_pct"),
        "Drift: " + sc("MS-13","Drift_dB") } };
    ReportSection mus{ "Musical Behaviour", {
        "Attack Tracking: " + sc("MS-14","AttackTracking_pct"),
        "Overshoot: " + sc("MS-14","Overshoot_dB"),
        "Aggregate Linearity: " + sc("MS-15","AggregateLinearity_dB"),
        "Nota: caracterizacao DSP; sem inferencia musical." } };
    ReportSection noise{ "Noise Analysis", {
        "Noise Floor: " + sc("MS-08","NoiseFloor_dBFS"),
        "Dynamic Range: " + sc("MS-09","DynamicRange_dB"),
        "SNR: " + sc("MS-10","SNR_dB") } };
    std::string stereoLine = (b.meta.mode == ChannelMode::Stereo)
        ? ("Crosstalk: " + sc("MS-11","Crosstalk_dB")) : "N/A - medicao mono";
    ReportSection stereo{ "Stereo Analysis", { stereoLine } };

    rm.sections = { summary, tech, dyn, mus, noise, stereo };
    return rm;
}

ExportStatus ExportEngine::exportPDF(const ResultBundle& b, IPdfWriter& writer,
                                     const std::string& path) const {
    ExportStatus st; st.path = path;
    ReportModel rm = buildReportModel(b);
    if (!writer.write(rm, path)) { st.message = "Falha ao gerar PDF em " + path; return st; }
    st.ok = true;
    return st;
}

} // namespace aamp

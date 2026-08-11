// AAMP - src/evidence/EvidenceSerializer.cpp
#include "EvidenceSerializer.h"
#include "../common/RiffWaveWriter.h"   // capacidade neutra (fmt+data), consumida independentemente
#include <fstream>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace aamp {
namespace evidence {
namespace {

// ---- Helpers determinísticos (P-1). Nenhum interpreta a evidência (P-2): só formatam/transportam.

std::string numD(double v)        { char b[64]; std::snprintf(b, sizeof(b), "%.10g", v); return std::string(b); }
std::string numU(unsigned long v) { char b[32]; std::snprintf(b, sizeof(b), "%lu", v);  return std::string(b); }

std::string jsonStr(const std::string& s) {
    std::string o = "\"";
    for (char c : s) {
        const unsigned char u = static_cast<unsigned char>(c);
        switch (c) {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n";  break;
            case '\r': o += "\\r";  break;
            case '\t': o += "\\t";  break;
            default:
                if (u < 0x20) { char b[8]; std::snprintf(b, sizeof(b), "\\u%04x", u); o += b; }
                else          { o += c; }
        }
    }
    o += "\"";
    return o;
}

// "ST-01" -> "ST01" (base do nome de arquivo, convenção STxx de evidence-package.md).
std::string fileBase(const std::string& code) {
    std::string r;
    for (char c : code) if (c != '-') r += c;
    return r;
}

const char* channelsStr(Channels ch) { return ch == Channels::Stereo ? "Stereo" : "Mono"; }

// ---- ZIP STORE determinístico (método 0, sem compressão; data/hora zeradas -> P-1) ----
// Reutiliza os emissores little-endian neutros (riff::putU16/putU32).

std::uint32_t crc32(const std::string& data) {
    static std::uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        init = true;
    }
    std::uint32_t c = 0xFFFFFFFFu;
    for (char ch : data) c = table[(c ^ static_cast<unsigned char>(ch)) & 0xFFu] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

struct Entry { std::string name; std::string content; };

std::string zipStore(const std::vector<Entry>& entries) {
    struct Central { std::string name; std::uint32_t crc; std::uint32_t size; std::uint32_t offset; };
    std::string out;
    std::vector<Central> cds;
    cds.reserve(entries.size());

    for (const Entry& e : entries) {
        const std::uint32_t crc  = crc32(e.content);
        const std::uint32_t size = static_cast<std::uint32_t>(e.content.size());
        const std::uint32_t off  = static_cast<std::uint32_t>(out.size());
        // local file header (STORE)
        riff::putU32(out, 0x04034b50); riff::putU16(out, 20); riff::putU16(out, 0); riff::putU16(out, 0);
        riff::putU16(out, 0); riff::putU16(out, 0);                       // time/date zerados (determinístico)
        riff::putU32(out, crc); riff::putU32(out, size); riff::putU32(out, size);
        riff::putU16(out, static_cast<std::uint16_t>(e.name.size())); riff::putU16(out, 0);
        out.append(e.name);
        out.append(e.content);
        cds.push_back({ e.name, crc, size, off });
    }

    const std::uint32_t cdStart = static_cast<std::uint32_t>(out.size());
    for (const Central& e : cds) {
        riff::putU32(out, 0x02014b50); riff::putU16(out, 20); riff::putU16(out, 20);
        riff::putU16(out, 0); riff::putU16(out, 0);
        riff::putU16(out, 0); riff::putU16(out, 0);
        riff::putU32(out, e.crc); riff::putU32(out, e.size); riff::putU32(out, e.size);
        riff::putU16(out, static_cast<std::uint16_t>(e.name.size())); riff::putU16(out, 0); riff::putU16(out, 0);
        riff::putU16(out, 0); riff::putU16(out, 0); riff::putU32(out, 0); riff::putU32(out, e.offset);
        out.append(e.name);
    }
    const std::uint32_t cdSize = static_cast<std::uint32_t>(out.size()) - cdStart;
    riff::putU32(out, 0x06054b50); riff::putU16(out, 0); riff::putU16(out, 0);
    riff::putU16(out, static_cast<std::uint16_t>(cds.size()));
    riff::putU16(out, static_cast<std::uint16_t>(cds.size()));
    riff::putU32(out, cdSize); riff::putU32(out, cdStart); riff::putU16(out, 0);
    return out;
}

// ---- Materialização JSON dos metadados (leitura direta do modelo; nenhuma interpretação) ----

std::string manifestJson(const EvidencePackage& pkg) {
    std::string m = "{\"format\":\"aamp\",\"version\":1,"
                    "\"session\":\"session.json\",\"hardware\":\"hardware.json\","
                    "\"acquisition\":\"metadata.json\",\"stimuli\":[";
    for (std::size_t i = 0; i < pkg.stimuli.size(); ++i) {
        const StimulusEvidence& s = pkg.stimuli[i];
        if (i) m += ",";
        m += "{\"code\":" + jsonStr(s.code)
           + ",\"name\":" + jsonStr(s.name)
           + ",\"category\":" + jsonStr(std::string(1, s.category))
           + ",\"file\":" + jsonStr("stimuli/" + fileBase(s.code) + "_Source.wav") + "}";
    }
    m += "],\"captures\":[";
    for (std::size_t i = 0; i < pkg.captures.size(); ++i) {
        const CaptureEvidence& c = pkg.captures[i];
        if (i) m += ",";
        m += "{\"code\":" + jsonStr(c.code)
           + ",\"file\":" + jsonStr("captures/" + fileBase(c.code) + "_Capture.wav")
           + ",\"partial\":" + (c.partial ? "true" : "false") + "}";
    }
    m += "]}";
    return m;
}

std::string sessionJson(const EvidencePackage& pkg) {
    return "{\"sessionId\":" + jsonStr(pkg.session.sessionId)
         + ",\"appVersion\":" + jsonStr(pkg.session.appVersion)
         + ",\"timestamp\":" + jsonStr(pkg.session.timestamp) + "}";
}

std::string hardwareJson(const EvidencePackage& pkg) {
    return "{\"deviceName\":" + jsonStr(pkg.hardware.deviceName)
         + ",\"sampleRate\":" + numD(pkg.hardware.sampleRate)
         + ",\"channels\":" + jsonStr(channelsStr(pkg.hardware.channels)) + "}";
}

std::string metadataJson(const EvidencePackage& pkg) {
    return "{\"clipThresholdDbfs\":" + numD(pkg.acquisition.clipThresholdDbfs)
         + ",\"prngSeed\":" + numU(pkg.acquisition.prngSeed)
         + ",\"preRollSeconds\":" + numD(pkg.acquisition.preRollSeconds)
         + ",\"postRollSeconds\":" + numD(pkg.acquisition.postRollSeconds) + "}";
}

} // namespace

std::string serialize(const EvidencePackage& pkg) {
    const double fs = pkg.hardware.sampleRate;
    std::vector<Entry> entries;
    entries.reserve(4 + pkg.stimuli.size() + pkg.captures.size());

    // Ordem fixa (determinismo P-1): metadados, depois origens, depois capturas.
    entries.push_back({ "manifest.json", manifestJson(pkg) });
    entries.push_back({ "session.json",  sessionJson(pkg)  });
    entries.push_back({ "hardware.json", hardwareJson(pkg) });
    entries.push_back({ "metadata.json", metadataJson(pkg) });

    for (const StimulusEvidence& s : pkg.stimuli)
        entries.push_back({ "stimuli/" + fileBase(s.code) + "_Source.wav",
                            riff::writeFloat32(s.source, fs, 1) });        // fmt+data puro (P-1)
    for (const CaptureEvidence& c : pkg.captures)
        entries.push_back({ "captures/" + fileBase(c.code) + "_Capture.wav",
                            riff::writeFloat32(c.primary, fs, 1) });       // captura mono bruta

    return zipStore(entries);
}

bool writeToFile(const EvidencePackage& pkg, const std::string& path) {
    const std::string bytes = serialize(pkg);
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    f.flush();
    return static_cast<bool>(f);
}

} // namespace evidence
} // namespace aamp

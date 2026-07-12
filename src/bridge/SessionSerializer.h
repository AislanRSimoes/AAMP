// AAMP - src/bridge/SessionSerializer.h
// Serialização da Session canônica (ARCH-01).
//
// Duas saídas a partir do MESMO ResultBundle (M-05):
//   1) serializeSessionVar()  — objeto JSON da análise (analysis.json), na forma exata que o
//      frontend consome em normalizeMetrics().
//   2) serializeAampBytes()   — arquivo .aamp CANÔNICO: ZIP STORE com manifest.json +
//      analysis.json. É o ÚNICO caminho de entrega da caracterização ao Laboratório
//      (ingest → fromAamp → bootSession), idêntico ao caminho de abrir arquivo.
//
// NOTA A2 (capturas): o ResultBundle não carrega áudio bruto (as capturas vivem no
// CaptureEngine, M-04); "capture" sai vazio no manifest e os instrumentos que dependem de
// áudio bruto degradam graciosamente, como já fazem quando M.captures está ausente.
#pragma once
#if defined(AAMP_WITH_JUCE)
#include <juce_core/juce_core.h>
#include "../common/ResultBundle.h"
#include <vector>
#include <string>
#include <cstring>

namespace aamp {

inline juce::var aamp_curveToVar(const Curve& c) {
    auto* o = new juce::DynamicObject();
    o->setProperty("name",  juce::String(c.name.c_str()));
    o->setProperty("xUnit", juce::String(c.xUnit.c_str()));
    o->setProperty("yUnit", juce::String(c.yUnit.c_str()));
    juce::Array<juce::var> x, y;
    x.ensureStorageAllocated((int) c.x.size());
    y.ensureStorageAllocated((int) c.y.size());
    for (double v : c.x) x.add(v);
    for (double v : c.y) y.add(v);
    o->setProperty("x", x);
    o->setProperty("y", y);
    if (c.hasY2) {
        juce::Array<juce::var> y2;
        for (double v : c.y2) y2.add(v);
        o->setProperty("y2", y2);
        o->setProperty("hasY2", true);
    }
    return juce::var(o);
}

inline juce::var aamp_scalarToVar(const Scalar& s) {
    auto* o = new juce::DynamicObject();
    o->setProperty("key",          juce::String(s.key.c_str()));
    o->setProperty("value",        s.value);
    o->setProperty("unit",         juce::String(s.unit.c_str()));
    o->setProperty("quality",      juce::String(qualityToString(s.quality))); // "Normal"/"LowConfidence"/"NotApplicable"/"Error"
    o->setProperty("valuePresent", s.valuePresent);
    return juce::var(o);
}

inline juce::var aamp_metricToVar(const MetricResult& m) {
    auto* o = new juce::DynamicObject();
    o->setProperty("id",      juce::String(m.id.c_str()));   // "MS-01".."MS-15"
    o->setProperty("name",    juce::String(m.name.c_str()));
    o->setProperty("quality", juce::String(qualityToString(m.quality)));
    juce::Array<juce::var> sc, cv;
    for (const auto& s : m.scalars) sc.add(aamp_scalarToVar(s));
    for (const auto& c : m.curves)  cv.add(aamp_curveToVar(c));
    o->setProperty("scalars", sc);
    o->setProperty("curves",  cv);
    return juce::var(o);
}

// analysis.json: payload da Session já como juce::var.
inline juce::var serializeSessionVar(const ResultBundle& b) {
    auto* meta = new juce::DynamicObject();
    meta->setProperty("device",     juce::String(b.meta.deviceName.c_str()));
    meta->setProperty("sampleRate", b.meta.sampleRate);
    meta->setProperty("mode",       juce::String(b.meta.mode == ChannelMode::Stereo ? "Stereo" : "Mono"));
    if (!b.meta.timestamp.empty())
        meta->setProperty("timestamp", juce::String(b.meta.timestamp.c_str()));

    juce::Array<juce::var> metrics;
    for (const auto& m : b.metrics) metrics.add(aamp_metricToVar(m));

    auto* root = new juce::DynamicObject();
    root->setProperty("source",   "Measurement Engine");
    root->setProperty("meta",     juce::var(meta));
    root->setProperty("metrics",  metrics);
    root->setProperty("captures", juce::var(juce::Array<juce::var>())); // A2
    root->setProperty("primary",  0);
    return juce::var(root);
}

// Conveniência: string JSON (log/diag).
inline juce::String serializeSessionJson(const ResultBundle& b) {
    return juce::JSON::toString(serializeSessionVar(b), true);
}

// ---------------------------------------------------------------------------
// .aamp canônico (ARCH-01): ZIP STORE (método 0, sem compressão) contendo
//   manifest.json  { format:"aamp", version:1, analysis:"analysis.json",
//                    device, sampleRate, mode, timestamp, capture:[], primaryCapture:"" }
//   analysis.json  (serializeSessionVar)
// Leitor de referência: readZipStore()/fromAamp() do frontend (EOCD + diretório central).
// ---------------------------------------------------------------------------
namespace aamp_zip_detail {
    inline juce::uint32 crc32(const void* data, size_t len) {
        static juce::uint32 table[256]; static bool init = false;
        if (!init) {
            for (juce::uint32 i = 0; i < 256; ++i) {
                juce::uint32 c = i;
                for (int k = 0; k < 8; ++k) c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
                table[i] = c;
            }
            init = true;
        }
        juce::uint32 c = 0xFFFFFFFFu;
        const auto* p = static_cast<const unsigned char*>(data);
        for (size_t i = 0; i < len; ++i) c = table[(c ^ p[i]) & 0xFFu] ^ (c >> 8);
        return c ^ 0xFFFFFFFFu;
    }
    struct Entry { std::string name; juce::uint32 crc; juce::uint32 size; juce::uint32 offset; };
    inline void put16(juce::MemoryOutputStream& s, juce::uint32 v) { s.writeShort((short) (v & 0xFFFF)); }
    inline void put32(juce::MemoryOutputStream& s, juce::uint32 v) { s.writeInt((int) v); }
    inline void writeFile(juce::MemoryOutputStream& s, std::vector<Entry>& es,
                          const std::string& name, const juce::String& textUtf8) {
        const auto bytes = textUtf8.toRawUTF8();
        const juce::uint32 n = (juce::uint32) strlen(bytes);
        Entry e { name, crc32(bytes, n), n, (juce::uint32) s.getPosition() };
        put32(s, 0x04034b50); put16(s, 20); put16(s, 0); put16(s, 0);   // local header, STORE
        put16(s, 0); put16(s, 0);                                        // time/date (zero: determinístico)
        put32(s, e.crc); put32(s, e.size); put32(s, e.size);
        put16(s, (juce::uint32) name.size()); put16(s, 0);
        s.write(name.data(), name.size());
        s.write(bytes, n);
        es.push_back(e);
    }
} // namespace aamp_zip_detail

// Produz os bytes do .aamp canônico. Retorna true em sucesso (sempre, salvo bundle vazio).
inline bool serializeAampBytes(const ResultBundle& b, juce::MemoryBlock& out) {
    using namespace aamp_zip_detail;
    const juce::String analysis = juce::JSON::toString(serializeSessionVar(b), true);

    auto* man = new juce::DynamicObject();
    man->setProperty("format",         "aamp");
    man->setProperty("version",        1);
    man->setProperty("analysis",       "analysis.json");
    man->setProperty("device",         juce::String(b.meta.deviceName.c_str()));
    man->setProperty("sampleRate",     b.meta.sampleRate);
    man->setProperty("mode",           juce::String(b.meta.mode == ChannelMode::Stereo ? "Stereo" : "Mono"));
    man->setProperty("timestamp",      juce::String(b.meta.timestamp.c_str()));
    man->setProperty("capture",        juce::var(juce::Array<juce::var>()));   // A2
    man->setProperty("primaryCapture", "");
    const juce::String manifest = juce::JSON::toString(juce::var(man), true);

    juce::MemoryOutputStream s;
    std::vector<Entry> es;
    writeFile(s, es, "manifest.json", manifest);
    writeFile(s, es, "analysis.json", analysis);

    const juce::uint32 cdStart = (juce::uint32) s.getPosition();
    for (const auto& e : es) {
        put32(s, 0x02014b50); put16(s, 20); put16(s, 20); put16(s, 0); put16(s, 0);
        put16(s, 0); put16(s, 0);
        put32(s, e.crc); put32(s, e.size); put32(s, e.size);
        put16(s, (juce::uint32) e.name.size()); put16(s, 0); put16(s, 0);
        put16(s, 0); put16(s, 0); put32(s, 0); put32(s, e.offset);
        s.write(e.name.data(), e.name.size());
    }
    const juce::uint32 cdSize = (juce::uint32) s.getPosition() - cdStart;
    put32(s, 0x06054b50); put16(s, 0); put16(s, 0);
    put16(s, (juce::uint32) es.size()); put16(s, (juce::uint32) es.size());
    put32(s, cdSize); put32(s, cdStart); put16(s, 0);

    out = s.getMemoryBlock();
    return out.getSize() > 0;
}

} // namespace aamp
#endif // AAMP_WITH_JUCE

// AAMP - src/bridge/SessionSerializer.h
// SCAFFOLDING DE MAQUINA-ALVO (requer JUCE; nao compilado/verificado neste ambiente headless).
//
// Serializa um ResultBundle (M-05, saida de MeasurementOrchestrator::runSuite) para o objeto
// JSON exatamente na forma que o frontend consome em normalizeMetrics()/fromEngineSession():
//   { source, meta:{device,sampleRate,mode,timestamp},
//     metrics:[ {id,name,quality,scalars:[{key,value,unit,quality,valuePresent}],
//                                curves:[{name,xUnit,yUnit,x:[],y:[],y2:[]?}]} ],
//     captures:[], primary }
//
// Contrato verificado no frontend por round-trip contra a fixture normativa: a Session produzida
// por fromEngineSession(payload) e IDENTICA, campo a campo, a produzida por fromAamp(.aamp).
// (device, MS-01 N=256, 14 metricas, 15 escalares, 3 capturas, primary).
//
// NOTA A2 (capturas): o ResultBundle NAO carrega audio bruto (as capturas vivem no CaptureEngine,
// M-04). Por isso "captures" sai vazio aqui; os instrumentos que dependem de audio bruto
// (temporal/espectrograma/waterfall/original x caracterizado) degradam graciosamente, como ja
// fazem quando M.captures esta ausente. Para alimenta-los ao vivo, exponha os buffers do
// CaptureEngine e preencha "captures" (ver INTEGRATION_BRIDGE.md).
#pragma once
#if defined(AAMP_WITH_JUCE)
#include <juce_core/juce_core.h>
#include "../common/ResultBundle.h"

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

// ===========================================================================================
// ARCH-01: serializacao CANONICA da Session = arquivo .aamp (mesmo formato lido por fromAamp()).
// Produz os bytes de um ZIP STORE (metodo 0, sem compressao) contendo:
//   manifest.json : {device,sampleRate,mode,timestamp,analysis:"analysis.json",capture:[],primaryCapture:null,acquisition:null}
//   analysis.json : {meta:{...}, metrics:[...]}   (MESMAS metricas/curvas/escalares; DBG preservado)
// captures = [] porque o ResultBundle NAO carrega audio bruto (NOTA A2); fromAamp() ja degrada
// graciosamente sem WAV — identico ao que a caracterizacao ja entregava. Nenhum DSP/metrica muda.
// ===========================================================================================

// analysis.json (meta + metricas), consumido por normalizeMetrics(an) no frontend.
inline juce::String aamp_analysisJson(const ResultBundle& b) {
    auto* meta = new juce::DynamicObject();
    meta->setProperty("device",     juce::String(b.meta.deviceName.c_str()));
    meta->setProperty("sampleRate", b.meta.sampleRate);
    meta->setProperty("mode",       juce::String(b.meta.mode == ChannelMode::Stereo ? "Stereo" : "Mono"));
    meta->setProperty("timestamp",  juce::String(b.meta.timestamp.c_str()));
    juce::Array<juce::var> metrics;
    for (const auto& m : b.metrics) metrics.add(aamp_metricToVar(m));
    auto* root = new juce::DynamicObject();
    root->setProperty("meta",    juce::var(meta));
    root->setProperty("metrics", metrics);
    return juce::JSON::toString(juce::var(root), false);
}

// manifest.json exigido por fromAamp() (readZipStore -> f['manifest.json']).
inline juce::String aamp_manifestJson(const ResultBundle& b) {
    auto* root = new juce::DynamicObject();
    root->setProperty("device",         juce::String(b.meta.deviceName.c_str()));
    root->setProperty("sampleRate",     b.meta.sampleRate);
    root->setProperty("mode",           juce::String(b.meta.mode == ChannelMode::Stereo ? "Stereo" : "Mono"));
    root->setProperty("timestamp",      juce::String(b.meta.timestamp.c_str()));
    root->setProperty("analysis",       "analysis.json");
    root->setProperty("capture",        juce::var(juce::Array<juce::var>())); // sem audio bruto (ResultBundle)
    root->setProperty("primaryCapture", juce::var());   // null
    root->setProperty("acquisition",    juce::var());   // null
    return juce::JSON::toString(juce::var(root), false);
}

// CRC-32 (poly 0xEDB88320) p/ os cabecalhos do ZIP (readZipStore nao valida, mas geramos correto).
inline juce::uint32 aamp_crc32(const juce::uint8* p, size_t len) {
    juce::uint32 table[256];
    for (juce::uint32 i = 0; i < 256; ++i) { juce::uint32 c = i;
        for (int k = 0; k < 8; ++k) c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1); table[i] = c; }
    juce::uint32 c = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) c = table[(c ^ p[i]) & 0xFFu] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

namespace aamp_zip_detail {
    struct Entry { juce::String name; juce::MemoryBlock data; };
    inline void put16(juce::MemoryBlock& b, juce::uint16 v) {
        const juce::uint8 t[2] = { (juce::uint8)(v & 0xFF), (juce::uint8)((v >> 8) & 0xFF) }; b.append(t, 2); }
    inline void put32(juce::MemoryBlock& b, juce::uint32 v) {
        const juce::uint8 t[4] = { (juce::uint8)(v & 0xFF), (juce::uint8)((v >> 8) & 0xFF),
                                   (juce::uint8)((v >> 16) & 0xFF), (juce::uint8)((v >> 24) & 0xFF) }; b.append(t, 4); }
}

// Monta um ZIP STORE (metodo 0) no layout exato que readZipStore() parseia (EOCD + dir central + local headers).
inline juce::MemoryBlock aamp_buildStoreZip(const std::vector<aamp_zip_detail::Entry>& entries) {
    using namespace aamp_zip_detail;
    juce::MemoryBlock out;
    std::vector<juce::uint32> lho(entries.size()), crc(entries.size());
    for (size_t i = 0; i < entries.size(); ++i) {
        const Entry& e = entries[i];
        lho[i] = (juce::uint32) out.getSize();
        const size_t nlen = e.name.getNumBytesAsUTF8();
        crc[i] = aamp_crc32((const juce::uint8*) e.data.getData(), e.data.getSize());
        put32(out, 0x04034b50u); put16(out, 20); put16(out, 0); put16(out, 0);     // sig, ver, flags, method=STORE
        put16(out, 0); put16(out, 0x21);                                           // mod time, mod date (1980-01-01)
        put32(out, crc[i]); put32(out, (juce::uint32) e.data.getSize()); put32(out, (juce::uint32) e.data.getSize());
        put16(out, (juce::uint16) nlen); put16(out, 0);                            // fnlen, extralen
        out.append(e.name.toRawUTF8(), nlen);
        out.append(e.data.getData(), e.data.getSize());
    }
    const juce::uint32 cdStart = (juce::uint32) out.getSize();
    for (size_t i = 0; i < entries.size(); ++i) {
        const Entry& e = entries[i];
        const size_t nlen = e.name.getNumBytesAsUTF8();
        put32(out, 0x02014b50u); put16(out, 20); put16(out, 20); put16(out, 0); put16(out, 0); // sig,vmade,vneed,flags,method
        put16(out, 0); put16(out, 0x21);                                           // time, date
        put32(out, crc[i]); put32(out, (juce::uint32) e.data.getSize()); put32(out, (juce::uint32) e.data.getSize());
        put16(out, (juce::uint16) nlen); put16(out, 0); put16(out, 0);             // fnlen, extralen, commentlen
        put16(out, 0); put16(out, 0); put32(out, 0);                               // disk#, intattr, extattr
        put32(out, lho[i]);
        out.append(e.name.toRawUTF8(), nlen);
    }
    const juce::uint32 cdSize = (juce::uint32) out.getSize() - cdStart;
    put32(out, 0x06054b50u); put16(out, 0); put16(out, 0);
    put16(out, (juce::uint16) entries.size()); put16(out, (juce::uint16) entries.size());
    put32(out, cdSize); put32(out, cdStart); put16(out, 0);
    return out;
}

// Serializacao CANONICA: bytes de um .aamp (manifest.json + analysis.json em ZIP STORE).
inline juce::MemoryBlock serializeAampBytes(const ResultBundle& b) {
    std::vector<aamp_zip_detail::Entry> entries;
    { aamp_zip_detail::Entry e; e.name = "manifest.json";
      const juce::String s = aamp_manifestJson(b); e.data.append(s.toRawUTF8(), s.getNumBytesAsUTF8());
      entries.push_back(std::move(e)); }
    { aamp_zip_detail::Entry e; e.name = "analysis.json";
      const juce::String s = aamp_analysisJson(b); e.data.append(s.toRawUTF8(), s.getNumBytesAsUTF8());
      entries.push_back(std::move(e)); }
    return aamp_buildStoreZip(entries);
}

} // namespace aamp
#endif // AAMP_WITH_JUCE

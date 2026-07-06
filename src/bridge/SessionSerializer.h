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

// Devolve o payload da Session ja como juce::var (para embutir em runFinished sem re-parse).
inline juce::var serializeSessionVar(const ResultBundle& b) {
    auto* meta = new juce::DynamicObject();
    meta->setProperty("device",     juce::String(b.meta.deviceName.c_str()));
    meta->setProperty("sampleRate", b.meta.sampleRate);
    meta->setProperty("mode",       juce::String(b.meta.mode == ChannelMode::Stereo ? "Stereo" : "Mono"));
    // meta.timestamp: consumido por fromEngineSession() (meta.timestamp). Emitido quando presente;
    // o frontend aceita ausencia ('—'). Nao emitir vazio para nao sobrepor o fallback do frontend.
    if (!b.meta.timestamp.empty())
        meta->setProperty("timestamp", juce::String(b.meta.timestamp.c_str()));

    juce::Array<juce::var> metrics;
    for (const auto& m : b.metrics) metrics.add(aamp_metricToVar(m));

    auto* root = new juce::DynamicObject();
    root->setProperty("source",   "Measurement Engine");
    root->setProperty("meta",     juce::var(meta));
    root->setProperty("metrics",  metrics);
    root->setProperty("captures", juce::var(juce::Array<juce::var>())); // A2: preencher com buffers do CaptureEngine
    root->setProperty("primary",  0);
    return juce::var(root);
}

// Conveniencia: string JSON (p/ log/diag).
inline juce::String serializeSessionJson(const ResultBundle& b) {
    return juce::JSON::toString(serializeSessionVar(b), true);
}

} // namespace aamp
#endif // AAMP_WITH_JUCE

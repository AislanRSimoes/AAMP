// AAMP - src/dashboard/DashboardModel.cpp
#include "DashboardModel.h"
#include <cstdio>

namespace aamp {

const char* itemStateName(ItemState s) {
    switch (s) {
        case ItemState::Normal: return "Normal";
        case ItemState::LowConfidence: return "LowConfidence";
        case ItemState::NotApplicable: return "NotApplicable";
        case ItemState::Empty: return "Empty";
    }
    return "Unknown";
}

static std::string fmt3(double v) {
    char b[64]; std::snprintf(b, sizeof(b), "%.3f", v);
    for (char& c : b) if (c == ',') c = '.';
    return std::string(b);
}

// Cria um item a partir de um escalar localizado por (metricId, key). Estado deriva da Quality.
static DashboardItem item(const ResultBundle& b, const std::string& label,
                          const char* metricId, const char* key) {
    DashboardItem it; it.label = label;
    const MetricResult* m = b.find(metricId);
    if (!m) { it.state = ItemState::Empty; return it; }
    const Scalar* found = nullptr;
    for (const Scalar& s : m->scalars) if (s.key == key) { found = &s; break; }
    if (!found) { it.state = ItemState::Empty; return it; }
    it.unit = found->unit;
    if (!found->valuePresent || found->quality == Quality::NotApplicable) {
        it.value = "N/A"; it.state = ItemState::NotApplicable; return it;
    }
    it.value = fmt3(found->value);
    it.state = (found->quality == Quality::LowConfidence) ? ItemState::LowConfidence : ItemState::Normal;
    return it;
}

DashboardModel buildDashboardModel(const ResultBundle& b) {
    DashboardModel dm;
    dm.isEmpty = b.metrics.empty();

    DashboardSection summary{ "Summary", {
        item(b, "THD", "MS-05", "THD_pct"),
        item(b, "SNR", "MS-10", "SNR_dB"),
        item(b, "Dynamic Range", "MS-09", "DynamicRange_dB"),
        item(b, "Latency", "MS-12", "Latency_ms") } };

    DashboardSection tech{ "Technical Metrics", {
        item(b, "THD", "MS-05", "THD_pct"),
        item(b, "THD+N", "MS-06", "THDN_pct"),
        item(b, "IMD", "MS-07", "IMD_pct") } };

    DashboardSection dyn{ "Dynamic Behaviour", {
        item(b, "Settling Time", "MS-13", "SettlingTime_ms"),
        // V3 / AAMP-HG-20260811-001: o escalar e um COEFICIENTE DE VARIACAO (0% = perfeito).
        // Rotulo de apresentacao neutralizado; a chave do contrato NAO muda.
        item(b, "Repetition Variation", "MS-13", "RepetitionConsistency_pct"),
        item(b, "Drift", "MS-13", "Drift_dB") } };

    DashboardSection mus{ "Musical Behaviour", {
        item(b, "Attack Tracking", "MS-14", "AttackTracking_pct"),
        item(b, "Overshoot", "MS-14", "Overshoot_dB"),
        item(b, "Aggregate Linearity", "MS-15", "AggregateLinearity_dB") } };

    DashboardSection noise{ "Noise Analysis", {
        item(b, "Noise Floor", "MS-08", "NoiseFloor_dBFS"),
        item(b, "Dynamic Range", "MS-09", "DynamicRange_dB"),
        item(b, "SNR", "MS-10", "SNR_dB") } };

    DashboardSection stereo{ "Stereo Analysis", {
        item(b, "Crosstalk", "MS-11", "Crosstalk_dB") } };

    dm.sections = { summary, tech, dyn, mus, noise, stereo };

    // Bundle vazio: garante itens em estado Empty (estado vazio do dashboard, FR-031).
    if (dm.isEmpty)
        for (auto& sec : dm.sections)
            for (auto& it : sec.items) { it.state = ItemState::Empty; it.value.clear(); }

    return dm;
}

} // namespace aamp

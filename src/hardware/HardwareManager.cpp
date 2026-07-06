// AAMP - src/hardware/HardwareManager.cpp
#include "HardwareManager.h"
#include <algorithm>

namespace aamp {

HardwareManager::HardwareManager(std::shared_ptr<IDeviceEnumerator> enumerator)
    : m_enum(std::move(enumerator)) {}

std::vector<DeviceInfo> HardwareManager::enumerateDevices() const {
    std::vector<DeviceInfo> all = m_enum ? m_enum->enumerate() : std::vector<DeviceInfo>{};
    std::vector<DeviceInfo> connected;
    for (DeviceInfo& d : all)
        if (d.connected) connected.push_back(d);   // BR-001: so conectados; sem catalogo estatico
    return connected;
}

const DeviceCapabilities* HardwareManager::getCapabilities(const std::string& deviceId) const {
    static DeviceCapabilities scratch;
    for (const DeviceInfo& d : enumerateDevices())
        if (d.id == deviceId) { scratch = d.caps; return &scratch; }
    return nullptr;
}

DeviceInfo HardwareManager::describeDevice(const std::string& deviceId) const {
    DeviceInfo d = m_enum ? m_enum->describe(deviceId) : DeviceInfo{};
    return d.connected ? d : DeviceInfo{};   // BR-001: so dispositivos efetivamente conectados
}

ConfigValidation HardwareManager::validateConfig(const DeviceInfo& dev, double sampleRate, int bufferSize) const {
    ConfigValidation v;
    const auto& sr = dev.caps.sampleRates;
    const auto& bs = dev.caps.bufferSizes;
    const bool srOk = std::find(sr.begin(), sr.end(), sampleRate) != sr.end();
    const bool bsOk = std::find(bs.begin(), bs.end(), bufferSize) != bs.end();
    if (!srOk || !bsOk) {
        v.ok = false; v.error = HwError::UnsupportedConfig;   // ER-003
        v.message = "Configuracao nao suportada por " + dev.name + ". Selecione valores disponiveis.";
        return v;
    }
    if (!hasUsableChannels(dev)) {
        v.ok = false; v.error = HwError::NoUsableChannels;    // ER-010
        v.message = "O dispositivo nao possui canais de entrada e saida utilizaveis.";
        return v;
    }
    v.ok = true;
    return v;
}

std::string HardwareManager::channelLabel(const DeviceInfo& dev, int index, bool input) {
    const auto& names = input ? dev.caps.inputChannelNames : dev.caps.outputChannelNames;
    if (index >= 0 && index < static_cast<int>(names.size()) && !names[static_cast<std::size_t>(index)].empty())
        return names[static_cast<std::size_t>(index)];
    return "Canal " + std::to_string(index);   // FR-007: fallback determinístico
}

LatencyEstimate HardwareManager::estimateRoundTripLatency(const DeviceInfo& dev, int bufferSize, double sampleRate) {
    LatencyEstimate e;
    if (sampleRate <= 0.0) { e.partial = true; return e; }
    // Round-trip aproximado: 2x buffer (in+out) + latencia reportada pelo driver, se conhecida.
    double samples = 2.0 * bufferSize;
    if (dev.caps.driverLatencyKnown) samples += dev.caps.driverReportedLatencySamples;
    else e.partial = true;                       // FR-008: estimativa parcial -> aviso
    e.milliseconds = samples / sampleRate * 1000.0;
    return e;
}

bool HardwareManager::hasUsableChannels(const DeviceInfo& dev) {
    return dev.caps.inputChannels > 0 && dev.caps.outputChannels > 0;
}

} // namespace aamp

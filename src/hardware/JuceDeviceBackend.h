// AAMP - src/hardware/JuceDeviceBackend.h
// SCAFFOLDING DE MAQUINA-ALVO (nao compilado/verificado neste ambiente headless: requer JUCE).
// Implementa IDeviceEnumerator e IAudioDevice usando juce::AudioDeviceManager, descobrindo
// dispositivos exclusivamente via OS/drivers (ASIO/WASAPI/CoreAudio) — hardware agnostic (DOC-00 §5).
#pragma once
#if defined(AAMP_WITH_JUCE)
#include <juce_audio_devices/juce_audio_devices.h>
#include "HardwareManager.h"
#include "../capture/IAudioDevice.h"
#include <string>
#include <utility>
#include <memory>
#include <algorithm>

namespace aamp {

// ─── Port FIEL da logica de listagem do dialogo "Configurar audio" (Main.cpp: buildDeviceList e
//     helpers). Somente dispositivos CONECTADOS; ASIO priorizado e deduplicado; mapeadores genericos
//     excluidos; nomes de exibicao limpos. Origem = OS/drivers (hardware agnostic, DOC-00 §5).
//     A UNICA mudanca em relacao ao Gerador e o destino: aqui produz DeviceInfo (fonte unica). ───
namespace juce_hw_detail {

inline int apiPriority(const juce::String& tn) {
    if (tn.containsIgnoreCase("ASIO"))          return 0;
    if (tn.containsIgnoreCase("Exclusive"))     return 1;
    if (tn.containsIgnoreCase("Low Latency"))   return 2;
    if (tn.containsIgnoreCase("Windows Audio")) return 3;
    if (tn.containsIgnoreCase("DirectSound"))   return 4;
    return 5;
}
inline juce::String cleanDeviceName(const juce::String& raw) {
    juce::String s = raw.trim();
    if (s.startsWithIgnoreCase("ASIO ")) s = s.substring(5).trim();
    int d = 0;
    while (d < s.length() && s[d] >= '0' && s[d] <= '9') ++d;
    if (d > 0 && d < s.length() && s[d] == '-') s = s.substring(d + 1).trim();
    const int a = s.indexOfChar('('), b = s.lastIndexOfChar(')');
    if (a >= 0 && b > a) s = s.substring(a + 1, b).trim();
    return s;
}
inline bool isGenericMapper(const juce::String& n) {
    const juce::String l = n.toLowerCase();
    return l.contains("som primario") || l.contains("som primário")
        || l.contains("primary sound") || l.contains("mapeador") || l.contains("mapper");
}
// Nome do dispositivo de ENTRADA (input) que corresponde ao de saida (nao-ASIO); fallback: 1o input.
inline juce::String bestInputFor(juce::AudioIODeviceType* t, const juce::String& outName) {
    const auto ins = t->getDeviceNames(true);
    const juce::String key = cleanDeviceName(outName);
    for (const auto& n : ins)
        if (key.isNotEmpty() && cleanDeviceName(n).equalsIgnoreCase(key)) return n;
    return ins.size() > 0 ? ins[0] : juce::String();
}

struct Entry { juce::String typeName, deviceName, display; int priority = 5; bool asio = false; };

// Espelha buildDeviceList (Main.cpp): so conectados, ASIO priorizado/dedup, mapeadores fora.
inline std::vector<Entry> buildConnectedList(juce::AudioDeviceManager& mgr) {
    std::vector<Entry> raw; std::vector<juce::String> winEndpoints;
    for (auto* t : mgr.getAvailableDeviceTypes()) {
        if (t == nullptr) continue;
        t->scanForDevices();
        const int prio = apiPriority(t->getTypeName());
        const bool asio = t->getTypeName().containsIgnoreCase("asio");
        for (const auto& name : t->getDeviceNames(false)) {
            if (!asio && isGenericMapper(name)) continue;                 // mapeadores genericos fora
            raw.push_back({ t->getTypeName(), name, cleanDeviceName(name), prio, asio });
            if (!asio) winEndpoints.push_back(name.toLowerCase());
        }
    }
    const juce::String curType = mgr.getCurrentAudioDeviceType();
    const juce::String curOut  = mgr.getAudioDeviceSetup().outputDeviceName;
    auto asioConnected = [&](const Entry& r) {                            // ASIO so se aparentar conectado
        if (r.typeName == curType && r.deviceName == curOut) return true;
        const juce::String tok = r.display.toLowerCase();
        if (tok.isEmpty()) return false;
        for (const auto& w : winEndpoints) if (w.contains(tok)) return true;
        return false;
    };
    std::vector<Entry> out; std::vector<juce::String> keptAsio;
    for (const auto& r : raw) {
        if (!r.asio || !asioConnected(r)) continue;
        out.push_back(r); keptAsio.push_back(r.display.toLowerCase());
    }
    std::vector<Entry> nonAsio;
    for (const auto& r : raw) {
        if (r.asio) continue;
        bool dup = false; const juce::String low = r.deviceName.toLowerCase();
        for (const auto& tok : keptAsio) if (low.contains(tok)) { dup = true; break; }
        if (!dup) nonAsio.push_back(r);
    }
    std::sort(nonAsio.begin(), nonAsio.end(), [](const Entry& a, const Entry& b){
        if (a.display != b.display) return a.display < b.display; return a.priority < b.priority; });
    for (const auto& o : nonAsio)
        if (std::none_of(out.begin(), out.end(), [&](const Entry& x){ return x.display == o.display; }))
            out.push_back(o);
    std::sort(out.begin(), out.end(), [](const Entry& a, const Entry& b){
        if (a.priority != b.priority) return a.priority < b.priority; return a.display < b.display; });
    return out;
}
inline juce::AudioIODeviceType* findType(juce::AudioDeviceManager& mgr, const juce::String& typeName) {
    for (auto* t : mgr.getAvailableDeviceTypes()) if (t && t->getTypeName() == typeName) return t;
    return nullptr;
}
// createDevice p/ ler capacidades: ASIO duplex usa o mesmo nome; nao-ASIO casa o input (bestInputFor).
inline std::unique_ptr<juce::AudioIODevice> makeDevice(juce::AudioIODeviceType* t, const juce::String& outName) {
    if (t == nullptr) return {};
    const bool asio = t->getTypeName().containsIgnoreCase("asio");
    const juce::String inName = asio ? outName : bestInputFor(t, outName);
    return std::unique_ptr<juce::AudioIODevice>(t->createDevice(outName, inName));
}

} // namespace juce_hw_detail

// Enumeracao real: DeviceInfo a partir de OS/drivers (so conectados; sem catalogo).
class JuceDeviceEnumerator : public IDeviceEnumerator {
public:
    // LISTAGEM: mesma logica do dialogo JUCE (buildConnectedList) — so conectados, nomes limpos,
    // ASIO priorizado. id = nome REAL (para open); name = nome de exibicao; caps.in/out = canais reais.
    std::vector<DeviceInfo> enumerate() override {
        using namespace juce_hw_detail;
        std::vector<DeviceInfo> out;
        juce::AudioDeviceManager mgr;
        for (const auto& e : buildConnectedList(mgr)) {
            DeviceInfo di;
            di.id = e.deviceName.toStdString();      // nome REAL do device (createDevice/open)
            di.name = e.display.toStdString();       // nome de exibicao limpo (como no dialogo JUCE)
            di.backend = e.typeName.toStdString();
            di.connected = true;
            if (auto dev = makeDevice(findType(mgr, e.typeName), e.deviceName)) {
                di.caps.inputChannels  = dev->getInputChannelNames().size();   // nIn
                di.caps.outputChannels = dev->getOutputChannelNames().size();  // nOut
            }
            out.push_back(std::move(di));
        }
        return out;
    }

    // DESCRICAO COMPLETA de UM device (usada no prepare): le todas as capacidades expostas pelo
    // driver (sample rates, buffers, nomes de canais). Sem open()/start() do stream.
    DeviceInfo describe(const std::string& id) override {
        using namespace juce_hw_detail;
        juce::AudioDeviceManager mgr;
        const juce::String target(id.c_str());
        for (const auto& e : buildConnectedList(mgr)) {
            if (e.deviceName != target) continue;
            auto dev = makeDevice(findType(mgr, e.typeName), e.deviceName);
            if (!dev) return {};
            DeviceInfo di;
            di.id = e.deviceName.toStdString(); di.name = e.display.toStdString();
            di.backend = e.typeName.toStdString(); di.connected = true;
            for (const auto& n : dev->getInputChannelNames())  di.caps.inputChannelNames.push_back(n.toStdString());
            for (const auto& n : dev->getOutputChannelNames()) di.caps.outputChannelNames.push_back(n.toStdString());
            di.caps.inputChannels  = (int) di.caps.inputChannelNames.size();
            di.caps.outputChannels = (int) di.caps.outputChannelNames.size();
            for (double sr : dev->getAvailableSampleRates()) di.caps.sampleRates.push_back(sr);
            for (int bs : dev->getAvailableBufferSizes())    di.caps.bufferSizes.push_back(bs);
            return di;
        }
        return {};
    }
};

// Stream real-time-safe (AudioThread): geracao/captura por callback, sem alocacao/lock/IO (AD-03).
class JuceAudioDevice : public IAudioDevice, private juce::AudioIODeviceCallback {
    // ... abre o device via AudioDeviceManager; playAndCapture sincroniza estimulo/captura.
    // Implementacao completa (abertura ASIO/WASAPI + playAndCapture) pertence ao build da
    // maquina-alvo (E2E/SLAs). O que ESTA definido aqui e a IDENTIDADE do dispositivo escolhido:
    // o id vindo de HardwareManager::enumerateDevices() (== nome do device) e as capacidades ja
    // detectadas pela enumeracao. Assim prepare() valida os canais reais do device selecionado
    // (BR-003 / ER-010) e o mesmo id percorre enumerate -> prepare -> start sem divergencia.
public:
    JuceAudioDevice() = default;
    // Construido a partir do DeviceInfo escolhido pelo usuario (id/nome + capacidades enumeradas).
    JuceAudioDevice(std::string deviceId, double fs, int inChannels, int outChannels)
        : m_id(std::move(deviceId)), m_fs(fs), m_in(inChannels), m_out(outChannels) {}

    const std::string& deviceId() const { return m_id; }

    double sampleRate() const override { return m_fs; }
    int inputChannels() const override { return m_in; }
    int outputChannels() const override { return m_out; }
    std::vector<double> playAndCapture(const std::vector<double>&, std::size_t) override { return {}; }
    void audioDeviceIOCallbackWithContext(const float* const*, int, float* const*, int, int,
                                          const juce::AudioIODeviceCallbackContext&) override {}
    void audioDeviceAboutToStart(juce::AudioIODevice*) override {}
    void audioDeviceStopped() override {}
private:
    std::string m_id;                       // == DeviceInfo::id (nome do device na enumeracao)
    double m_fs = 48000.0; int m_in = 0, m_out = 0;
};

} // namespace aamp
#endif // AAMP_WITH_JUCE

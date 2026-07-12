// AAMP - src/hardware/JuceDeviceBackend.h
// Backend real de descoberta (máquina-alvo, JUCE): implementa IDeviceEnumerator usando
// juce::AudioDeviceManager, descobrindo dispositivos exclusivamente via OS/drivers
// (ASIO/WASAPI/CoreAudio) — hardware agnostic (DOC-00 §5), sem catálogo estático.
#pragma once
#if defined(AAMP_WITH_JUCE)
#include <juce_audio_devices/juce_audio_devices.h>
#include "HardwareManager.h"
#include "../capture/IAudioDevice.h"
#include <algorithm>                                  // std::sort (dedup por prioridade)

namespace aamp {

// Enumeração real: traduz juce::AudioIODeviceType -> DeviceInfo.
// SOMENTE DISPOSITIVOS REALMENTE CONECTADOS (BR-001). Critérios da implementação de
// referência (buildDeviceList), integralmente restaurados:
//   1) isGenericMapper  -> descarta mapeadores/dispositivos "primários" virtuais (não-ASIO);
//   2) cleanDeviceName  -> nome de exibição igual ao do Windows (remove prefixo "ASIO ",
//      índice "1-", extrai o conteúdo "(...)" quando presente);
//   3) asioPresent      -> um driver ASIO só é listado se o hardware físico correspondente
//      estiver de fato presente no sistema: seu nome limpo precisa aparecer entre os
//      endpoints Windows (WASAPI/DirectSound) atuais. Drivers ASIO residuais de hardware
//      desconectado/desinstalado NÃO aparecem;
//   4) apiPriority + dedup -> um único item por dispositivo físico, mantendo a API de maior
//      prioridade (ASIO > Exclusive > Low Latency > Windows Audio > DirectSound).
// id = nome BRUTO do driver (é o que setAudioDeviceSetup abre); name = nome LIMPO (exibido).
class JuceDeviceEnumerator : public IDeviceEnumerator {
public:
    std::vector<DeviceInfo> enumerate() override {
        auto apiPriority = [](const juce::String& tn) -> int {
            if (tn.containsIgnoreCase("ASIO"))          return 0;
            if (tn.containsIgnoreCase("Exclusive"))     return 1;
            if (tn.containsIgnoreCase("Low Latency"))   return 2;
            if (tn.containsIgnoreCase("Windows Audio")) return 3;
            if (tn.containsIgnoreCase("DirectSound"))   return 4;
            return 5;
        };
        auto cleanName = [](const juce::String& raw) -> juce::String {
            juce::String s = raw.trim();
            if (s.startsWithIgnoreCase("ASIO ")) s = s.substring(5).trim();
            int d = 0;
            while (d < s.length() && s[d] >= '0' && s[d] <= '9') ++d;
            if (d > 0 && d < s.length() && s[d] == '-') s = s.substring(d + 1).trim();
            const int a = s.indexOfChar('('), b = s.lastIndexOfChar(')');
            if (a >= 0 && b > a) s = s.substring(a + 1, b).trim();
            return s;
        };
        auto isGenericMapper = [](const juce::String& n) -> bool {
            const juce::String l = n.toLowerCase();
            return l.contains("som primario") || l.contains("som prim\xc3\xa1rio")
                || l.contains("primary sound") || l.contains("mapeador") || l.contains("mapper");
        };

        struct Raw { juce::String typeName, rawName, display; int prio; bool asio; };
        std::vector<Raw> raw;
        std::vector<juce::String> winEndpoints;       // endpoints reais do OS (não-ASIO), minúsculos
        juce::AudioDeviceManager mgr;                 // não abre nenhum device (lista preguiçosa)
        for (auto* type : mgr.getAvailableDeviceTypes()) {
            if (type == nullptr) continue;
            type->scanForDevices();
            const juce::String tn   = type->getTypeName();
            const bool         asio = tn.containsIgnoreCase("asio");
            for (const auto& name : type->getDeviceNames()) {
                if (!asio && isGenericMapper(name)) continue;      // 1) fora mapeadores virtuais
                raw.push_back({ tn, name, cleanName(name), apiPriority(tn), asio });
                if (!asio) winEndpoints.push_back(name.toLowerCase());
            }
        }

        // 3) presença física: ASIO listado somente se o hardware está conectado AGORA.
        auto asioPresent = [&](const Raw& r) -> bool {
            const juce::String tok = r.display.toLowerCase();
            if (tok.isEmpty()) return false;
            for (const auto& w : winEndpoints) if (w.contains(tok)) return true;
            return false;
        };

        // 4) dedup por nome de exibição, mantendo a API de maior prioridade.
        std::sort(raw.begin(), raw.end(), [](const Raw& a, const Raw& b) {
            const int c = a.display.compareIgnoreCase(b.display);
            if (c != 0) return c < 0;
            return a.prio < b.prio;
        });
        std::vector<DeviceInfo> out;
        for (size_t i = 0; i < raw.size(); ++i) {
            if (raw[i].asio && !asioPresent(raw[i])) continue;                          // 3)
            if (i > 0 && raw[i].display.equalsIgnoreCase(raw[i - 1].display)
                      && !out.empty() && out.back().name == raw[i].display.toStdString())
                continue;                                                               // duplicata
            DeviceInfo di;
            di.id        = raw[i].rawName.toStdString();  // nome BRUTO: é o que doPrepare abre
            di.name      = raw[i].display.toStdString();  // nome LIMPO: igual ao exibido pelo Windows
            di.backend   = raw[i].typeName.toStdString(); // tipo real do OS (setCurrentAudioDeviceType)
            di.connected = true;
            // Canais/sample rates/buffers deliberadamente NÃO consultados aqui: recomputados
            // no device aberto por doPrepare(); a lista do frontend usa apenas nome + backend.
            out.push_back(std::move(di));
        }
        return out;
    }
};

// Stream real-time-safe (AudioThread): geração/captura por callback, sem alocação/lock/IO (AD-03).
class JuceAudioDevice : public IAudioDevice, private juce::AudioIODeviceCallback {
    // ... abre o device via AudioDeviceManager; playAndCapture sincroniza estímulo/captura.
    // Implementação completa pertence ao build da máquina-alvo (E2E/SLAs).
public:
    double sampleRate() const override { return m_fs; }
    int inputChannels() const override { return m_in; }
    int outputChannels() const override { return m_out; }
    std::vector<double> playAndCapture(const std::vector<double>&, std::size_t) override { return {}; }
    void audioDeviceIOCallbackWithContext(const float* const*, int, float* const*, int, int,
                                          const juce::AudioIODeviceCallbackContext&) override {}
    void audioDeviceAboutToStart(juce::AudioIODevice*) override {}
    void audioDeviceStopped() override {}
private:
    double m_fs = 48000.0; int m_in = 0, m_out = 0;
};

} // namespace aamp
#endif // AAMP_WITH_JUCE

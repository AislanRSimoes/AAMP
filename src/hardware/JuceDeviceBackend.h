// AAMP - src/hardware/JuceDeviceBackend.h
// SCAFFOLDING DE MAQUINA-ALVO (nao compilado/verificado neste ambiente headless: requer JUCE).
// Implementa IDeviceEnumerator e IAudioDevice usando juce::AudioDeviceManager, descobrindo
// dispositivos exclusivamente via OS/drivers (ASIO/WASAPI/CoreAudio) — hardware agnostic (DOC-00 §5).
#pragma once
#if defined(AAMP_WITH_JUCE)
#include <juce_audio_devices/juce_audio_devices.h>
#include "HardwareManager.h"
#include "../capture/IAudioDevice.h"
#include <algorithm>                                  // std::sort (DEV-01: dedup por prioridade)

namespace aamp {

// Enumeracao real: traduz juce::AudioIODeviceType -> DeviceInfo (so conectados; sem catalogo).
class JuceDeviceEnumerator : public IDeviceEnumerator {
public:
    std::vector<DeviceInfo> enumerate() override {
        std::vector<DeviceInfo> out;
        // Enumeracao LEVE + FILTRAGEM da implementacao de referencia (buildDeviceList, Main.cpp).
        // DEV-01: a listagem crua exibia entradas espurias ("Drive" = mapeadores genericos do
        // Windows, ex. "Primary Sound Driver"/"Mapeador de Som do Microsoft"), duplicatas do mesmo
        // endpoint entre APIs (WASAPI/DirectSound/Exclusive) e nomes decorados ("ASIO ...", "1- ...").
        // Criterios portados da referencia:
        //   1) isGenericMapper  -> descarta mapeadores/dispositivos "primarios" virtuais (nao-ASIO);
        //   2) cleanDeviceName  -> nome de exibicao igual ao do Windows (remove prefixo "ASIO ",
        //      indice "1-", e extrai o conteudo "(...)" quando presente);
        //   3) apiPriority + dedup -> um unico item por dispositivo fisico, mantendo a API de maior
        //      prioridade (ASIO > Exclusive > Low Latency > Windows Audio > DirectSound).
        // CONSERVADOR: nenhum ASIO e descartado (o filtro asioConnected da referencia exige device
        // aberto — enumerate() deliberadamente nao abre driver; ver doPrepare()). id = nome BRUTO do
        // driver (e o que setAudioDeviceSetup abre); name = nome LIMPO (o que o usuario ve).
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

        struct Raw { juce::String typeName, rawName, display; int prio; };
        std::vector<Raw> raw;
        juce::AudioDeviceManager mgr;                    // nao abre nenhum device (lista preguicosa)
        for (auto* type : mgr.getAvailableDeviceTypes()) {
            if (type == nullptr) continue;
            type->scanForDevices();                      // fiel a buildDeviceList
            const juce::String tn   = type->getTypeName();
            const bool         asio = tn.containsIgnoreCase("asio");
            for (const auto& name : type->getDeviceNames()) {
                if (!asio && isGenericMapper(name)) continue;      // 1) fora mapeadores ("Drive")
                raw.push_back({ tn, name, cleanName(name), apiPriority(tn) });
            }
        }
        // 3) dedup por nome de exibicao, mantendo a API de maior prioridade
        std::sort(raw.begin(), raw.end(), [](const Raw& a, const Raw& b) {
            const int c = a.display.compareIgnoreCase(b.display);
            if (c != 0) return c < 0;
            return a.prio < b.prio;
        });
        for (size_t i = 0; i < raw.size(); ++i) {
            if (i > 0 && raw[i].display.equalsIgnoreCase(raw[i - 1].display)) continue; // duplicata
            DeviceInfo di;
            di.id        = raw[i].rawName.toStdString();  // nome BRUTO: e o que doPrepare abre
            di.name      = raw[i].display.toStdString();  // nome LIMPO: igual ao exibido pelo Windows
            di.backend   = raw[i].typeName.toStdString(); // tipo real do OS (setCurrentAudioDeviceType)
            di.connected = true;
            // Canais/sample rates/buffers deliberadamente NAO consultados aqui: recomputados
            // no device aberto por doPrepare(); a lista do frontend usa apenas nome + backend.
            out.push_back(std::move(di));
        }
        return out;
    }
};

// Stream real-time-safe (AudioThread): geracao/captura por callback, sem alocacao/lock/IO (AD-03).
class JuceAudioDevice : public IAudioDevice, private juce::AudioIODeviceCallback {
    // ... abre o device via AudioDeviceManager; playAndCapture sincroniza estimulo/captura.
    // Implementacao completa pertence ao build da maquina-alvo (E2E/SLAs).
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

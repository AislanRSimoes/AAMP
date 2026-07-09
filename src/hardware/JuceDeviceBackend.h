// AAMP - src/hardware/JuceDeviceBackend.h
// SCAFFOLDING DE MAQUINA-ALVO (nao compilado/verificado neste ambiente headless: requer JUCE).
// Implementa IDeviceEnumerator e IAudioDevice usando juce::AudioDeviceManager, descobrindo
// dispositivos exclusivamente via OS/drivers (ASIO/WASAPI/CoreAudio) — hardware agnostic (DOC-00 §5).
#pragma once
#if defined(AAMP_WITH_JUCE)
#include <juce_audio_devices/juce_audio_devices.h>
#include "HardwareManager.h"
#include "../capture/IAudioDevice.h"

namespace aamp {

// Enumeracao real: traduz juce::AudioIODeviceType -> DeviceInfo (so conectados; sem catalogo).
class JuceDeviceEnumerator : public IDeviceEnumerator {
public:
    std::vector<DeviceInfo> enumerate() override {
        std::vector<DeviceInfo> out;
        // Enumeracao LEVE, fiel a buildDeviceList (Main.cpp, a implementacao de referencia):
        // itera os TIPOS do OS e lista os nomes de dispositivo. NAO abre o device default e NAO
        // faz createDevice por dispositivo. As capacidades REAIS (canais, sample rates, buffers,
        // latencia) sao lidas do device EFETIVAMENTE ABERTO em doPrepare() — nunca na listagem,
        // que so precisa dos nomes. Isso evita carregar o driver ASIO apenas para popular a lista.
        //  - Antes: initialiseWithDefaultDevices(2,2) abria o ASIO default so para listar, e
        //    createDevice(name) + getAvailableSampleRates/BufferSizes/ChannelNames por dispositivo
        //    carregavam cada driver — resultado descartado (doPrepare recomputa do device aberto e
        //    o frontend nao consome canais/taxas na lista). Puro custo bloqueante na Message Thread.
        // getAvailableDeviceTypes() cria/rastreia os tipos de forma preguicosa (sem abrir device).
        juce::AudioDeviceManager mgr;
        for (auto* type : mgr.getAvailableDeviceTypes()) {
            if (type == nullptr) continue;
            type->scanForDevices();                          // fiel a buildDeviceList
            const juce::String backend = type->getTypeName();
            for (const auto& name : type->getDeviceNames()) {// nomes de saida (default)
                DeviceInfo di;
                di.id        = name.toStdString();
                di.name      = name.toStdString();
                di.backend   = backend.toStdString();        // ANTES vazio: agora o tipo real do OS,
                di.connected = true;                         // habilitando setCurrentAudioDeviceType()
                // Canais/sample rates/buffers deliberadamente NAO consultados aqui: recomputados
                // no device aberto por doPrepare(); a lista do frontend usa apenas nome + backend.
                out.push_back(std::move(di));
            }
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

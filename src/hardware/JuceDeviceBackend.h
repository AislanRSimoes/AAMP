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
        juce::AudioDeviceManager mgr;
        mgr.initialiseWithDefaultDevices(2, 2);
        for (auto* type : mgr.getAvailableDeviceTypes()) {
            type->scanForDevices();
            for (const auto& name : type->getDeviceNames()) {
                std::unique_ptr<juce::AudioIODevice> dev(type->createDevice(name, name));
                if (!dev) continue;
                DeviceInfo di; di.id = name.toStdString(); di.name = name.toStdString();
                di.connected = true; // listado pelo OS => detectado
                di.caps.inputChannels  = dev->getInputChannelNames().size();
                di.caps.outputChannels = dev->getOutputChannelNames().size();
                for (double sr : dev->getAvailableSampleRates()) di.caps.sampleRates.push_back(sr);
                for (int bs : dev->getAvailableBufferSizes())    di.caps.bufferSizes.push_back(bs);
                // fabricante exibido apenas se o driver expoe (FR-003) — JUCE nem sempre fornece.
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

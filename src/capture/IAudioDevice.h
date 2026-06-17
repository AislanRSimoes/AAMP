// AAMP - src/capture/IAudioDevice.h
// Fronteira de dispositivo. O nucleo (M-04) depende apenas desta interface; os backends
// reais (ASIO/WASAPI/CoreAudio de M-01, camada L-04) a implementam na maquina-alvo.
// O LoopbackDevice virtual (Plano de Testes §2.1) permite testar M-04 sem hardware.
#pragma once
#include <vector>
#include <cstddef>

namespace aamp {

class IAudioDevice {
public:
    virtual ~IAudioDevice() = default;

    // Reproduz 'stimulus' e captura a resposta sincronizada, devolvendo 'captureLen' samples.
    // Implementacoes reais sao real-time safe (AudioThread). Aqui a interface e sincrona
    // porque o nucleo headless modela apenas a logica de captura/validacao.
    virtual std::vector<double> playAndCapture(const std::vector<double>& stimulus,
                                               std::size_t captureLen) = 0;

    virtual double sampleRate() const = 0;
    virtual int    inputChannels() const = 0;
    virtual int    outputChannels() const = 0;
};

// Dispositivo virtual de loopback: sistema sob teste sintetico H(f)=1 com latencia conhecida.
// Comportamentos injetaveis para teste: ganho (clipping), atenuacao (sinal baixo),
// truncamento (captura parcial) e piso de ruido.
class LoopbackDevice : public IAudioDevice {
public:
    explicit LoopbackDevice(double fs, int latencySamples = 0)
        : m_fs(fs), m_latency(latencySamples) {}

    std::vector<double> playAndCapture(const std::vector<double>& stimulus,
                                       std::size_t captureLen) override;

    double sampleRate() const override { return m_fs; }
    int inputChannels() const override { return m_inCh; }
    int outputChannels() const override { return m_outCh; }

    // injecao de comportamento (apenas para harness de teste)
    void setGain(double g)            { m_gain = g; }        // > 1 -> forca clipping
    void setNoiseFloorDbfs(double d)  { m_noiseDbfs = d; }   // piso de ruido sintetico
    void setTruncateTo(std::size_t n) { m_truncate = n; }    // captura parcial (< captureLen)
    void setChannels(int in, int out) { m_inCh = in; m_outCh = out; }

private:
    double m_fs;
    int    m_latency;
    double m_gain = 1.0;
    double m_noiseDbfs = -200.0; // desabilitado por padrao: loopback ideal H(f)=1
    std::size_t m_truncate = 0; // 0 = sem truncamento
    int m_inCh = 1, m_outCh = 1;
};

} // namespace aamp

// AAMP - src/hardware/HardwareManager.h
// M-01 (DOC-08 §3): enumerateDevices(), validateConfig(), getCapabilities(), onHotPlug().
// Camada de LOGICA agnostica de plataforma (testavel headless via IDeviceEnumerator mock).
// Os backends reais ASIO/WASAPI/CoreAudio (L-04) implementam IDeviceEnumerator na maquina-alvo.
// Hardware agnostic (DOC-00 §5): SEM catalogo estatico; so dispositivos detectados pelo OS.
#pragma once
#include <string>
#include <vector>
#include <memory>

namespace aamp {

// Codigos de erro do DOC-07 relevantes a hardware/configuracao.
enum class HwError { None, DeviceLost /*ER-002*/, UnsupportedConfig /*ER-003*/,
                     NoUsableChannels /*ER-010*/ };

struct DeviceCapabilities {
    int inputChannels = 0;
    int outputChannels = 0;
    std::vector<double> sampleRates;     // suportados pelo dispositivo (sem lista estatica)
    std::vector<int>    bufferSizes;     // suportados
    std::vector<int>    bitDepths;       // suportados
    std::vector<std::string> inputChannelNames;  // vazio => driver nao expoe (fallback)
    std::vector<std::string> outputChannelNames;
    std::string manufacturer;            // vazio => driver nao expoe (omitido na UI)
    bool driverLatencyKnown = false;     // FR-008: aviso quando estimativa e parcial
    int  driverReportedLatencySamples = 0;
};

struct DeviceInfo {
    std::string id;
    std::string name;
    std::string backend;                 // DOC-05 §3.2: tipo do driver (ASIO/WASAPI/CoreAudio); vazio => desconhecido
    bool connected = true;               // BR-001: so conectados aparecem
    DeviceCapabilities caps;
};

// Fronteira de descoberta. Mock nos testes; backends de OS na maquina-alvo.
class IDeviceEnumerator {
public:
    virtual ~IDeviceEnumerator() = default;
    virtual std::vector<DeviceInfo> enumerate() = 0;  // pode incluir desconectados; M-01 filtra

    // Capacidades COMPLETAS de UM dispositivo (abre/consulta somente o escolhido). Usado no prepare
    // (nao na listagem), evitando abrir todos os devices na enumeracao. Default: devolve o que a
    // enumeracao ja conhece (suficiente para mock/testes); backends de OS sobrescrevem com a
    // consulta completa (sample rates, buffers, etc.).
    virtual DeviceInfo describe(const std::string& id) {
        for (auto& d : enumerate()) if (d.id == id) return d;
        return {};
    }
};

struct ConfigValidation {
    bool ok = false;
    HwError error = HwError::None;
    std::string message;                 // amigavel (DOC-07)
};

struct LatencyEstimate {
    double milliseconds = 0.0;
    bool partial = false;                // FR-008: driver nao forneceu latencia completa
};

class HardwareManager {
public:
    explicit HardwareManager(std::shared_ptr<IDeviceEnumerator> enumerator);

    // FR-001/BR-001: apenas dispositivos efetivamente conectados; sem catalogo estatico.
    std::vector<DeviceInfo> enumerateDevices() const;

    // FR-003: capacidades do dispositivo (fabricante so quando o driver expoe).
    const DeviceCapabilities* getCapabilities(const std::string& deviceId) const;

    // FR-003 (prepare): descricao COMPLETA de um unico dispositivo — abre/consulta apenas o
    // escolhido e devolve caps completas + backend. Fonte unica de verdade para o painel (DOC-05 §3.2).
    DeviceInfo describeDevice(const std::string& deviceId) const;

    // FR-004/BR-002: valida SR/buffer contra as capacidades reais -> ER-003 se invalido.
    ConfigValidation validateConfig(const DeviceInfo& dev, double sampleRate, int bufferSize) const;

    // FR-007: nome de canal com fallback determinístico "Canal {indice}".
    static std::string channelLabel(const DeviceInfo& dev, int index, bool input);

    // FR-008: latencia round-trip estimada; aviso quando a info do driver e parcial.
    static LatencyEstimate estimateRoundTripLatency(const DeviceInfo& dev, int bufferSize, double sampleRate);

    // ER-010: dispositivo possui canais utilizaveis de entrada E saida?
    static bool hasUsableChannels(const DeviceInfo& dev);

private:
    std::shared_ptr<IDeviceEnumerator> m_enum;
};

} // namespace aamp

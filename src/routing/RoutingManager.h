// AAMP - src/routing/RoutingManager.h
// M-02 (DOC-08 §3): resolveRouting(), verifyRouting(), detectChannelFaults().
// Roteamento automatico mono/estereo pela contagem de canais (FR-010/011, BR-010).
// Verificacao por tom de calibracao -> ER-022; falha parcial de canal -> ER-025.
#pragma once
#include <string>
#include <vector>
#include <functional>
#include "../common/ResultBundle.h"
#include "../hardware/HardwareManager.h"

namespace aamp {

struct Routing {
    ChannelMode mode = ChannelMode::Mono;   // derivado da contagem de canais
    std::vector<int> outputChannels;        // indices de saida usados
    std::vector<int> inputChannels;         // indices de entrada usados
    bool resolved = false;
};

enum class RoutingError { None, NoSignal /*ER-022*/, ChannelFault /*ER-025*/ };

struct RoutingVerification {
    bool ok = false;
    RoutingError error = RoutingError::None;
    int faultyChannel = -1;
    std::string message;
};

class RoutingManager {
public:
    // FR-010/011: resolve mono/estereo automaticamente pela contagem de canais.
    static Routing resolveRouting(const DeviceInfo& dev);

    // FR-012/ER-022: verifica recepcao do tom de calibracao por canal.
    // levelByInputChannel: nivel de retorno medido em cada canal de entrada.
    static RoutingVerification verifyRouting(const Routing& routing,
                                             const std::vector<double>& levelByInputChannel,
                                             double minLevel = 1e-4);

    // FR-013/ER-025: detecta canal parcialmente funcional (entrada OK / saida falha ou vice-versa).
    // outputResponds/inputResponds: por canal, se respondeu ao estimulo de verificacao.
    static RoutingVerification detectChannelFaults(const std::vector<bool>& outputResponds,
                                                   const std::vector<bool>& inputResponds);
};

} // namespace aamp

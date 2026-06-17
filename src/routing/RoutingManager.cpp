// AAMP - src/routing/RoutingManager.cpp
#include "RoutingManager.h"
#include <algorithm>

namespace aamp {

Routing RoutingManager::resolveRouting(const DeviceInfo& dev) {
    Routing r;
    const int outs = dev.caps.outputChannels;
    const int ins  = dev.caps.inputChannels;
    if (outs <= 0 || ins <= 0) { r.resolved = false; return r; }
    // Estereo quando ha >= 2 canais de saida E entrada; caso contrario, mono (BR-010).
    if (outs >= 2 && ins >= 2) {
        r.mode = ChannelMode::Stereo;
        r.outputChannels = {0, 1};
        r.inputChannels = {0, 1};
    } else {
        r.mode = ChannelMode::Mono;
        r.outputChannels = {0};
        r.inputChannels = {0};
    }
    r.resolved = true;
    return r;
}

RoutingVerification RoutingManager::verifyRouting(const Routing& routing,
                                                  const std::vector<double>& levelByInputChannel,
                                                  double minLevel) {
    RoutingVerification v;
    if (!routing.resolved) { v.message = "Roteamento nao resolvido."; return v; }
    // ER-022: nenhum sinal recebido em qualquer canal de entrada usado -> bloqueia inicio.
    bool anySignal = false;
    for (int ch : routing.inputChannels)
        if (ch >= 0 && ch < static_cast<int>(levelByInputChannel.size())
            && levelByInputChannel[static_cast<std::size_t>(ch)] >= minLevel)
            anySignal = true;
    if (!anySignal) {
        v.ok = false; v.error = RoutingError::NoSignal;
        v.message = "Nenhum sinal recebido no canal de entrada. Verifique as conexoes fisicas.";
        return v;
    }
    v.ok = true;
    return v;
}

RoutingVerification RoutingManager::detectChannelFaults(const std::vector<bool>& outputResponds,
                                                        const std::vector<bool>& inputResponds) {
    RoutingVerification v;
    const std::size_t n = std::min(outputResponds.size(), inputResponds.size());
    for (std::size_t i = 0; i < n; ++i) {
        // ER-025: canal parcialmente funcional (entrada OK / saida falha ou vice-versa).
        if (outputResponds[i] != inputResponds[i]) {
            v.ok = false; v.error = RoutingError::ChannelFault;
            v.faultyChannel = static_cast<int>(i);
            v.message = "O canal " + std::to_string(i) + " nao esta respondendo corretamente. Verifique o roteamento.";
            return v;
        }
    }
    v.ok = true;
    return v;
}

} // namespace aamp

// AAMP - tests/test_hardware_routing.cpp
// M-01 Hardware + M-02 Routing (logica headless via mock): TC-001(parte)/003/004 (AC-M01-01..03),
// resolucao mono/estereo (AC-M02-01), verificacao ER-022 (AC-M02-02), falha de canal ER-025 (AC-M02-03).
#include "TestRunner.h"
#include "../src/hardware/HardwareManager.h"
#include "../src/routing/RoutingManager.h"
#include <memory>

using namespace aamp;

static DeviceInfo makeDevice(const std::string& id, int in, int out, bool connected,
                             std::vector<double> sr = {44100.0, 48000.0},
                             std::vector<int> bufs = {128, 256, 512}) {
    DeviceInfo d; d.id = id; d.name = id; d.connected = connected;
    d.caps.inputChannels = in; d.caps.outputChannels = out;
    d.caps.sampleRates = std::move(sr); d.caps.bufferSizes = std::move(bufs);
    return d;
}

struct MockEnumerator : IDeviceEnumerator {
    std::vector<DeviceInfo> devices;
    std::vector<DeviceInfo> enumerate() override { return devices; }
};

// TC-001 (parte unit): enumeracao lista apenas dispositivos conectados; sem catalogo estatico.
TEST_CASE("TC-001 Enumeracao so de conectados (BR-001)") {
    auto en = std::make_shared<MockEnumerator>();
    en->devices = { makeDevice("A", 2, 2, true), makeDevice("B", 2, 2, false),
                    makeDevice("C", 1, 1, true) };
    HardwareManager hw(en);
    auto list = hw.enumerateDevices();
    CHECK(list.size() == 2);                       // B (desconectado) filtrado
    bool hasB = false; for (auto& d : list) if (d.id == "B") hasB = true;
    CHECK(!hasB);
}

// TC-003: sample rate/buffer nao suportado rejeitado (ER-003).
TEST_CASE("TC-003 SR/buffer nao suportado rejeitado (ER-003)") {
    auto en = std::make_shared<MockEnumerator>();
    DeviceInfo d = makeDevice("X", 2, 2, true, {48000.0}, {256});
    en->devices = { d };
    HardwareManager hw(en);
    // valido
    CHECK(hw.validateConfig(d, 48000.0, 256).ok);
    // SR invalido
    auto r1 = hw.validateConfig(d, 96000.0, 256);
    CHECK(!r1.ok); CHECK(r1.error == HwError::UnsupportedConfig);
    // buffer invalido
    auto r2 = hw.validateConfig(d, 48000.0, 64);
    CHECK(!r2.ok); CHECK(r2.error == HwError::UnsupportedConfig);
}

// TC-004: fallback de nome de canal "Canal {indice}" quando o driver nao expoe.
TEST_CASE("TC-004 Fallback de nome de canal (FR-007)") {
    DeviceInfo d = makeDevice("Y", 2, 2, true);
    // driver nao expoe nomes -> fallback
    CHECK(HardwareManager::channelLabel(d, 0, true) == "Canal 0");
    CHECK(HardwareManager::channelLabel(d, 1, false) == "Canal 1");
    // driver expoe nome -> usa o nome
    d.caps.inputChannelNames = {"Mic L", "Mic R"};
    CHECK(HardwareManager::channelLabel(d, 0, true) == "Mic L");
    CHECK(HardwareManager::channelLabel(d, 1, true) == "Mic R");
}

// FR-008: latencia round-trip; aviso quando a info do driver e parcial.
TEST_CASE("Latencia round-trip: parcial quando driver nao informa (FR-008)") {
    DeviceInfo d = makeDevice("Z", 2, 2, true);
    auto e1 = HardwareManager::estimateRoundTripLatency(d, 256, 48000.0);
    CHECK(e1.partial);                              // driverLatencyKnown=false
    CHECK(e1.milliseconds > 0.0);
    d.caps.driverLatencyKnown = true; d.caps.driverReportedLatencySamples = 100;
    auto e2 = HardwareManager::estimateRoundTripLatency(d, 256, 48000.0);
    CHECK(!e2.partial);
}

// AC-M02-01: roteamento mono/estereo derivado da contagem de canais (FR-011, BR-010).
TEST_CASE("Routing mono/estereo por contagem de canais (AC-M02-01)") {
    Routing mono = RoutingManager::resolveRouting(makeDevice("m", 1, 1, true));
    CHECK(mono.resolved); CHECK(mono.mode == ChannelMode::Mono);
    CHECK(mono.outputChannels.size() == 1);
    Routing st = RoutingManager::resolveRouting(makeDevice("s", 2, 2, true));
    CHECK(st.resolved); CHECK(st.mode == ChannelMode::Stereo);
    CHECK(st.outputChannels.size() == 2);
    // 1 entrada / 2 saidas -> mono (limitado pela entrada).
    Routing mix = RoutingManager::resolveRouting(makeDevice("x", 1, 2, true));
    CHECK(mix.mode == ChannelMode::Mono);
}

// AC-M02-02 / ER-022: verificacao de roteamento bloqueia inicio sem sinal.
TEST_CASE("Routing verify: sem sinal bloqueia (ER-022)") {
    Routing st = RoutingManager::resolveRouting(makeDevice("s", 2, 2, true));
    // sinal presente
    auto ok = RoutingManager::verifyRouting(st, {0.5, 0.5});
    CHECK(ok.ok);
    // nenhum sinal -> ER-022
    auto no = RoutingManager::verifyRouting(st, {0.0, 0.0});
    CHECK(!no.ok); CHECK(no.error == RoutingError::NoSignal);
}

// AC-M02-03 / ER-025: canal parcialmente funcional detectado.
TEST_CASE("Routing faults: canal parcial detectado (ER-025)") {
    // canal 1: saida responde mas entrada nao -> falha parcial
    auto v = RoutingManager::detectChannelFaults({true, true}, {true, false});
    CHECK(!v.ok); CHECK(v.error == RoutingError::ChannelFault); CHECK(v.faultyChannel == 1);
    // tudo OK
    auto ok = RoutingManager::detectChannelFaults({true, true}, {true, true});
    CHECK(ok.ok);
}

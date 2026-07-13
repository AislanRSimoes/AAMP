// AAMP - src/common/RiffWaveWriter.h
// Capacidade NEUTRA de escrita RIFF/WAVE (IEEE float 32-bit), independente do domínio de
// exportação e do domínio de persistência da evidência. Emite fmt+data para um buffer de bytes.
//
// É consumida de forma INDEPENDENTE por quem precisar materializar áudio em WAV — hoje, o
// ExportEngine (que acrescenta seu próprio chunk LIST/INFO de exportação) e o EvidenceSerializer
// (que usa fmt+data puro). Nenhum desses consumidores conhece o outro, e esta capacidade não
// conhece nenhum deles.
//
// Determinismo: `writeFloat32` é função PURA de (samples, fs, channels[, trailingChunks]) — sem
// timestamps, sem estado global, sem qualquer informação externa. Mesma entrada -> mesmos bytes.
// A amostra mono é replicada por canal. `trailingChunks` (bytes opacos) é anexado após o chunk
// `data` e contabilizado no tamanho RIFF, permitindo ao chamador compor chunks próprios.
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>

namespace aamp {
namespace riff {

// Emissores little-endian (primitivas neutras compartilhadas).
inline void putU16(std::string& out, std::uint16_t v) {
    out.push_back(static_cast<char>(v & 0xFF));
    out.push_back(static_cast<char>((v >> 8) & 0xFF));
}
inline void putU32(std::string& out, std::uint32_t v) {
    out.push_back(static_cast<char>(v & 0xFF));
    out.push_back(static_cast<char>((v >> 8) & 0xFF));
    out.push_back(static_cast<char>((v >> 16) & 0xFF));
    out.push_back(static_cast<char>((v >> 24) & 0xFF));
}

// RIFF/WAVE IEEE float 32-bit (fmt + data [+ trailingChunks]).
inline std::string writeFloat32(const std::vector<double>& samples, double fs, int channels,
                                const std::string& trailingChunks = std::string()) {
    if (channels < 1) channels = 1;
    const std::uint16_t fmtCode = 3;   // IEEE float
    const std::uint16_t bits = 32;
    const std::uint32_t sr = static_cast<std::uint32_t>(fs + 0.5);
    const std::uint16_t ch = static_cast<std::uint16_t>(channels);
    const std::uint32_t blockAlign = ch * (bits / 8);
    const std::uint32_t byteRate = sr * blockAlign;
    const std::uint32_t numFrames = static_cast<std::uint32_t>(samples.size());
    const std::uint32_t dataBytes = numFrames * blockAlign;
    const std::uint32_t riffBytes =
        4 + (8 + 16) + (8 + dataBytes) + static_cast<std::uint32_t>(trailingChunks.size());

    std::string out;
    out.reserve(44 + dataBytes + trailingChunks.size());
    out.append("RIFF", 4); putU32(out, riffBytes); out.append("WAVE", 4);
    out.append("fmt ", 4); putU32(out, 16);
    putU16(out, fmtCode); putU16(out, ch); putU32(out, sr); putU32(out, byteRate);
    putU16(out, static_cast<std::uint16_t>(blockAlign)); putU16(out, bits);
    out.append("data", 4); putU32(out, dataBytes);
    for (std::uint32_t i = 0; i < numFrames; ++i) {
        const float v = static_cast<float>(samples[i]);
        char b[4]; std::memcpy(b, &v, 4);
        for (int c = 0; c < channels; ++c) out.append(b, 4);
    }
    out.append(trailingChunks);
    return out;
}

} // namespace riff
} // namespace aamp

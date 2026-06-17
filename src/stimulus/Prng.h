// AAMP - src/stimulus/Prng.h
// Geradores determinísticos (FR-017, DOC-03 ST-04/ST-05).
// Mapeamento raw->double proprio para garantir byte-identidade entre plataformas
// (std::uniform_real_distribution NAO e especificada de forma portavel).
#pragma once
#include <cstdint>
#include <random>
#include <vector>
#include "../common/Constants.h"

namespace aamp {

// White Noise: Mersenne Twister (std::mt19937) seed fixo = 1, mapeado para [-1, 1).
class WhitePrng {
public:
    explicit WhitePrng(unsigned long seed = k::PRNG_SEED) : m_mt(static_cast<std::uint32_t>(seed)) {}
    double next() {
        // raw 32-bit -> [-1, 1)
        const std::uint32_t r = m_mt();
        return (static_cast<double>(r) / 4294967296.0) * 2.0 - 1.0;
    }
private:
    std::mt19937 m_mt;
};

// Pink Noise: Voss-McCartney com 16 estagios, seed fixo = 1 (rolloff -3 dB/oitava).
class PinkPrng {
public:
    explicit PinkPrng(unsigned long seed = k::PRNG_SEED)
        : m_white(seed), m_rows(k::VOSS_STAGES, 0.0), m_counter(0) {
        for (auto& r : m_rows) r = m_white.next();
        m_running = 0.0;
        for (double r : m_rows) m_running += r;
    }
    double next() {
        // A cada amostra, atualiza um subconjunto de linhas conforme bits do contador.
        m_counter++;
        std::uint32_t c = m_counter;
        for (int i = 0; i < k::VOSS_STAGES; ++i) {
            if (c & 1u) {
                m_running -= m_rows[static_cast<std::size_t>(i)];
                m_rows[static_cast<std::size_t>(i)] = m_white.next();
                m_running += m_rows[static_cast<std::size_t>(i)];
                break;
            }
            c >>= 1;
        }
        return m_running / static_cast<double>(k::VOSS_STAGES);
    }
private:
    WhitePrng m_white;
    std::vector<double> m_rows;
    std::uint32_t m_counter;
    double m_running;
};

} // namespace aamp

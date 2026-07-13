// AAMP - src/evidence/EvidenceSerializer.h
// Serializer do Evidence Package (Passo 2 da migração). Concretiza:
//   B-1  materialização: EvidencePackage (modelo) -> representação serializada do conteúdo
//        (manifest/session/hardware/metadata.json + WAVs de origem/captura);
//   B-2  empacotamento : agrega os artefatos num único .aamp (ZIP STORE);
//   B-3  persistência  : grava o .aamp em disco.
//
// Princípios (autoridade oficial):
//   P-1  determinismo : o mesmo EvidencePackage produz SEMPRE os mesmos bytes — sem timestamps
//        injetados, sem ordem não determinística, sem estado global, sem informação externa.
//   P-2  não interpreta: apenas lê o EvidencePackage e materializa seu conteúdo; nunca calcula
//        métricas, valida ciência, reorganiza dados, normaliza conteúdo ou infere informação.
//   P-3  independência : o modelo (EvidencePackage) não conhece .aamp/ZIP/JSON/WAV; TODO o
//        conhecimento do formato físico vive exclusivamente aqui.
// O EvidencePackage é imutável e somente-leitura: recebido por const&, jamais modificado.
#pragma once
#include "EvidencePackage.h"
#include <string>

namespace aamp {
namespace evidence {

// B-1 + B-2: bytes do .aamp (ZIP STORE) a partir do EvidencePackage imutável.
std::string serialize(const EvidencePackage& pkg);

// B-3: persiste os bytes serializados em disco. Retorna true em sucesso.
bool writeToFile(const EvidencePackage& pkg, const std::string& path);

} // namespace evidence
} // namespace aamp

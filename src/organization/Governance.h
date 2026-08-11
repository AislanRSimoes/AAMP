// AAMP - src/organization/Governance.h
// Governance — COORDENADOR ARQUITETURAL da camada de organização (Etapa 3 da implementação).
//
// Materializa EXCLUSIVAMENTE o coordenador demonstrado pelas auditorias, sob o marco
// [D-GOVERNANÇA-NÃO-É-DOMÍNIO]: a Governança administra Projetos (e, por herança, as Missões
// dentro deles), mas NÃO pertence ao domínio científico nem ao organizacional. Ela NUNCA
// armazena patrimônio próprio — nenhum conhecimento, artefato, histórico, contexto ou memória.
// Sua única responsabilidade é COORDENAR entidades que já existem.
//
// Sem inteligência nesta etapa: sem IA, sem heurísticas, sem algoritmos, sem decisões, sem
// automação, sem planejamento e sem scheduler. Os únicos métodos são CONSULTAS ADMINISTRATIVAS
// PURAS (lookup por identidade). Uma busca linear por id não é decisão nem heurística — é
// consulta. As responsabilidades de decidir/priorizar/planejar/executar/promover pertencem a
// etapas futuras e estão deliberadamente ausentes.
//
// Posse e ownership:
//   - A Governança mantém apenas REFERÊNCIAS aos Projetos (ponteiro não-proprietário), nunca
//     cópias, sem ownership complexo. Os objetos Project são de propriedade de outro escopo.
//   - O Projeto ATIVO é apenas uma IDENTIDADE (string): sem ponteiro, sem referência, sem cache.
//     activeProject() resolve a identidade por consulta, em vez de manter um ponteiro.
//
// Como agregado, a coleção é pública e populada por acesso direto (mesmo padrão das coleções de
// Project/Mission); não há método mutador/decisório nesta etapa. Os helpers abaixo apenas
// consultam e nunca modificam ciência, nunca executam Missões e nunca chamam IA.
#pragma once
#include <string>
#include <vector>
#include "Project.h"

namespace aamp {
namespace organization {

// Coordenador puro: coleção de referências a Projetos + a identidade do Projeto ativo.
struct Governance {
    // Identidade do Projeto ativo — SOMENTE o id (sem ponteiro, sem referência, sem cache).
    std::string activeProjectId;

    // Referências (não-proprietárias) aos Projetos administrados — nunca cópias, sem ownership.
    std::vector<Project*> projects;

    // ---- Consultas administrativas puras (somente leitura; não decidem, não modificam) ----

    // Referência ao Projeto de dada identidade, ou nullptr se ausente.
    Project* findProject(const std::string& id) const {
        for (Project* p : projects) {
            if (p && p->id == id) return p;
        }
        return nullptr;
    }

    // Existe um Projeto com esta identidade?
    bool hasProject(const std::string& id) const {
        return findProject(id) != nullptr;
    }

    // Referência ao Projeto ativo (resolvida a partir de activeProjectId), ou nullptr se não
    // houver ativo definido ou se a identidade ativa não estiver na coleção.
    Project* activeProject() const {
        if (activeProjectId.empty()) return nullptr;
        return findProject(activeProjectId);
    }
};

} // namespace organization
} // namespace aamp

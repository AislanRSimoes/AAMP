// AAMP - src/orchestration/StateMachine.h
// Maquina de estados do fluxo de medicao (DOC-04 §5):
// IDLE -> READY -> MEASURING -> PROCESSING -> RESULTS -> EXPORTING (+ ERROR).
#pragma once
#include <string>

namespace aamp {

enum class State { Idle, Ready, Measuring, Processing, Results, Exporting, Error };

const char* stateName(State s);

class StateMachine {
public:
    State state() const { return m_state; }
    bool canTransition(State to) const;
    bool transition(State to);          // executa se permitido; devolve sucesso
    void forceError() { m_state = State::Error; }   // abortos seguros (ER-002/021)
private:
    State m_state = State::Idle;
};

} // namespace aamp

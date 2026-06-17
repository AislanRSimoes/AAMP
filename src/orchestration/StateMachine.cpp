// AAMP - src/orchestration/StateMachine.cpp
#include "StateMachine.h"

namespace aamp {

const char* stateName(State s) {
    switch (s) {
        case State::Idle: return "IDLE";
        case State::Ready: return "READY";
        case State::Measuring: return "MEASURING";
        case State::Processing: return "PROCESSING";
        case State::Results: return "RESULTS";
        case State::Exporting: return "EXPORTING";
        case State::Error: return "ERROR";
    }
    return "UNKNOWN";
}

bool StateMachine::canTransition(State to) const {
    switch (m_state) {
        case State::Idle:       return to == State::Ready || to == State::Error;
        case State::Ready:      return to == State::Measuring || to == State::Idle || to == State::Error;
        case State::Measuring:  return to == State::Processing || to == State::Idle || to == State::Error;
        case State::Processing: return to == State::Results || to == State::Error;
        case State::Results:    return to == State::Measuring || to == State::Exporting || to == State::Idle || to == State::Error;
        case State::Exporting:  return to == State::Results || to == State::Error;
        case State::Error:      return to == State::Idle;
    }
    return false;
}

bool StateMachine::transition(State to) {
    if (!canTransition(to)) return false;
    m_state = to;
    return true;
}

} // namespace aamp

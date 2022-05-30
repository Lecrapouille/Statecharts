// ############################################################################
// MIT License
//
// Copyright (c) 2022 Quentin Quadrat
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
// ############################################################################

#ifndef STATE_MACHINE_HPP
#  define STATE_MACHINE_HPP

#  include <map>
#  include <cassert>
#  include <stdlib.h>

//-----------------------------------------------------------------------------
//! \brief Verbosity activated in debug mode.
//-----------------------------------------------------------------------------
#  include <cstdio>
#  if defined(FSM_DEBUG)
#    define LOGD printf
#  else
#    define LOGD(...)
#  endif
#  define LOGE printf

//-----------------------------------------------------------------------------
//! \brief Return the given state as raw string (they shall not be free).
//! \note implement this function inside the C++ file of the derived class.
//-----------------------------------------------------------------------------
template<class STATES_ID>
const char* stringify(STATES_ID const state);

// *****************************************************************************
//! \brief Base class for depicting and running small Finite State Machine (FSM)
//! by implementing a subset of UML statechart. See this document for more
//! information about them: http://niedercorn.free.fr/iris/iris1/uml/uml09.pdf
//!
//! This class is not made for defining hierarchical state machine (HSM). It
//! also does not implement composites, history, concurrent parts of the FSM.
//! This class is fine for small Finite State Machine (FSM) and is limited due
//! to memory footprint (therefore no complex C++ designs, no dynamic containers
//! and few virtual methods). The code is based on the following link
//! https://www.codeproject.com/Articles/1087619/State-Machine-Design-in-Cplusplus-2
//! For bigger state machines, please use something more robust such as Esterel
//! SyncCharts or directly the Esterel language
//! https://www.college-de-france.fr/media/gerard-berry/UPL8106359781114103786_Esterelv5_primer.pdf
//!
//! This class holds the list of states \c State and the currently active state.
//! Each state holds actions to perform as function pointers 'on entering', 'on
//! leaving', 'on event' and 'do activity'.
//!
//! A state machine is depicted by a graph structure (nodes: states; arcs:
//! transitions) which can be represented by a matrix (states / events) usually
//! sparse. For example the following state machine, in plantuml syntax:
//!
//! @startuml
//! [*] --> Idle
//! Idle --> Starting : set speed
//! Starting --> Stopping : halt
//! Starting -> Spinning : set speed
//! Spinning -> Stopping: halt
//! Spinning --> Spinning : set speed
//! Stopping -> Idle
//! @enduml
//!
//! Can be depicted by the following matrix:
//! +-----------------+------------+-----------+-----------+
//! | States \ Event  | Set Speed  | Halt      |           |
//! +=================+============+===========+===========+
//! | IDLE            | STARTING   |           |           |
//! +-----------------+------------+-----------+-----------+
//! | STOPPING        |            |           | IDLE      |
//! +-----------------+------------+-----------+-----------+
//! | STARTING        | SPINNING   | STOPPING  |           |
//! +-----------------+------------+-----------+-----------+
//! | SPINNING        | SPINNING   | STOPPING  |           |
//! +-----------------+------------+-----------+-----------+
//!
//! The first column contains all states. The first line contains all events.
//! Each column depict a transition: given the current state (i.e. IDLE) and a
//! given event (i.e. Set Speed) the next state of the state machine will be
//! STARTING. Empty cells are forbidden transitions.
//!
//! This class does not hold directly tables for transitioning origin state to
//! destination state when an external event occured (like done in boost
//! lib). Instead, each external event shall be implemented as member function
//! in the derived FSM class and in each member function shall implement the
//! transition table.
//!
//! \tparam FSM the concrete Finite State Machine deriving from this base class.
//! In this class you shall implement external events as public methods,
//! reactions and guards as private methods, and set the first column of the
//! matrix and their guards/reactions in the constructor method. On each event
//! methods, you shall define the table of transition (implicit transition are
//! considered as ignoring the event).
//!
//! \tparam STATES_ID enumerate for giving an unique identifier for each state.
//! In our example:
//!   enum StatesID { IDLE = 0, STOPPING, STARTING, SPINNING,
//!                   IGNORING_EVENT, CANNOT_HAPPEN, MAX_STATES };
//!
//! The 3 last states are mandatory: in the matrix of the control motor of our
//! previous example, holes are implicitely IGNORING_EVENT, but the user can
//! explicitely set to CANNOT_HAPPEN to trap the whole system. Other state enum
//! shall be used to defined the table of states \c m_states which shall be
//! filled with these enums and pointer functions such as 'on entering' ...
//!
//! Transition, like states, can do reaction and have guards as pointer
//! functions.
// *****************************************************************************
template<typename FSM, class STATES_ID>
class StateMachine
{
public:

    //! \brief Pointer method with no argument and returning a boolean.
    using bFuncPtr = bool (FSM::*)();
    //! \brief Pointer method with no argument and returning void.
    using xFuncPtr = void (FSM::*)();

    //--------------------------------------------------------------------------
    //! \brief Class depicting a state of the state machine and hold pointer
    //! methods for each desired action to perform. In UML states are like
    //! Moore state machine: states can do action.
    //--------------------------------------------------------------------------
    struct State
    {
        //! \brief Call the "on entry" callback when entering for the first time
        //! (AND ONLY THE FIRST TIME) in the state. Note: the transition guard
        //! can prevent calling this function.
        xFuncPtr entering = nullptr;
        //! \brief Call the "on leaving" callback when leavinging for the first
        //! time (AND ONLY THE FIRST TIME) the state. Note: the guard can
        //! prevent calling this function.
        xFuncPtr leaving = nullptr;
        //! \brief Call the "on event" callback when the event occured. Note:
        //! the guard can prevent calling this function. Entry and leaving
        //! actions are not made if this function is called.

        //! \note "on event [ guard ] / action" could be seen as transition
        //! cycling on this state but this is partially false since "on entry"
        //! and "on leaving" would not have been called (because not entering
        //! leaving to a different state).
        xFuncPtr onevent = nullptr;
    };

    //--------------------------------------------------------------------------
    //! \brief Class depicting a transition from a source state to a destination
    //! state. A transition occurs when an event has occured. In UML,
    //! transitions are like Mealey state machine: transition can do action.
    //--------------------------------------------------------------------------
    struct Transition
    {
        //! \brief State of destination
        STATES_ID destination = STATES_ID::IGNORING_EVENT;
        //! \brief The condition validating the event and therefore preventing
        //! the transition to occur.
        bFuncPtr guard = nullptr;
        //! \brief The action to perform when transitioning to the destination
        //! state.
        xFuncPtr action = nullptr;
    };

    //! \brief Define the type of container holding all stated of the state
    //! machine.
    using States = std::array<State, STATES_ID::MAX_STATES>;
    //! \brief Define the type of container holding states transitions. Since
    //! a state machine is generally a sparse matrix we use red-back tree.
    using Transitions = std::map<STATES_ID, Transition>;

    //--------------------------------------------------------------------------
    //! \brief Default constructor. Pass the number of states the FSM will use,
    //! set the initial state and if mutex shall have to be used.
    //! \param[in] initial the initial state to start with.
    //--------------------------------------------------------------------------
    StateMachine(STATES_ID const initial) // FIXME should be ok for constexpr
        : m_current_state(initial), m_initial_state(initial)
    {
        // FIXME static_assert not working
        assert(initial < STATES_ID::MAX_STATES);
    }

    //--------------------------------------------------------------------------
    //! \brief Restore the state machin to its initial state.
    //--------------------------------------------------------------------------
    inline void reset()
    {
        m_current_state = m_initial_state;
        m_nesting_state = STATES_ID::CANNOT_HAPPEN;
        m_nesting = false;
    }

    //--------------------------------------------------------------------------
    //! \brief Return the current state.
    //--------------------------------------------------------------------------
    inline STATES_ID state() const
    {
        return m_current_state;
    }

    //--------------------------------------------------------------------------
    //! \brief Return the current state as string (shall not be free'ed).
    //--------------------------------------------------------------------------
    inline const char* c_str() const
    {
        return stringify(m_current_state);
    }

    //--------------------------------------------------------------------------
    //! \brief Internal transition: jump to the desired state from internal
    //! event. This will call the guard, leaving actions, entering actions ...
    //! \param[in] new_state the destination state.
    //! \param[in] tr optional transition.
    //--------------------------------------------------------------------------
    void transition(STATES_ID const new_state, const Transition* tr = nullptr);

protected:

    //--------------------------------------------------------------------------
    //! \brief From current state, jump to the destination state from external
    //! event and given transition.
    //! \param[in] transitions the table of transitions.
    //--------------------------------------------------------------------------
    void transition(Transitions const& transitions)
    {
        auto const& it = transitions.find(m_current_state);
        if (it != transitions.end())
        {
            transition(it->second.destination, &(it->second));
        }
        else
        {
            transition(STATES_ID::IGNORING_EVENT, nullptr);
        }
    }

protected:

    //! \brief Container of states.
    States m_states;

    //! \brief Current active state.
    STATES_ID m_current_state;

private:

    //! \brief Save the initial state need for restoring initial state.
    STATES_ID m_initial_state;
    //! \brief Temporary variable saving the next state.
    STATES_ID m_next_state;
    //! \brief Temporary variable saving the nesting state (needed for internal
    //! event).
    STATES_ID m_nesting_state = STATES_ID::CANNOT_HAPPEN;
    //! \brief is the state nested by an internal event.
    bool m_nesting = false;
};

//------------------------------------------------------------------------------
template<class FSM, class STATES_ID>
void StateMachine<FSM, STATES_ID>::transition(STATES_ID const new_state,
                                              const Transition* tr)
{
    LOGD("[STATE MACHINE] Reacting to event from state %s\n",
         stringify(m_current_state));

#if defined(THREAD_SAFETY)
    // If try_lock failed it is not important: it just means that we have called
    // an internal event from this method and internal states are still
    // protected.
    m_mutex.try_lock();
#endif

    m_nesting_state = new_state;

    // Reaction from internal event (therefore coming from this method called by
    // one of the action functions: memorize and leave the function: it will
    // continue thank to the while loop. This avoids recursion.
    if (m_nesting)
    {
        LOGD("[STATE MACHINE] Internal event. Memorize state %s\n",
             stringify(new_state));
        return ;
    }

    do
    {
        m_next_state = m_nesting_state;
        m_nesting_state = STATES_ID::CANNOT_HAPPEN;

        // Forbidden event: kill the system
        if (m_next_state == STATES_ID::CANNOT_HAPPEN)
        {
            LOGE("[STATE MACHINE] Forbidden event. Aborting!\n");
            exit(EXIT_FAILURE);
        }

        // Do not react to this event
        else if (m_next_state == STATES_ID::IGNORING_EVENT)
        {
            LOGD("[STATE MACHINE] Ignoring external event\n");
            return ;
        }

        // Unknown state: kill the system
        else if (m_next_state >= STATES_ID::MAX_STATES)
        {
            LOGE("[STATE MACHINE] Unknown state. Aborting!\n");
            exit(EXIT_FAILURE);
        }

        // Transition to new new state. Local variable mandatory since state
        // reactions can modify current state (side effect).
        STATES_ID current_state = m_current_state;
        m_current_state = m_next_state;

        // Reaction: call the member function associated to the current state
        StateMachine<FSM, STATES_ID>::State const& nst = m_states[m_next_state];
        StateMachine<FSM, STATES_ID>::State const& cst = m_states[current_state];

        // Call the guard
        m_nesting = true;
        bool guard_res = (tr->guard == nullptr);
        if (!guard_res)
        {
            guard_res = (reinterpret_cast<FSM*>(this)->*tr->guard)();
        }

        if (!guard_res)
        {
            LOGD("[STATE MACHINE] Transition refused by the %s guard. Stay"
                 " in state %s\n", stringify(new_state), stringify(current_state));
            m_current_state = current_state;
            m_nesting = false;
            return ;
        }
        else
        {
            // The guard allowed the transition to the next state
            LOGD("[STATE MACHINE] Transitioning to new state %s\n",
                 stringify(new_state));

            // Transition
            if ((tr != nullptr) && (tr->action != nullptr))
            {
                LOGD("[STATE MACHINE] Do the action of transition %s -> %s\n",
                     stringify(current_state), stringify(tr->destination));
                (reinterpret_cast<FSM*>(this)->*tr->action)();
            }

            // Entry and leaving actions are not made if the event specified by
            // the "on event" clause happened.
            if (cst.onevent != nullptr)
            {
                LOGD("[STATE MACHINE] Do the state %s 'on event' action\n",
                     stringify(current_state));
                (reinterpret_cast<FSM*>(this)->*cst.onevent)();
                return ;
            }

            // Transitioning to a new state ?
            else if (current_state != m_next_state)
            {
                if (cst.leaving != nullptr)
                {
                    LOGD("[STATE MACHINE] Do the state %s 'on leaving' action\n",
                         stringify(current_state));

                    // Do reactions when leaving the current state
                    (reinterpret_cast<FSM*>(this)->*cst.leaving)();
                }

                if (nst.entering != nullptr)
                {
                    LOGD("[STATE MACHINE] Do the state %s 'on entry' action\n",
                         stringify(new_state));

                    // Do reactions when entring into the new state
                    (reinterpret_cast<FSM*>(this)->*nst.entering)();
                }
            }
            else
            {
                LOGD("[STATE MACHINE] Was previously in this mode: no "
                     "actions to perform\n");
            }
        }
        m_nesting = false;
    }
    while (m_nesting_state != STATES_ID::CANNOT_HAPPEN);

#if defined(THREAD_SAFETY)
    m_mutex.unlock();
#endif
}

#endif // STATE_MACHINE_HPP
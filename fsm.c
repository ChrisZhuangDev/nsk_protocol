
/**
 * @file fsm.c
 * @brief Table-driven finite state machine (FSM) implementation
 *
 * This module implements a minimal table-driven FSM engine. The engine
 * provides two convenience functions: `fsm_process_event`, which processes a
 * single event against a transition table, and
 * `fsm_process_event_sequence` for processing an array of events.
 *
 * The implementation purposely keeps the runtime footprint small and has no
 * dependency on application-specific types; actions are simple parameterless
 * callbacks. Application state, if required by actions, should be stored in
 * application-owned storage that actions access directly.
 *
 * @author TOPBAND Team
 * @date 2025-11-29
 * @version 1.0
 */

#include "fsm.h"


/**
 * @brief Initialize an FSM instance (implementation)
 *
 * This function stores pointers to the transition table and sets the
 * initial state. The transition table memory is not copied and therefore
 * must remain valid for the lifetime of the `fsm` instance.
 *
 * @param[in,out] fsm Pointer to the fsm instance to initialize. If NULL
 *                    the function performs no action.
 * @param[in] table Pointer to the transition table (may be NULL if
 *                  table_sz is zero).
 * @param[in] table_sz Number of elements in @p table.
 * @param[in] initial_state Initial state to set in the FSM.
 */
void fsm_init(fsm_t *fsm, const struct fsm_transition *table, size_t table_sz, state_t initial_state, void* ctx)
{
    if (fsm == NULL) {
        return;
    }

    fsm->table = table;
    fsm->table_sz = table_sz;
    fsm->state = initial_state;
    fsm->ctx = ctx;
}


/**
 * @brief Process a single event through the FSM (implementation)
 *
 * The implementation searches the transition table sequentially and executes
 * the first matching transition. If the transition contains a non-NULL
 * `action` callback it is invoked before the state update.
 *
 * Behavior notes:
 * - If @p fsm is NULL or its table is NULL the function returns 0 and has
 *   no side effects.
 * - The search is linear; if performance for large tables is required the
 *   caller should consider organizing the table (for example by current
 *   state) or using a different engine.
 *
 * @param[in,out] fsm Pointer to an initialized fsm instance.
 * @param[in] event The event to process.
 * @return uint8_t 1 if a matching transition was found and executed, 0
 *                  otherwise.
 */
uint8_t fsm_process_event(fsm_t *fsm, event_t event)
{
    uint8_t result = 0u;

    if ((fsm == NULL) || (fsm->table == NULL)) {
        return 0u;
    }

    for (size_t i = 0u; i < fsm->table_sz; ++i) {
        const struct fsm_transition *t = &fsm->table[i];
        if ((t->state == fsm->state) && (t->event == event)) {
            if (t->action != NULL) {
                t->action(fsm->ctx);
            }

            fsm->state = t->next_state;
            result = 1u;
            break;
        }
    }

    return result;
}

event_t fsm_get_current_state(fsm_t *fsm)
{
    if (fsm == NULL) 
    {
        return 0u;
    }
    return fsm->state;
}

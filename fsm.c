
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
    fsm->event_queue = NULL;
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

/**
 * @brief Get the current state of the FSM
 *
 * This function retrieves the current state value from an initialized FSM
 * instance. It is useful for debugging, logging, or making decisions based
 * on the current FSM state without modifying it.
 *
 * @param[in] fsm Pointer to the FSM instance. If NULL, returns 0.
 * @return state_t The current state of the FSM, or 0 if fsm is NULL.
 *
 * @note The function performs no state transition and has no side effects
 *       other than reading the current state.
 */
event_t fsm_get_current_state(fsm_t *fsm)
{
    if (fsm == NULL) 
    {
        return 0u;
    }
    return fsm->state;
}

/**
 * @brief Create an event queue for asynchronous FSM event handling
 *
 * This function creates a CMSIS-RTOS2 message queue for storing FSM events.
 * When an event queue is attached to an FSM, events can be sent asynchronously
 * using fsm_send_event() and processed later in batch using fsm_poll().
 * This is useful for decoupling event sources from FSM processing in
 * multi-threaded environments.
 *
 * @param[in,out] fsm Pointer to the FSM instance. If NULL, no action is taken.
 * @param[in] msg_count Maximum number of events the queue can hold.
 *
 * @note The queue is created using osMessageQueueNew() and must be deleted
 *       manually if needed (not handled by FSM engine).
 * @note If queue creation fails, fsm->event_queue will be NULL.
 *
 * @see fsm_send_event(), fsm_poll()
 */
void fsm_create_event_queue(fsm_t *fsm, uint32_t msg_count)
{
    if (fsm == NULL) 
    {
        return;
    }
    fsm->event_queue = osMessageQueueNew(msg_count, sizeof(event_t), NULL);
}

/**
 * @brief Send an event to the FSM's event queue
 *
 * This function enqueues an event into the FSM's event queue for later
 * processing. The event is not processed immediately; instead, it is stored
 * in the queue and will be processed when fsm_poll() is called. This
 * enables asynchronous event handling and decouples event generation from
 * FSM state processing.
 *
 * @param[in,out] fsm Pointer to the FSM instance. If NULL or if the event
 *                    queue is NULL, no action is taken.
 * @param[in] event The event to enqueue.
 *
 * @note This function uses a timeout of 0 (no wait), so if the queue is full
 *       the event will be dropped silently.
 * @note The event queue must be created first using fsm_create_event_queue().
 *
 * @see fsm_create_event_queue(), fsm_poll()
 */
void fsm_send_event(fsm_t *fsm, event_t event)
{
    if (fsm == NULL || fsm->event_queue == NULL) 
    {
        return;
    }
    osMessageQueuePut(fsm->event_queue, &event, 0U, 0U);
}

/**
 * @brief Process all pending events in the FSM's event queue
 *
 * This function retrieves and processes all events currently waiting in the
 * FSM's event queue. Events are processed in FIFO order (first in, first out).
 * Each event is passed to fsm_process_event() which may trigger state
 * transitions and execute associated actions.
 *
 * This function is typically called periodically in a main loop or task to
 * handle events that were asynchronously enqueued via fsm_send_event().
 *
 * @param[in,out] fsm Pointer to the FSM instance. If NULL or if the event
 *                    queue is NULL, no action is taken.
 *
 * @note The function processes all events present in the queue at the time
 *       of the call. Events added during processing will be handled in the
 *       next call.
 * @note This is a non-blocking function; it returns immediately after
 *       processing all queued events.
 *
 * @see fsm_send_event(), fsm_create_event_queue(), fsm_process_event()
 */
void fsm_poll(fsm_t *fsm)
{
    event_t event = 0u;

    if (fsm == NULL || fsm->event_queue == NULL) 
    {
        return;
    }
    
    while(osMessageQueueGet(fsm->event_queue, &event, NULL, 0U) == osOK)
    {
        fsm_process_event(fsm, event);
    }
}
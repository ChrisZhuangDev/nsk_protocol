#/*******************************************************************************
 * @file fsm.h
 * @brief Simple table-driven finite state machine (FSM) public interface
 *
 * This header defines a minimal, portable table-driven finite state machine
 * API. The FSM engine is generic and accepts a transition table describing
 * (state,event) -> next_state and an optional action to execute on the
 * transition. States and events are application-defined values that fit into
 * a uint8_t.
 *
 * The design keeps the engine lightweight and free of application-specific
 * dependencies; actions are simple function pointers that take no parameters
 * (application state may be stored in module-level context if required).
 *
 * @author TOPBAND Team
 * @date 2025-11-29
 * @version 1.0
 ******************************************************************************/
#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdint.h>
#include <stddef.h>
#include "cmsis_os2.h"
/**
 * @typedef state_t
 * @brief Opaque integer type used to represent FSM states
 *
 * States are defined by the application and must fit into a uint8_t. Using a
 * fixed-width type keeps the API portable across translation units and
 * compilers.
 */
typedef uint8_t state_t;

/**
 * @typedef event_t
 * @brief Opaque integer type used to represent FSM events
 *
 * Events are defined by the application and must fit into a uint8_t. Keep
 * event value definitions close to the application logic (macros or enums
 * within the application code) to maintain readability.
 */
typedef uint8_t event_t;

/**
 * @typedef action_fn_t
 * @brief Action callback executed when a transition is taken
 *
 * Actions are application-defined functions that are executed by the FSM when
 * a transition is taken. For simplicity the action takes no parameters and
 * returns void. If the action needs access to application state it may read
 * that state from application-controlled storage (for example a module-level
 * variable). Keeping the callback signature parameterless simplifies the
 * engine interface and localises application-specific data to application
 * code.
 */
typedef void (*action_fn_t)(void* handle);

/**
 * @struct fsm_transition
 * @brief Entry describing a single FSM transition
 *
 * The transition table is an array of these entries. When the FSM is in
 * `state` and receives `event`, the engine will: (1) invoke `action` if it is
 * non-NULL, then (2) update the current state to `next_state`.
 */
struct fsm_transition {
    state_t state;      /**< current state */
    event_t event;      /**< triggering event */
    state_t next_state; /**< next state after transition */
    action_fn_t action; /**< optional action to execute (may be NULL) */
};

/**
 * @struct fsm
 * @brief Runtime object that encapsulates a transition table and current state
 *
 * Applications allocate and initialize an `fsm_t` instance with
 * `fsm_init()` then call `fsm_process_event()` to drive the machine. The
 * structure intentionally contains only the minimal runtime fields required
 * by the engine. Extensions (for example user context pointers or locking)
 * can be added by the application if needed.
 */
typedef struct fsm {
    const struct fsm_transition *table; /**< read-only transition table */
    size_t table_sz;                    /**< number of entries in table */
    state_t state;                      /**< current state */
    void* ctx;                          /**< optional user context pointer */
    osMessageQueueId_t event_queue;       /**< optional event queue */
    /* optional: const char *name; void *user_ctx; mutex_t *lock; */
} fsm_t;


/**
 * @brief Initialize an FSM instance
 *
 * Initialize the FSM structure with the provided transition table and
 * starting state. This function does not copy the table; the table must
 * remain valid for the lifetime of the `fsm` instance.
 *
 * @param[in,out] fsm Pointer to an `fsm_t` instance to initialize. If NULL
 *                    the function is a no-op.
 * @param[in] table Pointer to a read-only array of `fsm_transition` entries.
 *                  May be NULL if `table_sz` is zero.
 * @param[in] table_sz Number of entries in `table`.
 * @param[in] initial_state Initial value to store in `fsm->state`.
 */
void fsm_init(fsm_t *fsm, const struct fsm_transition *table, size_t table_sz, state_t initial_state, void* ctx);


/**
 * @brief Process a single event through the FSM
 *
 * The engine scans the transition table sequentially for the first entry
 * whose `state` equals the FSM's current state and whose `event` equals the
 * provided @p event. If a matching transition is found the optional action
 * callback is invoked (if non-NULL) and the FSM's current state is updated
 * to the transition's `next_state`.
 *
 * This function is safe to call with a NULL `fsm` pointer; in that case the
 * call is a no-op and the function returns 0.
 *
 * @param[in,out] fsm Pointer to the initialized `fsm_t` instance.
 * @param[in] event The event to process.
 * @return uint8_t Returns 1 if a matching transition was found and taken,
 *                  0 otherwise.
 */
uint8_t fsm_process_event(fsm_t *fsm, event_t event);

/**
 * @brief Get the current state of the FSM
 *
 * Retrieves the current state value from the FSM without modifying it.
 * Useful for debugging, logging, or conditional logic based on FSM state.
 *
 * @param[in] fsm Pointer to the FSM instance. If NULL, returns 0.
 * @return state_t The current state of the FSM, or 0 if fsm is NULL.
 */
event_t fsm_get_current_state(fsm_t *fsm);

/**
 * @brief Create an event queue for asynchronous FSM event handling
 *
 * Creates a CMSIS-RTOS2 message queue for storing FSM events. This enables
 * asynchronous event posting and batch processing, useful for multi-threaded
 * environments where events are generated in one context and processed in
 * another.
 *
 * @param[in,out] fsm Pointer to the FSM instance. If NULL, no action is taken.
 * @param[in] msg_count Maximum number of events the queue can hold.
 *
 * @note The created queue is not automatically cleaned up by the FSM engine.
 * @note If creation fails, fsm->event_queue will be NULL.
 *
 * @see fsm_send_event(), fsm_poll()
 */
void fsm_create_event_queue(fsm_t *fsm, uint32_t msg_count);

/**
 * @brief Send an event to the FSM's event queue
 *
 * Enqueues an event for later processing. The event is not processed
 * immediately but stored in the queue until fsm_proccess() is called.
 * This decouples event generation from FSM processing.
 *
 * @param[in,out] fsm Pointer to the FSM instance. Must have an event queue
 *                    created via fsm_create_event_queue().
 * @param[in] event The event to enqueue.
 *
 * @note Uses timeout of 0 (no wait). If queue is full, event is dropped.
 * @note Does nothing if fsm or fsm->event_queue is NULL.
 *
 * @see fsm_create_event_queue(), fsm_poll()
 */
void fsm_send_event(fsm_t *fsm, event_t event);

/**
 * @brief Poll and process all pending events in the FSM's event queue
 *
 * Retrieves and processes all events currently in the FSM's event queue
 * in FIFO order. Each event is passed to fsm_process_event() which may
 * trigger state transitions and execute actions.
 *
 * Typically called periodically in a main loop or task to handle events
 * that were asynchronously enqueued via fsm_send_event().
 *
 * @param[in,out] fsm Pointer to the FSM instance. Must have an event queue
 *                    created via fsm_create_event_queue().
 *
 * @note Non-blocking: processes all events present at call time and returns.
 * @note Events added during processing will be handled in the next call.
 * @note Does nothing if fsm or fsm->event_queue is NULL.
 *
 * @see fsm_send_event(), fsm_create_event_queue()
 */
void fsm_poll(fsm_t *fsm);

#endif /* STATE_MACHINE_H */

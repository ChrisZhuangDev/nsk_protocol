
/* 缓冲池 + 三个队列 */
typedef struct {
    comm_data_t buffers[COMM_RECV_DATA_QUEUE_SIZE];  /* 缓冲池 */
    
    /* 三个队列只存索引,不存数据 */
    uint8_t idle_queue[COMM_RECV_DATA_QUEUE_SIZE];
    uint8_t idle_head, idle_tail, idle_count;
    
    uint8_t recv_queue[COMM_RECV_DATA_QUEUE_SIZE];
    uint8_t recv_head, recv_tail, recv_count;
    
    uint8_t ready_queue[COMM_RECV_DATA_QUEUE_SIZE];
    uint8_t ready_head, ready_tail, ready_count;
    
    osMutexId_t pool_mutex;
} recv_buffer_pool_t;

/* 初始化:所有buffer放入idle队列 */
void recv_pool_init(recv_buffer_pool_t *pool) {
    pool->idle_count = COMM_RECV_DATA_QUEUE_SIZE;
    pool->recv_count = 0;
    pool->ready_count = 0;
    
    for (uint8_t i = 0; i < COMM_RECV_DATA_QUEUE_SIZE; i++) {
        pool->buffers[i].index = i;
        pool->buffers[i].state = BUF_STATE_IDLE;
        pool->idle_queue[i] = i;
    }
    
    pool->pool_mutex = osMutexNew(NULL);
}

/* 从idle队列分配 */
uint8_t recv_pool_alloc_idle(recv_buffer_pool_t *pool) {
    uint8_t idx = 0xFF;
    
    osMutexAcquire(pool->pool_mutex, osWaitForever);
    if (pool->idle_count > 0) {
        idx = pool->idle_queue[pool->idle_head];
        pool->idle_head = (pool->idle_head + 1) % COMM_RECV_DATA_QUEUE_SIZE;
        pool->idle_count--;
        pool->buffers[idx].state = BUF_STATE_RECV;
    }
    osMutexRelease(pool->pool_mutex);
    
    return idx;
}

/* 移动到ready队列 */
void recv_pool_move_to_ready(recv_buffer_pool_t *pool, uint8_t idx) {
    osMutexAcquire(pool->pool_mutex, osWaitForever);
    
    pool->buffers[idx].state = BUF_STATE_READY;
    pool->ready_queue[pool->ready_tail] = idx;
    pool->ready_tail = (pool->ready_tail + 1) % COMM_RECV_DATA_QUEUE_SIZE;
    pool->ready_count++;
    
    osMutexRelease(pool->pool_mutex);
}

/* 从ready队列取出 */
uint8_t recv_pool_dequeue_ready(recv_buffer_pool_t *pool) {
    uint8_t idx = 0xFF;
    
    osMutexAcquire(pool->pool_mutex, osWaitForever);
    if (pool->ready_count > 0) {
        idx = pool->ready_queue[pool->ready_head];
        pool->ready_head = (pool->ready_head + 1) % COMM_RECV_DATA_QUEUE_SIZE;
        pool->ready_count--;
    }
    osMutexRelease(pool->pool_mutex);
    
    return idx;
}

/* 归还到idle队列 */
void recv_pool_move_to_idle(recv_buffer_pool_t *pool, uint8_t idx) {
    osMutexAcquire(pool->pool_mutex, osWaitForever);
    
    pool->buffers[idx].state = BUF_STATE_IDLE;
    pool->idle_queue[pool->idle_tail] = idx;
    pool->idle_tail = (pool->idle_tail + 1) % COMM_RECV_DATA_QUEUE_SIZE;
    pool->idle_count++;
    
    osMutexRelease(pool->pool_mutex);
}
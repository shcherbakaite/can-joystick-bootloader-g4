

void cb_init(struct circular_buffer_t *cb) {
    cb->head = 0;
    cb->tail = 0;
}

int cb_push(struct circular_buffer_t *cb, const uint32_t x) {
    uint32_t next_head = (cb->head + 1) & (CIRCULAR_BUFFER_SIZE - 1);
    if (next_head == cb->tail) {
        return 0;
    }
    cb->data[cb->head] = x;
    cb->head = (cb->head + 1) & (CIRCULAR_BUFFER_SIZE - 1); // This only works if size is a power of 2
    return 1;
}

int cb_pop(struct circular_buffer_t *cb, uint32_t* x) {
    if (cb->tail != cb->head) {
        *x = cb->data[cb->tail];
        cb->tail = (cb->tail + 1) & (CIRCULAR_BUFFER_SIZE - 1);
        return 1;
    } else {
        return 0;
     }
}

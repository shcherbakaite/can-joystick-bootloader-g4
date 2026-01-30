
void cb_init(struct circular_buffer_t *cb) {

}

void cb_push(struct circular_buffer_t *cb, const uint32_t* x) {
    cb->data[cb->head] = x;
    cb->head = (cb->head + 1) & (CIRCULAR_BUFFER_SIZE - 1); // This only works if size is a power of 2
}

int cb_pop(struct circular_buffer_t *cb, uint32_t* x) {
	*x = cb->data[cb->tail];
    cb->tail = (cb->tail + 1) & (CIRCULAR_BUFFER_SIZE - 1);
}

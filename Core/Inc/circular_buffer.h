/*
 * circular_buffer.h
 *
 *  Created on: Jan 8, 2026
 *      Author: vlad.shcherbakov
 */

#ifndef INC_CIRCULAR_BUFFER_H_
#define INC_CIRCULAR_BUFFER_H_

#define CIRCULAR_BUFFER_SIZE 2048
#define CIRCULAR_BUFFER_SIZE_LOG_2 11

struct circular_buffer_t {
    uint16_t data[CIRCULAR_BUFFER_SIZE];
    uint32_t head;
    uint32_t len;
};

void cb_init(struct circular_buffer_t *cb);

void cb_push(struct circular_buffer_t *cb, uint_t* x);

int32_t cb_pop(struct circular_buffer_t *cb);

#endif /* INC_CIRCULAR_BUFFER_H_ */

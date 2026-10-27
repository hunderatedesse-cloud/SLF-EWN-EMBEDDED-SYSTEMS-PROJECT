#ifndef RECEIVER_STORAGE_H
#define RECEIVER_STORAGE_H

#include <stdint.h>

void receiver_storage_init(void);
uint32_t receiver_storage_count(void);
uint32_t receiver_storage_capacity(void);
void receiver_storage_clear(void);
void receiver_storage_append(const uint8_t *frame);
void receiver_storage_read(uint32_t record_id, uint8_t *record);

#endif
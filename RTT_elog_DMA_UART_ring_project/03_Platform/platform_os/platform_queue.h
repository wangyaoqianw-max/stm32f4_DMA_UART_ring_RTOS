#ifndef PLATFORM_QUEUE_H
#define PLATFORM_QUEUE_H

#include "platform_os_types.h"

platform_error_t platform_queue_create(platform_queue_t *queue, platform_size_t itemCount, platform_size_t itemSize);
platform_error_t platform_queue_send(platform_queue_t *queue, const void *item, uint32_t timeoutMs);
platform_error_t platform_queue_send_from_isr(platform_queue_t *queue, const void *item);
platform_error_t platform_queue_receive(platform_queue_t *queue, void *item, uint32_t timeoutMs);
platform_error_t platform_queue_get_count(const platform_queue_t *queue, platform_size_t *count);
platform_error_t platform_queue_get_space(const platform_queue_t *queue, platform_size_t *space);
platform_error_t platform_queue_delete(platform_queue_t *queue);

#endif

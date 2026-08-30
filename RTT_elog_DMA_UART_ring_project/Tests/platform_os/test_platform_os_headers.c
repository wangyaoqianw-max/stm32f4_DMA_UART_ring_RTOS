#include "platform_os.h"

int main(void)
{
    platform_thread_t thread = PLATFORM_OS_OBJECT_INITIALIZER;
    platform_mutex_t mutex = PLATFORM_OS_OBJECT_INITIALIZER;
    platform_semaphore_t semaphore = PLATFORM_OS_OBJECT_INITIALIZER;
    platform_queue_t queue = PLATFORM_OS_OBJECT_INITIALIZER;
    platform_timer_t timer = PLATFORM_OS_OBJECT_INITIALIZER;

    return (thread.native != 0) || (mutex.native != 0) || (semaphore.native != 0) ||
           (queue.native != 0) || (timer.native != 0);
}

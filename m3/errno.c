#include <stddef.h>
#include <features.h>

static int posix_errno = 0;

extern int *__errno_location(void) {
    return &posix_errno;
}
weak_alias(__errno_location, ___errno_location);

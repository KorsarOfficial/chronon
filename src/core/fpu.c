#include "core/fpu.h"
#include <string.h>

void fpu_reset(fpu_t* f) {
    memset(f, 0, sizeof(*f));
    f->enabled = true; /* simplified: always enabled */
}

/* RF archive compatibility only: no TMOS scheduler, timers or messages. */
#include <stdint.h>
#include <string.h>
#include "CH58x_common.h"

void tmos_memcpy(void *dst, const void *src, uint32_t len) { memcpy(dst, src, len); }
void tmos_memset(void *dst, uint8_t value, uint32_t len) { memset(dst, value, len); }
BOOL tmos_memcmp(const void *a, const void *b, uint32_t len) { return memcmp(a, b, len) == 0 ? TRUE : FALSE; }
uint32_t tmos_strlen(char *s) { return (uint32_t)strlen(s); }

/* RF objects share these small control blocks with the vendor TMOS object.
 * BLE IRQs are not enabled in this target, therefore no TMOS callback is
 * installed here. */
uint8_t ble[0x3c];
void *fnGetClockCBs;

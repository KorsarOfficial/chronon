#ifndef CORTEX_M_MPU_H
#define CORTEX_M_MPU_H

#include "core/types.h"
#include "core/bus.h"

/* Cortex-M3/M4 MPU at 0xE000ED90 (ARM ARM B3.5).
   8 regions, each: BASE (0x..0), ATTR/SIZE (0x..4).
   +0x00 TYPE      (RO): bits[15:8] DREGION (=8)
   +0x04 CTRL      : bit 0 ENABLE, bit 1 HFNMIENA, bit 2 PRIVDEFENA
   +0x08 RNR       : region number register (selects active region for RBAR/RASR)
   +0x0C RBAR      : region base address register
   +0x10 RASR      : region attribute and size register
   ... aliases follow */

#define MPU_BASE 0xE000ED90u
#define MPU_SIZE 0x60u
#define MPU_REGIONS 8

typedef struct mpu_region_s {
    u32 base;
    u32 rasr;     /* attributes + size */
    bool enabled;
} mpu_region_t;

typedef struct mpu_s {
    u32 ctrl;
    u32 rnr;
    mpu_region_t r[MPU_REGIONS];
} mpu_t;

int  mpu_attach(bus_t* b, mpu_t* m);
/* Returns true if access at addr (size bytes, write?) is permitted. */
bool mpu_check(const mpu_t* m, addr_t a, u32 size, bool is_write, bool is_priv);

#endif

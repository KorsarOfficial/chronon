#include "core/bus.h"
#include "test_harness.h"

TEST(flat_rw) {
    bus_t b; bus_init(&b);
    bus_add_flat(&b, "sram", 0x20000000, 0x1000, true);
    bus_w32(&b, 0x20000000, 0xDEADBEEF);
    ASSERT_EQ_U32(bus_r32(&b, 0x20000000), 0xDEADBEEF);
    bus_w16(&b, 0x20000010, 0xABCD);
    ASSERT_EQ_U32(bus_r16(&b, 0x20000010), 0xABCD);
    bus_w8(&b, 0x20000020, 0x42);
    ASSERT_EQ_U32(bus_r8(&b, 0x20000020), 0x42);
}

TEST(flat_readonly) {
    bus_t b; bus_init(&b);
    bus_add_flat(&b, "flash", 0, 0x1000, false);
    u32 v = 0;
    ASSERT_TRUE(!bus_write(&b, 0, 4, 0x1234));
    ASSERT_TRUE(bus_read(&b, 0, 4, &v));
    ASSERT_EQ_U32(v, 0); /* flash was zero-init */
}

TEST(blob_load) {
    bus_t b; bus_init(&b);
    bus_add_flat(&b, "flash", 0, 0x1000, false);
    u8 blob[4] = { 0x01, 0x02, 0x03, 0x04 };
    ASSERT_TRUE(bus_load_blob(&b, 0, blob, 4));
    ASSERT_EQ_U32(bus_r32(&b, 0), 0x04030201);
}

int main(void) {
    RUN(flat_rw);
    RUN(flat_readonly);
    RUN(blob_load);
    TEST_REPORT();
}

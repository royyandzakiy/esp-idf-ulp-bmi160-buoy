#include "ulp_riscv.h"
#include "ulp_riscv_utils.h"
#include "ulp_riscv_i2c.h"
#include "ulp_riscv_gpio.h"
#include "ulp_shared.h"

/* ---------- Config ---------- */
#define BMI160_ADDR        0x68
#define REG_ACC_X_L        0x12    // 0x12..0x17 = ax,ay,az (LSB first)
#define SAMPLES            8       // samples per wake
#define VAR_THRESHOLD      400000  // fixed-point variance threshold
#define HITS_TO_WAKE       5       // how many samples must exceed threshold

/* ---------- Shared with CPU (RTC_SLOW_MEM) ---------- */
ulp_shared_t ulp_shared;   // lives in RTC_SLOW_MEM

/* ---------- Fixed-point integer sqrt (Newton) ---------- */
static uint32_t isqrt(uint32_t n) {
    if (n == 0) return 0;
    uint32_t x = n, y = (x + 1) >> 1;
    while (y < x) { x = y; y = (x + n / x) >> 1; }
    return x;
}

/* ---------- Read BMI160 accel (raw int16) ---------- */
static void read_accel(int16_t *ax, int16_t *ay, int16_t *az) {
    uint8_t buf[6];
    ulp_riscv_i2c_master_set_slave_addr(BMI160_ADDR);
    ulp_riscv_i2c_master_set_slave_reg_addr(REG_ACC_X_L);
    ulp_riscv_i2c_master_read_from_device(buf, 6);
    *ax = (int16_t)((buf[1] << 8) | buf[0]);
    *ay = (int16_t)((buf[3] << 8) | buf[2]);
    *az = (int16_t)((buf[5] << 8) | buf[4]);
}

/* ---------- Compute |a| and variance over N samples ---------- */
static uint32_t compute_variance(void) {
    uint32_t sum = 0, sum_sq = 0;

    for (int i = 0; i < SAMPLES; i++) {
        int16_t ax, ay, az;
        read_accel(&ax, &ay, &az);

        // integer magnitude: |a| ~= sqrt(ax^2+ay^2+az^2)
        uint32_t mag = isqrt((uint32_t)(ax*ax) +
                             (uint32_t)(ay*ay) +
                             (uint32_t)(az*az));
        sum += mag;
        sum_sq += mag * mag;

        ulp_riscv_delay_cycles(100000);  // ~ a few ms between samples
    }

    uint32_t mean = sum / SAMPLES;
    uint32_t mean_sq = sum_sq / SAMPLES;
    uint32_t var = mean_sq - (mean * mean);  // integer variance
    return var;
}

/* ---------- Main ULP entry ---------- */
int main(void) {
    ulp_shared.wake_reason = 0;
    ulp_shared.hits = 0;

    // --- IDLE + CONFIRM state machine, single pass per wake ---
    uint32_t var = compute_variance();
    ulp_shared.last_variance = var;

    if (var > VAR_THRESHOLD) {
        ulp_shared.hits++;
        if (ulp_shared.hits >= HITS_TO_WAKE) {
            ulp_shared.wake_reason = 1;
            ulp_riscv_wakeup_main_processor();
        }
    } else {
        ulp_shared.hits = 0;   // reset on calm water
    }

    return 0;
}

#include <stdio.h>
#include "esp_sleep.h"
#include "ulp_riscv.h"
#include "ulp_main.h"   // generated header exporting ulp_shared
#include "esp32s3/ulp_riscv.h"

extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[]   asm("_binary_ulp_main_bin_end");

extern ulp_shared_t ulp_shared;   // symbol into RTC memory

/* ---------- Tiny "inference": 3-class rule-based model ---------- */
typedef enum {
    CLASS_CALM = 0,
    CLASS_FEEDING,
    CLASS_AGITATED
} pond_class_t;

static pond_class_t infer(uint32_t variance, uint32_t hits) {
    // Trivial rule-based "model":
    //   low variance             -> calm
    //   moderate + several hits  -> feeding
    //   extreme variance         -> agitated (bird, splash, storm)
    if (variance < 300000)              return CLASS_CALM;
    if (variance < 1500000 && hits >= 5) return CLASS_FEEDING;
    return CLASS_AGITATED;
}

static const char *class_name(pond_class_t c) {
    switch (c) {
        case CLASS_CALM:     return "CALM";
        case CLASS_FEEDING:  return "FEEDING";
        case CLASS_AGITATED: return "AGITATED";
    }
    return "UNKNOWN";
}

void app_main(void) {
    // Load & start ULP once
    ulp_riscv_load_binary(ulp_main_bin_start,
                          ulp_main_bin_end - ulp_main_bin_start);
    ulp_riscv_run();  // sets up RTC I2C + GPIO wake on BMI160 INT

    while (1) {
        // Enter deep sleep; wake only if ULP escalates
        esp_sleep_enable_ulp_wakeup();
        esp_deep_sleep_start();

        // ---- Woke up by ULP ----
        if (ulp_shared.wake_reason == 1) {
            pond_class_t cls = infer(ulp_shared.last_variance,
                                     ulp_shared.hits);

            printf("[WAKE] var=%lu hits=%lu -> %s\n",
                   (unsigned long)ulp_shared.last_variance,
                   (unsigned long)ulp_shared.hits,
                   class_name(cls));

            if (cls == CLASS_FEEDING) {
                printf("  -> Dispensing extra feed / logging event\n");
                // future: trigger feeder, send LoRa packet
            }
        }

        // Clear shared state so next ULP pass starts clean
        ulp_shared.wake_reason = 0;
        ulp_shared.hits = 0;
        ulp_shared.last_variance = 0;
    }
}
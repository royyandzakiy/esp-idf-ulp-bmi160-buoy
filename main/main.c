// main/main.c

/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* ULP riscv DS18B20 1wire temperature sensor example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include "esp_sleep.h"
#include "soc/sens_reg.h"
#include "driver/gpio.h"
#include "ulp_riscv.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ulp/ulp_shared.h"

#define WAKEUP_PIN 0

extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[]   asm("_binary_ulp_main_bin_end");
extern ulp_shared_t ulp_shared;   // symbol into RTC memory

static void init_ulp_program();

/* ---------- Tiny "inference": 3-class rule-based model ---------- */
typedef enum {
    STATE_CALM = 0,
    STATE_FISH_ACTIVE,
    STATE_AGITATED
} buoy_state_t;

static const char *buoy_state_to_str(buoy_state_t s) {
    switch (s) {
        case STATE_CALM:     return "CALM";
        case STATE_FISH_ACTIVE:  return "STATE_FISH_ACTIVE";
        case STATE_AGITATED: return "AGITATED";
    }
    return "UNKNOWN";
}

static buoy_state_t infer(uint32_t variance, uint32_t hits) {
    // Trivial rule-based "model":
    //   low variance             -> calm
    //   moderate + several hits  -> feeding
    //   extreme variance         -> agitated (bird, splash, storm)
    if (variance < 300000)                   return STATE_CALM;
    if (variance < 1500000 && hits >= 5)     return STATE_FISH_ACTIVE;
    return STATE_AGITATED;
}

static void wakeup_gpio_init()
{
    /* Configure the button GPIO as input, enable wakeup */
    gpio_config_t config = {
            .pin_bit_mask = BIT64(WAKEUP_PIN),
            .mode = GPIO_MODE_INPUT
    };
    ESP_ERROR_CHECK(gpio_config(&config));

    gpio_wakeup_enable(WAKEUP_PIN, GPIO_INTR_LOW_LEVEL);
    gpio_hold_en(WAKEUP_PIN);
}

void app_main()
{
    vTaskDelay(pdMS_TO_TICKS(1000));
    uint32_t causes = esp_sleep_get_wakeup_causes();

    /* not a wakeup from ULP, load the firmware */
    if (!(causes & BIT(ESP_SLEEP_WAKEUP_ULP))) {
        printf("Not a ULP-RISC-V wakeup, initializing it! \n");
        wakeup_gpio_init();
        init_ulp_program();
    }

    /* a wakeup from ULP, run the feeding logic */
    if (causes & BIT(ESP_SLEEP_WAKEUP_ULP)) {
        printf("ULP-RISC-V woke up the main CPU! \n");

        buoy_state_t state = infer(ulp_shared.last_variance, ulp_shared.hits);

        printf("[WAKE] var=%lu hits=%lu -> %s\n",
               (unsigned long)ulp_shared.last_variance,
               (unsigned long)ulp_shared.hits,
               buoy_state_to_str(state));

        if (state == STATE_FISH_ACTIVE) {
            printf("  -> Dispensing extra feed / logging event\n");
            // future: trigger feeder, send LoRa packet

            /* Feeding confirmed -> reset the persistent hit counter */
            ulp_shared.hits = 0;
        }

        /* Clear wake reason so next ULP pass starts clean.
           NOTE: do NOT reset hits here -- we want it to keep
           accumulating across wakes until feeding is confirmed
           or the ULP itself resets it on calm water. */
        ulp_shared.wake_reason = 0;
    }

    /* Go back to sleep, only the ULP Risc-V will run */
    printf("Entering in deep sleep\n\n");

    /* Small delay to ensure the messages are printed */
    vTaskDelay(100);

    /* RTC peripheral power domain needs to be kept on to detect
       the GPIO state change */
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);

    ESP_ERROR_CHECK( esp_sleep_enable_ulp_wakeup());
    esp_deep_sleep_start();
}

static void init_ulp_program()
{
    esp_err_t err = ulp_riscv_load_binary(ulp_main_bin_start, (ulp_main_bin_end - ulp_main_bin_start));
    ESP_ERROR_CHECK(err);

    /* Start the program */
    ulp_riscv_cfg_t cfg = {
        .wakeup_source = ULP_RISCV_WAKEUP_SOURCE_GPIO,
    };

    err = ulp_riscv_config_and_run(&cfg);
    ESP_ERROR_CHECK(err);
}

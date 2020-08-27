// Copyright 2015-2018 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.



#include "sdkconfig.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_types.h>
#include "esp_err.h"
#include "esp_intr_alloc.h"
#include "esp_attr.h"
#include "esp_freertos_hooks.h"
#include "soc/timer_periph.h"
#include "driver/timer.h"
#include "driver/periph_ctrl.h"
#include "esp_int_wdt.h"
#include "hal/timer_ll.h"
#include "esp_efuse.h"

#if CONFIG_ESP_INT_WDT

#define TG1_WDT_TICK_US 500
#define WDT_INT_NUM ETS_T1_WDT_INUM

#if CONFIG_ESP32_ECO3_CACHE_LOCK_FIX
/*
 * This parameter is indicates the response time of tg1 watchdog to
 * identify the live lock,
 */
#define TG1_WDT_LIVELOCK_TIMEOUT_MS    (20)

extern uint32_t _l4_intr_livelock_counter, _l4_intr_livelock_max;
#endif

//Take care: the tick hook can also be called before esp_int_wdt_init() is called.
#if CONFIG_ESP_INT_WDT_CHECK_CPU1
//Not static; the ISR assembly checks this.
bool int_wdt_app_cpu_ticked=false;

static void IRAM_ATTR tick_hook(void) {
    if (xPortGetCoreID()!=0) {
        int_wdt_app_cpu_ticked=true;
    } else {
        //Only feed wdt if app cpu also ticked.
        if (int_wdt_app_cpu_ticked) {
            timer_ll_wdt_set_protect(&TIMERG1, false);
            //Set timeout before interrupt
#if CONFIG_ESP32_ECO3_CACHE_LOCK_FIX
            _l4_intr_livelock_counter = 0;
            timer_ll_wdt_set_timeout(&TIMERG1, 0,
                CONFIG_ESP_INT_WDT_TIMEOUT_MS*1000/TG1_WDT_TICK_US/(_l4_intr_livelock_max+1));
#else
            timer_ll_wdt_set_timeout(&TIMERG1, 0,
                CONFIG_ESP_INT_WDT_TIMEOUT_MS*1000/TG1_WDT_TICK_US);
#endif
            //Set timeout before reset
            timer_ll_wdt_set_timeout(&TIMERG1, 1,
                2*CONFIG_ESP_INT_WDT_TIMEOUT_MS*1000/TG1_WDT_TICK_US);
            timer_ll_wdt_feed(&TIMERG1);
            timer_ll_wdt_set_protect(&TIMERG1, true);
            int_wdt_app_cpu_ticked=false;
        }
    }
}
#else
static void IRAM_ATTR tick_hook(void) {
    if (xPortGetCoreID()!=0) return;
    timer_ll_wdt_set_protect(&TIMERG1, false);
    //Set timeout before interrupt
    timer_ll_wdt_set_timeout(&TIMERG1, 0, CONFIG_ESP_INT_WDT_TIMEOUT_MS*1000/TG1_WDT_TICK_US);
    //Set timeout before reset
    timer_ll_wdt_set_timeout(&TIMERG1, 1, 2*CONFIG_ESP_INT_WDT_TIMEOUT_MS*1000/TG1_WDT_TICK_US);
    timer_ll_wdt_feed(&TIMERG1);
    timer_ll_wdt_set_protect(&TIMERG1, true);
}
#endif


void esp_int_wdt_init(void) {
    periph_module_enable(PERIPH_TIMG1_MODULE);
    //The timer configs initially are set to 5 seconds, to make sure the CPU can start up. The tick hook sets
    //it to their actual value.
    timer_ll_wdt_set_protect(&TIMERG1, false);
    timer_ll_wdt_init(&TIMERG1);
    timer_ll_wdt_set_tick(&TIMERG1, TG1_WDT_TICK_US); //Prescaler: wdt counts in ticks of TG1_WDT_TICK_US
    //1st stage timeout: interrupt
    timer_ll_wdt_set_timeout_behavior(&TIMERG1, 0, TIMER_WDT_INT);
    timer_ll_wdt_set_timeout(&TIMERG1, 0, 5*1000*1000/TG1_WDT_TICK_US);
    //2nd stage timeout: reset system
    timer_ll_wdt_set_timeout_behavior(&TIMERG1, 1, TIMER_WDT_RESET_SYSTEM);
    timer_ll_wdt_set_timeout(&TIMERG1, 1, 5*1000*1000/TG1_WDT_TICK_US);
    timer_ll_wdt_set_enable(&TIMERG1, true);
    timer_ll_wdt_feed(&TIMERG1);
    timer_ll_wdt_set_protect(&TIMERG1, true);

    timer_ll_wdt_clear_intr_status(&TIMERG1);
    timer_ll_wdt_enable_intr(&TIMERG1);
}

void esp_int_wdt_cpu_init(void)
{
    assert((CONFIG_ESP_INT_WDT_TIMEOUT_MS >= (portTICK_PERIOD_MS<<1)) && "Interrupt watchdog timeout needs to meet double SysTick period!");
    esp_register_freertos_tick_hook_for_cpu(tick_hook, xPortGetCoreID());
    ESP_INTR_DISABLE(WDT_INT_NUM);
    intr_matrix_set(xPortGetCoreID(), ETS_TG1_WDT_LEVEL_INTR_SOURCE, WDT_INT_NUM);
#if CONFIG_ESP32_ECO3_CACHE_LOCK_FIX
    /*
     * This is a workaround for issue 3.15 in "ESP32 ECO and Workarounds for Bugs" document.
     */
    _l4_intr_livelock_max = 0;
    if (soc_has_cache_lock_bug()) {
        assert((portTICK_PERIOD_MS<<1) <= TG1_WDT_LIVELOCK_TIMEOUT_MS);
        assert(CONFIG_ESP_INT_WDT_TIMEOUT_MS >= (TG1_WDT_LIVELOCK_TIMEOUT_MS*3));
        _l4_intr_livelock_max = CONFIG_ESP_INT_WDT_TIMEOUT_MS/TG1_WDT_LIVELOCK_TIMEOUT_MS - 1;
    }
#endif
    //We do not register a handler for the interrupt because it is interrupt level 4 which
    //is not servicable from C. Instead, xtensa_vectors.S has a call to the panic handler for
    //this interrupt.
    ESP_INTR_ENABLE(WDT_INT_NUM);
}



#endif

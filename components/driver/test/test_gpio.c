/*
 * SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * About test environment UT_T1_GPIO:
 * Please connect TEST_GPIO_EXT_OUT_IO and TEST_GPIO_EXT_IN_IO
 */
#include <stdio.h>
#include <string.h>

#include "esp_system.h"
#include "esp_sleep.h"
#include "unity.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "sdkconfig.h"
#include "esp_rom_uart.h"
#include "esp_rom_sys.h"
#include "test_utils.h"


#define WAKE_UP_IGNORE 1  // gpio_wakeup function development is not completed yet, set it deprecated.

#if CONFIG_IDF_TARGET_ESP32
#define TEST_GPIO_EXT_OUT_IO            18  // default output GPIO
#define TEST_GPIO_EXT_IN_IO             19  // default input GPIO
#define TEST_GPIO_OUTPUT_PIN            23
#define TEST_GPIO_INPUT_ONLY_PIN        34
#define TEST_GPIO_OUTPUT_MAX            GPIO_NUM_34
#define TEST_GPIO_INPUT_LEVEL_HIGH_PIN  2
#define TEST_GPIO_INPUT_LEVEL_LOW_PIN   4
#elif CONFIG_IDF_TARGET_ESP32S2
// ESP32_S2 DEVKIC uses IO19 and IO20 as USB functions, so it is necessary to avoid using IO19, otherwise GPIO io pull up/down function cannot pass
// Also the first version of ESP32-S2-Saola has pullup issue on GPIO18, which is tied to 3V3 on the
// runner. Also avoid using GPIO18.
#define TEST_GPIO_EXT_OUT_IO            17  // default output GPIO
#define TEST_GPIO_EXT_IN_IO             21  // default input GPIO
#define TEST_GPIO_OUTPUT_PIN            12
#define TEST_GPIO_INPUT_ONLY_PIN        46
#define TEST_GPIO_OUTPUT_MAX            GPIO_NUM_46
#define TEST_GPIO_INPUT_LEVEL_HIGH_PIN  17
#define TEST_GPIO_INPUT_LEVEL_LOW_PIN   1
#elif CONFIG_IDF_TARGET_ESP32S3
//  IO19 and IO20 are connected as USB functions.
#define TEST_GPIO_EXT_OUT_IO            17  // default output GPIO
#define TEST_GPIO_EXT_IN_IO             21  // default input GPIO
#define TEST_GPIO_OUTPUT_PIN            12
#define TEST_GPIO_OUTPUT_MAX            GPIO_NUM_MAX
#define TEST_GPIO_USB_DM_IO             19  // USB D- GPIO
#define TEST_GPIO_USB_DP_IO             20  // USB D+ GPIO
#define TEST_GPIO_INPUT_LEVEL_HIGH_PIN  17
#define TEST_GPIO_INPUT_LEVEL_LOW_PIN   1
#elif CONFIG_IDF_TARGET_ESP32C3
#define TEST_GPIO_EXT_OUT_IO            2  // default output GPIO
#define TEST_GPIO_EXT_IN_IO             3  // default input GPIO
#define TEST_GPIO_OUTPUT_PIN            1
#define TEST_GPIO_OUTPUT_MAX            GPIO_NUM_21
#define TEST_GPIO_USB_DM_IO             18  // USB D- GPIO
#define TEST_GPIO_USB_DP_IO             19  // USB D+ GPIO
#define TEST_GPIO_INPUT_LEVEL_HIGH_PIN  10
#define TEST_GPIO_INPUT_LEVEL_LOW_PIN   1
#endif

// If there is any input-only pin, enable input-only pin part of some tests.
#define SOC_HAS_INPUT_ONLY_PIN (CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2)

// define public test io on all boards(esp32, esp32s2, esp32s3, esp32c3)
#define TEST_IO_9 GPIO_NUM_9
#define TEST_IO_10 GPIO_NUM_10

static volatile int disable_intr_times = 0;  // use this to calculate how many times it go into interrupt
static volatile int level_intr_times = 0;  // use this to get how many times the level interrupt happened
static volatile int edge_intr_times = 0;   // use this to get how many times the edge interrupt happened
#if !WAKE_UP_IGNORE
static bool wake_up_result = false;  // use this to judge the wake up event happen or not
#endif


/**
 * do some initialization operation in this function
 * @param num: it is the destination GPIO wanted to be initialized
 *
 */
static gpio_config_t init_io(gpio_num_t num)
{
    TEST_ASSERT(num < TEST_GPIO_OUTPUT_MAX);
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << num);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    return io_conf;
}

// edge interrupt event
__attribute__((unused)) static void gpio_isr_edge_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    esp_rom_printf("GPIO[%d] intr on core %d, val: %d\n", gpio_num, cpu_hal_get_core_id(), gpio_get_level(gpio_num));
    edge_intr_times++;
}

#if !TEMPORARY_DISABLED_FOR_TARGETS(ESP32S2, ESP32S3, ESP32C3)
//No runners
// level interrupt event with "gpio_intr_disable"
static void gpio_isr_level_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    disable_intr_times++;
    esp_rom_printf("GPIO[%d] intr, val: %d, disable_intr_times = %d\n", gpio_num, gpio_get_level(gpio_num), disable_intr_times);
    gpio_intr_disable(gpio_num);
}

// level interrupt event
static void gpio_isr_level_handler2(void *arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    level_intr_times++;
    esp_rom_printf("GPIO[%d] intr, val: %d\n", gpio_num, gpio_get_level(gpio_num));
    if (gpio_get_level(gpio_num)) {
        gpio_set_level(TEST_GPIO_EXT_OUT_IO, 0);
    } else {
        gpio_set_level(TEST_GPIO_EXT_OUT_IO, 1);
    }
    esp_rom_printf("GPIO[%d] intr, val: %d, level_intr_times = %d\n", TEST_GPIO_EXT_OUT_IO, gpio_get_level(TEST_GPIO_EXT_OUT_IO), level_intr_times);
    esp_rom_printf("GPIO[%d] intr, val: %d, level_intr_times = %d\n", gpio_num, gpio_get_level(gpio_num), level_intr_times);
}
#endif //!TEMPORARY_DISABLED_FOR_TARGETS(ESP32S2, ESP32S3, ESP32C3)

#if !WAKE_UP_IGNORE
// get result of waking up or not
static void sleep_wake_up(void *arg)
{
    gpio_config_t io_config = init_io(TEST_GPIO_EXT_IN_IO);
    io_config.mode = GPIO_MODE_INPUT;
    gpio_config(&io_config);
    TEST_ESP_OK(gpio_wakeup_enable(TEST_GPIO_EXT_IN_IO, GPIO_INTR_HIGH_LEVEL));
    esp_light_sleep_start();
    wake_up_result = true;
}

// wake up light sleep event
static void trigger_wake_up(void *arg)
{
    gpio_config_t io_config = init_io(TEST_GPIO_EXT_OUT_IO);
    gpio_config(&io_config);
    gpio_set_level(TEST_GPIO_EXT_OUT_IO, 0);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(TEST_GPIO_EXT_OUT_IO, gpio_isr_level_handler, (void *) TEST_GPIO_EXT_IN_IO);
    gpio_set_level(TEST_GPIO_EXT_OUT_IO, 1);
    vTaskDelay(100 / portTICK_RATE_MS);
}
#endif //!WAKE_UP_IGNORE

static void prompt_to_continue(const char *str)
{
    printf("%s , please press \"Enter\" to go on!\n", str);
    char sign[5] = {0};
    while (strlen(sign) == 0) {
        /* Flush anything already in the RX buffer */
        while (esp_rom_uart_rx_one_char((uint8_t *) sign) == ETS_OK) {
        }
        /* Read line */
        esp_rom_uart_rx_string((uint8_t *) sign, sizeof(sign) - 1);
    }
}

static void drive_capability_set_get(gpio_num_t num, gpio_drive_cap_t capability)
{
    gpio_config_t pad_io = init_io(num);
    TEST_ESP_OK(gpio_config(&pad_io));
    TEST_ASSERT(gpio_set_drive_capability(num, GPIO_DRIVE_CAP_MAX) == ESP_ERR_INVALID_ARG);

    gpio_drive_cap_t cap;
    TEST_ESP_OK(gpio_set_drive_capability(num, capability));
    TEST_ESP_OK(gpio_get_drive_capability(num, &cap));
    TEST_ASSERT_EQUAL_INT(cap, capability);
}


// test the basic configuration function with right parameters and error parameters
TEST_CASE("GPIO config parameters test", "[gpio]")
{
    //error param test
    //ESP32 test 41 bit, ESP32-S2 test 48 bit, ESP32-S3 test 50 bit
    gpio_config_t io_config = { 0 };
    io_config.intr_type = GPIO_INTR_DISABLE;
    io_config.pin_bit_mask = ((uint64_t)1 << (GPIO_NUM_MAX + 1));
    TEST_ASSERT(gpio_config(&io_config) == ESP_ERR_INVALID_ARG);

    // test 0
    io_config.pin_bit_mask = 0;
    TEST_ASSERT(gpio_config(&io_config) == ESP_ERR_INVALID_ARG);

    //ESP32 test 40 bit, ESP32-S2 test 47 bit, ESP32-S3 test 49 bit
    io_config.pin_bit_mask = ((uint64_t)1 << GPIO_NUM_MAX);
    TEST_ASSERT(gpio_config(&io_config) == ESP_ERR_INVALID_ARG);

    io_config.pin_bit_mask = ((uint64_t)1 << TEST_GPIO_OUTPUT_PIN);
    TEST_ESP_OK(gpio_config(&io_config));

    //This IO is just used for input, C3 and S3 doesn't have input only pin.
#if SOC_HAS_INPUT_ONLY_PIN
    io_config.pin_bit_mask = ((uint64_t)1 << TEST_GPIO_INPUT_ONLY_PIN);
    io_config.mode = GPIO_MODE_INPUT;
    TEST_ESP_OK(gpio_config(&io_config));
    io_config.mode = GPIO_MODE_OUTPUT;
    // The pin is input only, once set as output should log something
    TEST_ASSERT(gpio_config(&io_config) == ESP_ERR_INVALID_ARG);
#endif // SOC_HAS_INPUT_ONLY_PIN
}

#if !TEMPORARY_DISABLED_FOR_TARGETS(ESP32S2, ESP32S3, ESP32C3)
//No runners
TEST_CASE("GPIO rising edge interrupt test", "[gpio][test_env=UT_T1_GPIO]")
{
    edge_intr_times = 0;  // set it as 0 prepare to test
    //init input and output gpio
    gpio_config_t output_io = init_io(TEST_GPIO_EXT_OUT_IO);
    gpio_config_t input_io = init_io(TEST_GPIO_EXT_IN_IO);
    input_io.intr_type = GPIO_INTR_POSEDGE;
    input_io.mode = GPIO_MODE_INPUT;
    input_io.pull_up_en = 1;
    TEST_ESP_OK(gpio_config(&output_io));
    TEST_ESP_OK(gpio_config(&input_io));
    TEST_ESP_OK(gpio_set_level(TEST_GPIO_EXT_OUT_IO, 0));

    //rising edge intr
    TEST_ESP_OK(gpio_set_intr_type(TEST_GPIO_EXT_IN_IO, GPIO_INTR_POSEDGE));
    TEST_ESP_OK(gpio_install_isr_service(0));
    gpio_isr_handler_add(TEST_GPIO_EXT_IN_IO, gpio_isr_edge_handler, (void *)TEST_GPIO_EXT_IN_IO);
    TEST_ESP_OK(gpio_set_level(TEST_GPIO_EXT_OUT_IO, 1));
    TEST_ASSERT_EQUAL_INT(edge_intr_times, 1);
    vTaskDelay(100 / portTICK_RATE_MS);
    gpio_isr_handler_remove(TEST_GPIO_EXT_IN_IO);
    gpio_uninstall_isr_service();
}

TEST_CASE("GPIO falling edge interrupt test", "[gpio][test_env=UT_T1_GPIO]")
{
    edge_intr_times = 0;
    gpio_config_t output_io = init_io(TEST_GPIO_EXT_OUT_IO);
    gpio_config_t input_io = init_io(TEST_GPIO_EXT_IN_IO);
    input_io.intr_type = GPIO_INTR_POSEDGE;
    input_io.mode = GPIO_MODE_INPUT;
    input_io.pull_up_en = 1;
    TEST_ESP_OK(gpio_config(&output_io));
    TEST_ESP_OK(gpio_config(&input_io));
    TEST_ESP_OK(gpio_set_level(TEST_GPIO_EXT_OUT_IO, 1));

    gpio_set_intr_type(TEST_GPIO_EXT_IN_IO, GPIO_INTR_NEGEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(TEST_GPIO_EXT_IN_IO, gpio_isr_edge_handler, (void *) TEST_GPIO_EXT_IN_IO);
    gpio_set_level(TEST_GPIO_EXT_OUT_IO, 0);
    vTaskDelay(100 / portTICK_RATE_MS);
    TEST_ASSERT_EQUAL_INT(edge_intr_times, 1);
    vTaskDelay(100 / portTICK_RATE_MS);
    gpio_isr_handler_remove(TEST_GPIO_EXT_IN_IO);
    gpio_uninstall_isr_service();
}

TEST_CASE("GPIO both rising and falling edge interrupt test", "[gpio][test_env=UT_T1_GPIO]")
{
    edge_intr_times = 0;
    gpio_config_t output_io = init_io(TEST_GPIO_EXT_OUT_IO);
    gpio_config_t input_io = init_io(TEST_GPIO_EXT_IN_IO);
    input_io.intr_type = GPIO_INTR_POSEDGE;
    input_io.mode = GPIO_MODE_INPUT;
    input_io.pull_up_en = 1;
    TEST_ESP_OK(gpio_config(&output_io));
    TEST_ESP_OK(gpio_config(&input_io));
    TEST_ESP_OK(gpio_set_level(TEST_GPIO_EXT_OUT_IO, 0));
    int level = 0;

    gpio_set_intr_type(TEST_GPIO_EXT_IN_IO, GPIO_INTR_ANYEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(TEST_GPIO_EXT_IN_IO, gpio_isr_edge_handler, (void *) TEST_GPIO_EXT_IN_IO);
    // for rising edge in GPIO_INTR_ANYEDGE
    while (1) {
        level = level + 1;
        gpio_set_level(TEST_GPIO_EXT_OUT_IO, level * 0.2);
        if (level > 10) {
            break;
        }
        vTaskDelay(100 / portTICK_RATE_MS);
    }
    vTaskDelay(100 / portTICK_RATE_MS);
    // for falling rdge in GPIO_INTR_ANYEDGE
    while (1) {
        level = level - 1;
        gpio_set_level(TEST_GPIO_EXT_OUT_IO, level / 5);
        if (level < 0) {
            break;
        }
        vTaskDelay(100 / portTICK_RATE_MS);
    }
    vTaskDelay(100 / portTICK_RATE_MS);
    TEST_ASSERT_EQUAL_INT(edge_intr_times, 2);
    vTaskDelay(100 / portTICK_RATE_MS);
    gpio_isr_handler_remove(TEST_GPIO_EXT_IN_IO);
    gpio_uninstall_isr_service();
}

TEST_CASE("GPIO input high level trigger, cut the interrupt source exit interrupt test", "[gpio][test_env=UT_T1_GPIO]")
{
    level_intr_times = 0;
    gpio_config_t output_io = init_io(TEST_GPIO_EXT_OUT_IO);
    gpio_config_t input_io = init_io(TEST_GPIO_EXT_IN_IO);
    input_io.intr_type = GPIO_INTR_POSEDGE;
    input_io.mode = GPIO_MODE_INPUT;
    input_io.pull_up_en = 1;
    TEST_ESP_OK(gpio_config(&output_io));
    TEST_ESP_OK(gpio_config(&input_io));
    TEST_ESP_OK(gpio_set_level(TEST_GPIO_EXT_OUT_IO, 0));

    gpio_set_intr_type(TEST_GPIO_EXT_IN_IO, GPIO_INTR_HIGH_LEVEL);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(TEST_GPIO_EXT_IN_IO, gpio_isr_level_handler2, (void *) TEST_GPIO_EXT_IN_IO);
    gpio_set_level(TEST_GPIO_EXT_OUT_IO, 1);
    vTaskDelay(100 / portTICK_RATE_MS);
    TEST_ASSERT_EQUAL_INT_MESSAGE(level_intr_times, 1, "go into high-level interrupt more than once with cur interrupt source way");
    gpio_isr_handler_remove(TEST_GPIO_EXT_IN_IO);
    gpio_uninstall_isr_service();

}

TEST_CASE("GPIO low level interrupt test", "[gpio][test_env=UT_T1_GPIO]")
{
    disable_intr_times = 0;
    gpio_config_t output_io = init_io(TEST_GPIO_EXT_OUT_IO);
    gpio_config_t input_io = init_io(TEST_GPIO_EXT_IN_IO);
    input_io.intr_type = GPIO_INTR_POSEDGE;
    input_io.mode = GPIO_MODE_INPUT;
    input_io.pull_up_en = 1;
    TEST_ESP_OK(gpio_config(&output_io));
    TEST_ESP_OK(gpio_config(&input_io));
    TEST_ESP_OK(gpio_set_level(TEST_GPIO_EXT_OUT_IO, 1));

    gpio_set_intr_type(TEST_GPIO_EXT_IN_IO, GPIO_INTR_LOW_LEVEL);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(TEST_GPIO_EXT_IN_IO, gpio_isr_level_handler, (void *) TEST_GPIO_EXT_IN_IO);
    gpio_set_level(TEST_GPIO_EXT_OUT_IO, 0);
    printf("get level:%d\n", gpio_get_level(TEST_GPIO_EXT_IN_IO));
    vTaskDelay(100 / portTICK_RATE_MS);
    TEST_ASSERT_EQUAL_INT_MESSAGE(disable_intr_times, 1, "go into low-level interrupt more than once with disable way");
    gpio_isr_handler_remove(TEST_GPIO_EXT_IN_IO);
    gpio_uninstall_isr_service();
}

TEST_CASE("GPIO multi-level interrupt test, to cut the interrupt source exit interrupt ", "[gpio][test_env=UT_T1_GPIO]")
{
    level_intr_times = 0;
    gpio_config_t output_io = init_io(TEST_GPIO_EXT_OUT_IO);
    gpio_config_t input_io = init_io(TEST_GPIO_EXT_IN_IO);
    input_io.intr_type = GPIO_INTR_POSEDGE;
    input_io.mode = GPIO_MODE_INPUT;
    input_io.pull_up_en = 1;
    TEST_ESP_OK(gpio_config(&output_io));
    TEST_ESP_OK(gpio_config(&input_io));
    TEST_ESP_OK(gpio_set_level(TEST_GPIO_EXT_OUT_IO, 0));

    gpio_set_intr_type(TEST_GPIO_EXT_IN_IO, GPIO_INTR_HIGH_LEVEL);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(TEST_GPIO_EXT_IN_IO, gpio_isr_level_handler2, (void *) TEST_GPIO_EXT_IN_IO);
    gpio_set_level(TEST_GPIO_EXT_OUT_IO, 1);
    vTaskDelay(100 / portTICK_RATE_MS);
    TEST_ASSERT_EQUAL_INT_MESSAGE(level_intr_times, 1, "go into high-level interrupt more than once with cur interrupt source way");
    gpio_set_level(TEST_GPIO_EXT_OUT_IO, 1);
    vTaskDelay(200 / portTICK_RATE_MS);
    TEST_ASSERT_EQUAL_INT_MESSAGE(level_intr_times, 2, "go into high-level interrupt more than once with cur interrupt source way");
    gpio_isr_handler_remove(TEST_GPIO_EXT_IN_IO);
    gpio_uninstall_isr_service();
}

TEST_CASE("GPIO enable and disable interrupt test", "[gpio][test_env=UT_T1_GPIO]")
{
    disable_intr_times = 0;
    gpio_config_t output_io = init_io(TEST_GPIO_EXT_OUT_IO);
    gpio_config_t input_io = init_io(TEST_GPIO_EXT_IN_IO);
    input_io.intr_type = GPIO_INTR_POSEDGE;
    input_io.mode = GPIO_MODE_INPUT;
    input_io.pull_up_en = 1;
    TEST_ESP_OK(gpio_config(&output_io));
    TEST_ESP_OK(gpio_config(&input_io));

    TEST_ESP_OK(gpio_set_level(TEST_GPIO_EXT_OUT_IO, 0)); // Because of GPIO_INTR_HIGH_LEVEL interrupt, 0 must be set first
    TEST_ESP_OK(gpio_set_intr_type(TEST_GPIO_EXT_IN_IO, GPIO_INTR_HIGH_LEVEL));
    TEST_ESP_OK(gpio_install_isr_service(0));
    TEST_ESP_OK(gpio_isr_handler_add(TEST_GPIO_EXT_IN_IO, gpio_isr_level_handler, (void *) TEST_GPIO_EXT_IN_IO));
    TEST_ESP_OK(gpio_set_level(TEST_GPIO_EXT_OUT_IO, 1));
    TEST_ESP_OK(gpio_isr_handler_remove(TEST_GPIO_EXT_IN_IO));
    TEST_ESP_OK(gpio_set_level(TEST_GPIO_EXT_OUT_IO, 0));
    TEST_ASSERT_EQUAL_INT_MESSAGE(disable_intr_times, 1, "go into high-level interrupt more than once with disable way");

    // not install service now
    vTaskDelay(100 / portTICK_RATE_MS);
    TEST_ESP_OK(gpio_intr_disable(TEST_GPIO_EXT_IN_IO));
    TEST_ESP_OK(gpio_set_level(TEST_GPIO_EXT_OUT_IO, 1));
    TEST_ASSERT_EQUAL_INT_MESSAGE(disable_intr_times, 1, "disable interrupt does not work, still go into interrupt!");

    gpio_uninstall_isr_service();  //uninstall the service
    TEST_ASSERT(gpio_isr_handler_add(TEST_GPIO_EXT_IN_IO, gpio_isr_level_handler, (void *) TEST_GPIO_EXT_IN_IO) == ESP_ERR_INVALID_STATE);
    TEST_ASSERT(gpio_isr_handler_remove(TEST_GPIO_EXT_IN_IO) == ESP_ERR_INVALID_STATE);
}
#endif //DISABLED_FOR_TARGETS(ESP32S2, ESP32S3, ESP32C3)

#if !CONFIG_FREERTOS_UNICORE
static void install_isr_service_task(void *arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    //rising edge intr
    TEST_ESP_OK(gpio_set_intr_type(gpio_num, GPIO_INTR_POSEDGE));
    TEST_ESP_OK(gpio_install_isr_service(0));
    gpio_isr_handler_add(gpio_num, gpio_isr_edge_handler, (void *) gpio_num);
    vTaskSuspend(NULL);
}

TEST_CASE("GPIO interrupt on other CPUs test", "[gpio]")
{
    TaskHandle_t gpio_task_handle;
    gpio_config_t input_output_io = init_io(TEST_GPIO_EXT_OUT_IO);
    input_output_io.mode = GPIO_MODE_INPUT_OUTPUT;
    input_output_io.pull_up_en = 1;
    TEST_ESP_OK(gpio_config(&input_output_io));

    for (int cpu_num = 1; cpu_num < portNUM_PROCESSORS; ++cpu_num) {
        // We assume unit-test task is running on core 0, so we install gpio interrupt on other cores
        edge_intr_times = 0;
        TEST_ESP_OK(gpio_set_level(TEST_GPIO_EXT_OUT_IO, 0));
        xTaskCreatePinnedToCore(install_isr_service_task, "install_isr_service_task", 2048, (void *) TEST_GPIO_EXT_OUT_IO, 1, &gpio_task_handle, cpu_num);

        vTaskDelay(200 / portTICK_RATE_MS);
        TEST_ESP_OK(gpio_set_level(TEST_GPIO_EXT_OUT_IO, 1));
        vTaskDelay(100 / portTICK_RATE_MS);
        TEST_ASSERT_EQUAL_INT(edge_intr_times, 1);
        gpio_isr_handler_remove(TEST_GPIO_EXT_OUT_IO);
        gpio_uninstall_isr_service();
        test_utils_task_delete(gpio_task_handle);
    }
}
#endif //!CONFIG_FREERTOS_UNICORE

// ESP32 Connect GPIO18 with GPIO19, ESP32-S2 Connect GPIO17 with GPIO21,
// ESP32-S3 Connect GPIO17 with GPIO21, ESP32C3 Connect GPIO2 with GPIO3
// use multimeter to test the voltage, so it is ignored in CI
TEST_CASE("GPIO set gpio output level test", "[gpio][ignore][UT_T1_GPIO]")
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = ((uint64_t)1 << TEST_GPIO_EXT_OUT_IO);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    io_conf.pin_bit_mask = ((uint64_t)1 << TEST_GPIO_EXT_IN_IO);
    io_conf.mode = GPIO_MODE_INPUT;
    gpio_config(&io_conf);

    gpio_set_level(TEST_GPIO_EXT_OUT_IO, 0);
    // tested voltage is around 0v
    TEST_ASSERT_EQUAL_INT_MESSAGE(gpio_get_level(TEST_GPIO_EXT_IN_IO), 0, "get level error! the level should be low!");
    vTaskDelay(1000 / portTICK_RATE_MS);
    gpio_set_level(TEST_GPIO_EXT_OUT_IO, 1);
    // tested voltage is around 3.3v
    TEST_ASSERT_EQUAL_INT_MESSAGE(gpio_get_level(TEST_GPIO_EXT_IN_IO), 1, "get level error! the level should be high!");

    //This IO is just used for input, C3 and S3 doesn't have input only pin.
#if SOC_HAS_INPUT_ONLY_PIN
    io_conf.pin_bit_mask = ((uint64_t)1 << TEST_GPIO_INPUT_ONLY_PIN);
    io_conf.mode = GPIO_MODE_OUTPUT;
    gpio_config(&io_conf);
    TEST_ASSERT(gpio_config(&io_conf) == ESP_ERR_INVALID_ARG);
#endif // SOC_HAS_INPUT_ONLY_PIN
}

// TEST_GPIO_INPUT_LEVEL_HIGH_PIN connects to 3.3v pin, TEST_GPIO_INPUT_LEVEL_LOW_PIN connects to the GND pin
// use multimeter to test the voltage, so it is ignored in CI
TEST_CASE("GPIO get input level test", "[gpio][ignore]")
{
    gpio_num_t num1 = TEST_GPIO_INPUT_LEVEL_HIGH_PIN;
    int level1 = gpio_get_level(num1);
    printf("TEST_GPIO_INPUT_LEVEL_HIGH_PIN's level is: %d\n", level1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(level1, 1, "get level error! the level should be high!");

    gpio_num_t num2 = TEST_GPIO_INPUT_LEVEL_LOW_PIN;
    int level2 = gpio_get_level(num2);
    printf("TEST_GPIO_INPUT_LEVEL_LOW_PIN's level is: %d\n", level2);
    TEST_ASSERT_EQUAL_INT_MESSAGE(level2, 0, "get level error! the level should be low!");
    printf("the memory get: %d\n", esp_get_free_heap_size());
    //when case finish, get the result from multimeter, the TEST_GPIO_INPUT_LEVEL_HIGH_PIN is 3.3v, the TEST_GPIO_INPUT_LEVEL_LOW_PIN is 0.00v
}

TEST_CASE("GPIO io pull up/down function", "[gpio]")
{
    // First, ensure that the output IO will not affect the level
    gpio_config_t  io_conf = init_io(TEST_GPIO_EXT_OUT_IO);
    gpio_config(&io_conf);
    gpio_set_direction(TEST_GPIO_EXT_OUT_IO, GPIO_MODE_INPUT);
    io_conf = init_io(TEST_GPIO_EXT_IN_IO);
    gpio_config(&io_conf);
    gpio_set_direction(TEST_GPIO_EXT_IN_IO, GPIO_MODE_INPUT);
    TEST_ESP_OK(gpio_pullup_en(TEST_GPIO_EXT_IN_IO));  // pull up first
    vTaskDelay(100 / portTICK_RATE_MS);
    TEST_ASSERT_EQUAL_INT_MESSAGE(gpio_get_level(TEST_GPIO_EXT_IN_IO), 1, "gpio_pullup_en error, it can't pull up");
    TEST_ESP_OK(gpio_pulldown_dis(TEST_GPIO_EXT_IN_IO)); //can't be pull down
    vTaskDelay(100 / portTICK_RATE_MS);
    TEST_ASSERT_EQUAL_INT_MESSAGE(gpio_get_level(TEST_GPIO_EXT_IN_IO), 1, "gpio_pulldown_dis error, it can pull down");
    TEST_ESP_OK(gpio_pulldown_en(TEST_GPIO_EXT_IN_IO)); // can be pull down now
    vTaskDelay(100 / portTICK_RATE_MS);
    TEST_ASSERT_EQUAL_INT_MESSAGE(gpio_get_level(TEST_GPIO_EXT_IN_IO), 0, "gpio_pulldown_en error, it can't pull down");
    TEST_ESP_OK(gpio_pullup_dis(TEST_GPIO_EXT_IN_IO)); // can't be pull up
    vTaskDelay(100 / portTICK_RATE_MS);
    TEST_ASSERT_EQUAL_INT_MESSAGE(gpio_get_level(TEST_GPIO_EXT_IN_IO), 0, "gpio_pullup_dis error, it can pull up");
}

#if !TEMPORARY_DISABLED_FOR_TARGETS(ESP32S2, ESP32S3, ESP32C3)
//No runners
TEST_CASE("GPIO output and input mode test", "[gpio][test_env=UT_T1_GPIO]")
{
    //ESP32 connect io18 and io19, ESP32-S2 connect io17 and io21, ESP32-S3 connect io17 and io21, ESP32C3 Connect GPIO2 with GPIO3
    gpio_config_t output_io = init_io(TEST_GPIO_EXT_OUT_IO);
    gpio_config_t input_io = init_io(TEST_GPIO_EXT_IN_IO);
    gpio_config(&output_io);
    gpio_config(&input_io);
    int level = gpio_get_level(TEST_GPIO_EXT_IN_IO);

    //disable mode
    gpio_set_direction(TEST_GPIO_EXT_OUT_IO, GPIO_MODE_DISABLE);
    gpio_set_direction(TEST_GPIO_EXT_IN_IO, GPIO_MODE_OUTPUT);
    gpio_set_level(TEST_GPIO_EXT_OUT_IO, !level);
    TEST_ASSERT_EQUAL_INT_MESSAGE(gpio_get_level(TEST_GPIO_EXT_IN_IO), level, "direction GPIO_MODE_DISABLE set error, it can output");

    //input mode and output mode
    gpio_set_direction(TEST_GPIO_EXT_OUT_IO, GPIO_MODE_OUTPUT);
    gpio_set_direction(TEST_GPIO_EXT_IN_IO, GPIO_MODE_INPUT);
    gpio_set_level(TEST_GPIO_EXT_OUT_IO, 1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(gpio_get_level(TEST_GPIO_EXT_IN_IO), 1, "direction GPIO_MODE_OUTPUT set error, it can't output");
    gpio_set_level(TEST_GPIO_EXT_OUT_IO, 0);
    TEST_ASSERT_EQUAL_INT_MESSAGE(gpio_get_level(TEST_GPIO_EXT_IN_IO), 0, "direction GPIO_MODE_OUTPUT set error, it can't output");

    // open drain mode(output), can just output low level
    gpio_set_direction(TEST_GPIO_EXT_OUT_IO, GPIO_MODE_OUTPUT_OD);
    gpio_set_direction(TEST_GPIO_EXT_IN_IO, GPIO_MODE_INPUT);
    gpio_set_level(TEST_GPIO_EXT_OUT_IO, 1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(gpio_get_level(TEST_GPIO_EXT_IN_IO), 0, "direction GPIO_MODE_OUTPUT set error, it can't output");
    gpio_set_level(TEST_GPIO_EXT_OUT_IO, 0);
    TEST_ASSERT_EQUAL_INT_MESSAGE(gpio_get_level(TEST_GPIO_EXT_IN_IO), 0, "direction GPIO_MODE_OUTPUT set error, it can't output");

    // open drain mode(output and input), can just output low level
    // output test
    gpio_set_direction(TEST_GPIO_EXT_OUT_IO, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_direction(TEST_GPIO_EXT_IN_IO, GPIO_MODE_INPUT);
    gpio_set_level(TEST_GPIO_EXT_OUT_IO, 1);
    TEST_ASSERT_EQUAL_INT_MESSAGE(gpio_get_level(TEST_GPIO_EXT_IN_IO), 0, "direction GPIO_MODE_OUTPUT set error, it can't output");
    gpio_set_level(TEST_GPIO_EXT_OUT_IO, 0);
    TEST_ASSERT_EQUAL_INT_MESSAGE(gpio_get_level(TEST_GPIO_EXT_IN_IO), 0, "direction GPIO_MODE_OUTPUT set error, it can't output");

    // GPIO_MODE_INPUT_OUTPUT mode
    // output test
    level = gpio_get_level(TEST_GPIO_EXT_IN_IO);
    gpio_set_direction(TEST_GPIO_EXT_OUT_IO, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_direction(TEST_GPIO_EXT_IN_IO, GPIO_MODE_INPUT);
    gpio_set_level(TEST_GPIO_EXT_OUT_IO, !level);
    TEST_ASSERT_EQUAL_INT_MESSAGE(gpio_get_level(TEST_GPIO_EXT_IN_IO), !level, "direction set error, it can't output");
}

TEST_CASE("GPIO repeate call service and isr has no memory leak test", "[gpio][test_env=UT_T1_GPIO][timeout=90]")
{
    gpio_config_t output_io = init_io(TEST_GPIO_EXT_OUT_IO);
    gpio_config_t input_io = init_io(TEST_GPIO_EXT_IN_IO);
    input_io.intr_type = GPIO_INTR_POSEDGE;
    input_io.mode = GPIO_MODE_INPUT;
    input_io.pull_up_en = 1;
    TEST_ESP_OK(gpio_config(&output_io));
    TEST_ESP_OK(gpio_config(&input_io));
    TEST_ESP_OK(gpio_set_level(TEST_GPIO_EXT_OUT_IO, 0));
    //rising edge
    uint32_t size = esp_get_free_heap_size();
    for (int i = 0; i < 1000; i++) {
        TEST_ESP_OK(gpio_set_intr_type(TEST_GPIO_EXT_IN_IO, GPIO_INTR_POSEDGE));
        TEST_ESP_OK(gpio_install_isr_service(0));
        TEST_ESP_OK(gpio_isr_handler_add(TEST_GPIO_EXT_IN_IO, gpio_isr_edge_handler, (void *)TEST_GPIO_EXT_IN_IO));
        gpio_set_level(TEST_GPIO_EXT_OUT_IO, 1);
        TEST_ESP_OK(gpio_isr_handler_remove(TEST_GPIO_EXT_IN_IO));
        gpio_set_level(TEST_GPIO_EXT_OUT_IO, 0);
        gpio_uninstall_isr_service();
    }
    TEST_ASSERT_INT32_WITHIN(size, esp_get_free_heap_size(), 100);
}
#endif //DISABLED_FOR_TARGETS(ESP32S2, ESP32S3, ESP32C3)

#if !WAKE_UP_IGNORE
//this function development is not completed yet, set it ignored
TEST_CASE("GPIO wake up enable and disenable test", "[gpio][ignore]")
{
    xTaskCreate(sleep_wake_up, "sleep_wake_up", 4096, NULL, 5, NULL);
    xTaskCreate(trigger_wake_up, "trigger_wake_up", 4096, NULL, 5, NULL);
    vTaskDelay(100 / portTICK_RATE_MS);
    TEST_ASSERT_TRUE(wake_up_result);

    wake_up_result = false;
    TEST_ESP_OK(gpio_wakeup_disable(TEST_GPIO_EXT_IN_IO));
    gpio_set_level(TEST_GPIO_EXT_OUT_IO, 1);
    vTaskDelay(100 / portTICK_RATE_MS);
    TEST_ASSERT_FALSE(wake_up_result);
}
#endif // !WAKE_UP_IGNORE

// this case need the resistance to pull up the voltage or pull down the voltage
// ignored because the voltage needs to be tested with multimeter
TEST_CASE("GPIO verify only the gpio with input ability can be set pull/down", "[gpio][ignore]")
{
    gpio_config_t  output_io = init_io(TEST_GPIO_EXT_OUT_IO);
    gpio_config_t  input_io = init_io(TEST_GPIO_EXT_IN_IO);
    gpio_config(&output_io);
    input_io.mode = GPIO_MODE_INPUT;
    gpio_config(&input_io);

    printf("pull up test!\n");
    // pull up test
    gpio_set_direction(TEST_GPIO_EXT_OUT_IO, GPIO_MODE_OUTPUT);
    TEST_ESP_OK(gpio_set_pull_mode(TEST_GPIO_EXT_OUT_IO, GPIO_PULLUP_ONLY));
    prompt_to_continue("mode: GPIO_MODE_OUTPUT");

    gpio_set_direction(TEST_GPIO_EXT_OUT_IO, GPIO_MODE_OUTPUT_OD);
    TEST_ESP_OK(gpio_set_pull_mode(TEST_GPIO_EXT_OUT_IO, GPIO_PULLUP_ONLY));

    // open drain just can output low level
    gpio_set_direction(TEST_GPIO_EXT_OUT_IO, GPIO_MODE_INPUT_OUTPUT_OD);
    TEST_ESP_OK(gpio_set_pull_mode(TEST_GPIO_EXT_OUT_IO, GPIO_PULLUP_ONLY));
    prompt_to_continue("mode: GPIO_MODE_OUTPUT_OD");

    gpio_set_direction(TEST_GPIO_EXT_OUT_IO, GPIO_MODE_INPUT_OUTPUT);
    TEST_ESP_OK(gpio_set_pull_mode(TEST_GPIO_EXT_OUT_IO, GPIO_PULLUP_ONLY));
    prompt_to_continue("mode: GPIO_MODE_INPUT_OUTPUT");

    gpio_set_direction(TEST_GPIO_EXT_OUT_IO, GPIO_MODE_INPUT);
    TEST_ESP_OK(gpio_set_pull_mode(TEST_GPIO_EXT_OUT_IO, GPIO_PULLUP_ONLY));
    prompt_to_continue("mode: GPIO_MODE_INPUT");

    // after pull up the level is high now
    // pull down test
    printf("pull down test!\n");

    gpio_set_direction(TEST_GPIO_EXT_OUT_IO, GPIO_MODE_OUTPUT);
    TEST_ESP_OK(gpio_set_pull_mode(TEST_GPIO_EXT_OUT_IO, GPIO_PULLDOWN_ONLY));
    prompt_to_continue("mode: GPIO_MODE_OUTPUT");

    gpio_set_direction(TEST_GPIO_EXT_OUT_IO, GPIO_MODE_OUTPUT_OD);
    TEST_ESP_OK(gpio_set_pull_mode(TEST_GPIO_EXT_OUT_IO, GPIO_PULLDOWN_ONLY));
    prompt_to_continue("mode: GPIO_MODE_OUTPUT_OD");

    gpio_set_direction(TEST_GPIO_EXT_OUT_IO, GPIO_MODE_INPUT_OUTPUT_OD);
    TEST_ESP_OK(gpio_set_pull_mode(TEST_GPIO_EXT_OUT_IO, GPIO_PULLDOWN_ONLY));
    prompt_to_continue("mode: GPIO_MODE_INPUT_OUTPUT_OD");

    gpio_set_direction(TEST_GPIO_EXT_OUT_IO, GPIO_MODE_INPUT_OUTPUT);
    TEST_ESP_OK(gpio_set_pull_mode(TEST_GPIO_EXT_OUT_IO, GPIO_PULLDOWN_ONLY));
    prompt_to_continue("mode: GPIO_MODE_INPUT_OUTPUT");

    gpio_set_direction(TEST_GPIO_EXT_OUT_IO, GPIO_MODE_INPUT);
    TEST_ESP_OK(gpio_set_pull_mode(TEST_GPIO_EXT_OUT_IO, GPIO_PULLDOWN_ONLY));
    prompt_to_continue("mode: GPIO_MODE_INPUT");
}

/**
 * There are 5 situation for the GPIO drive capability:
 * 1. GPIO drive weak capability test
 * 2. GPIO drive stronger capability test
 * 3. GPIO drive default capability test
 * 4. GPIO drive default capability test2
 * 5. GPIO drive strongest capability test
 *
 * How to test:
 * when testing, use the sliding resistor and a multimeter
 * adjust the resistor from low to high, 0-10k
 * watch the current change
 * the current test result:
 * weak capability: (0.32-10.1)mA
 * stronger capability: (0.32-20.0)mA
 * default capability: (0.33-39.8)mA
 * default capability2: (0.33-39.9)mA
 * strongest capability: (0.33-64.2)mA
 *
 * the data shows:
 * weak capability<stronger capability<default capability=default capability2<strongest capability
 *
 * all of these cases should be ignored that it will not run in CI
 */

// drive capability test
TEST_CASE("GPIO drive capability test", "[gpio][ignore]")
{
    printf("weak capability test! please view the current change!\n");
    drive_capability_set_get(TEST_GPIO_EXT_OUT_IO, GPIO_DRIVE_CAP_0);
    prompt_to_continue("If this test finishes");

    printf("stronger capability test! please view the current change!\n");
    drive_capability_set_get(TEST_GPIO_EXT_OUT_IO, GPIO_DRIVE_CAP_1);
    prompt_to_continue("If this test finishes");

    printf("default capability test! please view the current change!\n");
    drive_capability_set_get(TEST_GPIO_EXT_OUT_IO, GPIO_DRIVE_CAP_2);
    prompt_to_continue("If this test finishes");

    printf("default capability2 test! please view the current change!\n");
    drive_capability_set_get(TEST_GPIO_EXT_OUT_IO, GPIO_DRIVE_CAP_DEFAULT);
    prompt_to_continue("If this test finishes");

    printf("strongest capability test! please view the current change!\n");
    drive_capability_set_get(TEST_GPIO_EXT_OUT_IO, GPIO_DRIVE_CAP_3);
    prompt_to_continue("If this test finishes");
}

#if !CONFIG_FREERTOS_UNICORE
void gpio_enable_task(void *param)
{
    int gpio_num = (int)param;
    TEST_ESP_OK(gpio_intr_enable(gpio_num));
    vTaskDelete(NULL);
}

/** Test the GPIO Interrupt Enable API with dual core enabled. The GPIO ISR service routine is registered on one core.
 * When the GPIO interrupt on another core is enabled, the GPIO interrupt will be lost.
 * First on the core 0, Do the following steps:
 *     1. Configure the GPIO9 input_output mode, and enable the rising edge interrupt mode.
 *     2. Trigger the GPIO9 interrupt and check if the interrupt responds correctly.
 *     3. Disable the GPIO9 interrupt
 * Then on the core 1, Do the following steps:
 *     1. Enable the GPIO9 interrupt again.
 *     2. Trigger the GPIO9 interrupt and check if the interrupt responds correctly.
 *
 */
TEST_CASE("GPIO Enable/Disable interrupt on multiple cores", "[gpio][ignore]")
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.mode = GPIO_MODE_INPUT_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << TEST_IO_9);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    TEST_ESP_OK(gpio_config(&io_conf));
    TEST_ESP_OK(gpio_set_level(TEST_IO_9, 0));
    TEST_ESP_OK(gpio_install_isr_service(0));
    TEST_ESP_OK(gpio_isr_handler_add(TEST_IO_9, gpio_isr_edge_handler, (void *) TEST_IO_9));
    vTaskDelay(1000 / portTICK_RATE_MS);
    TEST_ESP_OK(gpio_set_level(TEST_IO_9, 1));
    vTaskDelay(100 / portTICK_RATE_MS);
    TEST_ESP_OK(gpio_set_level(TEST_IO_9, 0));
    vTaskDelay(100 / portTICK_RATE_MS);
    TEST_ESP_OK(gpio_intr_disable(TEST_IO_9));
    TEST_ASSERT(edge_intr_times == 1);
    xTaskCreatePinnedToCore(gpio_enable_task, "gpio_enable_task", 1024 * 4, (void *)TEST_IO_9, 8, NULL, (xPortGetCoreID() == 0));
    vTaskDelay(1000 / portTICK_RATE_MS);
    TEST_ESP_OK(gpio_set_level(TEST_IO_9, 1));
    vTaskDelay(100 / portTICK_RATE_MS);
    TEST_ESP_OK(gpio_set_level(TEST_IO_9, 0));
    vTaskDelay(100 / portTICK_RATE_MS);
    TEST_ESP_OK(gpio_intr_disable(TEST_IO_9));
    TEST_ESP_OK(gpio_isr_handler_remove(TEST_IO_9));
    gpio_uninstall_isr_service();
    TEST_ASSERT(edge_intr_times == 2);
}
#endif //!CONFIG_FREERTOS_UNICORE

typedef struct {
    int gpio_num;
    int isr_cnt;
} gpio_isr_param_t;

static void gpio_isr_handler(void *arg)
{
    gpio_isr_param_t *param = (gpio_isr_param_t *)arg;
    esp_rom_printf("GPIO[%d] intr, val: %d\n", param->gpio_num, gpio_get_level(param->gpio_num));
    param->isr_cnt++;
}

/** The previous GPIO interrupt service routine polls the interrupt raw status register to find the GPIO that triggered the interrupt.
 * But this will incorrectly handle the interrupt disabled GPIOs, because the raw interrupt status register can still be set when
 * the trigger signal arrives, even if the interrupt is disabled.
 * First on the core 0:
 *     1. Configure the GPIO9 and GPIO10(ESP32, ESP32C3)/GPIO21(ESP32-S2) input_output mode.
 *     2. Enable GPIO9 dual edge triggered interrupt, enable GPIO10(ESP32, ESP32C3)/GPIO21(ESP32-S2) falling edge triggered interrupt.
 *     3. Trigger GPIO9 interrupt, than disable the GPIO18 interrupt, and than trigger GPIO18 again(This time will not respond to the interrupt).
 *     4. Trigger GPIO10(ESP32, ESP32C3)/GPIO21(ESP32-S2) interrupt.
 * If the bug is not fixed, you will see, in the step 4, the interrupt of GPIO9 will also respond.
 */
TEST_CASE("GPIO ISR service test", "[gpio][ignore]")
{
    static gpio_isr_param_t io9_param = {
        .gpio_num =  TEST_IO_9,
        .isr_cnt = 0,
    };
    static gpio_isr_param_t io10_param = {
        .gpio_num =  TEST_IO_10,
        .isr_cnt = 0,
    };
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << TEST_IO_9) | (1ULL << TEST_IO_10);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    TEST_ESP_OK(gpio_config(&io_conf));
    TEST_ESP_OK(gpio_set_level(TEST_IO_9, 0));
    TEST_ESP_OK(gpio_set_level(TEST_IO_10, 0));
    TEST_ESP_OK(gpio_install_isr_service(0));
    TEST_ESP_OK(gpio_set_intr_type(TEST_IO_9, GPIO_INTR_ANYEDGE));
    TEST_ESP_OK(gpio_set_intr_type(TEST_IO_10, GPIO_INTR_NEGEDGE));
    TEST_ESP_OK(gpio_isr_handler_add(TEST_IO_9, gpio_isr_handler, (void *)&io9_param));
    TEST_ESP_OK(gpio_isr_handler_add(TEST_IO_10, gpio_isr_handler, (void *)&io10_param));
    printf("Triggering the interrupt of GPIO9\n");
    vTaskDelay(1000 / portTICK_RATE_MS);
    //Rising edge
    TEST_ESP_OK(gpio_set_level(TEST_IO_9, 1));
    printf("Disable the interrupt of GPIO9\n");
    vTaskDelay(100 / portTICK_RATE_MS);
    //Disable GPIO9 interrupt, GPIO18 will not respond to the next falling edge interrupt.
    TEST_ESP_OK(gpio_intr_disable(TEST_IO_9));
    vTaskDelay(100 / portTICK_RATE_MS);
    //Falling edge
    TEST_ESP_OK(gpio_set_level(TEST_IO_9, 0));

    printf("Triggering the interrupt of GPIO10\n");
    vTaskDelay(100 / portTICK_RATE_MS);
    TEST_ESP_OK(gpio_set_level(TEST_IO_10, 1));
    vTaskDelay(100 / portTICK_RATE_MS);
    //Falling edge
    TEST_ESP_OK(gpio_set_level(TEST_IO_10, 0));
    vTaskDelay(100 / portTICK_RATE_MS);
    TEST_ESP_OK(gpio_isr_handler_remove(TEST_IO_9));
    TEST_ESP_OK(gpio_isr_handler_remove(TEST_IO_10));
    gpio_uninstall_isr_service();
    TEST_ASSERT((io9_param.isr_cnt == 1) && (io10_param.isr_cnt == 1));
}

#if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32C3
TEST_CASE("GPIO input and output of USB pins test", "[gpio]")
{
    const int test_pins[] = {TEST_GPIO_USB_DP_IO, TEST_GPIO_USB_DM_IO};
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .pin_bit_mask = (BIT64(test_pins[0]) | BIT64(test_pins[1])),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&io_conf);

    for (int i = 0; i < sizeof(test_pins) / sizeof(int); i++) {
        int pin = test_pins[i];
        // test pin
        gpio_set_level(pin, 0);
        // tested voltage is around 0v
        esp_rom_delay_us(10);
        TEST_ASSERT_EQUAL_INT_MESSAGE(gpio_get_level(pin), 0, "get level error! the level should be low!");
        vTaskDelay(1000 / portTICK_RATE_MS);
        gpio_set_level(pin, 1);
        esp_rom_delay_us(10);
        // tested voltage is around 3.3v
        TEST_ASSERT_EQUAL_INT_MESSAGE(gpio_get_level(pin), 1, "get level error! the level should be high!");
        vTaskDelay(1000 / portTICK_RATE_MS);
        gpio_set_level(pin, 0);
        esp_rom_delay_us(10);
        // tested voltage is around 0v
        TEST_ASSERT_EQUAL_INT_MESSAGE(gpio_get_level(pin), 0, "get level error! the level should be low!");
        vTaskDelay(1000 / portTICK_RATE_MS);
        gpio_set_level(pin, 1);
        esp_rom_delay_us(10);
        // tested voltage is around 3.3v
        TEST_ASSERT_EQUAL_INT_MESSAGE(gpio_get_level(pin), 1, "get level error! the level should be high!");
    }
}
#endif //CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32C3

/*
 * SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
// The long term plan is to have a single soc_caps.h for each peripheral.
// During the refactoring and multichip support development process, we
// seperate these information into periph_caps.h for each peripheral and
// include them here.

#pragma once

#define SOC_CPU_CORES_NUM               1
#define SOC_GDMA_SUPPORTED              1
#define SOC_BT_SUPPORTED                1
#define SOC_DIG_SIGN_SUPPORTED          1
#define SOC_HMAC_SUPPORTED              1
#define SOC_ASYNC_MEMCPY_SUPPORTED      1

/*-------------------------- COMMON CAPS ---------------------------------------*/
#define SOC_SUPPORTS_SECURE_DL_MODE     1
#define SOC_EFUSE_SECURE_BOOT_KEY_DIGESTS 3
#define SOC_EFUSE_REVOKE_BOOT_KEY_DIGESTS 1
#define SOC_RTC_FAST_MEM_SUPPORTED      0
#define SOC_RTC_SLOW_MEM_SUPPORTED      0
#define SOC_SUPPORT_SECURE_BOOT_REVOKE_KEY             0
#define SOC_ICACHE_ACCESS_RODATA_SUPPORTED 1

/*-------------------------- AES CAPS -----------------------------------------*/
#define SOC_AES_SUPPORT_DMA     (1)

/* Has a centralized DMA, which is shared with all peripherals */
#define SOC_AES_GDMA            (1)

/*-------------------------- ADC CAPS -------------------------------*/
#define SOC_ADC_PERIPH_NUM                      (2)
#define SOC_ADC_PATT_LEN_MAX                    (16)
#define SOC_ADC_CHANNEL_NUM(PERIPH_NUM)         ((PERIPH_NUM==0)? 5 : 1)
#define SOC_ADC_MAX_CHANNEL_NUM                 (5)
#define SOC_ADC_MAX_BITWIDTH                    (12)
#define SOC_ADC_CALIBRATION_V1_SUPPORTED        (1) /*!< support HW offset calibration version 1*/
#define SOC_ADC_DIGI_FILTER_NUM                 (2)
#define SOC_ADC_DIGI_MONITOR_NUM                (2)
#define SOC_ADC_HW_CALIBRATION_V1               (1) /*!< support HW offset calibration */
#define SOC_ADC_SUPPORT_DMA_MODE(PERIPH_NUM)    1
//F_sample = F_digi_con / 2 / interval. F_digi_con = 5M for now. 30 <= interva <= 4095
#define SOC_ADC_SAMPLE_FREQ_THRES_HIGH          83333
#define SOC_ADC_SAMPLE_FREQ_THRES_LOW           611

/*-------------------------- APB BACKUP DMA CAPS -------------------------------*/
#define SOC_APB_BACKUP_DMA              (1)

/*-------------------------- BROWNOUT CAPS -----------------------------------*/
#define SOC_BROWNOUT_RESET_SUPPORTED 1

/*-------------------------- CPU CAPS ----------------------------------------*/
#define SOC_CPU_BREAKPOINTS_NUM         2
#define SOC_CPU_WATCHPOINTS_NUM         2
#define SOC_CPU_HAS_FLEXIBLE_INTC       1

#define SOC_CPU_WATCHPOINT_SIZE         0x80000000 // bytes

/*-------------------------- DIGITAL SIGNATURE CAPS ----------------------------------------*/
/** The maximum length of a Digital Signature in bits. */
#define SOC_DS_SIGNATURE_MAX_BIT_LEN (3072)

/** Initialization vector (IV) length for the RSA key parameter message digest (MD) in bytes. */
#define SOC_DS_KEY_PARAM_MD_IV_LENGTH (16)


/** Maximum wait time for DS parameter decryption key. If overdue, then key error.
    See TRM DS chapter for more details */
#define SOC_DS_KEY_CHECK_MAX_WAIT_US (1100)

/*-------------------------- GDMA CAPS -------------------------------------*/
#define SOC_GDMA_GROUPS                 (1U) // Number of GDMA groups
#define SOC_GDMA_PAIRS_PER_GROUP        (3)  // Number of GDMA pairs in each group
#define SOC_GDMA_TX_RX_SHARE_INTERRUPT  (1)  // TX and RX channel in the same pair will share the same interrupt source number

/*-------------------------- GPIO CAPS ---------------------------------------*/
// ESP8684 has 1 GPIO peripheral
#define SOC_GPIO_PORT               (1U)
#define SOC_GPIO_PIN_COUNT          (21)

// Target has no full RTC IO subsystem, so GPIO is 100% "independent" of RTC
// On ESP8684, Digital IOs have their own registers to control pullup/down capability, independent of RTC registers.
#define SOC_GPIO_SUPPORTS_RTC_INDEPENDENT   (1)
// Force hold is a new function of ESP8684
#define SOC_GPIO_SUPPORT_FORCE_HOLD         (1)
// GPIO0~5 on ESP8684 can support chip deep sleep wakeup
#define SOC_GPIO_SUPPORT_DEEPSLEEP_WAKEUP   (1)

// GPIO19 on ESP8684 is invalid.
#define SOC_GPIO_VALID_GPIO_MASK        (((1U<<SOC_GPIO_PIN_COUNT) - 1) & (~(BIT19)))
#define SOC_GPIO_VALID_OUTPUT_GPIO_MASK SOC_GPIO_VALID_GPIO_MASK
#define SOC_GPIO_DEEP_SLEEP_WAKE_VALID_GPIO_MASK        (0ULL | BIT0 | BIT1 | BIT2 | BIT3 | BIT4 | BIT5)

// Support to configure sleep status
#define SOC_GPIO_SUPPORT_SLP_SWITCH  (1)

/*-------------------------- I2C CAPS ----------------------------------------*/
// TODO IDF-3918
#define SOC_I2C_NUM                 (1U)

#define SOC_I2C_FIFO_LEN            (32) /*!< I2C hardware FIFO depth */

#define SOC_I2C_SUPPORT_HW_FSM_RST  (1)
#define SOC_I2C_SUPPORT_HW_CLR_BUS  (1)

#define SOC_I2C_SUPPORT_XTAL        (1)
#define SOC_I2C_SUPPORT_RTC         (1)

/*-------------------------- I2S CAPS ----------------------------------------*/
// TODO IDF-3896

/*-------------------------- LEDC CAPS ---------------------------------------*/
#define SOC_LEDC_SUPPORT_XTAL_CLOCK  (1)
#define SOC_LEDC_CHANNEL_NUM         (6)
#define SOC_LEDC_TIMER_BIT_WIDE_NUM  (14)

/*-------------------------- MPU CAPS ----------------------------------------*/
#define SOC_MPU_CONFIGURABLE_REGIONS_SUPPORTED    0
#define SOC_MPU_MIN_REGION_SIZE                   0x20000000U
#define SOC_MPU_REGIONS_MAX_NUM                   8
#define SOC_MPU_REGION_RO_SUPPORTED               0
#define SOC_MPU_REGION_WO_SUPPORTED               0

/*--------------------------- RMT CAPS ---------------------------------------*/
#define SOC_RMT_GROUPS                  (1U) /*!< One RMT group */
#define SOC_RMT_TX_CANDIDATES_PER_GROUP (2)  /*!< Number of channels that capable of Transmit */
#define SOC_RMT_RX_CANDIDATES_PER_GROUP (2)  /*!< Number of channels that capable of Receive */
#define SOC_RMT_CHANNELS_PER_GROUP      (4)  /*!< Total 4 channels */
#define SOC_RMT_MEM_WORDS_PER_CHANNEL   (48) /*!< Each channel owns 48 words memory (1 word = 4 Bytes) */
#define SOC_RMT_SUPPORT_RX_PINGPONG     (1)  /*!< Support Ping-Pong mode on RX path */
#define SOC_RMT_SUPPORT_RX_DEMODULATION (1)  /*!< Support signal demodulation on RX path (i.e. remove carrier) */
#define SOC_RMT_SUPPORT_TX_LOOP_COUNT   (1)  /*!< Support transmit specified number of cycles in loop mode */
#define SOC_RMT_SUPPORT_TX_SYNCHRO      (1)  /*!< Support coordinate a group of TX channels to start simultaneously */
#define SOC_RMT_SUPPORT_XTAL            (1)  /*!< Support set XTAL clock as the RMT clock source */

/*-------------------------- RTC CAPS --------------------------------------*/
#define SOC_RTC_CNTL_CPU_PD_DMA_BUS_WIDTH       (128)
#define SOC_RTC_CNTL_CPU_PD_REG_FILE_NUM        (108)
#define SOC_RTC_CNTL_CPU_PD_DMA_ADDR_ALIGN      (SOC_RTC_CNTL_CPU_PD_DMA_BUS_WIDTH >> 3)
#define SOC_RTC_CNTL_CPU_PD_DMA_BLOCK_SIZE      (SOC_RTC_CNTL_CPU_PD_DMA_BUS_WIDTH >> 3)

#define SOC_RTC_CNTL_CPU_PD_RETENTION_MEM_SIZE  (SOC_RTC_CNTL_CPU_PD_REG_FILE_NUM * (SOC_RTC_CNTL_CPU_PD_DMA_BUS_WIDTH >> 3))

/*--------------------------- RSA CAPS ---------------------------------------*/
#define SOC_RSA_MAX_BIT_LEN    (3072)

/*--------------------------- SHA CAPS ---------------------------------------*/

/* Max amount of bytes in a single DMA operation is 4095,
   for SHA this means that the biggest safe amount of bytes is
   31 blocks of 128 bytes = 3968
*/
#define SOC_SHA_DMA_MAX_BUFFER_SIZE     (3968)
#define SOC_SHA_SUPPORT_DMA             (1)

/* The SHA engine is able to resume hashing from a user */
#define SOC_SHA_SUPPORT_RESUME          (1)

/* Has a centralized DMA, which is shared with all peripherals */
#define SOC_SHA_GDMA             (1)

/* Supported HW algorithms */
#define SOC_SHA_SUPPORT_SHA1            (1)
#define SOC_SHA_SUPPORT_SHA224          (1)
#define SOC_SHA_SUPPORT_SHA256          (1)

/*-------------------------- SIGMA DELTA CAPS --------------------------------*/
#define SOC_SIGMADELTA_NUM         (1U) // 1 sigma-delta peripheral
#define SOC_SIGMADELTA_CHANNEL_NUM (4)  // 4 channels

/*-------------------------- SPI CAPS ----------------------------------------*/
#define SOC_SPI_PERIPH_NUM          2
#define SOC_SPI_PERIPH_CS_NUM(i)    6

#define SOC_SPI_MAXIMUM_BUFFER_SIZE     64

#define SOC_SPI_SUPPORT_DDRCLK              1
#define SOC_SPI_SLAVE_SUPPORT_SEG_TRANS     1
#define SOC_SPI_SUPPORT_CD_SIG              1
#define SOC_SPI_SUPPORT_CONTINUOUS_TRANS    1
#define SOC_SPI_SUPPORT_SLAVE_HD_VER2       1

// Peripheral supports DIO, DOUT, QIO, or QOUT
// host_id = 0 -> SPI0/SPI1, host_id = 1 -> SPI2,
#define SOC_SPI_PERIPH_SUPPORT_MULTILINE_MODE(host_id)  ({(void)host_id; 1;})

// Peripheral supports output given level during its "dummy phase"
#define SOC_SPI_PERIPH_SUPPORT_CONTROL_DUMMY_OUT 1

#define SOC_MEMSPI_IS_INDEPENDENT 1
#define SOC_SPI_MAX_PRE_DIVIDER 16

/*-------------------------- SPI MEM CAPS ---------------------------------------*/
#define SOC_SPI_MEM_SUPPORT_AUTO_WAIT_IDLE                (1)
#define SOC_SPI_MEM_SUPPORT_AUTO_SUSPEND                  (1)
#define SOC_SPI_MEM_SUPPORT_AUTO_RESUME                   (1)
#define SOC_SPI_MEM_SUPPORT_IDLE_INTR                     (1)
#define SOC_SPI_MEM_SUPPORT_SW_SUSPEND                    (1)
#define SOC_SPI_MEM_SUPPORT_CHECK_SUS                     (1)


/*-------------------------- SYSTIMER CAPS ----------------------------------*/
#define SOC_SYSTIMER_COUNTER_NUM           (2)  // Number of counter units
#define SOC_SYSTIMER_ALARM_NUM             (3)  // Number of alarm units
#define SOC_SYSTIMER_BIT_WIDTH_LO          (32) // Bit width of systimer low part
#define SOC_SYSTIMER_BIT_WIDTH_HI          (20) // Bit width of systimer high part
#define SOC_SYSTIMER_FIXED_TICKS_US        (16) // Number of ticks per microsecond is fixed
#define SOC_SYSTIMER_INT_LEVEL             (1)  // Systimer peripheral uses level interrupt
#define SOC_SYSTIMER_ALARM_MISS_COMPENSATE (1)  // Systimer peripheral can generate interrupt immediately if t(target) > t(current)

/*--------------------------- TIMER GROUP CAPS ---------------------------------------*/
#define SOC_TIMER_GROUPS                  (1U)
#define SOC_TIMER_GROUP_TIMERS_PER_GROUP  (1U)
#define SOC_TIMER_GROUP_COUNTER_BIT_WIDTH (54)
#define SOC_TIMER_GROUP_SUPPORT_XTAL      (1)
#define SOC_TIMER_GROUP_TOTAL_TIMERS (SOC_TIMER_GROUPS * SOC_TIMER_GROUP_TIMERS_PER_GROUP)

/*-------------------------- TOUCH SENSOR CAPS -------------------------------*/
#define SOC_TOUCH_SENSOR_NUM            (0U)    /*! No touch sensors on ESP8684 */

/*-------------------------- TWAI CAPS ---------------------------------------*/
// TODO IDF-3897

/*-------------------------- Flash Encryption CAPS----------------------------*/
#define SOC_FLASH_ENCRYPTED_XTS_AES_BLOCK_MAX   (32)

/*-------------------------- UART CAPS ---------------------------------------*/
// ESP8684 has 2 UARTs
#define SOC_UART_NUM                (2)

#define SOC_UART_FIFO_LEN           (128)      /*!< The UART hardware FIFO length */
#define SOC_UART_BITRATE_MAX        (5000000)  /*!< Max bit rate supported by UART */

#define SOC_UART_SUPPORT_RTC_CLK    (1)
#define SOC_UART_SUPPORT_XTAL_CLK   (1)

// UART has an extra TX_WAIT_SEND state when the FIFO is not empty and XOFF is enabled
#define SOC_UART_SUPPORT_FSM_TX_WAIT_SEND   (1)

/*-------------------------- WI-FI HARDWARE TSF CAPS -------------------------------*/
#define SOC_WIFI_HW_TSF                 (1)

/*-------------------------- COEXISTENCE HARDWARE PTI CAPS -------------------------------*/
#define SOC_COEX_HW_PTI                 (1)

/*--------------- PHY REGISTER AND MEMORY SIZE CAPS --------------------------*/
#define SOC_PHY_DIG_REGS_MEM_SIZE       (21*4)
#define SOC_MAC_BB_PD_MEM_SIZE          (192*4)

/*--------------- WIFI LIGHT SLEEP CLOCK WIDTH CAPS --------------------------*/
#define SOC_WIFI_LIGHT_SLEEP_CLK_WIDTH  (12)

/*-------------------------- Power Management CAPS ----------------------------*/
#define SOC_PM_SUPPORT_WIFI_WAKEUP      (1)

#define SOC_PM_SUPPORT_BT_WAKEUP        (1)

#define SOC_PM_SUPPORT_CPU_PD           (1)

#define SOC_PM_SUPPORT_WIFI_PD          (1)

#define SOC_PM_SUPPORT_BT_PD            (1)

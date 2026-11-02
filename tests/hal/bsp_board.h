/* Narrow HAL model for compiling the production QSPI BSP on the host. */
#ifndef TEST_BSP_BOARD_H
#define TEST_BSP_BOARD_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define __weak __attribute__((weak))
typedef int HAL_StatusTypeDef;
#define HAL_OK 0
#define HAL_ERROR 1
typedef struct { uint32_t Pin,Mode,Pull,Speed,Alternate; } GPIO_InitTypeDef;
typedef struct { uint32_t PeriphClockSelection,QspiClockSelection; } RCC_PeriphCLKInitTypeDef;
typedef struct {
    uint32_t InstructionMode,Instruction,AddressMode,AddressSize,Address;
    uint32_t AlternateByteMode,DataMode,NbData,DummyCycles,DdrMode,DdrHoldHalfCycle,SIOOMode;
} QSPI_CommandTypeDef;
typedef struct {
    void *Instance;
    struct { uint32_t ClockPrescaler,FifoThreshold,SampleShifting,FlashSize;
        uint32_t ChipSelectHighTime,ClockMode,FlashID,DualFlash; } Init;
} QSPI_HandleTypeDef;
#define GPIOF ((void *)6)
#define GPIOG ((void *)7)
#define QUADSPI ((void *)8)
#define GPIO_PIN_6 (1U<<6)
#define GPIO_PIN_7 (1U<<7)
#define GPIO_PIN_8 (1U<<8)
#define GPIO_PIN_9 (1U<<9)
#define GPIO_PIN_10 (1U<<10)
#define GPIO_AF9_QUADSPI 9U
#define GPIO_AF10_QUADSPI 10U
#define GPIO_MODE_AF_PP 2U
#define GPIO_PULLUP 1U
#define GPIO_SPEED_FREQ_VERY_HIGH 3U
#define RCC_PERIPHCLK_QSPI 1U
#define RCC_QSPICLKSOURCE_D1HCLK 3U
#define QSPI_INSTRUCTION_1_LINE 1U
#define QSPI_ADDRESS_NONE 0U
#define QSPI_ADDRESS_1_LINE 1U
#define QSPI_ADDRESS_24_BITS 24U
#define QSPI_ADDRESS_32_BITS 32U
#define QSPI_ALTERNATE_BYTES_NONE 0U
#define QSPI_DATA_1_LINE 1U
#define QSPI_DATA_NONE 0U
#define QSPI_DDR_MODE_DISABLE 0U
#define QSPI_DDR_HHC_ANALOG_DELAY 0U
#define QSPI_SIOO_INST_EVERY_CMD 0U
#define QSPI_SAMPLE_SHIFTING_HALFCYCLE 1U
#define QSPI_CS_HIGH_TIME_4_CYCLE 4U
#define QSPI_CLOCK_MODE_0 0U
#define QSPI_FLASH_ID_1 1U
#define QSPI_DUALFLASH_DISABLE 0U
#define __HAL_RCC_GPIOF_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOG_CLK_ENABLE() ((void)0)
#define __HAL_RCC_QSPI_CLK_ENABLE() ((void)0)
#define __HAL_RCC_QSPI_FORCE_RESET() ((void)0)
#define __HAL_RCC_QSPI_RELEASE_RESET() ((void)0)
void HAL_GPIO_Init(void *,GPIO_InitTypeDef *);
HAL_StatusTypeDef HAL_RCCEx_PeriphCLKConfig(RCC_PeriphCLKInitTypeDef *);
HAL_StatusTypeDef HAL_QSPI_Init(QSPI_HandleTypeDef *);
HAL_StatusTypeDef HAL_QSPI_Command(QSPI_HandleTypeDef *,QSPI_CommandTypeDef *,uint32_t);
HAL_StatusTypeDef HAL_QSPI_Receive(QSPI_HandleTypeDef *,uint8_t *,uint32_t);
HAL_StatusTypeDef HAL_QSPI_Transmit(QSPI_HandleTypeDef *,uint8_t *,uint32_t);
uint32_t HAL_GetTick(void);
void HAL_Delay(uint32_t);
void bsp_watchdog_feed(void);
#endif

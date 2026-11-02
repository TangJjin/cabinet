/*
 * 文件用途：接口与类型声明：本核入口及 CubeMX 外设初始化使用的公共定义。
 * 编译归属：随对应源文件或包含它的目标使用；具体归属见工程文件导读。
 */
/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

    /* Private includes ----------------------------------------------------------*/
    /* USER CODE BEGIN Includes */

    /* USER CODE END Includes */

    /* Exported types ------------------------------------------------------------*/
    /* USER CODE BEGIN ET */

    /* USER CODE END ET */

    /* Exported constants --------------------------------------------------------*/
    /* USER CODE BEGIN EC */

    /* USER CODE END EC */

    /* Exported macro ------------------------------------------------------------*/
    /* USER CODE BEGIN EM */

    /* USER CODE END EM */

    /* Exported functions prototypes ---------------------------------------------*/
    void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define CTP_INT_Pin GPIO_PIN_5
#define CTP_INT_GPIO_Port GPIOB
#define LCD_PWREN_Pin GPIO_PIN_11
#define LCD_PWREN_GPIO_Port GPIOI
#define LCD_RST_Pin GPIO_PIN_5
#define LCD_RST_GPIO_Port GPIOH
#define LED_Pin GPIO_PIN_1
#define LED_GPIO_Port GPIOB
#define CTP_RST_Pin GPIO_PIN_12
#define CTP_RST_GPIO_Port GPIOB
#define LEDA3_Pin GPIO_PIN_3
#define LEDA3_GPIO_Port GPIOA
#define LCD_BL_Pin GPIO_PIN_0
#define LCD_BL_GPIO_Port GPIOB

    /* USER CODE BEGIN Private defines */

    /* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

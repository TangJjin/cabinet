/*
 * 文件用途：接口与类型声明：OpenAMP 共享区地址、VRing 容量和 libmetal 平台设置。
 * 编译归属：随对应源文件或包含它的目标使用；具体归属见工程文件导读。
 */
/**
  ******************************************************************************
  * @file           : openamp_conf.h
  * @brief          : Configuration file for OpenAMP MW
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2018 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __OPENAMP_CONF__H__
#define __OPENAMP_CONF__H__

#ifdef __cplusplus
extern "C"
{
#endif

/* Includes ------------------------------------------------------------------*/
#if defined(__LOG_TRACE_IO_) || defined(__LOG_UART_IO_)
#include "log.h"
#endif

    /* ########################## Mailbox Interface Selection ############################## */
    /**
   * @brief This is the list of Mailbox interface  to be used in the OpenAMP MW
   *        Please note that not all interfaces are supported by a STM32 device
   */
//#define MAILBOX_IPCC_IF_ENABLED
#define MAILBOX_HSEM_IF_ENABLED

    /* Includes ------------------------------------------------------------------*/
    /**
   * @brief Include Maibox interface  header file
   */

#ifdef MAILBOX_IPCC_IF_ENABLED
#include "mbox_ipcc.h"
#endif /* MAILBOX_IPCC_IF_ENABLED */

#ifdef MAILBOX_HSEM_IF_ENABLED
#include "mbox_hsem.h"
#endif /* MAILBOX_HSEM_IF_ENABLED */

    /* ########################## Virtual Diver Module Selection ############################## */
    /**
   * @brief This is the list of modules to be used in the OpenAMP Virtual driver module
   *        Please note that virtual driver are not supported on all stm32 families
   */
    //#define VIRTUAL_UART_MODULE_ENABLED
    //#define VIRTUAL_I2C_MODULE_ENABLED


    /* Includes ------------------------------------------------------------------*/
    /**
   * @brief Include Virtual Driver module's  header file
   */

#ifdef VIRTUAL_UART_MODULE_ENABLED
#include "virt_uart.h"
#endif /* VIRTUAL_UART_MODULE_ENABLED */

#ifdef VIRTUAL_I2C_MODULE_ENABLED
#include "virt_i2c.h"
#endif /* VIRTUAL_I2C_MODULE_ENABLED */


    /* USER CODE BEGIN INCLUDE */

    /* USER CODE END INCLUDE */

    /** @addtogroup OPENAMP_MW
  * @{
  */

    /** @defgroup OPENAMP_CONF OPENAMP_CONF
  * @brief Configuration file for Openamp mw
  * @{
  */

    /** @defgroup OPENAMP_CONF_Exported_Variables OPENAMP_CONF_Exported_Variables
  * @brief Public variables.
  * @{
  */

    /**
  * @}
  */

    /** @defgroup OPENAMP_CONF_Exported_Defines OPENAMP_CONF_Exported_Defines
  * @brief Defines for configuration of the Openamp mw
  * @{
  */


#define SHM_START_ADDRESS 0x38000400UL
#define SHM_SIZE 0xB000U

#define VRING_RX_ADDRESS SHM_START_ADDRESS
#define VRING_TX_ADDRESS (SHM_START_ADDRESS + 0x400)
#define VRING_BUFF_ADDRESS (SHM_START_ADDRESS + 0x800)
#define VRING_ALIGNMENT 4
#define VRING_NUM_BUFFS 4 /* number of rpmsg buffers */

/* Fixed parameter */
#define NUM_RESOURCE_ENTRIES 2
#define VRING_COUNT 2

#define VDEV_ID 0xFF
#define VRING0_ID 0 /* VRING0: CM4 -> CM7; CM7 rvq / CM4 svq*/
#define VRING1_ID 1 /* VRING1: CM7 -> CM4; CM7 svq / CM4 rvq  */

    /**
  * @}
  */

    /** @defgroup OPENAMP_CONF_Exported_Macros OPENAMP_CONF_Exported_Macros
  * @brief Aliases.
  * @{
  */

    /* DEBUG macros */

#if defined(__LOG_TRACE_IO_) || defined(__LOG_UART_IO_)
#define OPENAMP_log_dbg log_dbg
#define OPENAMP_log_info log_info
#define OPENAMP_log_warn log_warn
#define OPENAMP_log_err log_err
#else
#define OPENAMP_log_dbg(...)
#define OPENAMP_log_info(...)
#define OPENAMP_log_warn(...)
#define OPENAMP_log_err(...)
#endif

    /**
  * @}
  */

    /** @defgroup OPENAMP_CONF_Exported_Types OPENAMP_CONF_Exported_Types
  * @brief Types.
  * @{
  */

    /**
  * @}
  */

    /** @defgroup OPENAMP_CONF_Exported_FunctionsPrototype OPENAMP_CONF_Exported_FunctionsPrototype
  * @brief Declaration of public functions for OpenAMP mw.
  * @{
  */

    /* Exported functions -------------------------------------------------------*/

    /**
  * @}
  */

    /**
  * @}
  */

    /**
  * @}
  */

#ifdef __cplusplus
}
#endif

#endif /* __OPENAMP_CONF__H__ */

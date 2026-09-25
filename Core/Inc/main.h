/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Common application and board definitions.
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"

void Error_Handler(void);

#define LED_GREEN_Pin       GPIO_PIN_0
#define LED_GREEN_GPIO_Port GPIOB
#define LED_RED_Pin         GPIO_PIN_14
#define LED_RED_GPIO_Port   GPIOB
#define LED_YELLOW_Pin      GPIO_PIN_1
#define LED_YELLOW_GPIO_Port GPIOE

#define STLK_VCP_TX_Pin     GPIO_PIN_9
#define STLK_VCP_TX_GPIO_Port GPIOD
#define STLK_VCP_RX_Pin     GPIO_PIN_8
#define STLK_VCP_RX_GPIO_Port GPIOD

#define W6300_RSTn_Pin      GPIO_PIN_4
#define W6300_RSTn_GPIO_Port GPIOF

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32h7xx_hal_msp.c
  * @brief   MCU Support Package initialization for the enabled peripherals.
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"

void HAL_MspInit(void)
{
  __HAL_RCC_SYSCFG_CLK_ENABLE();
}

void HAL_OSPI_MspInit(OSPI_HandleTypeDef *hospi)
{
  GPIO_InitTypeDef gpio = {0};

  if (hospi->Instance != OCTOSPI1) {
    return;
  }

  __HAL_RCC_OSPI1_CLK_ENABLE();
  __HAL_RCC_OCTOSPIM_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio.Alternate = GPIO_AF9_OCTOSPIM_P1;

  gpio.Pin = GPIO_PIN_2;
  HAL_GPIO_Init(GPIOB, &gpio);

  gpio.Pin = GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13;
  HAL_GPIO_Init(GPIOD, &gpio);

  gpio.Pin = GPIO_PIN_2;
  HAL_GPIO_Init(GPIOE, &gpio);

  gpio.Pin = GPIO_PIN_6;
  gpio.Alternate = GPIO_AF10_OCTOSPIM_P1;
  HAL_GPIO_Init(GPIOG, &gpio);
}

void HAL_OSPI_MspDeInit(OSPI_HandleTypeDef *hospi)
{
  if (hospi->Instance != OCTOSPI1) {
    return;
  }

  __HAL_RCC_OSPI1_CLK_DISABLE();
  HAL_GPIO_DeInit(GPIOB, GPIO_PIN_2);
  HAL_GPIO_DeInit(GPIOD, GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13);
  HAL_GPIO_DeInit(GPIOE, GPIO_PIN_2);
  HAL_GPIO_DeInit(GPIOG, GPIO_PIN_6);
}

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
  GPIO_InitTypeDef gpio = {0};
  RCC_PeriphCLKInitTypeDef peripheral_clock = {0};

  if (huart->Instance != USART3) {
    return;
  }

  peripheral_clock.PeriphClockSelection = RCC_PERIPHCLK_USART3;
  peripheral_clock.Usart234578ClockSelection = RCC_USART234578CLKSOURCE_D2PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&peripheral_clock) != HAL_OK) {
    Error_Handler();
  }

  __HAL_RCC_USART3_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  gpio.Pin = STLK_VCP_RX_Pin | STLK_VCP_TX_Pin;
  gpio.Mode = GPIO_MODE_AF_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  gpio.Alternate = GPIO_AF7_USART3;
  HAL_GPIO_Init(GPIOD, &gpio);
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
  if (huart->Instance != USART3) {
    return;
  }

  __HAL_RCC_USART3_CLK_DISABLE();
  HAL_GPIO_DeInit(GPIOD, STLK_VCP_RX_Pin | STLK_VCP_TX_Pin);
}

/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : STM32H723 W6300 TCP loopback entry point
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"

/* USER CODE BEGIN Includes */
#include <stdio.h>

#include "app_config.h"
#include "w6300_app.h"
/* USER CODE END Includes */

OSPI_HandleTypeDef hospi1;
UART_HandleTypeDef huart3;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_OCTOSPI1_Init(void);
static void MX_USART3_UART_Init(void);

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART3_UART_Init();
  MX_OCTOSPI1_Init();

  /* USER CODE BEGIN 2 */
  if (!w6300_app_init()) {
    Error_Handler();
  }
  /* USER CODE END 2 */

  while (1) {
    /* USER CODE BEGIN WHILE */
    w6300_app_poll();
    /* USER CODE END WHILE */
  }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef oscillator = {0};
  RCC_ClkInitTypeDef clocks = {0};
  RCC_PeriphCLKInitTypeDef peripheral_clocks = {0};

  if (HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY) != HAL_OK) {
    Error_Handler();
  }
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
  while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {
  }

  oscillator.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  oscillator.HSEState = RCC_HSE_BYPASS;
  oscillator.PLL.PLLState = RCC_PLL_ON;
  oscillator.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  oscillator.PLL.PLLM = 4U;
  oscillator.PLL.PLLN = 275U;
  oscillator.PLL.PLLP = 1U;
  oscillator.PLL.PLLQ = 4U;
  oscillator.PLL.PLLR = 2U;
  oscillator.PLL.PLLRGE = RCC_PLL1VCIRANGE_1;
  oscillator.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  oscillator.PLL.PLLFRACN = 0U;
  if (HAL_RCC_OscConfig(&oscillator) != HAL_OK) {
    Error_Handler();
  }

  clocks.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                     RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 |
                     RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
  clocks.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  clocks.SYSCLKDivider = RCC_SYSCLK_DIV1;
  clocks.AHBCLKDivider = RCC_HCLK_DIV2;
  clocks.APB3CLKDivider = RCC_APB3_DIV2;
  clocks.APB1CLKDivider = RCC_APB1_DIV2;
  clocks.APB2CLKDivider = RCC_APB2_DIV2;
  clocks.APB4CLKDivider = RCC_APB4_DIV2;
  if (HAL_RCC_ClockConfig(&clocks, FLASH_LATENCY_3) != HAL_OK) {
    Error_Handler();
  }

  /* Keep OCTOSPI on the existing 275 MHz D1HCLK: / (8 + 1) ~= 30.6 MHz. */
  peripheral_clocks.PeriphClockSelection = RCC_PERIPHCLK_OSPI;
  peripheral_clocks.OspiClockSelection = RCC_OSPICLKSOURCE_D1HCLK;
  if (HAL_RCCEx_PeriphCLKConfig(&peripheral_clocks) != HAL_OK) {
    Error_Handler();
  }
}

static void MX_OCTOSPI1_Init(void)
{
  OSPIM_CfgTypeDef io_manager = {0};

  hospi1.Instance = OCTOSPI1;
  hospi1.Init.FifoThreshold = 1U;
  hospi1.Init.DualQuad = HAL_OSPI_DUALQUAD_DISABLE;
  hospi1.Init.MemoryType = HAL_OSPI_MEMTYPE_MICRON;
  hospi1.Init.DeviceSize = 32U;
  hospi1.Init.ChipSelectHighTime = 1U;
  hospi1.Init.FreeRunningClock = HAL_OSPI_FREERUNCLK_DISABLE;
  hospi1.Init.ClockMode = HAL_OSPI_CLOCK_MODE_3;
  hospi1.Init.WrapSize = HAL_OSPI_WRAP_NOT_SUPPORTED;
  hospi1.Init.ClockPrescaler = W6300_OSPI_PRESCALER;
  hospi1.Init.SampleShifting = HAL_OSPI_SAMPLE_SHIFTING_NONE;
  /* No delay-block calibration is used during this blocking bring-up. */
  hospi1.Init.DelayBlockBypass = HAL_OSPI_DELAY_BLOCK_BYPASSED;
  hospi1.Init.DelayHoldQuarterCycle = HAL_OSPI_DHQC_DISABLE;
  if (HAL_OSPI_Init(&hospi1) != HAL_OK) {
    printf("[QSPI] HAL_OSPI_Init failed: 0x%08lX\r\n",
           (unsigned long)hospi1.ErrorCode);
    Error_Handler();
  }

  io_manager.ClkPort = 1U;
  io_manager.DQSPort = 0U;
  io_manager.NCSPort = 1U;
  io_manager.IOLowPort = HAL_OSPIM_IOPORT_1_LOW;
  io_manager.IOHighPort = HAL_OSPIM_IOPORT_NONE;
  io_manager.Req2AckTime = 1U;
  if (HAL_OSPIM_Config(&hospi1, &io_manager, W6300_OSPI_TIMEOUT_MS) != HAL_OK) {
    printf("[QSPI] HAL_OSPIM_Config failed: 0x%08lX\r\n",
           (unsigned long)hospi1.ErrorCode);
    Error_Handler();
  }
  printf("[QSPI] OCTOSPI1 Port 1 configured, Mode 3, STR, prescaler %u "
         "(~30.6 MHz)\r\n", W6300_OSPI_PRESCALER);
}

static void MX_USART3_UART_Init(void)
{
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200U;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK) {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart3, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK ||
      HAL_UARTEx_SetRxFifoThreshold(&huart3, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK ||
      HAL_UARTEx_DisableFifoMode(&huart3) != HAL_OK) {
    Error_Handler();
  }
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOB, LED_GREEN_Pin | LED_RED_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED_YELLOW_GPIO_Port, LED_YELLOW_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(W6300_RSTn_GPIO_Port, W6300_RSTn_Pin, GPIO_PIN_SET);

  gpio.Pin = LED_GREEN_Pin | LED_RED_Pin;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &gpio);

  gpio.Pin = LED_YELLOW_Pin;
  HAL_GPIO_Init(LED_YELLOW_GPIO_Port, &gpio);

  gpio.Pin = W6300_RSTn_Pin;
  HAL_GPIO_Init(W6300_RSTn_GPIO_Port, &gpio);
}

/* USER CODE BEGIN 4 */
int __io_putchar(int character)
{
  uint8_t byte = (uint8_t)character;
  if (HAL_UART_Transmit(&huart3, &byte, 1U, 100U) != HAL_OK) {
    return EOF;
  }
  return character;
}
/* USER CODE END 4 */

void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  printf("[FATAL] firmware initialization failed\r\n");
  HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_SET);
  while (1) {
    HAL_Delay(500U);
    HAL_GPIO_TogglePin(LED_RED_GPIO_Port, LED_RED_Pin);
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/* USER CODE BEGIN 6 */
void assert_failed(uint8_t *file, uint32_t line)
{
  printf("[ASSERT] %s:%lu\r\n", (char *)file, (unsigned long)line);
}
/* USER CODE END 6 */
#endif

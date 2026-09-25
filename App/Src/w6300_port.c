#include "w6300_port.h"

#include <stdio.h>

#include "app_config.h"
#include "main.h"
#include "wizchip_conf.h"

extern OSPI_HandleTypeDef hospi1;

static volatile uint32_t g_hal_error_count;
static volatile uint32_t g_last_hal_error_code;

static void w6300_cs_select(void);
static void w6300_cs_deselect(void);
static void w6300_critical_enter(void);
static void w6300_critical_exit(void);
static void w6300_qspi_read(uint8_t opcode, uint16_t address, uint8_t *buffer,
                            uint16_t length);
static void w6300_qspi_write(uint8_t opcode, uint16_t address, uint8_t *buffer,
                             uint16_t length);
static HAL_StatusTypeDef w6300_qspi_transfer(uint8_t opcode, uint16_t address,
                                             uint8_t *buffer, uint16_t length,
                                             bool read);

void w6300_port_reset(void)
{
  HAL_GPIO_WritePin(W6300_RSTn_GPIO_Port, W6300_RSTn_Pin, GPIO_PIN_RESET);
  HAL_Delay(W6300_RESET_LOW_MS);
  HAL_GPIO_WritePin(W6300_RSTn_GPIO_Port, W6300_RSTn_Pin, GPIO_PIN_SET);
  HAL_Delay(W6300_RESET_SETTLE_MS);
  printf("[W6300] hardware reset released; settle complete\r\n");
}

void w6300_port_register_callbacks(void)
{
  /* These CS callbacks intentionally do nothing; OCTOSPI1 owns hardware NCS. */
  reg_wizchip_cs_cbfunc(w6300_cs_select, w6300_cs_deselect);
  reg_wizchip_cris_cbfunc(w6300_critical_enter, w6300_critical_exit);
  reg_wizchip_qspi_cbfunc(w6300_qspi_read, w6300_qspi_write);
  printf("[QSPI] blocking callbacks registered (OCTOSPI1 P1, hardware NCS)\r\n");
}

void w6300_port_get_diagnostics(W6300_PortDiagnostics *diagnostics)
{
  if (diagnostics != NULL) {
    diagnostics->hal_error_count = g_hal_error_count;
    diagnostics->last_hal_error_code = g_last_hal_error_code;
  }
}

static void w6300_qspi_read(uint8_t opcode, uint16_t address, uint8_t *buffer,
                            uint16_t length)
{
  (void)w6300_qspi_transfer(opcode, address, buffer, length, true);
}

static void w6300_cs_select(void)
{
  /* HAL_OSPI_Command controls hardware NCS. */
}

static void w6300_cs_deselect(void)
{
  /* HAL_OSPI_Command controls hardware NCS. */
}

static void w6300_critical_enter(void)
{
  __disable_irq();
}

static void w6300_critical_exit(void)
{
  __enable_irq();
}

static void w6300_qspi_write(uint8_t opcode, uint16_t address, uint8_t *buffer,
                             uint16_t length)
{
  (void)w6300_qspi_transfer(opcode, address, buffer, length, false);
}

static HAL_StatusTypeDef w6300_qspi_transfer(uint8_t opcode, uint16_t address,
                                             uint8_t *buffer, uint16_t length,
                                             bool read)
{
  HAL_StatusTypeDef status;
  OSPI_RegularCmdTypeDef command = {0};

  if ((buffer == NULL) || (length == 0U)) {
    ++g_hal_error_count;
    g_last_hal_error_code = HAL_ERROR;
    printf("[QSPI] invalid %s request (length=%u)\r\n",
           read ? "read" : "write", (unsigned int)length);
    return HAL_ERROR;
  }

  command.OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;
  command.FlashId = HAL_OSPI_FLASH_ID_1;
  command.InstructionMode = HAL_OSPI_INSTRUCTION_1_LINE;
  command.InstructionSize = HAL_OSPI_INSTRUCTION_8_BITS;
  command.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
  command.Instruction = opcode;
  command.Address = (uint32_t)address;
  command.AddressMode = HAL_OSPI_ADDRESS_4_LINES;
  command.AddressSize = HAL_OSPI_ADDRESS_16_BITS;
  command.AddressDtrMode = HAL_OSPI_ADDRESS_DTR_DISABLE;
  command.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
  command.DataMode = HAL_OSPI_DATA_4_LINES;
  command.NbData = (uint32_t)length;
  command.DataDtrMode = HAL_OSPI_DATA_DTR_DISABLE;
  command.DummyCycles = W6300_QSPI_DUMMY_CYCLES;
  command.DQSMode = HAL_OSPI_DQS_DISABLE;
  command.SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;

  status = HAL_OSPI_Command(&hospi1, &command, W6300_OSPI_TIMEOUT_MS);
  if (status == HAL_OK) {
    status = read ? HAL_OSPI_Receive(&hospi1, buffer, W6300_OSPI_TIMEOUT_MS)
                  : HAL_OSPI_Transmit(&hospi1, buffer, W6300_OSPI_TIMEOUT_MS);
  }

  if (status != HAL_OK) {
    ++g_hal_error_count;
    g_last_hal_error_code = hospi1.ErrorCode;
    printf("[QSPI] %s failed: opcode=0x%02X address=0x%04X length=%u "
           "HAL=%d error=0x%08lX\r\n",
           read ? "read" : "write", opcode, address, (unsigned int)length,
           (int)status, (unsigned long)hospi1.ErrorCode);
  }

  return status;
}

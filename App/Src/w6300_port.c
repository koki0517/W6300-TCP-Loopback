#include "w6300_port.h"

#include <stdbool.h>
#include <stdio.h>

#include "app_config.h"
#include "main.h"
#include "wizchip_conf.h"

extern OSPI_HandleTypeDef hospi1;

static volatile uint32_t g_hal_error_count;
static W6300_QspiBusMode g_qspi_mode = W6300_QSPI_BUS_QUAD;

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

static void w6300_record_hal_error(const char *function,
                                  HAL_StatusTypeDef status, uint8_t opcode,
                                  uint16_t address, uint16_t length);

static const char *w6300_qspi_mode_name(W6300_QspiBusMode mode)
{
  switch (mode) {
    case W6300_QSPI_BUS_SINGLE:
      return "Single 1-1-1";
    case W6300_QSPI_BUS_QUAD:
      return "Quad 1-4-4";
    default:
      return "Unknown";
  }
}

static uint8_t w6300_qspi_mode_bits(W6300_QspiBusMode mode)
{
  switch (mode) {
    case W6300_QSPI_BUS_SINGLE:
      return QSPI_SINGLE_MODE;
    case W6300_QSPI_BUS_QUAD:
    default:
      return QSPI_QUAD_MODE;
  }
}

static uint8_t w6300_qspi_dummy_cycles(W6300_QspiBusMode mode)
{
  switch (mode) {
    case W6300_QSPI_BUS_SINGLE:
      return W6300_QSPI_SINGLE_DUMMY_CYCLES;
    case W6300_QSPI_BUS_QUAD:
    default:
      return W6300_QSPI_DUMMY_CYCLES;
  }
}

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
  /* No ISR accesses W6300; keep SysTick active for polling HAL timeouts. */
  reg_wizchip_cris_cbfunc(w6300_critical_enter, w6300_critical_exit);
  reg_wizchip_qspi_cbfunc(w6300_qspi_read, w6300_qspi_write);
  printf("[QSPI] %s mode; hardware NCS\r\n",
         w6300_qspi_mode_name(g_qspi_mode));
}

void w6300_port_set_qspi_mode(W6300_QspiBusMode mode)
{
  if ((mode == W6300_QSPI_BUS_SINGLE) || (mode == W6300_QSPI_BUS_QUAD)) {
    g_qspi_mode = mode;
  }
}

uint32_t w6300_port_error_count(void)
{
  return g_hal_error_count;
}

void w6300_port_clear_errors(void)
{
  g_hal_error_count = 0U;
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
  /* Single-threaded polling application; keep SysTick enabled for HAL timeout. */
}

static void w6300_critical_exit(void)
{
  /* No critical section is needed by the single-threaded application. */
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
  const uint8_t mode_bits = w6300_qspi_mode_bits(g_qspi_mode);
  const uint8_t dummy_cycles = w6300_qspi_dummy_cycles(g_qspi_mode);
  const uint8_t reconstructed_opcode = (uint8_t)((opcode & 0x3FU) |
                                                   mode_bits);

  if ((buffer == NULL) || (length == 0U)) {
    w6300_record_hal_error("request_validation", HAL_ERROR, opcode,
                           address, length);
    return HAL_ERROR;
  }

  command.OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;
  command.FlashId = HAL_OSPI_FLASH_ID_1;
  command.InstructionMode = HAL_OSPI_INSTRUCTION_1_LINE;
  command.InstructionSize = HAL_OSPI_INSTRUCTION_8_BITS;
  command.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
  /*
   * ioLibrary W6300 encodes register/RW bits in opcode[5:0] and its
   * compile-time bus mode in opcode[7:6]. Preserve the former and replace
   * the latter so READ/WRITE and READ_BUF/WRITE_BUF all use the active mode.
   */
  command.Instruction = reconstructed_opcode;
  command.Address = (uint32_t)address;
  command.AddressSize = HAL_OSPI_ADDRESS_16_BITS;
  command.AddressDtrMode = HAL_OSPI_ADDRESS_DTR_DISABLE;
  command.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
  switch (g_qspi_mode) {
    case W6300_QSPI_BUS_SINGLE:
      command.AddressMode = HAL_OSPI_ADDRESS_1_LINE;
      command.DataMode = HAL_OSPI_DATA_1_LINE;
      break;
    case W6300_QSPI_BUS_QUAD:
    default:
      command.AddressMode = HAL_OSPI_ADDRESS_4_LINES;
      command.DataMode = HAL_OSPI_DATA_4_LINES;
      break;
  }
  command.DummyCycles = dummy_cycles;
  command.NbData = (uint32_t)length;
  command.DataDtrMode = HAL_OSPI_DATA_DTR_DISABLE;
  command.DQSMode = HAL_OSPI_DQS_DISABLE;
  command.SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;

  status = HAL_OSPI_Command(&hospi1, &command, W6300_OSPI_TIMEOUT_MS);
  if (status != HAL_OK) {
    w6300_record_hal_error("HAL_OSPI_Command", status, opcode, address,
                           length);
  } else {
    status = read ? HAL_OSPI_Receive(&hospi1, buffer, W6300_OSPI_TIMEOUT_MS)
                  : HAL_OSPI_Transmit(&hospi1, buffer, W6300_OSPI_TIMEOUT_MS);
    if (status != HAL_OK) {
      w6300_record_hal_error(read ? "HAL_OSPI_Receive" : "HAL_OSPI_Transmit",
                             status, opcode, address, length);
    }
  }
  return status;
}

static void w6300_record_hal_error(const char *function,
                                  HAL_StatusTypeDef status, uint8_t opcode,
                                  uint16_t address, uint16_t length)
{
  const uint32_t state = (uint32_t)HAL_OSPI_GetState(&hospi1);
  const uint32_t error_code = hospi1.ErrorCode;

  ++g_hal_error_count;
  printf("[QSPI] %s failed: opcode=0x%02X address=0x%04X length=%u "
         "HAL=%u state=0x%02lX ErrorCode=0x%08lX\r\n",
         function, opcode, address, (unsigned int)length,
         (unsigned int)status, (unsigned long)state,
         (unsigned long)error_code);
}

#ifndef W6300_PORT_H
#define W6300_PORT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint32_t hal_error_count;
  uint32_t last_hal_error_code;
  uint32_t last_hal_status;
  uint32_t last_hal_state;
} W6300_PortDiagnostics;

typedef enum {
  W6300_QSPI_BUS_SINGLE = 0,
  W6300_QSPI_BUS_QUAD = 1
} W6300_QspiBusMode;

void w6300_port_reset(void);
void w6300_port_set_qspi_mode(W6300_QspiBusMode mode);
const char *w6300_port_mode_name(void);
void w6300_port_register_callbacks(void);
void w6300_port_get_diagnostics(W6300_PortDiagnostics *diagnostics);
void w6300_port_clear_diagnostics(void);

#endif /* W6300_PORT_H */

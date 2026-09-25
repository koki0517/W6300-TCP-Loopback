#ifndef W6300_PORT_H
#define W6300_PORT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint32_t hal_error_count;
  uint32_t last_hal_error_code;
} W6300_PortDiagnostics;

void w6300_port_reset(void);
void w6300_port_register_callbacks(void);
void w6300_port_get_diagnostics(W6300_PortDiagnostics *diagnostics);

#endif /* W6300_PORT_H */

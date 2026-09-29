#ifndef W6300_PORT_H
#define W6300_PORT_H

#include <stdint.h>

typedef enum {
  W6300_QSPI_BUS_SINGLE = 0,
  W6300_QSPI_BUS_QUAD = 1
} W6300_QspiBusMode;

void w6300_port_reset(void);
void w6300_port_set_qspi_mode(W6300_QspiBusMode mode);
void w6300_port_register_callbacks(void);
uint32_t w6300_port_error_count(void);
void w6300_port_clear_errors(void);

#endif /* W6300_PORT_H */

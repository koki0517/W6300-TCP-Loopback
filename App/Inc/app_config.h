#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdint.h>

/* Static IPv4 configuration used by the direct PC-to-WIZ630io test link. */
#define APP_MAC_ADDRESS          {0x02U, 0x00U, 0x00U, 0x00U, 0x00U, 0x10U}
#define APP_IPV4_ADDRESS         {192U, 168U, 0U, 10U}
#define APP_SUBNET_MASK          {255U, 255U, 255U, 0U}
#define APP_GATEWAY_ADDRESS      {0U, 0U, 0U, 0U}
#define APP_DNS_ADDRESS          {0U, 0U, 0U, 0U}

#define APP_TCP_PORT             5000U
#define APP_TCP_SOCKET           0U
#define APP_LOOPBACK_BUFFER_SIZE 2048U

#define W6300_RESET_LOW_MS       10U
#define W6300_RESET_SETTLE_MS    100U
#define W6300_OSPI_TIMEOUT_MS    1000U
#define W6300_OSPI_PRESCALER     9U
#define W6300_QSPI_DUMMY_CYCLES  2U
#define W6300_EXPECTED_CIDR      0x6100U

#define APP_STATUS_POLL_MS       1000U

#endif /* APP_CONFIG_H */

#include "w6300_app.h"

#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "loopback.h"
#include "main.h"
#include "socket.h"
#include "w6300_port.h"
#include "wizchip_conf.h"
#include "W6300/w6300.h"

static uint8_t loopback_buffer[APP_LOOPBACK_BUFFER_SIZE];
static uint8_t tx_buffer_sizes[8] = {32U, 0U, 0U, 0U, 0U, 0U, 0U, 0U};
static uint8_t rx_buffer_sizes[8] = {32U, 0U, 0U, 0U, 0U, 0U, 0U, 0U};

static bool g_initialized;
static bool g_fatal_qspi_error;
static uint32_t g_last_status_poll;
static int8_t g_last_phy_link = -1;
static uint8_t g_last_socket_state = 0xFFU;
static int32_t g_last_socket_error;

static void log_network_info(const wiz_NetInfo *network);
static bool check_qspi_errors(void);
static void poll_link_status(void);
static void log_socket_state(uint8_t state);

bool w6300_app_init(void)
{
  const uint8_t expected_mac[6] = APP_MAC_ADDRESS;
  const uint8_t expected_ip[4] = APP_IPV4_ADDRESS;
  const uint8_t expected_subnet[4] = APP_SUBNET_MASK;
  const uint8_t expected_gateway[4] = APP_GATEWAY_ADDRESS;
  const uint8_t expected_dns[4] = APP_DNS_ADDRESS;
  W6300_PortDiagnostics diagnostics = {0U, 0U};
  wiz_NetInfo network = {0};
  uint16_t cidr;
  uint16_t version;
  int8_t result;

  printf("\r\nW6300 TCP loopback firmware start\r\n");

  w6300_port_reset();
  w6300_port_register_callbacks();

  cidr = getCIDR();
  version = getVER();
  printf("[QSPI] CIDR=0x%04X VER=0x%04X SYSR=0x%02X\r\n",
         cidr, version, getSYSR());
  w6300_port_get_diagnostics(&diagnostics);
  if ((diagnostics.hal_error_count != 0U) ||
      (cidr != W6300_EXPECTED_CIDR) || (version == 0U) ||
      (version == 0xFFFFU)) {
    printf("[QSPI] sanity check failed (CIDR expected 0x%04X, "
           "HAL errors=%lu)\r\n",
           W6300_EXPECTED_CIDR,
           (unsigned long)diagnostics.hal_error_count);
    return false;
  }
  printf("[QSPI] communication sanity check passed\r\n");

  result = wizchip_init(tx_buffer_sizes, rx_buffer_sizes);
  if (result != 0) {
    printf("[W6300] socket memory initialization failed: %d\r\n", result);
    return false;
  }
  printf("[W6300] socket 0 TX/RX memory=32/32 KB; sockets 1-7 disabled\r\n");

  if (set_loopback_mode_W6x00(AS_IPV4) != 0) {
    printf("[W6300] failed to select IPv4 loopback mode\r\n");
    return false;
  }

  memcpy(network.mac, expected_mac, sizeof(expected_mac));
  memcpy(network.ip, expected_ip, sizeof(expected_ip));
  memcpy(network.sn, expected_subnet, sizeof(expected_subnet));
  memcpy(network.gw, expected_gateway, sizeof(expected_gateway));
  memcpy(network.dns, expected_dns, sizeof(expected_dns));
  network.ipmode = NETINFO_STATIC_V4;
  network.dhcp = NETINFO_STATIC;
  wizchip_setnetinfo(&network);

  wizchip_getnetinfo(&network);
  log_network_info(&network);

  g_initialized = true;
  g_last_status_poll = HAL_GetTick() - APP_STATUS_POLL_MS;
  poll_link_status();
  printf("[TCP] preparing IPv4 server socket %u on port %u\r\n",
         APP_TCP_SOCKET, APP_TCP_PORT);
  return true;
}

void w6300_app_poll(void)
{
  int32_t result;
  uint8_t socket_state;

  if (!g_initialized) {
    return;
  }

  if (g_fatal_qspi_error || !check_qspi_errors()) {
    g_fatal_qspi_error = true;
    HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_SET);
    HAL_Delay(100U);
    return;
  }

  poll_link_status();

  result = loopback_tcps(APP_TCP_SOCKET, loopback_buffer, APP_TCP_PORT);
  if (!check_qspi_errors()) {
    g_fatal_qspi_error = true;
    printf("[QSPI] fatal bus error; TCP polling stopped\r\n");
    return;
  }

  socket_state = getSn_SR(APP_TCP_SOCKET);
  log_socket_state(socket_state);

  if (result < 0) {
    if (result != g_last_socket_error) {
      printf("[TCP] socket %u error %ld, state=0x%02X; closing for recovery\r\n",
             APP_TCP_SOCKET, (long)result, socket_state);
      g_last_socket_error = result;
    }
    (void)close(APP_TCP_SOCKET);
  } else {
    g_last_socket_error = 0;
  }

  if (!check_qspi_errors()) {
    g_fatal_qspi_error = true;
    printf("[QSPI] fatal bus error while reading socket state\r\n");
  }
}

static void log_network_info(const wiz_NetInfo *network)
{
  printf("[NET] MAC %02X:%02X:%02X:%02X:%02X:%02X\r\n",
         network->mac[0], network->mac[1], network->mac[2],
         network->mac[3], network->mac[4], network->mac[5]);
  printf("[NET] IPv4 %u.%u.%u.%u / %u.%u.%u.%u\r\n",
         network->ip[0], network->ip[1], network->ip[2], network->ip[3],
         network->sn[0], network->sn[1], network->sn[2], network->sn[3]);
  printf("[NET] gateway %u.%u.%u.%u DNS %u.%u.%u.%u (static)\r\n",
         network->gw[0], network->gw[1], network->gw[2], network->gw[3],
         network->dns[0], network->dns[1], network->dns[2], network->dns[3]);
}

static bool check_qspi_errors(void)
{
  W6300_PortDiagnostics diagnostics;
  w6300_port_get_diagnostics(&diagnostics);
  return diagnostics.hal_error_count == 0U;
}

static void poll_link_status(void)
{
  const uint32_t now = HAL_GetTick();
  int8_t link;

  if ((uint32_t)(now - g_last_status_poll) < APP_STATUS_POLL_MS) {
    return;
  }

  g_last_status_poll = now;
  link = wizphy_getphylink();
  if (link != g_last_phy_link) {
    printf("[PHY] link %s\r\n", (link == PHY_LINK_ON) ? "UP" : "DOWN");
    g_last_phy_link = link;
  }
  HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin,
                    (link == PHY_LINK_ON) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void log_socket_state(uint8_t state)
{
  const char *name;

  if (state == g_last_socket_state) {
    return;
  }
  g_last_socket_state = state;

  switch (state) {
    case SOCK_CLOSED:
      name = "CLOSED";
      break;
    case SOCK_INIT:
      name = "INIT";
      break;
    case SOCK_LISTEN:
      name = "LISTEN";
      printf("[TCP] listening on port %u\r\n", APP_TCP_PORT);
      return;
    case SOCK_ESTABLISHED:
      name = "ESTABLISHED";
      break;
    case SOCK_CLOSE_WAIT:
      name = "CLOSE_WAIT";
      break;
    default:
      name = "OTHER";
      break;
  }
  printf("[TCP] socket %u state %s (0x%02X)\r\n",
         APP_TCP_SOCKET, name, state);
}

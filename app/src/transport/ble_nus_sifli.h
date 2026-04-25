#ifndef BUDDY_BLE_NUS_SIFLI_H
#define BUDDY_BLE_NUS_SIFLI_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*buddy_ble_nus_rx_cb_t)(const uint8_t *data, uint16_t len);
typedef void (*buddy_ble_nus_passkey_cb_t)(uint32_t passkey);

void buddy_ble_nus_set_rx_callback(buddy_ble_nus_rx_cb_t cb);
void buddy_ble_nus_set_passkey_callback(buddy_ble_nus_passkey_cb_t cb);

int buddy_ble_nus_start(void);
int buddy_ble_nus_send(const uint8_t *data, uint16_t len);
int buddy_ble_nus_unpair(void);

bool buddy_ble_nus_is_connected(void);
bool buddy_ble_nus_is_encrypted(void);
uint16_t buddy_ble_nus_mtu_payload(void);

#ifdef __cplusplus
}
#endif

#endif

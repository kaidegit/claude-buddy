#include "ble_nus_sifli.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "bf0_ble_common.h"
#include "bf0_ble_gap.h"
#include "bf0_ble_gatt.h"
#include "bf0_sibles.h"
#include "bf0_sibles_advertising.h"
#include "ble_connection_manager.h"
#include "rtthread.h"

#define DBG_TAG "buddy_ble"
#define DBG_LVL DBG_INFO
#include "rtdbg.h"

#define SERIAL_UUID_16(x) {((uint8_t)((x) & 0xff)), ((uint8_t)((x) >> 8))}

enum buddy_nus_att
{
    BUDDY_NUS_SVC,
    BUDDY_NUS_RX_CHAR,
    BUDDY_NUS_RX_VALUE,
    BUDDY_NUS_TX_CHAR,
    BUDDY_NUS_TX_VALUE,
    BUDDY_NUS_TX_CCCD,
    BUDDY_NUS_ATT_NB
};

#define BUDDY_NUS_SERVICE_UUID \
    {0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e}
#define BUDDY_NUS_RX_UUID \
    {0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e}
#define BUDDY_NUS_TX_UUID \
    {0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e}

#define BUDDY_NUS_MAX_ATTR_LEN 1024
#define BUDDY_NUS_MAX_NOTIFY_CHUNK 180
#define BUDDY_NUS_DEFAULT_MTU 23
#define BUDDY_NUS_WORKER_STACK 2048
#define BUDDY_NUS_WORKER_PRIORITY (RT_THREAD_PRIORITY_MAX / 2)
#define BUDDY_NUS_NAME_PREFIX "Claude"
#define BUDDY_NUS_ADV_FLAGS 0x06

typedef struct
{
    rt_mailbox_t mailbox;
    sibles_hdl service_handle;
    uint8_t conn_idx;
    uint16_t mtu;
    bool started;
    bool service_ready;
    bool connected;
    bool encrypted;
    bool tx_notify_enabled;
    uint16_t tx_cccd;
    buddy_ble_nus_rx_cb_t rx_cb;
    buddy_ble_nus_passkey_cb_t passkey_cb;
} buddy_ble_nus_env_t;

static buddy_ble_nus_env_t g_nus = {
    .conn_idx = INVALID_CONN_IDX,
    .mtu = BUDDY_NUS_DEFAULT_MTU,
};

static uint8_t g_nus_service_uuid[ATT_UUID_128_LEN] = BUDDY_NUS_SERVICE_UUID;

BLE_GATT_SERVICE_DEFINE_128(g_nus_att_db)
{
    BLE_GATT_SERVICE_DECLARE(BUDDY_NUS_SVC, SERIAL_UUID_16_PRI_SERVICE, BLE_GATT_PERM_READ_ENABLE),
    BLE_GATT_CHAR_DECLARE(BUDDY_NUS_RX_CHAR, SERIAL_UUID_16_CHARACTERISTIC, BLE_GATT_PERM_READ_ENABLE),
    BLE_GATT_CHAR_VALUE_DECLARE(BUDDY_NUS_RX_VALUE, BUDDY_NUS_RX_UUID,
                                BLE_GATT_PERM_WRITE_REQ_ENABLE |
                                BLE_GATT_PERM_WRITE_COMMAND_ENABLE |
                                BLE_GATT_PERM_WRITE_PERMISSION_SEC_CON,
                                BLE_GATT_VALUE_PERM_UUID_128 | BLE_GATT_VALUE_PERM_RI_ENABLE,
                                BUDDY_NUS_MAX_ATTR_LEN),
    BLE_GATT_CHAR_DECLARE(BUDDY_NUS_TX_CHAR, SERIAL_UUID_16_CHARACTERISTIC, BLE_GATT_PERM_READ_ENABLE),
    BLE_GATT_CHAR_VALUE_DECLARE(BUDDY_NUS_TX_VALUE, BUDDY_NUS_TX_UUID,
                                BLE_GATT_PERM_NOTIFY_ENABLE |
                                BLE_GATT_PERM_NOTIFY_PERMISSION_SEC_CON,
                                BLE_GATT_VALUE_PERM_UUID_128 | BLE_GATT_VALUE_PERM_RI_ENABLE,
                                BUDDY_NUS_MAX_ATTR_LEN),
    BLE_GATT_DESCRIPTOR_DECLARE(BUDDY_NUS_TX_CCCD, SERIAL_UUID_16_CLIENT_CHAR_CFG,
                                BLE_GATT_PERM_READ_ENABLE |
                                BLE_GATT_PERM_WRITE_REQ_ENABLE |
                                BLE_GATT_PERM_READ_PERMISSION_SEC_CON |
                                BLE_GATT_PERM_WRITE_PERMISSION_SEC_CON,
                                BLE_GATT_VALUE_PERM_RI_ENABLE,
                                2),
};

SIBLES_ADVERTISING_CONTEXT_DECLAR(g_buddy_nus_advertising_context);

uint8_t sibles_advertising_disc_mode_get(void)
{
    return GAPM_ADV_MODE_CUSTOMIZE;
}

static void buddy_ble_nus_worker(void *parameter);
static void buddy_ble_nus_service_init(void);
static void buddy_ble_nus_advertising_start(void);

void buddy_ble_nus_set_rx_callback(buddy_ble_nus_rx_cb_t cb)
{
    g_nus.rx_cb = cb;
}

void buddy_ble_nus_set_passkey_callback(buddy_ble_nus_passkey_cb_t cb)
{
    g_nus.passkey_cb = cb;
}

int buddy_ble_nus_start(void)
{
    rt_thread_t worker;

    if (g_nus.started)
    {
        return 0;
    }

    g_nus.mailbox = rt_mb_create("nus", 4, RT_IPC_FLAG_FIFO);
    if (g_nus.mailbox == RT_NULL)
    {
        return -1;
    }

    worker = rt_thread_create("nus_evt", buddy_ble_nus_worker, RT_NULL,
                              BUDDY_NUS_WORKER_STACK, BUDDY_NUS_WORKER_PRIORITY, 10);
    if (worker == RT_NULL)
    {
        rt_mb_delete(g_nus.mailbox);
        g_nus.mailbox = RT_NULL;
        return -1;
    }

    g_nus.started = true;
    rt_thread_startup(worker);
    sifli_ble_enable();
    return 0;
}

int buddy_ble_nus_unpair(void)
{
    return (int)connection_manager_delete_all_bond();
}

bool buddy_ble_nus_is_connected(void)
{
    return g_nus.connected;
}

bool buddy_ble_nus_is_encrypted(void)
{
    if (!g_nus.connected || g_nus.conn_idx == INVALID_CONN_IDX)
    {
        return false;
    }

    return connection_manager_get_enc_state(g_nus.conn_idx) == ENC_STATE_ON || g_nus.encrypted;
}

uint16_t buddy_ble_nus_mtu_payload(void)
{
    if (g_nus.mtu <= 3)
    {
        return 20;
    }

    return (uint16_t)(g_nus.mtu - 3);
}

int buddy_ble_nus_send(const uint8_t *data, uint16_t len)
{
    uint16_t sent = 0;
    uint16_t payload;

    if (data == NULL || len == 0)
    {
        return 0;
    }

    if (!g_nus.connected || !g_nus.tx_notify_enabled || g_nus.service_handle == 0)
    {
        LOG_I("send blocked connected=%u notify=%u svc=%u len=%u",
              g_nus.connected ? 1 : 0,
              g_nus.tx_notify_enabled ? 1 : 0,
              (unsigned int)g_nus.service_handle,
              len);
        return -1;
    }

    payload = buddy_ble_nus_mtu_payload();
    if (payload > BUDDY_NUS_MAX_NOTIFY_CHUNK)
    {
        payload = BUDDY_NUS_MAX_NOTIFY_CHUNK;
    }

    while (sent < len)
    {
        int ret;
        int retries = 20;
        uint16_t chunk = (uint16_t)(len - sent);
        sibles_value_t value;

        if (chunk > payload)
        {
            chunk = payload;
        }

        value.hdl = g_nus.service_handle;
        value.idx = BUDDY_NUS_TX_VALUE;
        value.len = chunk;
        value.value = (uint8_t *)&data[sent];

        do
        {
            ret = sibles_write_value(g_nus.conn_idx, &value);
            if (ret == chunk)
            {
                break;
            }
            rt_thread_mdelay(10);
        } while (--retries > 0);

        if (ret != chunk)
        {
            LOG_I("send failed ret=%d chunk=%u sent=%u", ret, chunk, sent);
            return sent > 0 ? (int)sent : -2;
        }

        sent = (uint16_t)(sent + chunk);
        rt_thread_mdelay(1);
    }

    return (int)sent;
}

static void buddy_ble_nus_worker(void *parameter)
{
    (void)parameter;

    while (1)
    {
        rt_ubase_t value;
        if (rt_mb_recv(g_nus.mailbox, &value, RT_WAITING_FOREVER) != RT_EOK)
        {
            continue;
        }

        if (value == BLE_POWER_ON_IND)
        {
            g_nus.mtu = BUDDY_NUS_DEFAULT_MTU;
            buddy_ble_nus_service_init();
            connection_manager_set_bond_ack(BOND_PENDING);
            connection_manager_set_security_level(3);
            connection_manager_set_connected_auth(GAP_AUTH_REQ_SEC_CON_MITM_BOND);
            buddy_ble_nus_advertising_start();
            LOG_I("NUS advertising");
        }
    }
}

static uint8_t buddy_ble_nus_adv_event(uint8_t event, void *context, void *data)
{
    (void)context;

    if (event == SIBLES_ADV_EVT_ADV_STARTED)
    {
        sibles_adv_evt_startted_t *evt = (sibles_adv_evt_startted_t *)data;
        LOG_I("adv started status=%u mode=%u", evt->status, evt->adv_mode);
    }
    else if (event == SIBLES_ADV_EVT_ADV_STOPPED)
    {
        sibles_adv_evt_stopped_t *evt = (sibles_adv_evt_stopped_t *)data;
        LOG_I("adv stopped reason=%u mode=%u", evt->reason, evt->adv_mode);
    }

    return 0;
}

static void buddy_ble_nus_make_name(char *name, size_t size)
{
    bd_addr_t addr;

    if (size == 0)
    {
        return;
    }

    if (ble_get_public_address(&addr) == HL_ERR_NO_ERROR)
    {
        rt_snprintf(name, size, "Claude-%02X%02X", addr.addr[1], addr.addr[0]);
    }
    else
    {
        rt_snprintf(name, size, "Claude-0000");
    }
}

static void buddy_ble_nus_advertising_start(void)
{
    sibles_advertising_para_t para = {0};
    sibles_adv_type_name_t *name_field = RT_NULL;
    sibles_adv_type_name_t *short_name_field = RT_NULL;
    sibles_adv_type_srv_uuid_t *uuid_field = RT_NULL;
    ble_gap_dev_name_t *dev_name = RT_NULL;
    char local_name[16];
    uint8_t ret;

    buddy_ble_nus_make_name(local_name, sizeof(local_name));

    dev_name = (ble_gap_dev_name_t *)rt_malloc(sizeof(ble_gap_dev_name_t) + strlen(local_name));
    if (dev_name != RT_NULL)
    {
        dev_name->len = (uint8_t)strlen(local_name);
        rt_memcpy(dev_name->name, local_name, dev_name->len);
        ble_gap_set_dev_name(dev_name);
        rt_free(dev_name);
    }

    para.own_addr_type = GAPM_STATIC_ADDR;
    para.config.adv_mode = SIBLES_ADV_CONNECT_MODE;
    para.config.mode_config.conn_config.duration = 0;
    para.config.mode_config.conn_config.interval = 0x30;
    para.config.is_auto_restart = 1;
    para.config.max_tx_pwr = 0x7f;
    para.evt_handler = buddy_ble_nus_adv_event;

    uuid_field = (sibles_adv_type_srv_uuid_t *)rt_malloc(sizeof(sibles_adv_type_srv_uuid_t) + sizeof(sibles_adv_uuid_t));
    short_name_field = (sibles_adv_type_name_t *)rt_malloc(sizeof(sibles_adv_type_name_t) + strlen(BUDDY_NUS_NAME_PREFIX));
    name_field = (sibles_adv_type_name_t *)rt_malloc(sizeof(sibles_adv_type_name_t) + strlen(local_name));
    if (uuid_field == RT_NULL || short_name_field == RT_NULL || name_field == RT_NULL)
    {
        rt_free(uuid_field);
        rt_free(short_name_field);
        rt_free(name_field);
        return;
    }

    uuid_field->count = 1;
    uuid_field->uuid_list[0].uuid_len = ATT_UUID_128_LEN;
    rt_memcpy(uuid_field->uuid_list[0].uuid.uuid_128, g_nus_service_uuid, ATT_UUID_128_LEN);

    short_name_field->name_len = (uint8_t)strlen(BUDDY_NUS_NAME_PREFIX);
    rt_memcpy(short_name_field->name, BUDDY_NUS_NAME_PREFIX, short_name_field->name_len);

    /* Keep Claude Desktop's name-prefix and NUS-service filters in the primary legacy ADV packet. */
    para.adv_data.flags = BUDDY_NUS_ADV_FLAGS;
    para.adv_data.shortened_name = short_name_field;
    para.adv_data.completed_uuid = uuid_field;

    name_field->name_len = (uint8_t)strlen(local_name);
    rt_memcpy(name_field->name, local_name, name_field->name_len);
    para.rsp_data.completed_name = name_field;

    ret = sibles_advertising_init(g_buddy_nus_advertising_context, &para);
    if (ret == SIBLES_ADV_NO_ERR)
    {
        sibles_advertising_start(g_buddy_nus_advertising_context);
    }
    else
    {
        LOG_I("adv init failed=%u", ret);
    }

    rt_free(uuid_field);
    rt_free(short_name_field);
    rt_free(name_field);
}

static uint8_t *buddy_ble_nus_get_cbk(uint8_t conn_idx, uint8_t idx, uint16_t *len)
{
    (void)conn_idx;
    *len = 0;
    if (idx == BUDDY_NUS_TX_CCCD)
    {
        *len = sizeof(g_nus.tx_cccd);
        LOG_I("CCCD read value=0x%04x", g_nus.tx_cccd);
        return (uint8_t *)&g_nus.tx_cccd;
    }

    return RT_NULL;
}

static uint8_t buddy_ble_nus_set_cbk(uint8_t conn_idx, sibles_set_cbk_t *para)
{
    (void)conn_idx;

    if (para == RT_NULL)
    {
        return SIBLES_ATT_ERR_INVALID_PDU;
    }

    if (para->idx == BUDDY_NUS_RX_VALUE)
    {
        LOG_I("RX len=%u", para->len);
        if (g_nus.rx_cb != NULL && para->value != RT_NULL && para->len > 0)
        {
            g_nus.rx_cb(para->value, para->len);
        }
    }
    else if (para->idx == BUDDY_NUS_TX_CCCD)
    {
        if (para->len >= sizeof(g_nus.tx_cccd) && para->value != RT_NULL)
        {
            g_nus.tx_cccd = (uint16_t)para->value[0] | ((uint16_t)para->value[1] << 8);
        }
        else if (para->len > 0 && para->value != RT_NULL)
        {
            g_nus.tx_cccd = para->value[0];
        }
        else
        {
            g_nus.tx_cccd = 0;
        }
        g_nus.tx_notify_enabled = (g_nus.tx_cccd & 0x0001U) != 0;
        LOG_I("CCCD write value=0x%04x notify=%u",
              g_nus.tx_cccd,
              g_nus.tx_notify_enabled ? 1 : 0);
    }

    return SIBLES_ATT_ERR_NO_ERROR;
}

static void buddy_ble_nus_service_init(void)
{
    BLE_GATT_SERVICE_INIT_128(svc, g_nus_att_db, BUDDY_NUS_ATT_NB,
                              BLE_GATT_SERVICE_PERM_NOAUTH |
                              BLE_GATT_SERVICE_PERM_UUID_128,
                              g_nus_service_uuid);

    if (g_nus.service_ready)
    {
        return;
    }

    g_nus.service_handle = sibles_register_svc_128(&svc);
    if (g_nus.service_handle != 0)
    {
        sibles_register_cbk(g_nus.service_handle, buddy_ble_nus_get_cbk, buddy_ble_nus_set_cbk);
        g_nus.service_ready = true;
    }
    else
    {
        LOG_I("NUS service registration failed");
    }
}

static void buddy_ble_nus_handle_bond_info(connection_manager_bond_ack_infor_t *info)
{
    if (info == RT_NULL)
    {
        return;
    }

    if (info->request == GAPC_PAIRING_REQ)
    {
        connection_manager_bond_ack_reply(info->conn_idx, GAPC_PAIRING_REQ, true);
    }
    else if (info->request == GAPC_TK_EXCH)
    {
        if (info->type == GAP_TK_DISPLAY)
        {
            if (g_nus.passkey_cb != NULL)
            {
                g_nus.passkey_cb(info->confirm_data);
            }
            LOG_I("passkey %06u", (unsigned int)info->confirm_data);
            connection_manager_bond_ack_reply(info->conn_idx, GAPC_TK_EXCH, true);
        }
        else
        {
            connection_manager_bond_ack_reply(info->conn_idx, GAPC_TK_EXCH, false);
        }
    }
    else if (info->request == GAPC_NC_EXCH)
    {
        if (g_nus.passkey_cb != NULL)
        {
            g_nus.passkey_cb(info->confirm_data);
        }
        LOG_I("numeric comparison %06u", (unsigned int)info->confirm_data);
        connection_manager_bond_ack_reply(info->conn_idx, GAPC_NC_EXCH, true);
    }
}

static int buddy_ble_nus_event_handler(uint16_t event_id, uint8_t *data, uint16_t len, uint32_t context)
{
    (void)len;
    (void)context;

    switch (event_id)
    {
    case BLE_POWER_ON_IND:
        if (g_nus.mailbox != RT_NULL)
        {
            rt_mb_send(g_nus.mailbox, BLE_POWER_ON_IND);
        }
        break;
    case BLE_GAP_CONNECTED_IND:
    {
        ble_gap_connect_ind_t *ind = (ble_gap_connect_ind_t *)data;
        g_nus.conn_idx = ind->conn_idx;
        g_nus.connected = true;
        g_nus.encrypted = false;
        g_nus.tx_notify_enabled = false;
        g_nus.tx_cccd = 0;
        g_nus.mtu = BUDDY_NUS_DEFAULT_MTU;
        LOG_I("connected idx=%u", g_nus.conn_idx);
        break;
    }
    case BLE_GAP_DISCONNECTED_IND:
    {
        ble_gap_disconnected_ind_t *ind = (ble_gap_disconnected_ind_t *)data;
        g_nus.conn_idx = INVALID_CONN_IDX;
        g_nus.connected = false;
        g_nus.encrypted = false;
        g_nus.tx_notify_enabled = false;
        g_nus.tx_cccd = 0;
        g_nus.mtu = BUDDY_NUS_DEFAULT_MTU;
        LOG_I("disconnected reason=%u", ind != RT_NULL ? ind->reason : 0xff);
        break;
    }
    case SIBLES_MTU_EXCHANGE_IND:
    {
        sibles_mtu_exchange_ind_t *ind = (sibles_mtu_exchange_ind_t *)data;
        if (ind->conn_idx == g_nus.conn_idx)
        {
            g_nus.mtu = ind->mtu;
            LOG_I("MTU=%u", g_nus.mtu);
        }
        break;
    }
    case CONNECTION_MANAGER_ENCRYPT_IND_EVENT:
    {
        connection_manager_encrypt_ind_t *ind = (connection_manager_encrypt_ind_t *)data;
        if (ind->conn_idx == g_nus.conn_idx)
        {
            g_nus.encrypted = connection_manager_get_enc_state(g_nus.conn_idx) == ENC_STATE_ON;
            LOG_I("encrypt auth=%u encrypted=%u",
                  ind->auth,
                  g_nus.encrypted ? 1 : 0);
        }
        break;
    }
    case CONNECTION_MANAGER_BOND_AUTH_INFOR:
        buddy_ble_nus_handle_bond_info((connection_manager_bond_ack_infor_t *)data);
        break;
    case CONNECTION_MANAGER_PAIRING_SUCCEED:
        LOG_I("pairing succeeded");
        break;
    case CONNECTION_MANAGER_PAIRING_FAILED:
    {
        ble_gap_bond_ind_t *ind = (ble_gap_bond_ind_t *)data;
        LOG_I("pairing failed reason=%u",
              ind != RT_NULL ? ind->data.reason : 0xff);
        break;
    }
    default:
        break;
    }

    return 0;
}
BLE_EVENT_REGISTER(buddy_ble_nus_event_handler, NULL);

#ifndef NVDS_AUTO_UPDATE_MAC_ADDRESS_ENABLE
ble_common_update_type_t ble_request_public_address(bd_addr_t *addr)
{
    int ret = bt_mac_addr_generate_via_uid_v2(addr);
    return ret == 0 ? BLE_UPDATE_ONCE : BLE_UPDATE_NO_UPDATE;
}
#endif

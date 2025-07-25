/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2023. All rights reserved.
 * Description: sle uuid server sample.
 */
#include "app_init.h"
// #include "watchdog.h"
#include "tcxo.h"
#include "systick.h"
// #include "los_memory.h"
#include "securec.h"
#include "errcode.h"
#include "osal_addr.h"
#include "soc_osal.h"
#include "common_def.h"
#include "pm_clock.h"
#include "sle_device_manager.h"

#include "sle_common.h"
#include "sle_errcode.h"
#include "sle_ssap_server.h"
#include "sle_connection_manager.h"
#include "sle_device_discovery.h"
#include "sle_transmition_manager.h"
#include "nv.h"

#include "sle_speed_server_adv.h"
#include "sle_speed_server.h"

#define OCTET_BIT_LEN 8
#define UUID_LEN_2     2
#define BT_INDEX_4     4
#define BT_INDEX_5     5
#define BT_INDEX_0     0
extern void send_data_thread_function(void);
#define encode2byte_little(_ptr, data) \
    do { \
        *(uint8_t *)((_ptr) + 1) = (uint8_t)((data) >> 8); \
        *(uint8_t *)(_ptr) = (uint8_t)(data); \
    } while (0)

/* sle server app uuid for test */
static char g_sle_uuid_app_uuid[UUID_LEN_2] = {0x0, 0x0};
/* server notify property uuid for test */
static char g_sle_property_value[OCTET_BIT_LEN] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
/* sle connect acb handle */
static uint16_t g_sle_conn_hdl = 0;
/* sle server handle */
static uint8_t g_server_id = 0;
/* sle service handle */
static uint16_t g_service_handle = 0;
/* sle ntf property handle */
static uint16_t g_property_handle = 0;
#ifdef SLE_QOS_FLOWCTRL_FUNCTION_SWITCH
static sle_link_qos_state_t g_sle_link_state = 0;  /* sle link state */
#endif

// #ifdef CONFIG_LARGE_THROUGHPUT_SERVER
// #define PKT_DATA_LEN 1200
// #define SPEED_DEFAULT_CONN_INTERVAL 0x14
#define PKT_DATA_LEN CONFIG_PKT_DATA_LEN
#define SPEED_DEFAULT_CONN_INTERVAL CONFIG_SPEED_DEFAULT_CONN_INTERVAL
// #else
// #define PKT_DATA_LEN 600
// #define SPEED_DEFAULT_CONN_INTERVAL 0xA0
// #endif

#define SPEED_DEFAULT_KTHREAD_SIZE 0x2000
#define SPEED_DEFAULT_KTHREAD_PROI 26
// #define DEFAULT_SLE_SPEED_DATA_LEN 1500
// #define DEFAULT_SLE_SPEED_MTU_SIZE 1500
#define DEFAULT_SLE_SPEED_DATA_LEN 300
#define DEFAULT_SLE_SPEED_MTU_SIZE 300
#define SPEED_DEFAULT_TIMEOUT_MULTIPLIER 0x1f4
#define SPEED_DEFAULT_SCAN_INTERVAL 400
#define SPEED_DEFAULT_SCAN_WINDOW 20
static unsigned char data[PKT_DATA_LEN];

static uint8_t sle_uuid_base[] = { 0x37, 0xBE, 0xA8, 0x80, 0xFC, 0x70, 0x11, 0xEA, \
    0xB7, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

static void sle_uuid_set_base(sle_uuid_t *out)
{
    (void)memcpy_s(out->uuid, SLE_UUID_LEN, sle_uuid_base, SLE_UUID_LEN);
    out->len = UUID_LEN_2;
}

static void sle_uuid_setu2(uint16_t u2, sle_uuid_t *out)
{
    sle_uuid_set_base(out);
    out->len = UUID_LEN_2;
    encode2byte_little(&out->uuid[14], u2);
}

static void ssaps_read_request_cbk(uint8_t server_id, uint16_t conn_id, ssaps_req_read_cb_t *read_cb_para,
    errcode_t status)
{
    osal_printk("[speed server] ssaps read request cbk server_id:%x, conn_id:%x, handle:%x, status:%x\r\n",
        server_id, conn_id, read_cb_para->handle, status);
    osal_task *task_handle = NULL;
    osal_kthread_lock();
    task_handle = osal_kthread_create((osal_kthread_handler)send_data_thread_function,
        0, "RadarTask", SPEED_DEFAULT_KTHREAD_SIZE);
    osal_kthread_set_priority(task_handle, SPEED_DEFAULT_KTHREAD_PROI + 1);
    if (task_handle != NULL) {
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
    osal_printk("kthread success\r\n");
}

static void ssaps_write_request_cbk(uint8_t server_id, uint16_t conn_id, ssaps_req_write_cb_t *write_cb_para,
    errcode_t status)
{
    osal_printk("[speed server] ssaps write request cbk server_id:%d, conn_id:%d, handle:%d, status:%d\r\n",
        server_id, conn_id, write_cb_para->handle, status);
}

static void ssaps_mtu_changed_cbk(uint8_t server_id, uint16_t conn_id,  ssap_exchange_info_t *mtu_size,
    errcode_t status)
{
    osal_printk("[speed server] ssaps write request cbk server_id:%d, conn_id:%d, mtu_size:%d, status:%d\r\n",
        server_id, conn_id, mtu_size->mtu_size, status);
}

static void ssaps_start_service_cbk(uint8_t server_id, uint16_t handle, errcode_t status)
{
    osal_printk("[speed server] start service cbk server_id:%d, handle:%d, status:%d\r\n",
        server_id, handle, status);
}

static void sle_ssaps_register_cbks(void)
{
    ssaps_callbacks_t ssaps_cbk = {0};
    ssaps_cbk.start_service_cb = ssaps_start_service_cbk;
    ssaps_cbk.mtu_changed_cb = ssaps_mtu_changed_cbk;
    ssaps_cbk.read_request_cb = ssaps_read_request_cbk;
    ssaps_cbk.write_request_cb = ssaps_write_request_cbk;
    ssaps_register_callbacks(&ssaps_cbk);
}

errcode_t sle_uuid_server_send_report_by_handle_id(uint8_t *data, uint16_t len, uint16_t connect_id)
{
    // osal_printk("[speed server] sle uuid server send report by handle id, connect_id:%d, len:%d\r\n",
    //     connect_id, len);
    ssaps_ntf_ind_t param = {0};
    param.handle = g_property_handle;
    param.type = SSAP_PROPERTY_TYPE_VALUE;
    param.value = data;
    param.value_len = len;
    ssaps_notify_indicate(g_server_id, connect_id, &param);
    return ERRCODE_SLE_SUCCESS;
}

#ifdef SLE_QOS_FLOWCTRL_FUNCTION_SWITCH
static void sle_send_data_cbk(uint16_t conn_id, sle_link_qos_state_t link_state)
{
    conn_id = conn_id;
    g_sle_link_state = link_state;
    osal_printk("%s enter, gle_tx_acb_data_num_get:%d.\n", __FUNCTION__, link_state);
}

static void sle_transmission_register_cbks(void)
{
    sle_transmission_callbacks_t trans_cbk = {0};
    trans_cbk.send_data_cb = sle_send_data_cbk;
    sle_transmission_register_callbacks(&trans_cbk);
}
#else
extern uint8_t gle_tx_acb_data_num_get(void);
#endif

uint8_t sle_flow_ctrl_flag(void)
{
#ifdef SLE_QOS_FLOWCTRL_FUNCTION_SWITCH
    return (g_sle_link_state <= SLE_QOS_FLOWCTRL) ? 1 : 0;
#else
    return gle_tx_acb_data_num_get();
#endif
}

void send_data_thread_function(void)
{
    sle_set_data_len(g_sle_conn_hdl, DEFAULT_SLE_SPEED_DATA_LEN);
// #ifdef CONFIG_LARGE_THROUGHPUT_SERVER
// #define DEFAULT_SLE_SPEED_MCS 10
#define DEFAULT_SLE_SPEED_MCS CONFIG_MCS
    sle_set_phy_t phy_parm = {
        // .tx_format = SLE_RADIO_FRAME_2,
        // .rx_format = SLE_RADIO_FRAME_2,
        // .tx_phy = SLE_PHY_4M,
        // .rx_phy = SLE_PHY_4M,
        .tx_format = CONFIG_FRAME_FORMAT,
        .rx_format = CONFIG_FRAME_FORMAT,
        .tx_phy = CONFIG_PHY,
        .rx_phy = CONFIG_PHY,
        .tx_pilot_density = SLE_PHY_PILOT_DENSITY_16_TO_1,
        .rx_pilot_density = SLE_PHY_PILOT_DENSITY_16_TO_1,
        .g_feedback = 0,
        .t_feedback = 0,
    };
    sle_set_phy_param(g_sle_conn_hdl, &phy_parm);
    // sle_set_mcs(g_sle_conn_hdl, DEFAULT_SLE_SPEED_MCS);
    // osal_printk("code: ploar MCS10, PHY 4MHZ, power: 20dbm \r\n");
    osal_printk("Frame format:%d, MCS:%d, PHY: %d\r\n", CONFIG_FRAME_FORMAT, DEFAULT_SLE_SPEED_MCS, CONFIG_PHY);
// #else
    // osal_printk("code: GFSK, PHY 1MHZ, power: 20dbm \r\n");
// #endif

//     int i = 0;
//     while (1) {
//         if (sle_flow_ctrl_flag() > 0) {
//             i++;
//             osal_printk("send data %d\r\n", i);
//             data[0] = (i >> 8) & 0xFF;  /* offset 8bits */
//             data[1] = i & 0xFF;
//             sle_uuid_server_send_report_by_handle_id(data, PKT_DATA_LEN, g_sle_conn_hdl);
//         }
// // #ifndef CONFIG_LARGE_THROUGHPUT_SERVER
//         // osal_msleep(10);     /* sleep 10ms */
//         // osal_msleep(1000);      /* sleep 1000ms */
//         i = 0;
// // #endif
//     }


// // int i = 0;
// osal_printk("sle_flow_ctrl_flag() = %d\r\n", sle_flow_ctrl_flag());
// while (1) {
//     if (sle_flow_ctrl_flag() > 0) {
//     // i++;
//     // // osal_printk("send data %d\r\n", i);
//     // data[0] = (i >> 8) & 0xFF;  /* offset 8bits */
//     // data[1] = i & 0xFF;
//     sle_uuid_server_send_report_by_handle_id(data, PKT_DATA_LEN, g_sle_conn_hdl);
//     }
// }

// int i = 0;
osal_msleep(3000); /* sleep 1000ms */
osal_printk("sle_flow_ctrl_flag() = %d\r\n", sle_flow_ctrl_flag());
int num = sle_flow_ctrl_flag();
while (1) {
    for (int i = 0; i < num; i++) {
        data[0] = (i >> 8) & 0xFF;  /* offset 8bits */
        data[1] = i & 0xFF;
        sle_uuid_server_send_report_by_handle_id(data, PKT_DATA_LEN, g_sle_conn_hdl);
    }
    osal_msleep(1); /* sleep 1ms */
    // sle_uuid_server_send_report_by_handle_id(data, PKT_DATA_LEN, g_sle_conn_hdl);
    // osal_msleep(1); /* sleep 1ms */
}
}

static errcode_t sle_uuid_server_service_add(void)
{
    errcode_t ret;
    sle_uuid_t service_uuid = {0};
    sle_uuid_setu2(SLE_UUID_SERVER_SERVICE, &service_uuid);
    ret = ssaps_add_service_sync(g_server_id, &service_uuid, 1, &g_service_handle);
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[speed server] sle uuid add service fail, ret:%x\r\n", ret);
        return ERRCODE_SLE_FAIL;
    }
    return ERRCODE_SLE_SUCCESS;
}

static errcode_t sle_uuid_server_property_add(void)
{
    errcode_t ret;
    ssaps_property_info_t property = {0};
    ssaps_desc_info_t descriptor = {0};
    uint8_t ntf_value[] = {0x01, 0x0};

    property.permissions = SLE_UUID_TEST_PROPERTIES;
    sle_uuid_setu2(SLE_UUID_SERVER_NTF_REPORT, &property.uuid);
    property.value = osal_vmalloc(sizeof(g_sle_property_value));
    property.operate_indication = SSAP_OPERATE_INDICATION_BIT_READ | SSAP_OPERATE_INDICATION_BIT_NOTIFY;
    if (property.value == NULL) {
        osal_printk("[speed server] sle property mem fail\r\n");
        return ERRCODE_SLE_FAIL;
    }
    if (memcpy_s(property.value, sizeof(g_sle_property_value), g_sle_property_value,
        sizeof(g_sle_property_value)) != EOK) {
        osal_vfree(property.value);
        osal_printk("[speed server] sle property mem cpy fail\r\n");
        return ERRCODE_SLE_FAIL;
    }
    ret = ssaps_add_property_sync(g_server_id, g_service_handle, &property,  &g_property_handle);
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[speed server] sle uuid add property fail, ret:%x\r\n", ret);
        osal_vfree(property.value);
        return ERRCODE_SLE_FAIL;
    }
    descriptor.permissions = SLE_UUID_TEST_DESCRIPTOR;
    descriptor.operate_indication = SSAP_OPERATE_INDICATION_BIT_READ | SSAP_OPERATE_INDICATION_BIT_WRITE;
    descriptor.type = SSAP_DESCRIPTOR_USER_DESCRIPTION;
    descriptor.value = ntf_value;
    descriptor.value_len = sizeof(ntf_value);

    ret = ssaps_add_descriptor_sync(g_server_id, g_service_handle, g_property_handle, &descriptor);
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[speed server] sle uuid add descriptor fail, ret:%x\r\n", ret);
        osal_vfree(property.value);
        return ERRCODE_SLE_FAIL;
    }
    osal_vfree(property.value);
    return ERRCODE_SLE_SUCCESS;
}

static errcode_t sle_uuid_server_add(void)
{
    errcode_t ret;
    sle_uuid_t app_uuid = {0};

    osal_printk("[speed server] sle uuid add service in\r\n");
    app_uuid.len = sizeof(g_sle_uuid_app_uuid);
    if (memcpy_s(app_uuid.uuid, app_uuid.len, g_sle_uuid_app_uuid, sizeof(g_sle_uuid_app_uuid)) != EOK) {
        return ERRCODE_SLE_FAIL;
    }
    ssaps_register_server(&app_uuid, &g_server_id);

    if (sle_uuid_server_service_add() != ERRCODE_SLE_SUCCESS) {
        ssaps_unregister_server(g_server_id);
        return ERRCODE_SLE_FAIL;
    }

    if (sle_uuid_server_property_add() != ERRCODE_SLE_SUCCESS) {
        ssaps_unregister_server(g_server_id);
        return ERRCODE_SLE_FAIL;
    }
    osal_printk("[speed server] sle uuid add service, server_id:%x, service_handle:%x, property_handle:%x\r\n",
        g_server_id, g_service_handle, g_property_handle);
    ret = ssaps_start_service(g_server_id, g_service_handle);
    if (ret != ERRCODE_SLE_SUCCESS) {
        osal_printk("[speed server] sle uuid add service fail, ret:%x\r\n", ret);
        return ERRCODE_SLE_FAIL;
    }
    osal_printk("[speed server] sle uuid add service out\r\n");
    return ERRCODE_SLE_SUCCESS;
}

static void sle_connect_state_changed_cbk(uint16_t conn_id, const sle_addr_t *addr,
    sle_acb_state_t conn_state, sle_pair_state_t pair_state, sle_disc_reason_t disc_reason)
{
    osal_printk("[speed server] connect state changed conn_id:0x%02x, conn_state:0x%x, pair_state:0x%x, \
        disc_reason:0x%x\r\n", conn_id, conn_state, pair_state, disc_reason);
    osal_printk("[speed server] connect state changed addr:%02x:**:**:**:%02x:%02x\r\n",
        addr->addr[BT_INDEX_0], addr->addr[BT_INDEX_4], addr->addr[BT_INDEX_5]);
    g_sle_conn_hdl = conn_id;
    sle_connection_param_update_t parame = {0};
    parame.conn_id = conn_id;
    parame.interval_min = SPEED_DEFAULT_CONN_INTERVAL;
    parame.interval_max = SPEED_DEFAULT_CONN_INTERVAL;
    parame.max_latency = 0;
    parame.supervision_timeout = SPEED_DEFAULT_TIMEOUT_MULTIPLIER;
    if (conn_state ==  SLE_ACB_STATE_CONNECTED) {
        sle_update_connect_param(&parame);
    } else if (conn_state == SLE_ACB_STATE_DISCONNECTED) {
        sle_start_announce(SLE_ADV_HANDLE_DEFAULT);
    }
}

static void sle_pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status)
{
    osal_printk("[speed server] pair complete conn_id:%02x, status:%x\r\n",
        conn_id, status);
    osal_printk("[speed server] pair complete addr:%02x:**:**:**:%02x:%02x\r\n",
        addr->addr[BT_INDEX_0], addr->addr[BT_INDEX_4], addr->addr[BT_INDEX_5]);
}

void sle_sample_update_cbk(uint16_t conn_id, errcode_t status, const sle_connection_param_update_evt_t *param)
{
    unused(status);
    osal_printk("[ssap server] updat state changed conn_id:%d, interval = %02x\n", conn_id, param->interval);
}

void sle_sample_update_req_cbk(uint16_t conn_id, errcode_t status, const sle_connection_param_update_req_t *param)
{
    unused(conn_id);
    unused(status);
    osal_printk("[ssap server] sle_sample_update_req_cbk interval_min:%02x, interval_max:%02x\n",
        param->interval_min, param->interval_max);
}

void sle_sample_rssi_cbk(uint16_t conn_id, int8_t rssi, errcode_t status)
{
    osal_printk("[ssap server] conn_id:%d, rssi = %c, status = %x\n", conn_id, rssi, status);
}

static void sle_conn_register_cbks(void)
{
    sle_connection_callbacks_t conn_cbks = {0};
    conn_cbks.connect_state_changed_cb = sle_connect_state_changed_cbk;
    conn_cbks.pair_complete_cb = sle_pair_complete_cbk;
    conn_cbks.connect_param_update_req_cb = sle_sample_update_req_cbk;
    conn_cbks.connect_param_update_cb = sle_sample_update_cbk;
    conn_cbks.read_rssi_cb = sle_sample_rssi_cbk;
    sle_connection_register_callbacks(&conn_cbks);
}

void sle_ssaps_set_info(void)
{
    ssap_exchange_info_t info = {0};
    info.mtu_size = DEFAULT_SLE_SPEED_MTU_SIZE;
    info.version = 1;
    ssaps_set_info(g_server_id, &info);
}

// void sle_speed_connect_param_init(void)
// {
//     sle_default_connect_param_t param = {0};
//     param.enable_filter_policy = 0;
//     param.gt_negotiate = 0;
//     param.initiate_phys = 1;
//     param.max_interval = SPEED_DEFAULT_CONN_INTERVAL;
//     param.min_interval = SPEED_DEFAULT_CONN_INTERVAL;
//     param.scan_interval = SPEED_DEFAULT_SCAN_INTERVAL;
//     param.scan_window = SPEED_DEFAULT_SCAN_WINDOW;
//     param.timeout = SPEED_DEFAULT_TIMEOUT_MULTIPLIER;
//     sle_default_connection_param_set(&param);
// }

void sle_set_local_addr_init(void)
{
    sle_addr_t addr = {0};
    // uint8_t mac[SLE_ADDR_LEN] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    uint8_t mac[SLE_ADDR_LEN] = {CONFIG_SLE_MAC_ADDR_1ST_BYTE, CONFIG_SLE_MAC_ADDR_2ND_BYTE, CONFIG_SLE_MAC_ADDR_3RD_BYTE, CONFIG_SLE_MAC_ADDR_4TH_BYTE, CONFIG_SLE_MAC_ADDR_5TH_BYTE, CONFIG_SLE_MAC_ADDR_6TH_BYTE};
    addr.type = 0;
    memcpy_s(addr.addr, SLE_ADDR_LEN, mac, SLE_ADDR_LEN);
    sle_set_local_addr(&addr);
}

// void sle_speed_server_set_nv(void)
// {
//     // uint16_t nv_value_len = 0;
//     uint8_t nv_value = 0;
//     // uapi_nv_read(0x20A0, sizeof(uint16_t), &nv_value_len, &nv_value);
//     // if (nv_value != 7) {     // 7:btc功率档位
//     //     nv_value = 7;       // 7:btc功率档位
//     //     uapi_nv_write(0x20A0, (uint8_t *)&(nv_value), sizeof(nv_value));
//     // }
//     nv_value = CONFIG_MAX_TX_POWER_LEVEL;
//     uapi_nv_write(0x20A0, (uint8_t *)&(nv_value), sizeof(nv_value));
//     osal_printk("[speed server] The value of nv is set to %d.\r\n", nv_value);
// }

void sle_speed_server_set_nv(void)
{
    //     #define BTH_BLE_NV_RESERVED_ID 0xE 
    // errcode_t uapi_nv_read(uint16_t key, uint16_t kvalue_max_length, uint16_t *kvalue_length, uint8_t *kvalue); 
    // errcode_t uapi_nv_write(uint16_t key, const uint8_t *kvalue, uint16_t kvalue_length); 
    errcode_t ret;
    uint16_t nv_value_len = 0;
    #ifndef BTH_BLE_RESERVE_LEN
        #define BTH_BLE_RESERVE_LEN 128
    #endif
    #ifndef BTH_BLE_NV_RESERVED_ID
        #define BTH_BLE_NV_RESERVED_ID 0xE
    #endif
    uint8_t nv_value[BTH_BLE_RESERVE_LEN] = {0};  // nv_value 是一个字节数组，用于存储读取到的数据
    ret = uapi_nv_read(BTH_BLE_NV_RESERVED_ID, sizeof(nv_value), &nv_value_len, nv_value);
    if (ret != ERRCODE_SUCC) {
        osal_printk("[speed server] uapi_nv_read fail, ret:%x\r\n", ret);
        return;
    }
    osal_printk("[speed server] Read NV value: ");
    for (int i = 0; i < nv_value_len; i++) {
        osal_printk("nv_value[%d]: %d ", i, nv_value[i]);
    }
    osal_printk(", length: %d\r\n", nv_value_len);
}

/* 初始化speed server */
errcode_t sle_speed_server_init(void)
{
    // uapi_watchdog_disable();
    enable_sle();
    osal_printk("sle enable\r\n");
    sle_speed_server_set_nv();
    sle_conn_register_cbks();
    sle_ssaps_register_cbks();
    sle_uuid_server_add();
    sle_uuid_server_adv_init();
    sle_ssaps_set_info();
#ifdef SLE_QOS_FLOWCTRL_FUNCTION_SWITCH
    sle_transmission_register_cbks();
#endif
    // sle_speed_connect_param_init();
    sle_set_local_addr_init();
    osal_printk("[speed server] init ok\r\n");
    return ERRCODE_SLE_SUCCESS;
}

// errcode_t sle_enable_server_cbk(void)
// {
//     errcode_t ret;
//     ret = sle_uart_server_add();
//     if (ret != ERRCODE_SLE_SUCCESS) {
//         sample_at_log_print("%s sle_uart_server_init,sle_uart_server_add fail :%x\r\n", SLE_UART_SERVER_LOG, ret);
//         return ret;
//     }
//     ret = sle_uart_server_adv_init();
//     if (ret != ERRCODE_SLE_SUCCESS) {
//         sample_at_log_print("%s sle_uart_server_init,sle_uart_server_adv_init fail :%x\r\n", SLE_UART_SERVER_LOG, ret);
//         return ret;
//     }
//     return ERRCODE_SLE_SUCCESS;
// }

int sle_speed_init(void)
{
    for (int i = 0; i < PKT_DATA_LEN; i++) {
        data[i] = 'A';
        data[PKT_DATA_LEN - 1] = '\0';
    }
    osal_printk("sle speed server init, enter\r\n");
    osal_printk("CONFIG_SPEED_DEFAULT_CONN_INTERVAL = %d\n", CONFIG_SPEED_DEFAULT_CONN_INTERVAL);
    osal_printk("CONFIG_PKT_DATA_LEN = %d\n", CONFIG_PKT_DATA_LEN);
    osal_printk("CONFIG_FRAME_FORMAT = %d\n", CONFIG_FRAME_FORMAT);
    osal_printk("CONFIG_PHY = %d\n", CONFIG_PHY);
    osal_printk("CONFIG_MCS = %d\n", CONFIG_MCS);
    osal_printk("CONFIG_MAX_TX_POWER_LEVEL = %d\n", CONFIG_MAX_TX_POWER_LEVEL);
    osal_printk("CONFIG_SLE_MAC_ADDR_1ST_BYTE = %02x\n", CONFIG_SLE_MAC_ADDR_1ST_BYTE);
    osal_printk("CONFIG_SLE_MAC_ADDR_2ND_BYTE = %02x\n", CONFIG_SLE_MAC_ADDR_2ND_BYTE);
    osal_printk("CONFIG_SLE_MAC_ADDR_3RD_BYTE = %02x\n", CONFIG_SLE_MAC_ADDR_3RD_BYTE);
    osal_printk("CONFIG_SLE_MAC_ADDR_4TH_BYTE = %02x\n", CONFIG_SLE_MAC_ADDR_4TH_BYTE);
    osal_printk("CONFIG_SLE_MAC_ADDR_5TH_BYTE = %02x\n", CONFIG_SLE_MAC_ADDR_5TH_BYTE);
    osal_printk("CONFIG_SLE_MAC_ADDR_6TH_BYTE = %02x\n", CONFIG_SLE_MAC_ADDR_6TH_BYTE);
    osal_msleep(1000);  /* sleep 1000ms */
    sle_speed_server_init();
    return 0;
}

static void sle_speed_entry(void)
{
    osal_task *task_handle1 = NULL;
    if (uapi_clock_control(CLOCK_CONTROL_FREQ_LEVEL_CONFIG, CLOCK_FREQ_LEVEL_HIGH) == ERRCODE_SUCC) {
        osal_printk("Clock config succ.\r\n");
    } else {
        osal_printk("Clock config fail.\r\n");
    }
    #ifdef BS21_PRODUCT_EVB
        osal_printk("BS21_PRODUCT_EVB is defined.\n");
    #endif
    #ifdef BTH_HIGH_POWER
        printf("BTH_HIGH_POWER is defined.\n");
    #else
        printf("BTH_HIGH_POWER is not defined.\n");
    #endif
    sle_dev_register_cbks();
    osal_kthread_lock();
    task_handle1= osal_kthread_create((osal_kthread_handler)sle_speed_init, 0, "speed", SPEED_DEFAULT_KTHREAD_SIZE);
    if (task_handle1 != NULL) {
        osal_kthread_set_priority(task_handle1, SPEED_DEFAULT_KTHREAD_PROI);
        osal_kfree(task_handle1);
    }
    osal_kthread_unlock();
}

/* Run the blinky_entry. */
app_run(sle_speed_entry);


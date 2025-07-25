/*
 * Copyright (c) HiSilicon (Shanghai) Technologies Co., Ltd. 2022. All rights reserved.
 * Description: ble speed client sample.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "securec.h"
#include "app_init.h"
#include "systick.h"
#include "soc_osal.h"
#include "osal_list.h"
#include "common_def.h"

#include "bts_le_gap.h"
#include "bts_gatt_client.h"
#include "ble_speed_client.h"

#include "bts_device_manager.h"

#include "product.h"
#include "pm_clock.h"

#define UUID16_LEN 2
#define TEMP_LINE_LEN 32
static int g_recv_pkt_num = 0;
static uint64_t g_count_before_get_us;
static uint64_t g_count_after_get_us;
#define RECV_PKT_CNT 100

/* client id, invalid client id is "0" */
static uint8_t g_client_id = 0;
/* client app uuid for test */
static bt_uuid_t g_client_app_uuid = {UUID16_LEN, {0}};

bd_addr_t g_ble_speed_addr = {
    .type = 0,
    .addr = {0x11, 0x22, 0x33, 0x63, 0x88, 0x63},
};

extern errcode_t ble_gatt_client_discover_all_service(uint16_t conn_id);

static void ble_gatt_client_discover_service_cbk(uint8_t client_id, uint16_t conn_id,
    gattc_discovery_service_result_t *service, errcode_t status)
{
    gattc_discovery_character_param_t param = {0};
    osal_printk("[GATTClient]Discovery service----client:%d conn_id:%d\n", client_id, conn_id);
    osal_printk("            start handle:%d end handle:%d uuid_len:%d\n            uuid:",
        service->start_hdl, service->end_hdl, service->uuid.uuid_len);
    for (uint8_t i = 0; i < service->uuid.uuid_len; i++) {
        osal_printk("%02x", service->uuid.uuid[i]);
    }
    osal_printk("\n            status:%d\n", status);
    param.service_handle = service->start_hdl;
    param.uuid.uuid_len = 0; /* uuid length is zero, discover all character */
    gattc_discovery_character(g_client_id, conn_id, &param);
}

static void ble_gatt_client_discover_character_cbk(uint8_t client_id, uint16_t conn_id,
    gattc_discovery_character_result_t *character, errcode_t status)
{
    osal_printk("[GATTClient]Discovery character----client:%d conn_id:%d uuid_len:%d\n            uuid:",
        client_id, conn_id, character->uuid.uuid_len);
    for (uint8_t i = 0; i < character->uuid.uuid_len; i++) {
        osal_printk("%02x", character->uuid.uuid[i]);
    }
    osal_printk("\n            declare handle:%d value handle:%d properties:%x\n", character->declare_handle,
        character->value_handle, character->properties);
    osal_printk("            status:%d\n", status);
    gattc_discovery_descriptor(g_client_id, conn_id, character->declare_handle);
}

static void ble_gatt_client_discover_descriptor_cbk(uint8_t client_id, uint16_t conn_id,
    gattc_discovery_descriptor_result_t *descriptor, errcode_t status)
{
    osal_printk("[GATTClient]Discovery descriptor----client:%d conn_id:%d uuid len:%d\n            uuid:",
        client_id, conn_id, descriptor->uuid.uuid_len);
    for (uint8_t i = 0; i < descriptor->uuid.uuid_len; i++) {
        osal_printk("%02x", descriptor->uuid.uuid[i]);
    }
    osal_printk("\n            descriptor handle:%d\n", descriptor->descriptor_hdl);
    osal_printk("            status:%d\n", status);
}

static void ble_gatt_client_discover_service_compl_cbk(uint8_t client_id, uint16_t conn_id, bt_uuid_t *uuid,
    errcode_t status)
{
    osal_printk(
        "[GATTClient]Discovery service complete----client:%d conn_id:%d uuid len:%d\n            uuid:",
        client_id, conn_id, uuid->uuid_len);
    for (uint8_t i = 0; i < uuid->uuid_len; i++) {
        osal_printk("%02x", uuid->uuid[i]);
    }
    osal_printk("            status:%d\n", status);
}

static void ble_gatt_client_discover_character_compl_cbk(uint8_t client_id, uint16_t conn_id,
    gattc_discovery_character_param_t *param, errcode_t status)
{
    osal_printk(
        "[GATTClient]Discovery character complete----client:%d conn_id:%d uuid len:%d\n            uuid:",
        client_id, conn_id, param->uuid.uuid_len);
    for (uint8_t i = 0; i < param->uuid.uuid_len; i++) {
        osal_printk("%02x", param->uuid.uuid[i]);
    }
    osal_printk("\n            service handle:%d\n", param->service_handle);
    osal_printk("            status:%d\n", status);
}

static void ble_gatt_client_discover_descriptor_compl_cbk(uint8_t client_id, uint16_t conn_id,
    uint16_t character_handle, errcode_t status)
{
    osal_printk("[GATTClient]Discovery descriptor complete----client:%d conn_id:%d\n", client_id, conn_id);
    osal_printk("            charatcer handle:%d\n", character_handle);
    osal_printk("            status:%d\n", status);
}

static void ble_gatt_client_read_cfm_cbk(uint8_t client_id, uint16_t conn_id, gattc_handle_value_t *read_result,
    gatt_status_t status)
{
    osal_printk("[GATTClient]Read result----client:%d conn_id:%d\n", client_id, conn_id);
    osal_printk("            handle:%d data_len:%d\ndata:", read_result->handle, read_result->data_len);
    for (uint8_t i = 0; i < read_result->data_len; i++) {
        osal_printk("%02x", read_result->data[i]);
    }
    osal_printk("\n            status:%d\n", status);
}

static void ble_gatt_client_read_compl_cbk(uint8_t client_id, uint16_t conn_id, gattc_read_req_by_uuid_param_t *param,
    errcode_t status)
{
    osal_printk("[GATTClient]Read by uuid complete----client:%d conn_id:%d\n", client_id, conn_id);
    osal_printk("start handle:%d end handle:%d uuid len:%d\n            uuid:",
        param->start_hdl, param->end_hdl, param->uuid.uuid_len);
    for (uint8_t i = 0; i < param->uuid.uuid_len; i++) {
        osal_printk("%02x", param->uuid.uuid[i]);
    }
    osal_printk("\n            status:%d\n", status);
}

static void ble_gatt_client_write_cfm_cbk(uint8_t client_id, uint16_t conn_id, uint16_t handle, gatt_status_t status)
{
    osal_printk("[GATTClient]Write result----client:%d conn_id:%d handle:%d\n", client_id, conn_id, handle);
    osal_printk("            status:%d\n", status);
}

static void ble_gatt_client_mtu_changed_cbk(uint8_t client_id, uint16_t conn_id, uint16_t mtu_size, errcode_t status)
{
    osal_printk("[GATTClient]Mtu changed----client:%d conn_id:%d mtu size:%d\n", client_id, conn_id,
        mtu_size);
    osal_printk("            status:%d\n", status);
    // ble_gatt_client_discover_all_service(conn_id);
}

#define BLE_SPEED_HUNDRED 100
static uint32_t get_float_int(float in)
{
    return (uint32_t)(((uint64_t)(in * BLE_SPEED_HUNDRED)) / BLE_SPEED_HUNDRED);
}

static uint32_t get_float_dec(float in)
{
    return (uint32_t)(((uint64_t)(in * BLE_SPEED_HUNDRED)) % BLE_SPEED_HUNDRED);
}

static void ble_gatt_client_notification_cbk(uint8_t client_id, uint16_t conn_id, gattc_handle_value_t *data,
    errcode_t status)
{
    unused(status);
    unused(conn_id);
    unused(client_id);

    g_recv_pkt_num++;
    if (g_recv_pkt_num == 1) {
        g_count_before_get_us = uapi_systick_get_us();
    } else if (g_recv_pkt_num == RECV_PKT_CNT) {
        g_count_after_get_us = uapi_systick_get_us();
        printf("count_us = %llu, recv %d pkt.\r\n",
            g_count_after_get_us - g_count_before_get_us, RECV_PKT_CNT);
        float time = (float)(g_count_after_get_us - g_count_before_get_us) / 1000000.0;  /* 1s = 1000000.0us */
        printf("time = %d.%d s\r\n", get_float_int(time), get_float_dec(time));
        float speed = (data->data_len) * RECV_PKT_CNT * 8 / time;  /* 1B = 8bits */
        printf("speed = %d.%d bps\r\n", get_float_int(speed), get_float_dec(speed));
        printf("data_len = %d\r\n", data->data_len);
        g_recv_pkt_num = 0;
    }
}

static void ble_gatt_client_indication_cbk(uint8_t client_id, uint16_t conn_id, gattc_handle_value_t *data,
    errcode_t status)
{
    osal_printk("[GATTClient]Receive indication----client:%d conn_id:%d\n", client_id, conn_id);
    osal_printk("            handle:%d data_len:%d\ndata:", data->handle, data->data_len);
    for (uint8_t i = 0; i < data->data_len; i++) {
        osal_printk("%02x", data->data[i]);
    }
    osal_printk("\n            status:%d\n", status);
}

// static void ble_gatt_client_enable_cbk(errcode_t status)
// {
//     status = status;
// }

void ble_uart_client_power_on_cbk(uint8_t status)
{
    osal_printk("ble power on: %d\n", status);
    enable_ble();
}

/* ble client bt stack enable callback */
void ble_uart_client_enable_cbk(uint8_t status)
{
    osal_printk("ble enable: %d\n", status);
    // gattc_register_client(&g_client_app_uuid, &g_uart_client_id);
    // ble_uart_set_scan_parameters();
}

static int convert_ble_mac(uint8_t *dest_mac, uint16_t dest_len, uint8_t *src_mac, uint16_t src_len)
{
    if (dest_len != src_len) {
        return -1;
    }
    for (uint8_t i = 0; i < src_len; i++) {
        dest_mac[i] = src_mac[src_len - 1 - i];
    }
    return 0;
}

static void ble_gatt_client_scan_result_cbk(gap_scan_result_data_t *scan_result_data)
{
    uint8_t ble_mac[BD_ADDR_LEN] = {0};
    convert_ble_mac(ble_mac, BD_ADDR_LEN, g_ble_speed_addr.addr, BD_ADDR_LEN);
    if (memcmp(scan_result_data->addr.addr, ble_mac, BD_ADDR_LEN) == 0) {
        osal_printk("Find The Target Device.\n");
        gap_ble_stop_scan();
        bd_addr_t client_addr = { 0 };
        client_addr.type = scan_result_data->addr.type;
        if (memcpy_s(client_addr.addr, BD_ADDR_LEN, scan_result_data->addr.addr, BD_ADDR_LEN) != EOK) {
            osal_printk("%s add server app addr memcpy failed\r\n", __FUNCTION__);
            return;
        }
        gap_ble_connect_remote_device(&client_addr);
    } else {
        osal_printk("\naddr:");
        for (uint8_t i = 0; i < BD_ADDR_LEN; i++) {
            osal_printk(" %02x:", scan_result_data->addr.addr[i]);
        }
    }
}

static void ble_gatt_client_conn_state_change_cbk(uint16_t conn_id, bd_addr_t *addr,
    gap_ble_conn_state_t conn_state, gap_ble_pair_state_t pair_state, gap_ble_disc_reason_t disc_reason)
{
    osal_printk("%s connect state change conn_id: %d, status: %d, pair_status:%d, disc_reason %x\n",
        __FUNCTION__, conn_id, conn_state, pair_state, disc_reason);
    
    if (conn_state == GAP_BLE_STATE_CONNECTED) {
        // Set the MTU size to 250, with BS21 as the slave device
        gattc_exchange_mtu_req(g_client_id, conn_id, 250);
        osal_msleep(1000);
        gap_ble_pair_remote_device(addr);
    }
}

static void ble_gatt_client_pair_result_cbk(uint16_t conn_id, const bd_addr_t *addr, errcode_t status)
{
    unused(addr);
    osal_printk("%s pair result conn_id: %d,status: %d \n", __FUNCTION__, conn_id, status);

    ble_gatt_client_discover_all_service(conn_id);
}

static void ble_gatt_client_conn_param_update_cbk(uint16_t conn_id, errcode_t status,
    const gap_ble_conn_param_update_t *param)
{
    osal_printk("%s conn_param_update conn_id: %d,status: %d \n", __FUNCTION__, conn_id, status);
    osal_printk("interval:%d latency:%d timeout:%d.\n", param->interval, param->latency, param->timeout);
}

errcode_t ble_gatt_client_callback_register(void)
{
    errcode_t ret = ERRCODE_BT_UNHANDLED;

    bts_dev_manager_callbacks_t dev_cb = { 0 };
    dev_cb.power_on_cb = ble_uart_client_power_on_cbk;
    dev_cb.ble_enable_cb = ble_uart_client_enable_cbk;
    ret |= bts_dev_manager_register_callbacks(&dev_cb);

    gap_ble_callbacks_t gap_cb = {0};
    // gap_cb.ble_enable_cb = ble_gatt_client_enable_cbk;
    gap_cb.scan_result_cb = ble_gatt_client_scan_result_cbk;
    gap_cb.conn_state_change_cb = ble_gatt_client_conn_state_change_cbk;
    gap_cb.pair_result_cb = ble_gatt_client_pair_result_cbk;
    gap_cb.conn_param_update_cb = ble_gatt_client_conn_param_update_cbk;
    ret |= gap_ble_register_callbacks(&gap_cb);

    gattc_callbacks_t cb = {0};
    cb.discovery_svc_cb = ble_gatt_client_discover_service_cbk;
    cb.discovery_svc_cmp_cb = ble_gatt_client_discover_service_compl_cbk;
    cb.discovery_chara_cb = ble_gatt_client_discover_character_cbk;
    cb.discovery_chara_cmp_cb = ble_gatt_client_discover_character_compl_cbk;
    cb.discovery_desc_cb = ble_gatt_client_discover_descriptor_cbk;
    cb.discovery_desc_cmp_cb = ble_gatt_client_discover_descriptor_compl_cbk;
    cb.read_cb = ble_gatt_client_read_cfm_cbk;
    cb.read_cmp_cb = ble_gatt_client_read_compl_cbk;
    cb.write_cb = ble_gatt_client_write_cfm_cbk;
    cb.mtu_changed_cb = ble_gatt_client_mtu_changed_cbk;
    cb.notification_cb = ble_gatt_client_notification_cbk;
    cb.indication_cb = ble_gatt_client_indication_cbk;
    ret |= gattc_register_callbacks(&cb);

    if (ret != ERRCODE_BT_SUCCESS) {
        osal_printk("reg gatt cbk failed ret = %d\n", ret);
    }

#if (CORE_NUMS < 2)
    enable_ble();
#endif
    return ret;
}

void ble_speed_start_scan(void)
{
    gap_ble_scan_params_t ble_device_scan_params = { 0 };
    ble_device_scan_params.scan_interval = DEFAULT_SCAN_INTERVAL;
    ble_device_scan_params.scan_window = DEFAULT_SCAN_INTERVAL;
    ble_device_scan_params.scan_type = 0x00;
    ble_device_scan_params.scan_phy = GAP_BLE_PHY_2M;
    ble_device_scan_params.scan_filter_policy = 0x00;
    gap_ble_set_scan_parameters(&ble_device_scan_params);
    gap_ble_remove_all_pairs();
    gap_ble_start_scan();
}

errcode_t ble_gatt_client_init(void)
{
    errcode_t ret = ERRCODE_BT_SUCCESS;
    // ret |= enable_ble();
    ret |= ble_gatt_client_callback_register();
    ret |= gattc_register_client(&g_client_app_uuid, &g_client_id);
    osal_printk("[BLE Client] init ok.\n");
    ble_speed_start_scan();
    return ret;
}

errcode_t ble_gatt_client_discover_all_service(uint16_t conn_id)
{
    errcode_t ret = ERRCODE_BT_SUCCESS;
    bt_uuid_t service_uuid = {0}; /* uuid length is zero, discover all service */
    ret |= gattc_discovery_service(g_client_id, conn_id, &service_uuid);
    return ret;
}

#define BLE_SPEED_TASK_PRIO 26
#define BLE_SPEED_STACK_SIZE 0x2000

static void ble_speed_entry(void)
{
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
    osal_task *task_handle = NULL;
    osal_kthread_lock();
    task_handle= osal_kthread_create((osal_kthread_handler)ble_gatt_client_init, 0, "ble_speed",
        BLE_SPEED_STACK_SIZE);
    if (task_handle != NULL) {
        osal_kthread_set_priority(task_handle, BLE_SPEED_TASK_PRIO);
        osal_kfree(task_handle);
    }
    osal_kthread_unlock();
}

/* Run the app entry. */
app_run(ble_speed_entry);
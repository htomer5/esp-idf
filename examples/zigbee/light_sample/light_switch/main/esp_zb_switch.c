/*
 * Copyright (c) 2021 Espressif Systems (Shanghai) CO LTD
 * All rights reserved.
 *
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Espressif Systems
 *    integrated circuit in a product or a software update for such product,
 *    must reproduce the above copyright notice, this list of conditions and
 *    the following disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * 4. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "esp_zb_switch.h"
#include "esp_log.h"

/**
 * @note Make sure set idf.py menuconfig in zigbee component as zigbee end device!
*/
#if !defined ZB_ED_ROLE
#error Define ZB_ED_ROLE in idf.py menuconfig to compile light switch (End Device) source code.
#endif

/* define Button function currently only 1 switch define */
static switch_func_pair_t button_func_pair[] = {
    {GPIO_INPUT_IO_TOGGLE_SWITCH, SWITCH_ONOFF_TOGGLE_CONTROL}
};

static switch_device_ctx_t esp_switch_ctx = {
    /* basic cluster attributes data */
    .basic_attr.zcl_version = ZB_ZCL_VERSION,
    .basic_attr.power_source = ZB_ZCL_BASIC_POWER_SOURCE_UNKNOWN,
    /* identify cluster attributes data */
    .identify_attr.identify_time = 0,
    /* On_Off cluster attributes data */
    .on_off_attr.on_off = 0,
    /* bulb parameters */
    .bulb_params.short_addr = 0xffff,
};

static const char *TAG = "ESP_ZB_SWITCH";
/******************* Declare attributes ************************/

/* basic cluster attributes data */
ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST(basic_attr_list,
                                 &esp_switch_ctx.basic_attr.zcl_version,
                                 &esp_switch_ctx.basic_attr.power_source);

/* identify cluster attributes data */
ZB_ZCL_DECLARE_IDENTIFY_ATTRIB_LIST(identify_attr_list,
                                    &esp_switch_ctx.identify_attr.identify_time);

/* switch config cluster attributes data */
zb_uint8_t attr_switch_type =
    ZB_ZCL_ON_OFF_SWITCH_CONFIGURATION_SWITCH_TYPE_TOGGLE;
zb_uint8_t attr_switch_actions =
    ZB_ZCL_ON_OFF_SWITCH_CONFIGURATION_SWITCH_ACTIONS_DEFAULT_VALUE;

ZB_ZCL_DECLARE_ON_OFF_SWITCH_CONFIGURATION_ATTRIB_LIST(switch_cfg_attr_list, &attr_switch_type, &attr_switch_actions);

/********************* Declare device **************************/

ZB_HA_DECLARE_ON_OFF_SWITCH_CLUSTER_LIST(on_off_switch_clusters, switch_cfg_attr_list, basic_attr_list, identify_attr_list);

ZB_HA_DECLARE_ON_OFF_SWITCH_EP(on_off_switch_ep, HA_ONOFF_SWITCH_ENDPOINT, on_off_switch_clusters);

ZB_HA_DECLARE_ON_OFF_SWITCH_CTX(on_off_switch_ctx, on_off_switch_ep);

static void esp_zb_find_light_bulb_cb(zb_bufid_t bufid);

/********************* Define functions **************************/
/**
 * @brief return true is switch find bulb
 */
static bool esp_zb_already_find_light_bulb(void)
{
    return !(esp_switch_ctx.bulb_params.short_addr == 0xffff);
}

/**
 * @brief Function for sending on/off and Level Control find request.
 *
 * @param bufid   Zigbee zboss stack buffer id will be used to construct find request.
 */
static void esp_zb_find_light_bulb(zb_bufid_t bufid)
{
    zb_zdo_match_desc_param_t *p_req;

    /* initialize pointers inside buffer and reserve space for zb_zdo_match_desc_param_t request */
    p_req = zb_buf_initial_alloc(bufid, sizeof(zb_zdo_match_desc_param_t) + (1) * sizeof(zb_uint16_t));
    p_req->nwk_addr         = MATCH_DESC_REQ_ROLE;              /* send to devices specified by MATCH_DESC_REQ_ROLE */
    p_req->addr_of_interest = MATCH_DESC_REQ_ROLE;              /* get responses from devices specified by MATCH_DESC_REQ_ROLE */
    p_req->profile_id       = ZB_AF_HA_PROFILE_ID;              /* look for Home Automation profile clusters */

    /* we are searching for 1 cluster:
    on/off
    */
    p_req->num_in_clusters  = 1;
    p_req->num_out_clusters = 0;
    p_req->cluster_list[0]  = ZB_ZCL_CLUSTER_ID_ON_OFF;
    /* set 0xFFFF to reset short address in order to parse only one response. */
    esp_switch_ctx.bulb_params.short_addr = 0xFFFF;
    zb_zdo_match_desc_req(bufid, esp_zb_find_light_bulb_cb);
}

/**
 * @brief Finding procedure timeout handler.
 *
 * @param bufid   Zigbee zboss stack buffer id will be used to construct find request.
 */
static void esp_zb_find_light_bulb_timeout(zb_bufid_t bufid)
{
    zb_ret_t zb_err_code;

    if (bufid) {
        ESP_LOGW(TAG, "Bulb not found, try again");
        zb_err_code = ZB_SCHEDULE_APP_ALARM(esp_zb_find_light_bulb, bufid, MATCH_DESC_REQ_START_DELAY);
        ESP_ERROR_CHECK(zb_err_code);
        zb_err_code = ZB_SCHEDULE_APP_ALARM(esp_zb_find_light_bulb_timeout, 0, MATCH_DESC_REQ_TIMEOUT);
        ESP_ERROR_CHECK(zb_err_code);
    } else {
        zb_err_code = zb_buf_get_out_delayed(esp_zb_find_light_bulb_timeout);
        ESP_ERROR_CHECK(zb_err_code);
    }
}

/**
 * @brief Callback function receiving finding procedure results.
 *
 * @param bufid   Zigbee zboss stack buffer id is used to pass received data.
 */
static void esp_zb_find_light_bulb_cb(zb_bufid_t bufid)
{
    zb_zdo_match_desc_resp_t    *p_resp = (zb_zdo_match_desc_resp_t *) zb_buf_begin(bufid);    /* get the beginning of the response */
    zb_apsde_data_indication_t  *p_ind  = ZB_BUF_GET_PARAM(bufid, zb_apsde_data_indication_t);  /* get the pointer to the parameters buffer, which stores APS layer response */
    zb_uint8_t                  *p_match_ep;
    zb_ret_t                     zb_err_code;

    if ((p_resp->status == ZB_ZDP_STATUS_SUCCESS) && (p_resp->match_len > 0) && (esp_switch_ctx.bulb_params.short_addr == 0xFFFF)) {
        /* match EP list follows right after response header */
        p_match_ep = (zb_uint8_t *)(p_resp + 1);

        /* We are searching for exact cluster, so only 1 EP may be found */
        esp_switch_ctx.bulb_params.endpoint   = *p_match_ep;
        esp_switch_ctx.bulb_params.short_addr = p_ind->src_addr;

        ESP_LOGI(TAG, "Found bulb addr: 0x%x ep: %d", esp_switch_ctx.bulb_params.short_addr, esp_switch_ctx.bulb_params.endpoint);
        zb_err_code = ZB_SCHEDULE_APP_ALARM_CANCEL(esp_zb_find_light_bulb_timeout, ZB_ALARM_ANY_PARAM);
        ESP_ERROR_CHECK(zb_err_code);
    }
    if (bufid) {
        zb_buf_free(bufid);
    }
}

/**
 * @brief Function for sending on/off Toggle requests to the light bulb.
 *
 * @param bufid    Zigbee zboss stack buffer id will be used to construct on/off request.
 * @param on_off_toggle   unused value.
 */
static void esp_zb_light_switch_send_on_off_toggle(zb_bufid_t bufid, zb_uint16_t on_off_toggle)
{
    static zb_uint8_t cmd_id = ZB_ZCL_CMD_ON_OFF_TOGGLE_ID;

    ESP_LOGI(TAG, "Send ON/OFF toggle command");
    ZB_ZCL_ON_OFF_SEND_REQ(bufid,
                           esp_switch_ctx.bulb_params.short_addr,
                           ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
                           esp_switch_ctx.bulb_params.endpoint,
                           HA_ONOFF_SWITCH_ENDPOINT,
                           ZB_AF_HA_PROFILE_ID,
                           ZB_ZCL_DISABLE_DEFAULT_RESPONSE,
                           cmd_id,
                           NULL);
}

/**
 * @brief Callback for button events, currently only toggle event available
 *
 * @param button_func_pair      Incoming event from the button_pair.
 */
static void esp_zb_buttons_handler(switch_func_pair_t button_func_pair)
{
    zb_ret_t zb_err_code = ESP_OK;

    if (!esp_zb_already_find_light_bulb()) {
        /* no bulb found yet */
        return;
    }
    if (button_func_pair.func == SWITCH_ONOFF_TOGGLE_CONTROL) {
        /* implemented light switch toggle functionality */
        zb_err_code = zb_buf_get_out_delayed_ext(esp_zb_light_switch_send_on_off_toggle, 0, 0);
    }
    ESP_ERROR_CHECK(zb_err_code);
}

static void bdb_start_top_level_commissioning_cb(zb_uint8_t mode_mask)
{
    if (!bdb_start_top_level_commissioning(mode_mask)) {
        ESP_LOGE(TAG, "In BDB commissioning, an error occurred (for example: the device has already been running)");
    }
}

/**
 * @brief Zigbee zboss stack event signal handler.
 *
 * @param bufid   Zigbee zboss stack buffer id used to pass signal.
 */
void zboss_signal_handler(zb_uint8_t bufid)
{
    zb_zdo_app_signal_hdr_t       *p_sg_p = NULL;
    zb_zdo_app_signal_type_t       sig    = zb_get_app_signal(bufid, &p_sg_p);
    zb_ret_t                       status = ZB_GET_APP_SIGNAL_STATUS(bufid);
    zb_ret_t                       zb_err_code;

    switch (sig) {
    case ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Zigbee stack initialized");
        bdb_start_top_level_commissioning(ZB_BDB_INITIALIZATION);
        break;
    case ZB_BDB_SIGNAL_DEVICE_FIRST_START:
        if (status == RET_OK) {
            ESP_LOGI(TAG, "Start network steering");
            bdb_start_top_level_commissioning(ZB_BDB_NETWORK_STEERING);
        } else {
            ESP_LOGE(TAG, "Failed to initialize Zigbee stack (status: %d)", status);
        }
        break;
    case ZB_BDB_SIGNAL_STEERING:
        if (status == RET_OK) {
            zb_ext_pan_id_t extended_pan_id;
            zb_get_extended_pan_id(extended_pan_id);
            ESP_LOGI(TAG, "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx)",
                     extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                     extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                     ZB_PIBCACHE_PAN_ID());
            /* check the light device address */
            if (!esp_zb_already_find_light_bulb()) {
                zb_err_code = ZB_SCHEDULE_APP_ALARM(esp_zb_find_light_bulb, bufid, MATCH_DESC_REQ_START_DELAY);
                ESP_ERROR_CHECK(zb_err_code);
                zb_err_code = ZB_SCHEDULE_APP_ALARM(esp_zb_find_light_bulb_timeout, 0, MATCH_DESC_REQ_TIMEOUT);
                ESP_ERROR_CHECK(zb_err_code);
                bufid = 0; /* Do not free buffer - it will be reused by find_light_bulb callback */
            }
        } else {
            ESP_LOGI(TAG, "Network steering was not successful (status: %d)", status);
            ZB_SCHEDULE_APP_ALARM((zb_callback_t)bdb_start_top_level_commissioning_cb, ZB_BDB_NETWORK_STEERING, ZB_TIME_ONE_SECOND);
        }
        break;
    default:
        ESP_LOGI(TAG, "status: %d", status);
        break;
    }
    if (bufid) {
        zb_buf_free(bufid);
    }
}

void app_main()
{
    zb_ret_t    zb_err_code;
    zb_ieee_addr_t g_ieee_addr;

    /* initialize Zigbee stack */
    ZB_INIT("light_switch");
    esp_read_mac(g_ieee_addr, ESP_MAC_IEEE802154);
    zb_set_long_address(g_ieee_addr);
    zb_set_network_ed_role(IEEE_CHANNEL_MASK);
    zb_set_nvram_erase_at_start(ERASE_PERSISTENT_CONFIG);
    zb_set_ed_timeout(ED_AGING_TIMEOUT_64MIN);
    zb_set_keepalive_timeout(ZB_MILLISECONDS_TO_BEACON_INTERVAL(3000));
    /* hardware related and device init */
    switch_driver_init(button_func_pair, PAIR_SIZE(button_func_pair), esp_zb_buttons_handler);
    /* register on_off switch device context (endpoints) */
    ZB_AF_REGISTER_DEVICE_CTX(&on_off_switch_ctx);
    zb_err_code = zboss_start_no_autostart();
    ESP_ERROR_CHECK(zb_err_code);

    while (1) {
        zboss_main_loop_iteration();
    }
}

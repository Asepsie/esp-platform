// =============================================================================
// zigbee_coordinator.c — H2 Zigbee 3.0 coordinator (esp-zigbee-sdk, target-only).
//
// See zigbee_coordinator.h. Calls esp_zb_* APIs; not host-testable. The pure
// ZCL-attribute conversion is delegated to zigbee_cluster_handler (host-tested).
// =============================================================================
#include "zigbee_coordinator.h"
#include "zigbee_cluster_handler.h"

#include <string.h>

#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_zigbee_core.h"

static const char *TAG = "zb_coord";

// --- Tunables ----------------------------------------------------------------
#define ZB_COORD_ENDPOINT       1U          // our local (coordinator) endpoint
#define ZB_PRIMARY_CHANNEL      11U         // 2.4 GHz channel (spec default)
#define ZB_PRIMARY_CHANNEL_MASK (1UL << ZB_PRIMARY_CHANNEL)
#define ZB_MAX_CHILDREN         10U
#define ZB_TASK_STACK           4096
#define ZB_TASK_PRIO            5
// Attribute reporting window requested from joined devices (spec 3b).
#define REPORT_MIN_INTERVAL_S   10U
#define REPORT_MAX_INTERVAL_S   60U
// Endpoint we address on remote sensors. Endpoint 1 is the common HA endpoint
// for the target Sonoff devices. TODO(hw): replace with the endpoint discovered
// via active-endpoint + simple-descriptor before production.
#define REMOTE_SENSOR_ENDPOINT  1U

static zb_report_cb_t s_report_cb = NULL;
static zb_join_cb_t   s_join_cb   = NULL;

// =============================================================================
// Callback registration
// =============================================================================
esp_err_t zigbee_coordinator_register_report_cb(zb_report_cb_t cb)
{
    s_report_cb = cb;
    return ESP_OK;
}

esp_err_t zigbee_coordinator_register_join_cb(zb_join_cb_t cb)
{
    s_join_cb = cb;
    return ESP_OK;
}

// =============================================================================
// Attribute report path: ZCL report -> cluster handler -> report callback
// =============================================================================
static esp_err_t handle_attr_report(const esp_zb_zcl_report_attr_message_t *msg)
{
    if (msg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Resolve the reporting device's IEEE address (reports usually carry a
    // short address; look up the long address from the stack's table).
    uint8_t ieee[8] = {0};
    if (msg->src_address.addr_type == ESP_ZB_ZCL_ADDR_TYPE_SHORT) {
        esp_zb_ieee_address_by_short(msg->src_address.u.short_addr, ieee);
    } else {
        memcpy(ieee, msg->src_address.u.ieee_addr, sizeof(ieee));
    }

    bridge_sensor_report_t report;
    // NOTE: the report message does not carry LQI; mark unknown (0xFF). LQI per
    // device would come from a neighbour-table lookup (hardware bring-up TODO).
    esp_err_t err = zigbee_cluster_handler_process(
        ieee, msg->cluster, msg->attribute.id,
        (uint8_t)msg->attribute.data.type,
        (const uint8_t *)msg->attribute.data.value,
        msg->attribute.data.size, 0xFF, &report);

    if (err == ESP_OK) {
        if (s_report_cb != NULL) {
            s_report_cb(&report);
        }
    } else if (err == ESP_ERR_NOT_FOUND) {
        ESP_LOGD(TAG, "ignoring unmapped attr c=0x%04x a=0x%04x",
                 msg->cluster, msg->attribute.id);
    } else {
        ESP_LOGW(TAG, "rejected report c=0x%04x a=0x%04x (%s)",
                 msg->cluster, msg->attribute.id, esp_err_to_name(err));
    }
    return ESP_OK;
}

// Unified core action handler — we only care about attribute reports.
static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id,
                                   const void *message)
{
    switch (callback_id) {
        case ESP_ZB_CORE_REPORT_ATTR_CB_ID:
            return handle_attr_report((const esp_zb_zcl_report_attr_message_t *)message);
        default:
            ESP_LOGD(TAG, "unhandled core action 0x%x", callback_id);
            return ESP_OK;
    }
}

// =============================================================================
// Join path: configure reporting + notify C6
// =============================================================================
// Ask a freshly-joined device to report the sensor attributes we map. Records
// target REMOTE_SENSOR_ENDPOINT; a device that lacks a cluster simply rejects
// that record. Runs in the stack (signal-handler) context — no extra lock.
static void configure_device_reporting(uint16_t short_addr)
{
    // reportable_change values must match each attribute's ZCL type; keep them
    // static so the pointers stay valid through the async command send.
    static int16_t  temp_change = 50;   // 0.5 °C in 1/100 °C units
    static uint16_t hum_change  = 100;  // 1.0 %  in 1/100 % units
    static uint16_t co2_change  = 25;   // 25 ppm (uint16 reporting)

    const struct {
        uint16_t cluster;
        uint16_t attr;
        uint8_t  type;
        void    *change;   // NULL for discrete types (report on any change)
    } items[] = {
        { 0x0402, 0x0000, ESP_ZB_ZCL_ATTR_TYPE_S16,  &temp_change }, // temperature
        { 0x0405, 0x0000, ESP_ZB_ZCL_ATTR_TYPE_U16,  &hum_change  }, // humidity
        { 0x040D, 0x0000, ESP_ZB_ZCL_ATTR_TYPE_U16,  &co2_change  }, // CO2
        { 0x000F, 0x0055, ESP_ZB_ZCL_ATTR_TYPE_BOOL, NULL         }, // dry contact
    };

    for (size_t i = 0; i < sizeof(items) / sizeof(items[0]); i++) {
        esp_zb_zcl_config_report_record_t record = {
            .direction         = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
            .attributeID       = items[i].attr,
            .attrType          = items[i].type,
            .min_interval      = REPORT_MIN_INTERVAL_S,
            .max_interval      = REPORT_MAX_INTERVAL_S,
            .reportable_change = items[i].change,
        };
        esp_zb_zcl_config_report_cmd_t cmd = {
            .zcl_basic_cmd = {
                .dst_addr_u.addr_short = short_addr,
                .dst_endpoint          = REMOTE_SENSOR_ENDPOINT,
                .src_endpoint          = ZB_COORD_ENDPOINT,
            },
            .address_mode  = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
            .clusterID     = items[i].cluster,
            .record_number = 1,
            .record_field  = &record,
        };
        esp_zb_zcl_config_report_cmd_req(&cmd);
    }
}

static void handle_device_annce(const esp_zb_zdo_signal_device_annce_params_t *p)
{
    if (p == NULL) {
        return;
    }
    ESP_LOGI(TAG, "device joined: short=0x%04x", p->device_short_addr);

    // Notify the C6 immediately with addressing info. TODO(hw bring-up): enrich
    // manufacturer/model (Basic cluster read) and supported_clusters
    // (active-endpoint + simple-descriptor discovery) before sending.
    bridge_device_join_t join;
    memset(&join, 0, sizeof(join));
    memcpy(join.ieee_addr, p->ieee_addr, sizeof(join.ieee_addr));
    join.short_addr = p->device_short_addr;
    if (s_join_cb != NULL) {
        s_join_cb(&join);
    }

    configure_device_reporting(p->device_short_addr);
}

// =============================================================================
// Stack signal handler (called by esp-zigbee from the stack task)
// =============================================================================
void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_zb_app_signal_type_t sig_type = (esp_zb_app_signal_type_t)*p_sg_p;
    esp_err_t err_status = signal_struct->esp_err_status;

    switch (sig_type) {
        case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
        case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
            if (err_status == ESP_OK) {
                ESP_LOGI(TAG, "stack started, forming network");
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_FORMATION);
            } else {
                ESP_LOGE(TAG, "stack start failed: %s", esp_err_to_name(err_status));
            }
            break;

        case ESP_ZB_BDB_SIGNAL_FORMATION:
            if (err_status == ESP_OK) {
                // Network is up but CLOSED — devices join only after permit_join.
                ESP_LOGI(TAG, "network formed (closed; awaiting permit-join)");
            } else {
                ESP_LOGE(TAG, "network formation failed: %s",
                         esp_err_to_name(err_status));
            }
            break;

        case ESP_ZB_BDB_SIGNAL_STEERING:
            ESP_LOGI(TAG, "join window %s",
                     err_status == ESP_OK ? "opened" : "closed");
            break;

        case ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE:
            handle_device_annce(
                (esp_zb_zdo_signal_device_annce_params_t *)esp_zb_app_signal_get_params(p_sg_p));
            break;

        case ESP_ZB_ZDO_SIGNAL_LEAVE:
            ESP_LOGI(TAG, "a device left the network");
            break;

        default:
            ESP_LOGD(TAG, "zb signal 0x%x, status %s", sig_type,
                     esp_err_to_name(err_status));
            break;
    }
}

// =============================================================================
// Public API
// =============================================================================
esp_err_t zigbee_coordinator_init(void)
{
    esp_zb_platform_config_t platform_cfg = {
        .radio_config = { .radio_mode = ZB_RADIO_MODE_NATIVE },
        .host_config  = { .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE },
    };
    ESP_RETURN_ON_ERROR(esp_zb_platform_config(&platform_cfg), TAG, "platform config");

    // Centralized coordinator. install_code_policy=false: devices join with the
    // default Trust-Center link key (what the target Sonoff sensors use). For a
    // hardened deployment, set true and pre-register per-device install codes.
    esp_zb_cfg_t zb_cfg = {
        .esp_zb_role         = ESP_ZB_DEVICE_TYPE_COORDINATOR,
        .install_code_policy = false,
        .nwk_cfg.zczr_cfg    = { .max_children = ZB_MAX_CHILDREN },
    };
    esp_zb_init(&zb_cfg);

    // Minimal coordinator endpoint: Basic + Identify (server role). NULL cfg =
    // default attribute set.
    esp_zb_cluster_list_t *clusters = esp_zb_zcl_cluster_list_create();
    ESP_RETURN_ON_ERROR(
        esp_zb_cluster_list_add_basic_cluster(clusters,
            esp_zb_basic_cluster_create(NULL), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE),
        TAG, "add basic cluster");
    ESP_RETURN_ON_ERROR(
        esp_zb_cluster_list_add_identify_cluster(clusters,
            esp_zb_identify_cluster_create(NULL), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE),
        TAG, "add identify cluster");

    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
    esp_zb_endpoint_config_t ep_cfg = {
        .endpoint           = ZB_COORD_ENDPOINT,
        .app_profile_id     = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id      = ESP_ZB_HA_CONFIGURATION_TOOL_DEVICE_ID,
        .app_device_version = 0,
    };
    ESP_RETURN_ON_ERROR(esp_zb_ep_list_add_ep(ep_list, clusters, ep_cfg),
                        TAG, "add endpoint");
    ESP_RETURN_ON_ERROR(esp_zb_device_register(ep_list), TAG, "register device");

    esp_zb_core_action_handler_register(zb_action_handler);
    ESP_RETURN_ON_ERROR(esp_zb_set_primary_network_channel_set(ZB_PRIMARY_CHANNEL_MASK),
                        TAG, "set channel");

    ESP_LOGI(TAG, "coordinator initialized (channel %u, network closed)",
             (unsigned)ZB_PRIMARY_CHANNEL);
    return ESP_OK;
}

// The stack main loop must run in its own task.
static void zb_main_task(void *arg)
{
    (void)arg;
    ESP_ERROR_CHECK(esp_zb_start(false));   // false: we drive commissioning ourselves
    esp_zb_stack_main_loop();               // never returns
}

esp_err_t zigbee_coordinator_start(void)
{
    BaseType_t ok = xTaskCreate(zb_main_task, "zb_main", ZB_TASK_STACK, NULL,
                                ZB_TASK_PRIO, NULL);
    return ok == pdPASS ? ESP_OK : ESP_FAIL;
}

esp_err_t zigbee_coordinator_permit_join(uint8_t duration_s)
{
    // Called from other tasks (UART command dispatch) -> hold the stack lock.
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_err_t err = (duration_s == 0) ? esp_zb_bdb_close_network()
                                      : esp_zb_bdb_open_network(duration_s);
    esp_zb_lock_release();
    ESP_LOGI(TAG, "permit_join(%u) -> %s", duration_s, esp_err_to_name(err));
    return err;
}

esp_err_t zigbee_coordinator_poll_attr(const uint8_t *ieee, uint16_t cluster_id,
                                       uint16_t attr_id)
{
    if (ieee == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    uint16_t attr_field = attr_id;          // read_attr copies this synchronously
    esp_zb_zcl_read_attr_cmd_t cmd = {
        .zcl_basic_cmd = {
            .dst_endpoint = REMOTE_SENSOR_ENDPOINT,
            .src_endpoint = ZB_COORD_ENDPOINT,
        },
        .address_mode = ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT,
        .clusterID    = cluster_id,
        .attr_number  = 1,
        .attr_field   = &attr_field,
    };
    memcpy(cmd.zcl_basic_cmd.dst_addr_u.addr_long, ieee, 8);

    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_read_attr_cmd_req(&cmd);
    esp_zb_lock_release();
    return ESP_OK;
}

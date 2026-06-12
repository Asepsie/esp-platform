// =============================================================================
// bacnet_server.c — M0 BACnet device: discoverable + readable.
//
// The ONLY file that touches bacnet-stack types directly (plus the datalink
// glue). It builds the Device object + AI/BI objects from bacnet_object_map,
// installs the standard service handlers, and runs bacnet_task which:
//   1. services the datalink (RX → npdu_handler),
//   2. advances the stack's TSM timer,
//   3. refreshes present values from sensor_state (the COV trigger point, M3).
//
// Datalinks are registered by the caller (bacnet_server_add_transport) before
// bacnet_server_start(); the task sends one I-Am at entry so a BMS already
// listening learns of us immediately (we also answer Who-Is thereafter).
// RT: prio below the control task (HVAC always wins); static task; WDT-fed.
// =============================================================================
#include "bacnet_server.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "bacnet/bacdef.h"
#include "bacnet/apdu.h"
#include "bacnet/bacstr.h"
#include "bacnet/datalink/datalink.h"
#include "bacnet/basic/services.h"
#include "bacnet/basic/tsm/tsm.h"
#include "bacnet/basic/binding/address.h"
#include "bacnet/basic/object/device.h"
#include "bacnet/basic/object/ai.h"
#include "bacnet/basic/object/bi.h"

#include "bacnet_object_map.h"
#include "bacnet_objects.h"
#include "hal_timer.h"
#include "hal_wdt.h"
#include "thermostat_config.h"

static const char *TAG = "bacnet";

// Receive scratch buffer (RT-06: static, one per task — not reentrant).
static uint8_t s_pdu[MAX_MPDU];

// RT-06: task control block + stack allocated once.
static StaticTask_t s_task_tcb;
static StackType_t s_task_stack[BACNET_TASK_STACK_SIZE / sizeof(StackType_t)];
static TaskHandle_t s_task_handle;

// ---- object table: Device + Analog Input + Binary Input ----------------------
// The configurable Device object (basic/server/bacnet_device.c) dispatches each
// supported type through this table; types are gated by CONFIG_BACNET_BASIC_*
// in port/bacnet-config.h. Writable control types (AV/BV/MSV/BO) arrive in M2.
static object_functions_t My_Object_Table[] = {
    { OBJECT_DEVICE, NULL, Device_Count, Device_Index_To_Instance,
      Device_Valid_Object_Instance_Number, Device_Object_Name,
      Device_Read_Property_Local, Device_Write_Property_Local,
      Device_Property_Lists, DeviceGetRRInfo, NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, NULL, Device_Writable_Property_List },
    { OBJECT_ANALOG_INPUT, Analog_Input_Init, Analog_Input_Count,
      Analog_Input_Index_To_Instance, Analog_Input_Valid_Instance,
      Analog_Input_Object_Name, Analog_Input_Read_Property,
      Analog_Input_Write_Property, Analog_Input_Property_Lists, NULL, NULL,
      Analog_Input_Encode_Value_List, Analog_Input_Change_Of_Value,
      Analog_Input_Change_Of_Value_Clear, Analog_Input_Intrinsic_Reporting,
      NULL, NULL, Analog_Input_Create, Analog_Input_Delete, NULL,
      Analog_Input_Writable_Property_List },
    { OBJECT_BINARY_INPUT, Binary_Input_Init, Binary_Input_Count,
      Binary_Input_Index_To_Instance, Binary_Input_Valid_Instance,
      Binary_Input_Object_Name, Binary_Input_Read_Property, NULL,
      Binary_Input_Property_Lists, NULL, NULL, Binary_Input_Encode_Value_List,
      Binary_Input_Change_Of_Value, Binary_Input_Change_Of_Value_Clear, NULL,
      NULL, NULL, Binary_Input_Create, Binary_Input_Delete, NULL,
      Binary_Input_Writable_Property_List },
    { MAX_BACNET_OBJECT_TYPE, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL }
};

// Map a data category to its BACnet engineering Units enum.
static BACNET_ENGINEERING_UNITS units_for_category(data_category_t cat)
{
    switch (cat) {
    case DATA_CAT_TEMPERATURE:
        return UNITS_DEGREES_CELSIUS;
    case DATA_CAT_HUMIDITY:
        return UNITS_PERCENT;
    case DATA_CAT_CO2:
        return UNITS_PARTS_PER_MILLION;
    default:
        return UNITS_NO_UNITS;
    }
}

// Instantiate the AI/BI objects described by bacnet_object_map.
static void create_objects(void)
{
    for (size_t i = 0; i < bacnet_object_map_size(); i++) {
        const bacnet_object_map_entry_t *e = bacnet_object_map_get(i);
        switch (e->type) {
        case OBJ_ANALOG_INPUT:
            (void)Analog_Input_Create(e->instance);
            (void)Analog_Input_Name_Set(e->instance, e->object_name);
            (void)Analog_Input_Description_Set(e->instance, e->description);
            (void)Analog_Input_Units_Set(
                e->instance, units_for_category(e->source_category));
            break;
        case OBJ_BINARY_INPUT:
            (void)Binary_Input_Create(e->instance);
            (void)Binary_Input_Name_Set(e->instance, e->object_name);
            (void)Binary_Input_Description_Set(e->instance, e->description);
            break;
        default:
            // Writable types (AV/BV/MSV/BO) are not instantiated until M2.
            break;
        }
    }
}

// Push the latest sensor_state-derived value into each AI/BI object. Unresolved
// rows (e.g. topology not yet commissioned) keep their last value — we simply
// skip them rather than publish a misleading zero.
static void refresh_present_values(void)
{
    for (size_t i = 0; i < bacnet_object_map_size(); i++) {
        const bacnet_object_map_entry_t *e = bacnet_object_map_get(i);
        float v = 0.0f;
        if (bacnet_object_present_value(e, &v) != ESP_OK) {
            continue;
        }
        switch (e->type) {
        case OBJ_ANALOG_INPUT:
            Analog_Input_Present_Value_Set(e->instance, v);
            break;
        case OBJ_BINARY_INPUT:
            (void)Binary_Input_Present_Value_Set(
                e->instance, (v >= 0.5f) ? BINARY_ACTIVE : BINARY_INACTIVE);
            break;
        default:
            break;
        }
    }
}

esp_err_t bacnet_server_init(void)
{
    address_init();

    Device_Init(My_Object_Table);
    Device_Set_Object_Instance_Number(BACNET_DEVICE_INSTANCE);
    Device_Set_Vendor_Identifier(BACNET_VENDOR_ID);
    (void)Device_Set_Vendor_Name(BACNET_VENDOR_NAME, sizeof(BACNET_VENDOR_NAME) - 1);
    (void)Device_Set_Model_Name(BACNET_MODEL_NAME, sizeof(BACNET_MODEL_NAME) - 1);

    create_objects();

    // Service handlers (DM-DDB / DM-DOB discovery + DS read; DCC for comms ctrl).
    apdu_set_unrecognized_service_handler_handler(handler_unrecognized_service);
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_WHO_IS, handler_who_is);
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_WHO_HAS, handler_who_has);
    apdu_set_confirmed_handler(
        SERVICE_CONFIRMED_READ_PROPERTY, handler_read_property);
    apdu_set_confirmed_handler(
        SERVICE_CONFIRMED_READ_PROP_MULTIPLE, handler_read_property_multiple);
    apdu_set_confirmed_handler(
        SERVICE_CONFIRMED_DEVICE_COMMUNICATION_CONTROL,
        handler_device_communication_control);

    ESP_LOGI(TAG, "device %lu (%s) initialized, %zu objects",
             (unsigned long)BACNET_DEVICE_INSTANCE, BACNET_VENDOR_NAME,
             bacnet_object_map_size());
    return ESP_OK;
}

static void bacnet_task(void *arg)
{
    (void)arg;
    ESP_ERROR_CHECK(hal_wdt_add_current_task()); // RT-07

    // Announce ourselves once now that the datalink(s) are up.
    Send_I_Am(&Handler_Transmit_Buffer[0]);

    uint32_t last_ms = hal_timer_get_ms();
    for (;;) {
        hal_wdt_reset();

        // 1. Datalink RX → protocol handler (bounded by the poll period).
        BACNET_ADDRESS src = { 0 };
        uint16_t pdu_len =
            datalink_receive(&src, s_pdu, sizeof(s_pdu), BACNET_TASK_PERIOD_MS);
        if (pdu_len) {
            npdu_handler(&src, s_pdu, pdu_len);
        }

        // 2. Advance stack timers by the real elapsed time.
        const uint32_t now_ms = hal_timer_get_ms();
        uint32_t elapsed = (uint32_t)(now_ms - last_ms);
        last_ms = now_ms;
        if (elapsed > 0) {
            tsm_timer_milliseconds((uint16_t)(elapsed > 0xFFFF ? 0xFFFF : elapsed));
        }

        // 3. Project sensor_state onto present values (COV detect hook, M3).
        refresh_present_values();

        // datalink_receive() already blocks up to one period, so no extra delay
        // is needed when a transport implements a real timeout. Guard against a
        // busy-spin if every transport returns immediately (e.g. the stub).
        if (pdu_len == 0) {
            vTaskDelay(pdMS_TO_TICKS(BACNET_TASK_PERIOD_MS));
        }
    }
}

esp_err_t bacnet_server_start(void)
{
    if (s_task_handle != NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    s_task_handle = xTaskCreateStatic(
        bacnet_task, "bacnet",
        sizeof(s_task_stack) / sizeof(StackType_t), NULL,
        BACNET_TASK_PRIORITY, s_task_stack, &s_task_tcb);
    return (s_task_handle != NULL) ? ESP_OK : ESP_ERR_NO_MEM;
}

# ESP-IDF component: bacnet — northbound BACnet server (vendored bacnet-stack).
#
# Layout:
#   bacnet_transport*.c       — datalink registry + MS/TP transport (stub → M4)
#   bacnet_datalink_glue.c    — bacnet-stack datalink_*() ⇄ transport registry
#   bacnet_object_map.c       — static object table (pure; host-tested)
#   bacnet_objects.c          — present-value binding to sensor_state (pure)
#   bacnet_server.c           — Device object, handlers, bacnet_task (M0)
#   port/                     — port-only customization (config + time hook)
#   vendor/bacnet-stack/      — pinned submodule (tag bacnet-stack-1.5.0), READ-ONLY
#
# License: bacnet-stack is GPL-2.0 WITH the linking exception. The core is never
# edited — all customization is in port/ (bacnet-config.h pulled in via the
# -DBACNET_CONFIG_H define) and in our own sources. See docs/architecture/
# bacnet-server-design.md §3.
#
# REQUIRES bsp (hal_uart_mstp/hal_wdt/hal_timer), sensor_state/data_model
# (object binding), platform (esp_err_t), and IDF freertos/esp_timer/log.

# --- Curated bacnet-stack core (mirrors ports/esp32 minus the MS/TP datalink,
#     which our transport+glue replaces until M4). Relative to this dir. ---------
set(BACNET_STACK_DIR vendor/bacnet-stack/src)
set(BACNET_STACK_SRCS
    ${BACNET_STACK_DIR}/bacnet/basic/binding/address.c
    ${BACNET_STACK_DIR}/bacnet/basic/server/bacnet_device.c
    ${BACNET_STACK_DIR}/bacnet/basic/object/ai.c
    ${BACNET_STACK_DIR}/bacnet/basic/object/bi.c
    ${BACNET_STACK_DIR}/bacnet/basic/npdu/h_npdu.c
    ${BACNET_STACK_DIR}/bacnet/basic/service/h_apdu.c
    ${BACNET_STACK_DIR}/bacnet/basic/service/h_dcc.c
    ${BACNET_STACK_DIR}/bacnet/basic/service/h_noserv.c
    ${BACNET_STACK_DIR}/bacnet/basic/service/h_rd.c
    ${BACNET_STACK_DIR}/bacnet/basic/service/h_rp.c
    ${BACNET_STACK_DIR}/bacnet/basic/service/h_rpm.c
    ${BACNET_STACK_DIR}/bacnet/basic/service/h_whohas.c
    ${BACNET_STACK_DIR}/bacnet/basic/service/h_whois.c
    ${BACNET_STACK_DIR}/bacnet/basic/service/h_wp.c
    ${BACNET_STACK_DIR}/bacnet/basic/service/s_iam.c
    ${BACNET_STACK_DIR}/bacnet/basic/service/s_ihave.c
    ${BACNET_STACK_DIR}/bacnet/basic/sys/datetime_mstimer.c
    ${BACNET_STACK_DIR}/bacnet/basic/sys/days.c
    ${BACNET_STACK_DIR}/bacnet/basic/sys/dst.c
    ${BACNET_STACK_DIR}/bacnet/basic/sys/debug.c
    ${BACNET_STACK_DIR}/bacnet/basic/sys/ringbuf.c
    ${BACNET_STACK_DIR}/bacnet/basic/sys/fifo.c
    ${BACNET_STACK_DIR}/bacnet/basic/sys/mstimer.c
    ${BACNET_STACK_DIR}/bacnet/basic/sys/keylist.c
    ${BACNET_STACK_DIR}/bacnet/basic/tsm/tsm.c
    ${BACNET_STACK_DIR}/bacnet/abort.c
    ${BACNET_STACK_DIR}/bacnet/bacaction.c
    ${BACNET_STACK_DIR}/bacnet/bacaddr.c
    ${BACNET_STACK_DIR}/bacnet/bacapp.c
    ${BACNET_STACK_DIR}/bacnet/bacdcode.c
    ${BACNET_STACK_DIR}/bacnet/bacdest.c
    ${BACNET_STACK_DIR}/bacnet/bacdevobjpropref.c
    ${BACNET_STACK_DIR}/bacnet/bacerror.c
    ${BACNET_STACK_DIR}/bacnet/bacint.c
    ${BACNET_STACK_DIR}/bacnet/bacreal.c
    ${BACNET_STACK_DIR}/bacnet/bacstr.c
    ${BACNET_STACK_DIR}/bacnet/cov.c
    ${BACNET_STACK_DIR}/bacnet/datetime.c
    ${BACNET_STACK_DIR}/bacnet/dcc.c
    ${BACNET_STACK_DIR}/bacnet/hostnport.c
    ${BACNET_STACK_DIR}/bacnet/iam.c
    ${BACNET_STACK_DIR}/bacnet/ihave.c
    ${BACNET_STACK_DIR}/bacnet/indtext.c
    ${BACNET_STACK_DIR}/bacnet/memcopy.c
    ${BACNET_STACK_DIR}/bacnet/npdu.c
    ${BACNET_STACK_DIR}/bacnet/proplist.c
    ${BACNET_STACK_DIR}/bacnet/rd.c
    ${BACNET_STACK_DIR}/bacnet/reject.c
    ${BACNET_STACK_DIR}/bacnet/rp.c
    ${BACNET_STACK_DIR}/bacnet/rpm.c
    ${BACNET_STACK_DIR}/bacnet/timestamp.c
    ${BACNET_STACK_DIR}/bacnet/whohas.c
    ${BACNET_STACK_DIR}/bacnet/whois.c
    ${BACNET_STACK_DIR}/bacnet/wp.c
)

idf_component_register(
    SRCS
        "bacnet_transport.c"
        "bacnet_transport_mstp.c"
        "bacnet_datalink_glue.c"
        "bacnet_object_map.c"
        "bacnet_objects.c"
        "bacnet_server.c"
        "port/bacnet_port_systime.c"
        ${BACNET_STACK_SRCS}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "port" "${BACNET_STACK_DIR}"
    REQUIRES bsp platform sensor_state data_model
    PRIV_REQUIRES esp_timer)

# config.h pulls in port/bacnet-config.h only when this is defined.
target_compile_definitions(${COMPONENT_LIB} PRIVATE BACNET_CONFIG_H)

# Vendored third-party code is read-only: silence its warnings so they neither
# clutter nor (under any -Werror) break our build. Our own sources keep the
# project's full warning set.
set_source_files_properties(${BACNET_STACK_SRCS}
    PROPERTIES COMPILE_OPTIONS "-w"
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

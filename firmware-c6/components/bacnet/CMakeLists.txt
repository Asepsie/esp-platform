# ESP-IDF component: bacnet — northbound BACnet (transport abstraction + MS/TP).
#
#   bacnet_transport.c       — transport registry (SC + MS/TP can coexist)
#   bacnet_transport_mstp.c  — MS/TP datalink stub over bsp hal_uart_mstp
#
# The full object model / APDU handling / BACnet-SC transport are future work.
# REQUIRES bsp for hal_uart_mstp; no driver/* here (HAL boundary).
idf_component_register(SRCS "bacnet_transport.c" "bacnet_transport_mstp.c"
                       INCLUDE_DIRS "include"
                       REQUIRES bsp platform)

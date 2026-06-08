# ESP-IDF component: sensor_state — central mutex-protected runtime store.
idf_component_register(SRCS "sensor_state.c"
                       INCLUDE_DIRS "include"
                       REQUIRES data_model cluster_map platform)

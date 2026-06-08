# ESP-IDF component: cluster_map — ZCL (cluster,attr) → semantics lookup table.
idf_component_register(SRCS "cluster_map.c"
                       INCLUDE_DIRS "include"
                       REQUIRES data_model)

# ESP-IDF component: control — thermostat control loop + 1 Hz RT task.
#
#   control_loop.c — pure decision + relay actuation (host-unit-tested)
#   control_task.c — RT-01 FreeRTOS task driving control_loop_tick() at 1 Hz
#                    (target-only; watchdog/timing via bsp hal_wdt/hal_timer)
idf_component_register(SRCS "control_loop.c" "control_task.c"
                       INCLUDE_DIRS "include"
                       REQUIRES bsp sensor_state data_model platform)

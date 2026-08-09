zephyr_library()
zephyr_library_sources_ifdef(CONFIG_TRAVELERS_BATTERY_VOLTAGE_DIVIDER
                             drivers/travelers_battery_voltage_divider.c)

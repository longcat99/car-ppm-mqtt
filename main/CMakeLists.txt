# Edit following two lines to set component requirements (see docs)
# set(COMPONENT_REQUIRES )
# set(COMPONENT_PRIV_REQUIRES )

# set(COMPONENT_SRCS "main.c.1.c" "main.c")
# set(COMPONENT_ADD_INCLUDEDIRS "")

# register_component()
idf_component_register(SRCS
        "main.c"
        "led/led.c"
        "mqtt/iothub.c"
        "utility/utility.c"
        "rc/rc.c"
        "time/time-1.c"
        "ps4/esp_hid_gap.c"
        "ps4/ps4.c"

        

        INCLUDE_DIRS "."

        INCLUDE_DIRS "led"
        INCLUDE_DIRS "mqtt"
        INCLUDE_DIRS "utility"
        INCLUDE_DIRS "rc"
        INCLUDE_DIRS "time"
        INCLUDE_DIRS "ps4"
        )

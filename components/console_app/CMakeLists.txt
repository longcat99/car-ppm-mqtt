idf_component_register(SRCS  "console_app.c"
                    INCLUDE_DIRS "."
                    REQUIRES console spi_flash cmd_system
                    
                    )

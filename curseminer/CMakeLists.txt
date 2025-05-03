file(GLOB SRCS_CORE "src/*.c")
file(GLOB SRCS_GAMES "src/games/*.c")
idf_component_register(SRCS ${SRCS_CORE} ${SRCS_GAMES} "src/frontends/esp32s3-waveshare.c"
                       PRIV_REQUIRES spi_flash
                       INCLUDE_DIRS "include"
                       REQUIRES esp_lcd)

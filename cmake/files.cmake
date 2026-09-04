# Add sources to executable/library
target_sources(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Device/Geehy/APM32E10x/Source/system_apm32e10x.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Drivers/APM32E10x_StdPeriphDriver/Src/apm32e10x_rcm.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Drivers/APM32E10x_StdPeriphDriver/Src/apm32e10x_gpio.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Drivers/APM32E10x_StdPeriphDriver/Src/apm32e10x_misc.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Drivers/APM32E10x_StdPeriphDriver/Src/apm32e10x_spi.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Drivers/APM32E10x_StdPeriphDriver/Src/apm32e10x_adc.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Drivers/APM32E10x_StdPeriphDriver/Src/apm32e10x_usart.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Drivers/APM32E10x_StdPeriphDriver/Src/apm32e10x_tmr.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Drivers/APM32E10x_StdPeriphDriver/Src/apm32e10x_sdio.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Drivers/APM32E10x_StdPeriphDriver/Src/apm32e10x_dma.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/FatFs/ff.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/FatFs/ffsystem.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/FatFs/diskio.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/lcd.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/menu.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/temp.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/serial.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/backlight.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/sdcard.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/sdlog.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/spiflash.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/demo.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/video.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/livestream.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/main.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Src/startup_apm32e10x_hd.S"
)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/apm32e10x_flash.ld" "${CMAKE_CURRENT_BINARY_DIR}" COPYONLY)

set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/apm32e10x_flash.ld")

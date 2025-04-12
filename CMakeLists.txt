#===============================================================================
# @brief    cmake file
# Copyright (c) CompanyNameMagicTag 2023-2023. All rights reserved.
#===============================================================================


if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_UART)
    add_subdirectory_if_exist(sle_uart)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_BLE_UART)
    add_subdirectory_if_exist(ble_uart)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_SPEED_SERVER_SAMPLE)
    add_subdirectory_if_exist(sle_speed_server)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_SLE_SPEED_CLIENT_SAMPLE)
    add_subdirectory_if_exist(sle_speed_client)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_BLE_SPEED_SERVER_SAMPLE)
    add_subdirectory_if_exist(ble_speed_server)
endif()

if(DEFINED CONFIG_SAMPLE_SUPPORT_BLE_SPEED_CLIENT_SAMPLE)
    add_subdirectory_if_exist(ble_speed_client)
endif()

set(SOURCES "${SOURCES}" PARENT_SCOPE)
set(PUBLIC_HEADER "${PUBLIC_HEADER}" PARENT_SCOPE)
set(LIBS "${LIBS}" PARENT_SCOPE)
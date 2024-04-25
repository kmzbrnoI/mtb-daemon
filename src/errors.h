#ifndef ERRORS_H
#define ERRORS_H

constexpr size_t MTB_MODULE_INVALID_ADDR = 1100;
constexpr size_t MTB_MODULE_FAILED = 1102;
constexpr size_t MTB_INVALID_SPEED = 1105;
constexpr size_t MTB_INVALID_DV = 1106;
constexpr size_t MTB_MODULE_ACTIVE = 1107;
constexpr size_t MTB_FILE_CANNOT_ACCESS = 1010;
constexpr size_t MTB_MODULE_ALREADY_WRITING = 1110;
constexpr size_t MTB_UNKNOWN_COMMAND = 1020;

constexpr size_t MTB_DEVICE_DISCONNECTED = 2004;
constexpr size_t MTB_ALREADY_STARTED = 2012;

constexpr size_t MTB_MODULE_UPGRADING_FW = 3110;
constexpr size_t MTB_MODULE_IN_BOOTLOADER = 3111;
constexpr size_t MTB_MODULE_CONFIG_SETTING = 3112;
constexpr size_t MTB_MODULE_REBOOTING = 3113;
constexpr size_t MTB_MODULE_FWUPGD_ERROR = 3114;

// Codes directly from MTB-USB errors
constexpr size_t MTB_MODULE_UNKNOWN_COMMAND = 0x1001;
constexpr size_t MTB_MODULE_UNSUPPORTED_COMMAND = 0x1002;
constexpr size_t MTB_MODULE_BAD_ADDRESS = 0x1003;
constexpr size_t MTB_SERIAL_PORT_CLOSED = 0x1010;
constexpr size_t MTB_USB_NO_RESPONSE = 0x1011;
constexpr size_t MTB_BUS_NO_RESPONSE = 0x1012;

#endif

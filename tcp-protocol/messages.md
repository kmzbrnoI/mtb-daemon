MTB daemon's TCP sever messages specification
=============================================

## Request & responses

### Daemon status

```json
{
    "command": "mtbusb",
    "type": "request",
    "id": 42
}
```

```json
{
    "command": "mtbusb",
    "type": "response",
    "id": 42,
    "status": "ok",
    "mtbusb": {
        "connected": true,
        "type": 1,
        "speed": 115200,
        "firmware_version": "1.0",
        "protocol_version": "1.0",
        "active_modules": [1, 5, 2, 121]
    }
}
```

* When daemon connects to / disconnects from MTB-USB, this message is sent as
  event.
  - When connect event occurs, modules are not scanned.
    Client is informed about scanned modules via `module_activated` command.
  - When disconnect event occurs, error responses to pending commands are sent.
* Fields after `connected` are sent if and only if `connected=True`.

### MTB-USB Change Speed

```json
{
    "command": "mtbusb",
    "type": "request",
    "id": 42,
    "mtbusb": {
        "speed": 115200
    }
}
```

Response in same as *Daemon Status* response.

### Module

```json
{
    "command": "module",
    "type": "request",
    "id": 42,
    "address": 1,
    "state": false,
}
```

```json
{
    "command": "module",
    "type": "response",
    "id": 42,
    "status": "ok",
    "module": {
        "address": 1,
        "name": "Testing module 1",
        "type_code": 21,
        "type": "MTB-UNI v4",
        "state": "active",
        "firmware_version": "1.0",
        "protocol_version": "4.0",
        "bootloader_version": "1.0",
        "error": false,
        "warning": false,
        "beacon": false,
        "MTB-UNI v4": {
            # data specific for module type, e.g.:
            "ir": false,
            "config": {
                "outputsSafe": [
                    {"type": "plain", "value": 0},
                    {"type": "s-com", "value": 10},
                    {"type": "flicker", "value": 60},
                    ... # 16 values
                ],
                "inputsDelay": [0, 0.5, 0.2, ..., 0.3] # 16 values
            },
            "state": {
                "outputs": [
                    {"type": "plain", "value": 0},
                    {"type": "s-com", "value": 10},
                    {"type": "flicker", "value": 60},
                    ...
                ],
                "inputs": {"uniinputs": [false, false, true, true, ..., false]} # 16 values
            }
        }
    }
}
```

* `state`: `inactive`, `active`, `rebooting`, `fw_upgrading`, `bootloader_err`,
  `bootloader_int`.
* Flicker value: number of ticks in minute. Allowed values:
  60, 120, 180, 240, 320, 600, 33, 66.
* Inputs delay: 0–1.5 (including bounds), step=0.1.

### Modules

```json
{
    "command": "modules",
    "type": "request",
    "id": 10,
    "state": false,
}
```

```json
{
    "command": "modules",
    "type": "response",
    "id": 10,
    "status": "ok",
    "modules": {
        "1": {...}, # See module definition above
        "132": {...}
    }
}
```

### Module set output/s

```json
{
    "command": "module_set_outputs",
    "type": "request",
    "id": 123,
    "address": 1,
    "outputs": {
        # Data format specific for module type, e.g. for MTB-UNI:
        # Any number of outputs specific for module, e.g.:
        "1": {"type": "plain", "value": 0},
        "2": {"type": "s-com", "value": 10},
        "12": {"type": "flicker", "value": 60},
        ...
    }
}
```

```json
{
    "command": "module_set_outputs",
    "type": "response",
    "id": 123,
    "status": "ok",
    "address": 1,
    "outputs": {
        # Data format specific for module type, e.g. for MTB-UNI:
        # Current state of (not neccessarry all) outputs
        "1": {"type": "plain", "value": 0},
        "2": {"type": "s-com", "value": 10},
        "12": {"type": "flicker", "value": 60},
        ...
    }
}
```

### Module set configuration

```json
{
    "command": "module_set_config",
    "type": "request",
    "id": 10,
    "address": 5,
    "type_code" 15,
    "name": "Klobouky MTB DK",
    "config": {
        # Config specific for module, see module definition above
    }
}
```

```json
{
    "command": "module_set_config",
    "type": "response",
    "id": 10,
    "status": "ok",
    "address": 1
}
```

### Module set address (only for modules without hardware setting)

```json
{
    "command": "module_set_address",
    "type:" "request",
    "id": 11,
    "address": 1,     # only for readdress, can be ommited
    "new_address": 8
}
```

```json
{
    "command": "module_set_address",
    "type": "response",
    "id": 11,
    "status": "ok",
    "address": 8
}
```

### Module firmware upgrade request

```json
{
    "command": "module_upgrade_fw",
    "type": "request",
    "id": 20,
    "address": 32,
    "firmware": {
        "0": "0C9446010C9465010C9465010C946501",
        "16": "0C9465010C9465010C9465010C946501",
    }
}
```

```json
{
    "command": "module_upgrade_fw",
    "type": "response",
    "id": 20,
    "status": "ok",
    "address": 1,
}
```

* Request `firmware: {start_addres: data}`
  - Any `start_address` and any length of `data` could be sent
  - Server joins `data`.
  - Format is designed for hex files to be easily sendible.

### Module-specific command

```json
{
    "command": "module_specific_command",
    "type": "request",
    "id": 20,
    "address": 32,
    "data": [10, 15, 10, 0, 5]
}
```

```json
{
    "command": "module_specific_command",
    "type": "response",
    "id": 20,
    "status": "ok",
    "address": 1,
    "response": {
        "command_code": 10,
        "data": [5, 10, 0, 5]
    }
}
```

### Module reboot

```json
{
    "command": "module_reboot",
    "type": "request",
    "id": 22,
    "address": 32
}
```

```json
{
    "command": "module_reboot",
    "type": "response",
    "id": 22,
    "status": "ok",
    "address": 32,
}
```

### Module beacon

```json
{
    "command": "module_beacon",
    "type": "request",
    "id": 22,
    "address": 32,
    "beacon": true
}
```

```json
{
    "command": "module_beacon",
    "type": "response",
    "id": 22,
    "status": "ok",
    "address": 32,
    "beacon": true
}
```

### Module subscribe/unsubscribe

```json
{
    "command": "module_subscribe"/"module_unsubscribe",
    "type": "request",
    "id": 12,
    "addresses": [10, 11, 20]
}
```

```json
{
    "command": "module_subscribe"/"module_unsubscribe",
    "type": "response",
    "id": 12,
    "status": "ok",
    "addresses": [10, 11, 20]
}
```

### Reset all outputs set by client

```json
{
    "command": "reset_my_outputs",
    "type": "request",
    "id": 12
}
```

```json
{
    "command": "reset_my_outputs",
    "type": "response",
    "id": 12,
    "status": "ok"
}
```

### Diagnostics

```json
{
    "command": "module_diag",
    "type": "request",
    "id": 12,
    "address": 32,
    "DVkey": "mcu_voltage", / "DVnum": 10
}
```

When `DVNum` is present, `DVKey` is ignored.

```json
{
    "command": "module_diag",
    "type": "response",
    "id": 12,
    "status": "ok",
    "address": 32,
    "DVkey": "mcu_voltage",
    "DVvalue": {
        "voltage": 5.05
    },
    "DVvalueRaw": [234]
}
```


## Events

### Module input/s changed

```json
{
    "command": "module_inputs_changed",
    "type": "event",
    "module_inputs_changed": {
        "address": 10,
        "type": "MTB-UNI v4",
        "type_code": 21,
        "inputs": {...} # Inputs definition specific for module
    }
}
```

### Module output/s changed

```json
{
    "command": "module_outputs_changed",
    "type": "event",
    "module_output_changed": {
        "address": 20,
        "type": "MTB-UNI v4",
        "type_code": 21,
        "outputs": {...} # Outputs definition specific for modules
    }
}
```

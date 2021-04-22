Commands of MTB daemon TCP protocol
==================================

Command types:
* `request`
* `response`
* `event`

## Common *request* attributes

 * `command`: string identifier of command
 * `type`: `request`
 * `id`: any number, will be sent in response
   - `id` could be omitted

## Common *response* attributes

 * `command`: identifier of response
 * `type`: `response`
 * `id`: request id for which response is generated
 * `status`: `ok` / `error`
   - In case of error, `error` dict is provided:
     `error: {"code": int, "message": str}`

## Common *event* attributes

 * `command`: string identifier of command
 * `type`: `event`


## Commands

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

### Module

```json
{
    "command": "module",
    "type": "request",
    "id": 42,
    "address": 1,
    "state": false
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
        "type": "MTB-UNI v4",
        "type_code": 21,
        "bootloader_intentional": false,
        "bootloader_error": false,
        "firmware_version": "1.0",
        "protocol_version": "4.0",
        "MTB-UNI v4": {
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
                "inputs": [false, false, true, true, ..., false] # 16 values
            }
        }
    }
}
```

* Flicker value: number of ticks in minute. Allowed values:
  60, 120, 180, 240, 320, 600, 33, 66.
* Inputs delay: 0–1.5 (including bounds), step=0.1.

### Modules

```json
{
    "command": "modules",
    "type": "request",
    "id": 10,
    "state": false
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
    "outputs": {
        // Current state of (not neccessarry all) outputs
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
}
```

### Firmware upgrade request

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
    "response": {
        "command_code": 10,
        "data": [5, 10, 0, 5]
    }
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

## Events

### Module input changed

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

### Module output changed

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

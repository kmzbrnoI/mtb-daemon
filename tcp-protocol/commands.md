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
    "command": "status",
    "type": "request",
    "id": int
}
```

```json
{
    "command": "status",
    "type": "response",
    "id": int,
    "status": "ok",
    "status": {
        "connected": true/false,
        "mtb-usb": {
            "type": int,
            "speed": int,
            "firmware_version": string like "1.0",
            "protocol_version": string like "1.0",
            "active_modules": [1, 5, 2, 121]
        }
    }
}
```

* When daemon connects to MTB-USB, this message is sent as event.
* "mtb-usb" section is present if and only if daemon is connected to MTB-USB.

### Module

```json
{
    "command": "module",
    "type": "request",
    "id": int,
    "address: int,
    "data": bool,
    "state": bool
}
```

```json
{
    "command": "module",
    "type": "response",
    "id": int,
    "status": "ok",
    "module": {
        "address": int,
        "type": "MTB-UNI",
        "type_code": int,
        "bootloader_intentional": bool,
        "bootloader_error": bool,
        "firmware_version": string like "1.0",
        "protocol_version": string like "1.0",
        "MTB-UNI": {
            "ir": bool,
            "config": {
                "outputsSafe": [
                    {"type": "plain", "value": 0},
                    {"type": "s-com", "value": 10},
                    {"type": "flicker", "value": 60},
                    ...
                ],
                "inputsDelay": [0, 0.5, 0.2, ..., 0.3]
            },
            "state": {
                "outputs": [
                    {"type": "plain", "value": 0},
                    {"type": "s-com", "value": 10},
                    {"type": "flicker", "value": 60},
                    ...
                ],
                "inputs": [bool, bool, bool, ..., bool]
            }
        }
    }
}
```

* Flicker value: number of ticks in minute. Allowed values: 60, 120, 180, 240,
  320, 600, 33, 66.
* Inputs delay: 0–1.5 (including limits), step=0.1.

### Modules

```json
{
    "command": "modules",
    "type": "request",
    "id": int,
    "data": bool,
    "state": bool
}
```

```json
{
    "command": "modules",
    "type": "response",
    "id": int,
    "status": "ok",
    "modules": {
        "1": {},
        "132": {}
    }
}
```

### Set module configuration

```json
{
    "command": "module_set_config",
    "type": "request",
    "id": int,
    "address": int,
    "config": {
        // Config specific for module
    }
}
```

```json
{
    "command": "module_set_config",
    "type": "response",
    "id": int,
    "status": "ok",
}
```

### Firmware upgrade request

```json
{
    "command": "module_upgrade_fw",
    "type": "request",
    "id": int,
    "address": int,
    "firmware": str (large string of bytes TODO)
}
```

```json
{
    "command": "module_set_config",
    "type": "response",
    "id": int,
    "status": "ok",
}
```

## Events

### Input changed

```json
{
    "command": "input_changed",
    "type": "event",
    "inputChanged": {
        "address": int,
        "type": "MTB-UNI",
        "type_code": int,
        "inputs": [...]
    }
}
```

### New module found / module restored

```json
{
    "command": "module_activated",
    "type": "event",
    "modules": [...]
}
```

### Module/s lost / module/s failed

```json
{
    "command": "module_lost",
    "type": "event",
    "modules": [...]
}
```

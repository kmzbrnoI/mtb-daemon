MTB daemon's TCP sever messages specification
=============================================

For the sake of clarity, each message in this description is divided into
multiple lines. It real protocol each message uses 1 line only.

## Request & responses

### Daemon status

This request allows the client to obtain basic information about MTBbus: active
modules etc.

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
        "firmware_deprecated": false,
        "protocol_version": "1.0",
        "active_modules": [1, 5, 2, 121]
    }
}
```

* Fields after `connected` are sent if and only if `connected=True`.

### MTB-USB Change Speed

This request allows the client to change speed of MTBbus.

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

### Daemon version

Since MTB Daemon v1.5 (sorry).

This request allows the client to obtain version of MTB Daemon.

```json
{
    "command": "version",
    "type": "request",
    "id": 42
}
```

```json
{
    "command": "version",
    "type": "response",
    "id": 42,
    "status": "ok",
    "version": {
        "sw_version": "1.0"
        "sw_version_major": 1,
        "sw_version_minor": 0
    }
}
```

### Module

This request allows the client to obtain all information about the module.

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
        "state": "active", # Following items are present only for active modules
        "firmware_version": "1.0",
        "protocol_version": "4.0",
        "bootloader_version": "1.0",
        "error": false,
        "warning": false,
        "beacon": false,
        "fw_deprecated": false,
        "MTB-UNI v4": {
            # data specific for module type
        }
    }
}
```

* `state`: `inactive`, `active`, `rebooting`, `fw_upgrading`, `bootloader_err`,
  `bootloader_int`.

### Module delete request

Since MTB Daemon v1.5.

This request allows the client to delete a module from the server's database.
Only inactive module on the bus can be deleted. As a reaction to deletion,
*Module deleted event* is sent to other client (see below).

```json
{
    "command": "module_delete",
    "type": "request",
    "id": 42,
    "address": 1
}
```

```json
{
    "command": "module_delete",
    "type": "response",
    "id": 42,
    "status": "ok",
    "address": 1
}
```

### Modules

This request allows the client to obtain data about all modules.

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

This request allows the client to set outputs of a module.

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

This request allows the client to set configuration of a module.

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

### Module set address

This request allows the client to change address of a specific module.

This option is available only for modules without hardware address setting.

```json
{
    "command": "module_set_address",
    "type:" "request",
    "id": 11,
    "address": 1,
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

### Module set address

This request allows the client to change address of a module with active
*readdressing mode* (usually via button on the module).

This option is available only for modules without hardware address setting.

```json
{
    "command": "set_address",
    "type:" "request",
    "id": 11,
    "new_address": 8
}
```

```json
{
    "command": "set_address",
    "type": "response",
    "id": 11,
    "status": "ok",
}
```

### Module firmware upgrade request

This request allows the client to upgrade firmware of a module.

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

* Format of request `firmware: {start_address: data}`
  - Any `start_address` and any length of `data` could be sent
  - Server joins `data`.
  - Format is designed for hex files to be easily sendible.

### Module-specific command

This request allows the client to send a specific command for the module.

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

This request allows the client to reboot a module.

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

This request allows the client to de/activate a beacon on a module (turn on/off
a special LED on the module).

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

This request allow the client to subscribe/unsubscribe to the input and
output change events from a module, list of modules or all modules.

```json
{
    "command": "module_subscribe"/"module_unsubscribe",
    "type": "request",
    "id": 12,
    "addresses": [10, 11, 20]
}
```

* In case `addresses` in the request is not present, all modules are
  subscribed/unsubscribed.

```json
{
    "command": "module_subscribe"/"module_unsubscribe",
    "type": "response",
    "id": 12,
    "status": "ok",
    "addresses": [10, 11, 20]
}
```

### My module subscribes

Since MTB Daemon v1.5.

This request allows the client to obtain list of it's subscribed modules or
set which modules are subscribed by the client precisely.

Complete list of client's subscribed modules is always sent back.

```json
{
    "command": "my_module_subscribes",
    "type": "request",
    "id": 5,
    "addresses": [5, 7, 100]
}
```

* `addresses` in the request could be empty.

```json
{
    "command": "my_module_subscribes",
    "type": "response",
    "id": 5,
    "status": "ok",
    "addresses": [5, 7, 100]
}
```


### Topology subscribe/unsubscribe

Since MTB Daemon v1.5.

These requests enable/disable subscription of topology change events to the
client.

```json
{
    "command": "topology_subscribe"/"topology_unsubscribe",
    "type": "request",
    "id": 12,
}
```

```json
{
    "command": "topology_subscribe"/"topology_unsubscribe",
    "type": "response",
    "id": 12,
    "status": "ok"
}
```

### Reset all outputs set by client

This request allows the client to reset outputs set by the client.

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

### Save config file

This request instructs the server to store server's data into it's internal
persistent configuration file.

```json
{
    "command": "save_config",
    "type": "request",
    "id": 12
}
```

```json
{
    "command": "save_config",
    "type": "response",
    "id": 12,
    "status": "ok"
}
```

### Load config file

Reloads config file. You should practically never need to execute this command,
config should be changed by client via `module_set_config` command.

```json
{
    "command": "load_config",
    "type": "request",
    "id": 12
}
```

```json
{
    "command": "load_config",
    "type": "response",
    "id": 12,
    "status": "ok"
}
```

### Diagnostics

This request allows the client to obtain a diagnostic value (DV) of a module.

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

This event is sent to all clients with subscribed module in case of any input
change on the module.

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

This event is sent to all clients with subscribed module excluding the client
that requested the output setting in case of any output change on the module.

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

### MTB-USB changed

This event is sent to all clients with subscribed topology changes in case of:
* Any new module occurs on MTBbus
* Any module becomes inactive on MTBbus

This event is sent to all clients in case of:
* MTB-USB becomes connected or disconnected

```json
{
    "command": "mtbusb",
    "type": "event",
    "mtbusb": {
        # 'mtbusb' section in *Daemon status* response
    }
}
```

* When connect event is sent, modules are not scanned yet.
  Client is informed about modules activation via `module` event.
* When disconnect event occurs, error responses to affected pending commands
  are sent.

### Module changed

This event is sent to all clients with subscribed topology or subscribed module
in case of any module's data change. E.g. module name change, module status
change, module configuration change etc. This event is not sent in case of inputs/outputs change!

```json
{
    "command": "module",
    "type": "event",
    "module": {
        # 'module' in *Module* response
    }
}
```

### Module deleted

Since MTB Daemon v1.5.

This event is sent to all clients with subscribed topology or subscribed module
in case of the module is deleted from the server's database. Only inactive
module on the bus could be deleted.

```json
{
    "command": "module_deleted",
    "type": "event",
    "module": 10
}
```

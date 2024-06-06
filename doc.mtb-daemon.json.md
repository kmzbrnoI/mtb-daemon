Description of mtb-daemon.json file
===================================

`mtb-daemon.json` is a single configuration file of a mtb-daemon application.
It is loaded at the start of the application. MTB Daemon's clients may instruct
the daemon to store its current setting to the file (overwrite the file)
via `save_config` command.

The file is not intended to be reloaded when MTB Daemon is running. All the
changes in e.g. module configuration should be done via MTB Daemon's TCP API.
`mtb-daemon.json` is used only as a data storage when the daemon is not
running.

An example of a simple `mtb-daemon.json`:

```json
{
    "loglevel": 5,
    "modules": {
        "001": {
            "config": {
                "inputsDelay": [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
                "outputsSafe": [
                    {"type": "plain", "value": 0},
                    {"type": "plain", "value": 0},
                    {"type": "plain", "value": 0},
                    {"type": "plain", "value": 0},
                    {"type": "plain", "value": 0},
                    {"type": "plain", "value": 0},
                    {"type": "plain", "value": 0},
                    {"type": "plain", "value": 0},
                    {"type": "plain", "value": 0},
                    {"type": "plain", "value": 0},
                    {"type": "plain", "value": 0},
                    {"type": "plain", "value": 0},
                    {"type": "plain", "value": 0},
                    {"type": "plain", "value": 0},
                    {"type": "plain", "value": 0},
                    {"type": "plain", "value": 0}
                ]
            },
            "name": "Module 1",
            "type": 22
        }
    },
    "mtb-usb": {
        "keepAlive": true,
        "port": "auto"
    },
    "production_logging": {
        "detectLevel": 2,
        "directory": "prodLog",
        "enabled": false,
        "future": 20,
        "history": 100,
        "loglevel": 5
    },
    "server": {
        "allowedClients": [
            "127.0.0.1"
        ],
        "host": "0.0.0.0",
        "keepAlive": true,
        "port": 3841
    }
}
```

## Description of the content

* `loglevel`: main loglevel of stdout. See <src/mtbusb/mtbusb.h> :: `LogLevel`.
* `modules`: configuration of all the modules. The configuration is authoritative.
  It is sent to all the modules present in the file when modules are being
  activated (e.g. after start / module discovery).
* `mtb-usb`
  - `keepAlive`: whether to periodically send keep-alive message to check MTB-USB
    connection health (recommended safe value: true).
  - `port`: either `auto` (MTB-USB is automatically detected) or e.g. `COM4` on
    Windows or `/dev/ttyUSB1` on Linux.
* `production\_logging`: when a log message with a priority number <= `detectLevel`
   (`detectLevel` or higher priority) in emitted (let us call the message 'alert
   message'), a log file inside the `directory` directory is created and neighbor
   log messages before and after the alert message are stored. `history` sets how many
   messages before the alert message are stored, `future` sets how many messages
   after the alert message are stored. `loglevel' sets the level of neighbor
   messages. `enabled` enables the feature.

   Main aim of this feature is to set `detectLevel` to WARNING or ERROR in the
   production environment. The maintenance can check later what caused warnings
   and errors based on the generated files.
* `server`: main configuration of the TCP JSON server.
  - `allowedClients`: list of the IPv4 addresses which can **write** to the server
    (e.g. set outputs). All the clients can read the server's state, but only
    the mentioned can set the state. This is used for simple access control.
  - `host`: address the server is running on.
  - `port`: server's port (default: 3841).
  - `keepAlive`: whether to check aliveness of the clients by periodically sending
    empty json dict messages (recommended: true).

MTB Daemon tests
================

This directory contains tests of MTB Daemon. Tests connect to TCP JSON API of
the MTB Daemon.

## Requirements

* `pytest`

## Test bench setup

1. Connect MTB-USB to PC.
2. Connect single MTB-UNI v4 module to the MTBbus.
3. Connect output 0 with input 0 of the MTB-UNI v4 module.
4. Power everything on.
5. Start mtb-daemon with configuration 'test-config.json`.

## Tests running

To run tests, execute:

```bash
make test
```

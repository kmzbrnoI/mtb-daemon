Definition of messages for MTB-UNIS
===================================

## Module information & configuration

```json
"MTB-UNIS": {
    "config": {
        "outputsSafe": [
            {"type": "plain", "value": 0},
            {"type": "s-com", "value": 10},
            {"type": "flicker", "value": 60},
            ... # 16 values
        ],
        "inputsDelay": [0, 0.5, 0.2, ..., 0.3], # 16 values
        "servoEnabledMask": 3, # 0–0x3F, mask with servo enable outputs
        "servoPosition": [0, 255, ..., 32], # 12 values, 0—255
        "servoSpeed": [100, 50, 100, 100, 200, 200] # 6 values 0—255
    },
    "state": {
        "outputs": ..., # See 'Outputs' below
        "inputs": ..., # Seet 'Inputs' below
    }
}
```

* Flicker value: number of ticks in minute. Allowed values:
  60, 120, 180, 240, 320, 600, 33, 66.
* Inputs delay: 0–1.5 (including bounds), step=0.1.

## Inputs

16 values representing inputs 0–15.

```json
"inputs": {"full": [false, false, true, true, ..., false], "packed": 5421}
```

## Outputs

* 16 plain/scom/flicker outputs
* 6×2 servo positions outputs (plain)
  - First: go to position `+`, second: go to position `-`

```json
"outputs": {
    "0": {"type": "plain", "value": 0},
    "2": {"type": "s-com", "value": 10},
    "3": {"type": "flicker", "value": 60},
    ...
    "15": {"type": "flicker", "value": 60},
    "16": {"type": "plain", "value": 1},
    ...
    "27": {"type": "plain", "value": 0}
}
```

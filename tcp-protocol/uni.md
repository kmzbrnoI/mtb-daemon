Definition of messages for MTB-UNI
==================================

## Module information & configuration

```json
"MTB-UNI v4": {
    "ir": false,
    "config": {
        "outputsSafe": [
            {"type": "plain", "value": 0},
            {"type": "s-com", "value": 10},
            {"type": "flicker", "value": 60},
            ... # 16 values
        ],
        "inputsDelay": [0, 0.5, 0.2, ..., 0.3], # 16 values
        "irs": [False, False, True, True, ...] # 16 values, just for IR modules
    },
    "state": {
        "outputs": [
            {"type": "plain", "value": 0},
            {"type": "s-com", "value": 10},
            {"type": "flicker", "value": 60},
            ...
        ],
        "inputs": [false, false, true, true, ..., false], # 16 values
        "inputsPacked": 1234, # Int representing bit state of inputs
        "irs": [false, false, true, ..., true], # 15 values, just for IR modules
        "irsPacked": 6541, # int representing ir inputs in bits, just for IR modules
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

```json
"outputs": {
    "1": {"type": "plain", "value": 0},
    "2": {"type": "s-com", "value": 10},
    "3": {"type": "flicker", "value": 60},
    ...
    "15": {"type": "flicker", "value": 60},
}
```

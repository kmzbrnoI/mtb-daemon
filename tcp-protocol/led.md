Definition of messages for MTB-LED
==================================

## Module information & configuration

```json
"MTB-LED": {
    "config": {
        "outputsSafe": [False, False, True, False, ..., True], # 32 values
        "brightness": [100, 255, 1, ..., 100], # 32 values
    },
    "state": {
        "outputs": ..., # See 'Outputs' below
        "inputs": ..., # Seet 'Inputs' below
    }
}
```

## Inputs

32 values representing whether outputs 0–31 are connected to LEDs or not.

```json
"inputs": {"full": [False, False, True, True, ..., False], "packed": 5421}
```

## Outputs

32 values representing outputs state.

```json
"outputs": {"full": [False, False, True, True, ..., False], "packed": 5421}
```

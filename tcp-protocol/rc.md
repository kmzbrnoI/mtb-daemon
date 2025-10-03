Definition of messages for MTB-RC
==================================

## Module information & configuration

```json
"MTB-RC": {
    "config": {},
    "state": {
        "inputs": ..., # See 'Inputs' below
    }
}
```

No configuration, no outputs.

## Inputs

16 values representing inputs 0–15.

```json
"inputs": [
    [addr01, addr02, ..., addr0n], # All addresses detected at track 0
    [addr11, addr12, ..., addr1n], # All addresses detected at track 1
    [...], # All addresses detected at track 2
    [...], # All addresses detected at track 3
    [...], # All addresses detected at track 4
    [...], # All addresses detected at track 5
    [...], # All addresses detected at track 6
    [...] # All addresses detected at track 7
]
```

Each PWM device has 2 outputs. To enable PWMs to be dephased, each output must be on a dedicated device.

The following changes need to be made to the PCB:

|signal |     pin     | Channel |  todo                                                     |
|-------|-------------|---------|-----------------------------------------------------------|
| pwmL1 | GP8  → GP2  | `1A`    | pin 11 cut and connect to U2                              |
| pwmL2 | GP9  → GP9  | `4B`    | -                                                         |
| pwmL3 | GP6  → GP6  | `3A`    | -                                                         |
| pwmL4 | GP7  → GP0  | `0A`    | pin 10 cut B <-> pin 1 cut B (fan0). connect to R2 and U6 |
| pwmL5 | GP10 → GP10 | `5A`    | -                                                         |
| pwmL6 | GP11 → GP15 | `7B`    | cut pin 15 B and connect pin 20 to U3                     |
| pwmL7 | GP12 → GP20 | `2B`    | cut pin 16 and connect pin 26 to U4                       |
| pwmL8 | GP13 → GP13 | `6B`    | -                                                         |

As a consequence:
|Fan    |  todo                                                                   |
|-------|-------------------------------------------------------------------------|
| fan 0 | now on pin 10 (GP7), PWM device `3B`… with the same freq as GP6 → pwmL3 |
| fan 1 | unchanged on GP1, PWM device `0B`, with same freq as GP0 → pwmL4        |

```c
const int lineToPWM[] = {1,4,3,0,5,7,2,6};
```


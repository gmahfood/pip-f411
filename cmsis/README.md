# CMSIS — vendor these once

This folder is intentionally empty in git. The build depends on ARM's CMSIS-Core
and ST's CMSIS-Device for the F4. Drop the files in so the tree looks like this:

```
cmsis/
├── Include/                         <- from ARM-software/CMSIS_5 (CMSIS/Core/Include)
│   ├── core_cm4.h
│   ├── cmsis_gcc.h
│   ├── cmsis_compiler.h
│   ├── cmsis_version.h
│   └── ...
└── Device/ST/STM32F4xx/
    ├── Include/                     <- from STMicroelectronics/cmsis_device_f4
    │   ├── stm32f4xx.h
    │   ├── stm32f411xe.h
    │   └── system_stm32f4xx.h
    └── Source/Templates/
        ├── system_stm32f4xx.c
        └── gcc/
            └── startup_stm32f411xe.s
```

## Sources
- CMSIS-Core: https://github.com/ARM-software/CMSIS_5  (CMSIS/Core/Include)
- CMSIS-Device F4: https://github.com/STMicroelectronics/cmsis_device_f4
  (Include/, Source/Templates/, Source/Templates/gcc/)

Both also ship inside the STM32CubeF4 pack if you already have it
(Drivers/CMSIS/...). The Makefile's CMSIS_INC / CMSIS_SRC / STARTUP paths expect
exactly the layout above — adjust them if you place files differently.

## Why borrowed, not hand-written
Per the project principle (learn the peripheral that's the point, borrow the
incidental): the vector table, clock-tree boilerplate, and register definitions
are scaffolding. The servo timer is the thing worth writing by hand. So we lean
on the vendor headers and spend the effort in src/tim_servo.c.

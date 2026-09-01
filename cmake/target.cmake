# Target definition for the Geehy APM32E103ZE (APM32E10x EVAL board), Cortex-M3, no FPU.
set(CMSIS_Dvendor Geehy)
set(CMSIS_Dname APM32E103ZE)
set(CMSIS_Dcore Cortex-M3)
set(CMSIS_Dfpu NO_FPU)
set(CMSIS_Ddsp NO_DSP)
set(CMSIS_Dmve NO_MVE)
set(CMSIS_Dendian Little-endian)
set(CMSIS_Tcompiler GCC)

# Set target definition variables needed for the TOOLCHAIN_FILE
set(CPU ${CMSIS_Dcore})
set(FPU ${CMSIS_Dfpu})
set(DSP ${CMSIS_Ddsp})
set(MVE ${CMSIS_Dmve})
set(BYTE_ORDER ${CMSIS_Dendian})

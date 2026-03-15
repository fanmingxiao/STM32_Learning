# Test Plan
1. Rewrite `MatrixKey_Scan` to output on `ROW_PINS` (PA0-3) and read on `COL_PINS` (PA4-7).
2. Rewrite `MX_GPIO_Init` to configure `ROW_PINS` as `GPIO_MODE_OUTPUT_PP` and `COL_PINS` as `GPIO_MODE_INPUT` + `GPIO_PULLUP`.

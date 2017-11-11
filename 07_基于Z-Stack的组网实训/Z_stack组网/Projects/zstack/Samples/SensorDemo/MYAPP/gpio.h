/* ------------------------------------------------------------------------------------------------
 *                                       GPIO Configuration
 * ------------------------------------------------------------------------------------------------
 */

#define GPIO_0_PORT         0
#define GPIO_0_PIN          0
#define GPIO_1_PORT         0
#define GPIO_1_PIN          1
#define GPIO_2_PORT         0
#define GPIO_2_PIN          6
#define GPIO_3_PORT         1
#define GPIO_3_PIN          0

#define GPIO_DIR_IN(IDX)    MCU_IO_DIR_INPUT(GPIO_##IDX##_PORT, GPIO_##IDX##_PIN)
#define GPIO_DIR_OUT(IDX)   MCU_IO_DIR_OUTPUT(GPIO_##IDX##_PORT, GPIO_##IDX##_PIN)
#define GPIO_TRI(IDX)       MCU_IO_INPUT(GPIO_##IDX##_PORT, GPIO_##IDX##_PIN, MCU_IO_TRISTATE)
#define GPIO_PULL_UP(IDX)   MCU_IO_INPUT(GPIO_##IDX##_PORT, GPIO_##IDX##_PIN, MCU_IO_PULLUP)
#define GPIO_PULL_DN(IDX)   MCU_IO_INPUT(GPIO_##IDX##_PORT, GPIO_##IDX##_PIN, MCU_IO_PULLDOWN)
#define GPIO_SET(IDX)       MCU_IO_SET_HIGH(GPIO_##IDX##_PORT, GPIO_##IDX##_PIN)
#define GPIO_CLR(IDX)       MCU_IO_SET_LOW(GPIO_##IDX##_PORT, GPIO_##IDX##_PIN)
#define GPIO_TOG(IDX)       MCU_IO_TGL(GPIO_##IDX##_PORT, GPIO_##IDX##_PIN)
#define GPIO_GET(IDX)       MCU_IO_GET(GPIO_##IDX##_PORT, GPIO_##IDX##_PIN)
#define GPIO_HiD_SET()     (PICTL |=  BV(7))  /* PADSC */
#define GPIO_HiD_CLR()     (PICTL &= ~BV(7))  /* PADSC */

// Used as "func" for the macros below
#define MCU_IO_TRISTATE     1
#define MCU_IO_PULLUP       2
#define MCU_IO_PULLDOWN     3

//-----------------------------------------------------------------------------
//  Macros for simple configuration of IO pins on TI LPW SoCs
//-----------------------------------------------------------------------------
#define MCU_IO_PERIPHERAL(port, pin)   MCU_IO_PERIPHERAL_PREP(port, pin)
#define MCU_IO_INPUT(port, pin, func)  MCU_IO_INPUT_PREP(port, pin, func)
#define MCU_IO_OUTPUT(port, pin, val)  MCU_IO_OUTPUT_PREP(port, pin, val)
#define MCU_IO_SET(port, pin, val)     MCU_IO_SET_PREP(port, pin, val)
#define MCU_IO_SET_HIGH(port, pin)     MCU_IO_SET_HIGH_PREP(port, pin)
#define MCU_IO_SET_LOW(port, pin)      MCU_IO_SET_LOW_PREP(port, pin)
#define MCU_IO_TGL(port, pin)          MCU_IO_TGL_PREP(port, pin)
#define MCU_IO_GET(port, pin)          MCU_IO_GET_PREP(port, pin)

#define MCU_IO_DIR_INPUT(port, pin)    MCU_IO_DIR_INPUT_PREP(port, pin)
#define MCU_IO_DIR_OUTPUT(port, pin)   MCU_IO_DIR_OUTPUT_PREP(port, pin)

//----------------------------------------------------------------------------------
//  Macros for internal use (the macros above need a new round in the preprocessor)
//----------------------------------------------------------------------------------
#define MCU_IO_PERIPHERAL_PREP(port, pin)   st( P##port##SEL |= BV(pin); )

#define MCU_IO_INPUT_PREP(port, pin, func)  st( P##port##SEL &= ~BV(pin); \
                                                P##port##DIR &= ~BV(pin); \
                                                switch (func) { \
                                                case MCU_IO_PULLUP: \
                                                    P##port##INP &= ~BV(pin); \
                                                    P2INP &= ~BV(port + 5); \
                                                    break; \
                                                case MCU_IO_PULLDOWN: \
                                                    P##port##INP &= ~BV(pin); \
                                                    P2INP |= BV(port + 5); \
                                                    break; \
                                                default: \
                                                    P##port##INP |= BV(pin); \
                                                    break; } )

#define MCU_IO_OUTPUT_PREP(port, pin, val)  st( P##port##SEL &= ~BV(pin); \
                                                P##port##_##pin## = val; \
                                                P##port##DIR |= BV(pin); )

#define MCU_IO_SET_HIGH_PREP(port, pin)     st( P##port##_##pin## = 1; )
#define MCU_IO_SET_LOW_PREP(port, pin)      st( P##port##_##pin## = 0; )

#define MCU_IO_SET_PREP(port, pin, val)     st( P##port##_##pin## = val; )
#define MCU_IO_TGL_PREP(port, pin)          st( P##port##_##pin## ^= 1; )
#define MCU_IO_GET_PREP(port, pin)          (P##port## & BV(pin))

#define MCU_IO_DIR_INPUT_PREP(port, pin)    st( P##port##DIR &= ~BV(pin); )
#define MCU_IO_DIR_OUTPUT_PREP(port, pin)   st( P##port##DIR |= BV(pin); )

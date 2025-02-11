#include <stdint.h>

//extern void _estack(void); //defined in linker script
extern uint32_t _estack;

/*----------------------------------------------------------------------------
  Global variables shared between main code and ISR
  ----------------------------------------------------------------------------*/
volatile uint8_t g_button_pressed = 0;
uint32_t g_button_press_count = 0;


/*----------------------------------------------------------------------------
  Peripheral Register Definitions
  (Addresses are written as base + offset for clarity)
  ----------------------------------------------------------------------------*/
// RCC base: 0x40021000
uint32_t volatile *pRCC_AHBENR  = (uint32_t*)(0x40021000 + 0x14);  // RCC_AHBENR: enables GPIO clocks; for GPIOA, bit 17
uint32_t volatile *pRCC_APB2ENR = (uint32_t*)(0x40021000 + 0x18);  // RCC_APB2ENR: enables SYSCFG (bit 0) and USART1 (bit 14)

// GPIOA base: 0x48000000
uint32_t volatile *pGPIOA_MODER = (uint32_t*)(0x48000000 + 0x00);   // GPIOA mode register
uint32_t volatile *pGPIOA_AFRH  = (uint32_t*)(0x48000000 + 0x24);   // GPIOA alternate function high register

// SYSCFG base: 0x40010000
uint32_t volatile *pSYSCFG_EXTICR1 = (uint32_t*)(0x40010000 + 0x08); // SYSCFG_EXTICR1: for EXTI0, bits[3:0] (0 = Port A)

// EXTI base: 0x40010400
uint32_t volatile *pEXTI_IMR  = (uint32_t*)(0x40010400 + 0x00);      // EXTI Interrupt Mask Register
uint32_t volatile *pEXTI_RTSR = (uint32_t*)(0x40010400 + 0x08);      // EXTI Rising Trigger Selection Register
uint32_t volatile *pEXTI_PR   = (uint32_t*)(0x40010400 + 0x14);      // EXTI Pending Register

// NVIC: using its ISER register; base address 0xE000E100
uint32_t volatile *pNVIC_ISER = (uint32_t*)(0xE000E100 + 0x00);      // NVIC Interrupt Set-Enable Register

// USART1 base: 0x40013800
uint32_t volatile *pUSART1_CR  = (uint32_t*)(0x40013800 + 0x00);      // USART1 Control Register
uint32_t volatile *pUSART1_BRR = (uint32_t*)(0x40013800 + 0x0C);      // USART1 Baud Rate Register
uint32_t volatile *pUSART1_ISR = (uint32_t*)(0x40013800 + 0x1C);      // USART1 Interrupt and Status Register
uint32_t volatile *pUSART1_TDR = (uint32_t*)(0x40013800 + 0x28);      // USART1 Transmit Data Register

/*----------------------------------------------------------------------------
  Function Prototypes
  ----------------------------------------------------------------------------*/
void usart1_init(void);
void usart1_send_char(char c);
void usart1_send_string(const char *s);
void usart1_send_uint32(uint32_t num);
void button_init(void);
void delay(void);
int main(void);

void _reset(void);
void Default_Handler(void);
void NMI_Handler(void)       __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void)       __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void)    __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void EXTI0_1_IRQHandler(void);

/*----------------------------------------------------------------------------
  USART1 Initialization
  ----------------------------------------------------------------------------*/
void usart1_init(void)
{
    // Enable clock for GPIOA: set bit 17 in RCC_AHBENR (IOPAEN)
    *pRCC_AHBENR |= (1 << 17);

    // Enable clocks for SYSCFG (bit 0) and USART1 (bit 14) in RCC_APB2ENR
    *pRCC_APB2ENR |= (1 << 0) | (1 << 14);

    // Configure PA9 and PA10 for alternate function mode (for USART1)
    *pGPIOA_MODER &= ~((0x3U << 18) | (0x3U << 20));  // clear existing bits
    *pGPIOA_MODER |= ((0x2U << 18) | (0x2U << 20));   // set to alternate function mode

    // Set alternate function AF1 for PA9 and PA10 (in AFRH; PA8-15 share this register)
    *pGPIOA_AFRH &= ~((0xFU << 4) | (0xFU << 8));
    *pGPIOA_AFRH |= ((0x1U << 4) | (0x1U << 8));      // AF1: USART1

    // Disable USART1 before configuration
    *pUSART1_CR &= ~(1U << 0);

    // Configure USART1 for desired baud rate
    uint32_t FREQ = 8000000; // Peripheral clock frequency
    uint32_t baud = 115200;  // Desired baud rate
    uint32_t usartdiv = FREQ / baud;
    *pUSART1_BRR = usartdiv;

    // Enable USART1: set UE (bit 0) and TE (bit 3) in CR
    *pUSART1_CR = (1U << 0) | (1U << 3);
}

/*----------------------------------------------------------------------------
  Simple USART1 send functions
  ----------------------------------------------------------------------------*/
// Send a single character over USART1.
void usart1_send_char(char c)
{
// Wait until TXE (Transmit Data Register Empty, bit 7) is set in USART1_ISR.
    // The expression (1 << 7) creates a mask with bit 7 set. We check the status
    // register pointed to by pUSART1_ISR to see if bit 7 is 1 (meaning ready to send).
    while(((*pUSART1_ISR) & (1 << 7)) == 0);
    *pUSART1_TDR = c;
}

/*
 * Function: usart1_send_string
 * ----------------------------
 * Sends a null-terminated string over USART1.
 *
 * Parameters:
 *   s - pointer to the string to be sent
 *
 * How it works:
 *   - The function loops through each character in the string (until it reaches the
 *     null terminator '\0') and sends each character using usart1_send_char.
 */
void usart1_send_string(const char *s)
{
    while(*s)
    {
        usart1_send_char(*s++);
    }
}


/*
 * Function: div10
 * ---------------
 * Approximates division by 10 using fixed-point multiplication and a bit-shift.
 *
 * Parameters:
 *   n - the number to be divided by 10
 *
 * Returns:
 *   The quotient of n divided by 10.
 *
 * How it works:
 *   - Instead of performing an expensive division operation, we multiply n by a
 *     pre-computed "magic" constant (0xCCCCCCCD). This constant approximates (2^35 / 10).
 *   - We then shift the 64-bit result right by 35 bits to get the quotient.
 *   - A right shift of 35 bits effectively "divides" by 2^35, undoing the earlier
 *     multiplication.
 *
 * Note:
 *   - Shifting by 3 or 4 bits would only be correct for division by 8 or 16, not 10.
 */

static inline uint32_t div10(uint32_t n) {
    return (uint32_t)(((uint64_t)n * 0xCCCCCCCDULL) >> 35);
}

/*
 * Function: mod10
 * ---------------
 * Calculates the remainder of n divided by 10.
 *
 * Parameters:
 *   n - the number to compute the remainder for
 *
 * Returns:
 *   The remainder when n is divided by 10.
 *
 * How it works:
 *   - First, the function calculates the quotient using div10(n).
 *   - Then it computes the remainder as: remainder = n - (quotient * 10).
 */

static inline uint32_t mod10(uint32_t n) {
    uint32_t q = div10(n);// Get quotient (n / 10)
    return n - q * 10;// Subtract (quotient * 10) from n to get the remainder
}


/*
 * Function: usart1_send_uint32
 * ----------------------------
 * Sends a 32-bit unsigned integer over USART1 as a decimal (ASCII) string.
 *
 * Parameters:
 *   num - the unsigned 32-bit number to send
 *
 * How it works:
 *   - First, it checks if the number is zero. If so, it sends '0' and returns.
 *   - Otherwise, it repeatedly:
 *       1. Calculates the last digit of the number using mod10.
 *       2. Converts that digit to its ASCII character by adding '0'.
 *       3. Stores the character in a buffer.
 *       4. Divides the number by 10 using div10.
 *     Note: This loop stores digits in reverse order (least-significant digit first).
 *   - Finally, it sends the characters in the buffer in reverse order so that the
 *     digits appear in the correct order.
 */
void usart1_send_uint32(uint32_t num)
{
    char buffer[11];  // up to 10 digits + null terminator
    int i = 0;
    if (num == 0) {
        usart1_send_char('0');// Special case: if the number is 0, send '0' and exit.
        return;
    }
    // Convert number to ASCII digits:
    while (num) {
        buffer[i++] = (char)('0' +mod10(num));
        num = div10(num);// Remove the last digit from the number by dividing it by 10.
    }
    while (i--) {
        usart1_send_char(buffer[i]);// Send them in reverse so that the most significant digit is sent first.
    }
}


/*----------------------------------------------------------------------------
  Button Initialization (User button on PA0, connected to EXTI0)
  ----------------------------------------------------------------------------*/
void button_init(void)
{
    // GPIOA clock is already enabled by usart1_init.
    // PA0 is used for the user button; input mode is default.
    
    // Configure SYSCFG_EXTICR1 for EXTI0: For PA0, bits [3:0] must be 0.
    *pSYSCFG_EXTICR1 &= ~(0xFU);  // 0 selects Port A
    
    // Enable rising edge trigger for EXTI line 0.
    *pEXTI_RTSR |= (1 << 0);
    
    // Unmask EXTI line 0 (enable interrupt for line 0).
    *pEXTI_IMR |= (1 << 0);
    
    // Enable NVIC interrupt for EXTI0_1.
    // For STM32F0, EXTI0_1 is typically enabled by setting bit 5 in NVIC_ISER.
    *pNVIC_ISER |= (1 << 5);
}

/*----------------------------------------------------------------------------
  Simple Delay Function for Debouncing
  ----------------------------------------------------------------------------*/
void delay(void)
{
    for(uint32_t i = 0; i < 250000; i++);
}

/*----------------------------------------------------------------------------
  Main Function
  ----------------------------------------------------------------------------*/
int main(void)
{
    usart1_init();
    button_init();
    
    while(1)
    {
        
        if(g_button_pressed)
        {
            delay();  // Debounce delay.
            g_button_press_count++;
            usart1_send_string("Button is pressed : ");
            usart1_send_uint32(g_button_press_count);
            usart1_send_string("\n");
            g_button_pressed = 0;
        }
        
    }
    
    return 0;
}

/*----------------------------------------------------------------------------
  EXTI0_1 IRQ Handler for PA0 external interrupt
  ----------------------------------------------------------------------------*/
void EXTI0_1_IRQHandler(void)
{

// Clear pending flag
    *pEXTI_PR |= (1 << 0);
    g_button_pressed = 1;

}

/*----------------------------------------------------------------------------
  Reset Handler: Called upon microcontroller reset.
  ----------------------------------------------------------------------------*/
//Startup code
extern uint32_t _sbss, _ebss, _sdata, _edata, _srcdata;
__attribute__((naked, noreturn)) 
void _reset(void) {
    uint8_t *src = (uint8_t *)&_srcdata;   // Use the numeric value
    uint8_t *dest = (uint8_t *)&_sdata;
    while (dest < (uint8_t *)&_edata) {
        *dest++ = *src++;
    }
    dest = (uint8_t *)&_sbss;
    while (dest < (uint8_t *)&_ebss) {
        *dest++ = 0;
    }
    main();
    while (1);
}



/*----------------------------------------------------------------------------
  Default Handler: Catches unused interrupts.
  ----------------------------------------------------------------------------*/
void Default_Handler(void)
{
    while(1);
}

/*----------------------------------------------------------------------------
  Minimal Vector Table placed in .vectors section
  ----------------------------------------------------------------------------*/
__attribute__((section(".vectors")))
void (*const vector_table[7+32])(void) = {
    (void (*)(void))&_estack,  // Use the address of _estack
    _reset,
    NMI_Handler,
    HardFault_Handler,
    Default_Handler,  // Reserved
    Default_Handler,  // Reserved
    Default_Handler,  // Reserved
    Default_Handler, 
    Default_Handler,
    Default_Handler, 
    Default_Handler,  
    Default_Handler,  
    Default_Handler, 
    Default_Handler,  
    Default_Handler,  
    Default_Handler,  
    Default_Handler,  
    Default_Handler,  
    Default_Handler, 
    Default_Handler,  
    Default_Handler,  
    EXTI0_1_IRQHandler,//IRQ5  
    Default_Handler,  
    Default_Handler,  
    Default_Handler,  
    Default_Handler,  
    Default_Handler,  
    Default_Handler,  
    Default_Handler,  
    Default_Handler,  
    Default_Handler,  
    Default_Handler,  
    Default_Handler,  
    Default_Handler,  
    Default_Handler,  
    Default_Handler,  
    Default_Handler,  
    Default_Handler,  
    Default_Handler   
};

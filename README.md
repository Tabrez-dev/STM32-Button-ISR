# STM32 Button ISR Project

## Overview

This project demonstrates a bare-metal implementation on an STM32 microcontroller, focusing on handling a button press via an Interrupt Service Routine (ISR) and transmitting data over USART. It serves as a practical example of direct register manipulation without relying on high-level libraries.

## Features

- **Bare-Metal Implementation:** Direct interaction with hardware registers.
- **Button Interrupt Handling:** Configures an external interrupt on a GPIO pin connected to a button.
- **USART Communication:** Initializes USART1 for serial data transmission upon button press.
- **J-Link & ST-Link Flashing Support:** Provides scripts to flash the firmware using J-Link or ST-Link.

## Hardware Requirements

- **Microcontroller:** STM32F072RB (or any compatible STM32 MCU)
- **Button:** Connected to GPIO pin PA0.
- **USB-to-Serial Adapter:** For monitoring USART1 transmissions on a PC.

## Software Requirements

- **Toolchain:** `arm-none-eabi-gcc`
- **Debugger/Programmer:** J-Link, ST-Link V2
- **Development Environment:** Vim editor using ubuntu and a serial monitor to see the output like minicom.
- **Flashing Tools:** `J-Link` or  `st-flash`

## Setup and Configuration

### 1. Clock Configuration
- Enable clock for GPIOA and USART1 peripherals.
- Configure system clock frequency.

### 2. GPIO Configuration
- Configure **PA9 (TX)** and **PA10 (RX)** for USART1.

### 3. USART1 Initialization
- Set baud rate to 115200.
- Enable transmitter and configure USART1 parameters.

### 4. Interrupt Configuration
- Map **EXTI0** line to **PA0**.
- Enable **rising edge trigger** for EXTI0.
- Enable NVIC interrupt for **EXTI0_1**.

## Technical Details

### 1. Vector Table Configuration
- **Reference:** [ARM Cortex-M0 Generic User Guide](https://www.keil.com/dd/docs/datashts/arm/cortex_m0p/r0p0/dui0662a_cortex_m0p_r0p0_dgug.pdf)
 
  The vector table holds the addresses for all exception handlers (including the reset vector and ISR pointers). This is fundamental for the processor to correctly jump to the appropriate handler when an interrupt occurs.
  
![Vector Table Layout](https://github.com/user-attachments/assets/86b776d5-5fe5-4c1a-a6eb-fca785ca43e6)

### 2. Writing the Reset Handler
  The reset handler initializes the system after a reset. It sets up the stack pointer, copies initialized data to RAM, clears the BSS section, and then calls `main()`.  

### 3. USART Configuration
- **Reference:** STM32F072RB Datasheet
 
  Configuring USART involves setting parameters such as baud rate and word length, and enabling the transmitter and receiver. This ensures proper serial communication over USART1.  
 ![image](https://github.com/user-attachments/assets/b256b618-a15c-4d72-8e01-8f53773c38ad)

![image](https://github.com/user-attachments/assets/a87a6053-8c1a-4ddf-b5cf-68197f19af40)

### 4. Configuring the Button and External Interrupt
- **Reference:** RM0091 Documentation

  The external interrupt (EXTI) is configured to trigger on a button press. By mapping the EXTI line to a specific GPIO pin (PA0) and setting the trigger condition (e.g., rising edge), the system can respond immediately via the corresponding ISR.

  ![image](https://github.com/user-attachments/assets/ecc9632e-05ea-46d8-b449-5f0beb7c8831)

*From STM32F072RBT6-DISCO Board Schematic we see that PA0 is connected to the user button internally*

![image](https://github.com/user-attachments/assets/4439a036-c840-46ac-92bd-ff4358561b58)

*RM0091 Reference Manual*

![image](https://github.com/user-attachments/assets/0f7a8d22-cb91-4d0f-b662-1f6765410324)

*UM1690 user manual*

![image](https://github.com/user-attachments/assets/74d02d84-512b-44b3-98aa-fe78308a13a5)

*STM32F0X2RB data sheet*

## Build and Flash

### 1. Build the Firmware
```sh
make
```

### 2. Flash using J-Link
```sh
make stflash
```
or

```
make jflash
```
Note: To use J-Link/ make jflash to flash firmware.bin you need to install j-link from STReflash Utility which is only available in Windows operating system.

## Output

As seen using minicom serial interface

![image](https://github.com/user-attachments/assets/cfea4bd8-db75-4fe9-b6b1-2cf4068a6509)


## Challenges and Solutions

Throughout the development of this STM32 Button ISR project, several challenges were encountered and resolved:

- **Unpredictable Data/BSS Layout Without SORT**  
  **Problem:** With compiler flags like `-ffunction-sections` and `-fdata-sections`, each function and variable ended up in its own subsection (e.g. `.data.foo`, `.bss.bar`). When simply using wildcards (e.g. `*(.data)`) in the linker script, the ordering was unpredictable. This led to an incomplete copy of the initialized data and an incomplete zeroing of the uninitialized data (BSS). As a result, global variables (such as the button press counter) were not reset properly, and sometimes a hard fault occurred.  
  **Resolution:** Modified the .data and .bss sections to include the `SORT()` operator (for example, `*(.data SORT(.data.*))` and `*(.bss SORT(.bss.*) COMMON)`). This forced a predictable, contiguous layout so that the startup code correctly copies all of .data from flash and zeros all of .bss.

- **Vector Table Misconfiguration:**  
  - **Issue:** The vector table was not being placed at the expected memory location.  
  - **Cause:** The section name used for the vector table (e.g., `.vector`) did not match the section specified in the linker script.  
  - **Resolution:** Updated the section attribute in the vector table to match the linker script (e.g., `.vectors`).

- **UART Baud Rate Mismatch:**  
  - **Issue:** When both UART and button functionalities were enabled, the USART output was garbled.  
  - **Cause:** A mismatch between the configured baud rate on the STM32 and the baud rate set in the terminal, likely due to an incorrect calculation of the BRR value.  
  - **Resolution:** Recalculated the baud rate register (BRR) value based on the actual peripheral clock frequency and ensured that the terminal settings matched.  
    - For example, for an 8 MHz clock and 115200 bps, the BRR value was set appropriately, and the system clock configuration was verified.

- **EXTI and NVIC Configuration:**  
  - **Issue:** The button press interrupt (EXTI0_1) was triggering the Default_Handler instead of the correct interrupt service routine.  
  - **Cause:** Issues with EXTI configuration or the vector table entry for the button interrupt, including improper clearing of the pending flag.  
  - **Resolution:**  
    - Verified that SYSCFG_EXTICR1 correctly mapped EXTI0 to Port A.  
    - Ensured that EXTI_RTSR and EXTI_IMR were correctly configured.  
    - Modified the EXTI0_1 IRQ handler to clear the pending flag properly and set the button-pressed flag.
    - Placed the EXTI int the correct postition in the vector table (index 21) by looking at vector table in the cortex m0 user guide

- **Debugging and Flash Issues:**  
  - **Issue:** Debugging with SEGGER Ozone revealed issues like missing DWARF debug information and flash content mismatches.  
  - **Cause:** Incorrect linker script entry point 
  - **Resolution:** Updated the linker script to correctly point to startingof the program i.e Reset handler

- **USART Data Representation:**  
  - *Problem:* Directly sending integers over USART transmitted raw binary data (non-printable characters) rather than human-readable digits.  
  - *Solution:* Implemented custom conversion functions (`div10` and `mod10`) to convert a 32-bit integer into an ASCII string, ensuring that numbers appear correctly in the terminal.

These solutions not only resolved the immediate issues but also contributed to a more robust understanding of bare-metal STM32 programming.


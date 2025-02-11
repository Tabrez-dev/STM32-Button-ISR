ENTRY(_reset);

MEMORY
{
    FLASH(Rx) : ORIGIN = 0x08000000, LENGTH = 128K
    SRAM(RWX) : ORIGIN = 0x20000000, LENGTH = 16K
}

_estack = ORIGIN(SRAM) + LENGTH(SRAM);

SECTIONS
{
    .text : {
 	KEEP(*(.vectors))
	*(.text)
	*(.rodata)
	
     } > FLASH
    .data : {
        _sdata = .;
        /**(.data)*/
        *(.data SORT(.data.*))
        _edata = .;
    } > SRAM AT > FLASH
    _srcdata = LOADADDR(.data);

    .bss : {
        _sbss = .;
       /* *(.bss) */ 
        *(.bss SORT(.bss.*) COMMON)
        _ebss = .;
    } > SRAM
}

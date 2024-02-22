OUTPUT_ARCH(riscv)
ENTRY( _start )
MEMORY
{
  LOCAL        : ORIGIN = 0x70010000, LENGTH = 0x70000
  L2           : ORIGIN = 0x80000000, LENGTH = 0x0fffffff
}



SECTIONS
{
  .init :
  {
    . = ALIGN(4);
    KEEP( *(.init) )
  } > LOCAL


  .fini :
  {
    . = ALIGN(4);
    KEEP( *(.fini) )
  } > LOCAL

  .preinit_array : {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } > LOCAL

  .init_array : {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__init_array_start = .);
    __CTOR_LIST__ = .;
    LONG((__CTOR_END__ - __CTOR_LIST__) / 4 - 2)
    KEEP(*(.ctors.start))
    KEEP(*(.ctors))
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array ))
    LONG(0)
    __CTOR_END__ = .;
    PROVIDE_HIDDEN (__init_array_end = .);
  } > LOCAL

  .fini_array : {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__fini_array_start = .);
    __DTOR_LIST__ = .;
    LONG((__DTOR_END__ - __DTOR_LIST__) / 4 - 2)
    KEEP(*(.dtors.start))
    KEEP(*(.dtors))
    LONG(0)
    __DTOR_END__ = .;
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array ))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } > LOCAL

  .boot : {
    . = ALIGN(4);
    *(.boot)
    *(.boot.data)
  } > LOCAL

  .rodata : {
    . = ALIGN(4);
    *(.rodata);
    *(.rodata.*)
    *(.srodata);
    *(.srodata.*)
    *(.eh_frame*)
  } > LOCAL

  .shbss : {
    . = ALIGN(4);
    *(.shbss)
  } > LOCAL

  .talias : {
  } > LOCAL

  .gnu.offload_funcs : {
    . = ALIGN(4);
    KEEP(*(.gnu.offload_funcs))
  } > LOCAL

  .gnu.offload_vars : {
    . = ALIGN(4);
    KEEP(*(.gnu.offload_vars))
  } > LOCAL

  .stack : {
    . = ALIGN(4);
    . = ALIGN(16);
    stack_start = .;
    . = . + 0x1000;
    stack = .;
  } > LOCAL

  .data : {
    . = ALIGN(4);
    sdata  =  .;
    _sdata  =  .;
    *(.data_fc)
    *(.data_fc.*)
    *(.data);
    *(.data.*)
    *(.sdata);
    *(.sdata.*)
    *(.heapl2ram)
    *(.fcTcdm)
    *(.fcTcdm.*)
    *(.fcTcdm_g)
    *(.fcTcdm_g.*)
    . = ALIGN(4);
    edata  =  .;
    _edata  =  .;
  } > LOCAL

  .bss : {
    . = ALIGN(8);
    _bss_start = .;
    *(.bss)
    *(.bss.*)
    *(.sbss)
    *(.sbss.*)
    *(COMMON)
    . = ALIGN(4);
    _bss_end = .;
  } > LOCAL

  __l2_priv0_end = ALIGN(4);

  /*
   * LOCAL PRIVATE BANK1
   *
   * Contains FC code
   */

  .vectors MAX(0x60010000,ALIGN(256)) :
  {
    __irq_vector_base = .;
    KEEP(*(.vectors))
  } > LOCAL

  .text :
  {
    . = ALIGN(4);
    _stext = .;
    *(.text)
    *(.text.*)
    _etext  =  .;
    *(.lit)
    *(.shdata)
    _endtext = .;
    . = ALIGN(4);
  } > LOCAL

  __l2_priv1_end = ALIGN(4);

  /*
   * L2 SHARED BANKS
   *
   * Contains other data such as peripheral data and cluster code and data
   */

  .l2_data :
  {
    . = ALIGN(4);
    *(.l2_data)
    *(.l2_data.*)
    *(.data_fc_shared)
    *(.data_fc_shared.*)
    . = ALIGN(4);
  } > L2

  __l2_shared_end = ALIGN(4);

}

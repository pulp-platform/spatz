
void __attribute__ ((noinline)) FUNC_NAME(void* addrA,void* addrB, void* addrC, int K, int N, int M, int shift)
{
    int strideA;
    int strideB;
    int strideC;
    asm volatile(
        // --- PROLOGUE: Save registers ---
        "addi sp, sp, -0x30   \n\t" 
        "sw s0 , 0x2c(sp)     \n\t"   
        "sw s1 , 0x28(sp)     \n\t"   
        "sw s2 , 0x24(sp)     \n\t"   
        "sw s3 , 0x20(sp)     \n\t"   
        "sw s4 , 0x1c(sp)     \n\t"   
        "sw s5 , 0x18(sp)     \n\t"   
        "sw s6 , 0x14(sp)     \n\t"   
        "sw s7 , 0x10(sp)     \n\t"   
        "sw s8 , 0x0c(sp)     \n\t"   
        "sw s9 , 0x08(sp)     \n\t"   
        "sw s10, 0x04(sp)     \n\t"   
        "sw s11, 0x00(sp)     \n\t"   

        // 1. Data Types Configuration
        "mmac.dt %[dt_typeC], %[dt_typeA], %[dt_typeB] \n\t"

        "add t0, x0, %[M] \n\t"        // t0 = Remaining M
        "add s0, x0, %[addrC] \n\t"    // s0 = Current row pointer for C
        "add s1, x0, %[addrA] \n\t"    // s1 = Current row pointer for A (LHS)

        "sll %[strideA], %[M], %[shift] \n\t"   // Compute strideA = M * 2^shift
        "sll %[strideB], %[N], %[shift] \n\t"   // Compute strideB = M * 2^shift
        "sll %[strideC], %[N], %[shift] \n\t"   // Compute strideC = M * 2^shift

        "1: \n\t"
        "mcfgm t3, t0, %[rmul] \n\t"   // t3 = Processed M rows

        "add t1, x0, %[N] \n\t"        // t1 = Remaining N
        "add s2, x0, s0 \n\t"          // s2 = C tile pointer
        "add s3, x0, %[addrB] \n\t"    // s3 = B (RHS) tile pointer 

        "2: \n\t"
        "add t2, x0, %[K] \n\t"        // t2 = Remaining K
        "mcfgk t5, t2 \n\t"            // t5 = Processed K depth for a single block
        "mcfgn t4, t1, %[cmul] \n\t"   // t4 = Processed N columns
        "mzero.a acc0 \n\t"            // Reset accumulator
        "add s4, x0, s1 \n\t"          // s4 = A tile pointer
        "add s5, x0, s3 \n\t"          // s5 = B tile pointer

        "3: \n\t"

        // 2. Load First Tiles
        "mld.lhs m0, (s4), %[strideA] \n\t"  
        "mld.rhs m4, (s5), %[strideB] \n\t"       
        "mmacc   acc0, m4, m0 \n\t" 
        "mul  s8, t5, %[strideA] \n\t"   
        "mul  s9, t5, %[strideB] \n\t"   
          
        "add  s6, s4, s8 \n\t"               // s6 = Pointer to 2nd tile of A
        "mld.lhs m2, (s6), %[strideA] \n\t" 
          
        "add  s7, s5, s9 \n\t"               // s7 = Pointer to 2nd tile of B
        "mld.rhs m6, (s7), %[strideB] \n\t"          

        // 5. Advance K pointers by TWO blocks
        "add  t6, s8, s8 \n\t"               
        "add  s4, s4, t6 \n\t"               
        
        "add  t6, s9, s9 \n\t"               
        "add  s5, s5, t6 \n\t"               

        // Decrease remaining K by two blocks
        "add  t6, t5, t5 \n\t"
        "sub  t2, t2, t6 \n\t"          
        "mmacc   acc0, m6, m2 \n\t" 
        "mcfgk t5, t2 \n\t"            // t5 = Processed K depth for a single block             
        "bgtz t2, 3b \n\t"          

        // 6. Transfer to MR and Store
        "mmov.am m8, acc0 \n\t"     
        "mst     m8, (s2), %[strideC] \n\t"  

        // 7. Advance along N
        "slli t6, t4, 2 \n\t"                
        "add  s2, s2, t6 \n\t"               
        "add  s3, s3, t6 \n\t"               

        "sub t1, t1, t4 \n\t"                
        "bgtz t1, 2b \n\t"          

        // 8. Advance along M
        "mul t6, t3, %[strideC] \n\t"        
        "add s0, s0, t6 \n\t"
        
        "slli t6, t3, 2 \n\t"                
        "add s1, s1, t6 \n\t"

        "sub t0, t0, t3 \n\t"                
        "bgtz t0, 1b \n\t"

        // --- EPILOGUE: Restore registers ---
        "lw s0 , 0x2c(sp)         \n\t"
        "lw s1 , 0x28(sp)         \n\t"
        "lw s2 , 0x24(sp)         \n\t"
        "lw s3 , 0x20(sp)         \n\t"
        "lw s4 , 0x1c(sp)         \n\t"
        "lw s5 , 0x18(sp)         \n\t"
        "lw s6 , 0x14(sp)         \n\t"
        "lw s7 , 0x10(sp)         \n\t"
        "lw s8 , 0x0c(sp)         \n\t"
        "lw s9 , 0x08(sp)         \n\t"
        "lw s10, 0x04(sp)         \n\t"
        "lw s11, 0x00(sp)         \n\t"
        "addi sp, sp, 0x30        \n\t" 
        // ----------------------------------

        : 
        : [addrA] "r" (addrA), [addrB] "r" (addrB), [addrC] "r" (addrC),
          [M] "r" (M), [N] "r" (N), [K] "r" (K), [shift] "r" (shift),
          [strideA] "r" (strideA), [strideB] "r" (strideB), [strideC] "r" (strideC),
          [dt_typeC] "i" (DTC), [dt_typeA] "i" (DTA), [dt_typeB] "i" (DTB),
          [rmul] "i" (RMUL_2), [cmul] "i" (CMUL_2)
        : "t0", "t1", "t2", "t3", "t4", "t5", "t6", "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "memory"
    );
}
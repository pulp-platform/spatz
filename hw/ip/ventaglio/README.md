# Ventaglio Streamline Sparse Execution Unit

**WIP**

## Change of encoding
- The current vfxmacc.vf instruction does not encode the *vid* for the index metadata; as a result, the dependency in Spatz controller often fail to trace and gate the requests. Therefore, we need to encode this metadata explicitly in the encoding space.

Encoding proposal — vfxmacc.vrrf in custom-0  

   31    27 26 25 24    20 19    15 14  12 11   7 6      0                                                                                                                                                         
  +--------+-----+--------+--------+-----+------+--------+                                                                                                                                                         
  |  vs1   |fnct2|  vs2   |  rs1   |fnct3|  vd  |opcode  |                                                                                                                                                         
  +--------+-----+--------+--------+-----+------+--------+                                                                                                                                                         
     idx          weight    fp_scal         acc   custom-0   

Concretely:                                                                                                                                                                                                      
  VFXMACC_VRRF = 32'b?????00????????????010?????0001011                                                                                                                                                            
  - opcode[6:0] = 0001011 (custom-0)                                                                                                                                                                               
  - funct3[14:12] = 010 (picks this op out of the custom-0 family; reserve 011/100/etc. for future variants)
  - funct2[26:25] = 00 (precision tag — FP32; future 01=FP16, 10=BF16…)                                                                                                                                            
  - vd[11:7], rs1[19:15], vs2[24:20], vs1[31:27]                                                                                                                                                                   
                                                                                                                                                                                                                   
  vfxmacc.vrrf v16, ft0, v8, v20 → vd=16, rs1=ft0(0), vs2=8, vs1=20, funct2=00, funct3=010, opcode=0001011. 
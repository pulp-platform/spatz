Implement CSR SpMV (y=A*x) with fp16/fp32/fp64 kernels similar to the existing GEMV style.

CSR:
  row_ptr[nrows+1] u32, col_idx[nnz] u32, val[nnz] fp16/32/64, x[ncols], y[nrows].

Scalar reference:
  for i: sum=0; for k=row_ptr[i]..row_ptr[i+1)-1: sum += val[k]*x[col_idx[k]]; y[i]=sum.

Vector kernel (within each row, strip-mine nnz):
  for i:
    sum_scalar = 0
    k = row_ptr[i]
    while k < row_ptr[i+1]:
      vl = vsetvli(min(VLMAX, row_ptr[i+1]-k), e16/e32/e64, m1, ta, ma)
      v_val = vleXX.v(&val[k])
      v_idx = vle32.v(&col_idx[k])
      v_off = v_idx << shift   (shift=1 for fp16, 2 for fp32, 3 for fp64)
      v_x   = vluxei32.v(x_base, v_off)
      v_p   = v_val * v_x
      reduce v_p to scalar with vfredsum; add into sum_scalar
      k += vl
    y[i] = sum_scalar

Add small-row fallback: if nnz < N (e.g., 4 or 8) do scalar loop.

Keep same multi-core row partitioning scheme as GEMV main.c and add timing + checksum.
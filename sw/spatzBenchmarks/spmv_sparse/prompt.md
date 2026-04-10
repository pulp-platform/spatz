Implement dense-backed sparse SpMV (y=A*x) for the Spatz vector processor.

Data model:
- `row_ptr[M+1]` and `col_idx[K]` describe the logical sparse structure.
- `A_dense[M*N]` stores the full row-major matrix including zeros.
- `x[N]`, `y[M]` are dense vectors.

Cache-mode kernel goal:
- gather `A_dense[row][col_idx[k]]`
- gather `x[col_idx[k]]`
- multiply and reduce with supported RVV instructions only

SPM baseline goal:
- remain correct for irregular row lengths
- double-buffer dense row tiles and matching `col_idx` tiles
- performance can be poor; correctness matters more than speed

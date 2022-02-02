#ifndef CONV2D_H
#define CONV2D_H

#include <stdint.h>

void conv2d_3x3(int32_t *o, int32_t *i, int32_t *f, int32_t R, int32_t C,
                int32_t F);
void conv2d_vec_4xC_slice_init_3x3(int32_t *o, int32_t C);
void conv2d_vec_4xC_slice_preload_3x3(int32_t *i, int32_t C, int32_t F);
void conv2d_vec_4xC_slice_move_3x3(int32_t C, int32_t F);
void conv2d_vec_4xC_3x3(int32_t *o, int32_t *i, int32_t *f, int32_t C,
                        int32_t F);

void conv2d_5x5(int32_t *o, int32_t *i, int32_t *f, int32_t R, int32_t C,
                int32_t F);
void conv2d_vec_4xC_slice_init_5x5(int32_t *o, int32_t C);
void conv2d_vec_4xC_slice_preload_5x5(int32_t *i, int32_t C, int32_t F);
void conv2d_vec_4xC_slice_move_5x5(int32_t C, int32_t F);
void conv2d_vec_4xC_5x5(int32_t *o, int32_t *i, int32_t *f, int32_t C,
                        int32_t F);

void conv2d_7x7(int32_t *o, int32_t *i, int32_t *f, int32_t R, int32_t C,
                int32_t F);
void conv2d_vec_4xC_slice_init_7x7(int32_t *o, int32_t C);
void conv2d_vec_4xC_slice_preload_7x7(int32_t *i, int32_t C, int32_t F);
void conv2d_vec_4xC_slice_move_7x7(int32_t C, int32_t F);
void conv2d_vec_4xC_7x7(int32_t *o, int32_t *i, int32_t *f, int32_t C,
                        int32_t F);

#endif

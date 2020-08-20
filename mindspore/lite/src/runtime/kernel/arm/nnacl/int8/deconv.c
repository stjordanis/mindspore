/**
 * Copyright 2020 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "nnacl/int8/deconv.h"
#include "nnacl/int8/matmul_int8.h"
#include "nnacl/int8/common_func.h"
int DeConvPostInt8C8(const int32_t *src, const int32_t *bias, int32_t *tmp, int8_t *out, int output_channel,
                     ConvParameter *conv_param) {
  /* row8x8-major(ih*iw x oc*kh*kw)  ->  row8-major(oh*ow x oc) */
  size_t input_plane = conv_param->input_w_ * conv_param->input_h_;
  size_t kernel_plane = conv_param->kernel_w_ * conv_param->kernel_h_;
  size_t output_plane = conv_param->output_w_ * conv_param->output_h_;
  int oc8 = UP_DIV(output_channel, C8NUM);
  int in_plane8 = UP_ROUND(input_plane, 8);

  for (int c = 0; c < oc8; c++) {
    int32_t *dst_ptr = tmp + c * output_plane * C8NUM;
    const int32_t *src_ptr = src + c * in_plane8 * kernel_plane * C8NUM;
    memset(dst_ptr, 0, output_plane * C8NUM * sizeof(int32_t));

    for (int ih = 0; ih < conv_param->input_h_; ih++) {
      for (int iw = 0; iw < conv_param->input_w_; iw++) {
        int oh = ih * conv_param->stride_h_ - conv_param->pad_h_;
        int ow = iw * conv_param->stride_w_ - conv_param->pad_w_;

        int kh_start = MSMAX(0, UP_DIV(-oh, conv_param->dilation_h_));
        int kh_end = MSMIN(conv_param->kernel_h_, UP_DIV(conv_param->output_h_ - oh, conv_param->dilation_h_));
        int kw_start = MSMAX(0, UP_DIV(-ow, conv_param->dilation_w_));
        int kw_end = MSMIN(conv_param->kernel_w_, UP_DIV(conv_param->output_w_ - ow, conv_param->dilation_w_));
        for (int kh = kh_start; kh < kh_end; kh++) {
          for (int kw = kw_start; kw < kw_end; kw++) {
            int src_index = ih * conv_param->input_w_ * C8NUM + iw * C8NUM +
                            kh * input_plane * conv_param->kernel_w_ * C8NUM + kw * input_plane * C8NUM;
            int dst_index = oh * conv_param->output_w_ * C8NUM + ow * C8NUM +
                            kh * conv_param->dilation_h_ * conv_param->output_w_ * C8NUM +
                            kw * conv_param->dilation_w_ * C8NUM;
            for (int i = 0; i < C8NUM; i++) {
              dst_ptr[dst_index + i] += src_ptr[src_index + i];
            }
          } /*kw*/
        }   /*kh*/
      }     /*iw*/
    }       /*ih*/
  }         /*oc8*/

  PostFuncInt8C8(tmp, bias, out, output_channel, output_plane, conv_param->conv_quant_arg_.quant_multiplier_[0],
                 conv_param->conv_quant_arg_.left_shift_[0], conv_param->conv_quant_arg_.right_shift_[0],
                 conv_param->conv_quant_arg_.output_quant_args_[0].zp_, conv_param->conv_quant_arg_.out_act_min_[0],
                 conv_param->conv_quant_arg_.out_act_max_[0]);
  return NNACL_OK;
}

int DeConvPostInt8C4(const int32_t *src, const int32_t *bias, int32_t *tmp, int8_t *out, int output_channel,
                     ConvParameter *conv_param) {
  /* row4x4-major(ih*iw x oc*kh*kw)  ->  row4-major(oh*ow x oc) */
  size_t input_plane = conv_param->input_w_ * conv_param->input_h_;
  size_t kernel_plane = conv_param->kernel_w_ * conv_param->kernel_h_;
  size_t output_plane = conv_param->output_w_ * conv_param->output_h_;
  int oc4 = UP_DIV(output_channel, C4NUM);
  int in_plane4 = UP_ROUND(input_plane, C4NUM);

  int src_iw_stride = C4NUM;
  int src_ih_stride = conv_param->input_w_ * C4NUM;
  int src_kw_stride = input_plane * C4NUM;
  int src_kh_stride = input_plane * conv_param->kernel_w_ * C4NUM;
  int dst_oh_stride = conv_param->output_w_ * C4NUM;
  int dst_ow_stride = C4NUM;
  int dst_kh_stride = conv_param->dilation_h_ * conv_param->output_w_ * C4NUM;
  int dst_kw_stride = conv_param->dilation_w_ * C4NUM;

  for (int c = 0; c < oc4; c++) {
    int32_t *dst_ptr = tmp + c * output_plane * C4NUM;
    const int32_t *src_ptr = src + c * in_plane4 * kernel_plane * C4NUM;
    memset(dst_ptr, 0, output_plane * C4NUM * sizeof(int32_t));

    for (int ih = 0; ih < conv_param->input_h_; ih++) {
      for (int iw = 0; iw < conv_param->input_w_; iw++) {
        int oh = ih * conv_param->stride_h_ - conv_param->pad_h_;
        int ow = iw * conv_param->stride_w_ - conv_param->pad_w_;

        int kh_start = MSMAX(0, UP_DIV(-oh, conv_param->dilation_h_));
        int kh_end = MSMIN(conv_param->kernel_h_, UP_DIV(conv_param->output_h_ - oh, conv_param->dilation_h_));
        int kw_start = MSMAX(0, UP_DIV(-ow, conv_param->dilation_w_));
        int kw_end = MSMIN(conv_param->kernel_w_, UP_DIV(conv_param->output_w_ - ow, conv_param->dilation_w_));
        for (int kh = kh_start; kh < kh_end; kh++) {
          for (int kw = kw_start; kw < kw_end; kw++) {
            int src_index = ih * src_ih_stride + iw * src_iw_stride + kh * src_kh_stride + kw * src_kw_stride;
            int dst_index = oh * dst_oh_stride + ow * dst_ow_stride + kh * dst_kh_stride + kw * dst_kw_stride;
            int32_t *tmp_dst = dst_ptr + dst_index;
            const int32_t *tmp_src = src_ptr + src_index;
#ifndef ENABLE_ARM64
            for (int i = 0; i < C4NUM; i++) {
              tmp_dst[i] += tmp_src[i];
            }
#else
            asm volatile(
              "mov x0, %[tmp_src] \n"
              "mov x1, %[tmp_dst] \n"

              "ld1 {v0.4s}, [x0] \n"
              "ld1 {v1.4s}, [x1] \n"

              "add v0.4s, v0.4s, v1.4s \n"

              "st1 {v0.4s}, [x1] \n"

              :
              : [ tmp_src ] "r"(tmp_src), [ tmp_dst ] "r"(tmp_dst)
              : "x0", "x1", "v0", "v1");
#endif
          } /*kw*/
        }   /*kh*/
      }     /*iw*/
    }       /*ih*/
  }         /*oc*/

  PostFuncInt8C4(tmp, bias, out, output_channel, output_plane, conv_param->output_channel_,
                 conv_param->conv_quant_arg_.quant_multiplier_[0], conv_param->conv_quant_arg_.left_shift_[0],
                 conv_param->conv_quant_arg_.right_shift_[0], conv_param->conv_quant_arg_.output_quant_args_[0].zp_,
                 conv_param->conv_quant_arg_.out_act_min_[0], conv_param->conv_quant_arg_.out_act_max_[0]);
  return NNACL_OK;
}

void DeConvWeightTransInt8(int8_t *src, int8_t *dst, int input_channel, int output_channel, int plane,
                           bool support_optimize_) {
  if (support_optimize_) {
    int ic16 = UP_ROUND(input_channel, C16NUM);
    int oc4 = UP_ROUND(output_channel, C4NUM);
    for (int ic = 0; ic < input_channel; ic++) {
      int ic16div = ic / C16NUM, ic16mod = ic % C16NUM;
      for (int oc = 0; oc < output_channel; oc++) {
        int oc4div = oc / C4NUM, oc4mod = oc % C4NUM;
        for (int hw = 0; hw < plane; hw++) {
          int src_index = ic * output_channel * plane + hw * output_channel + oc;
          int dst_index =
            hw * ic16 * oc4 + oc4div * ic16 * C4NUM + ic16div * C16NUM * C4NUM + oc4mod * C16NUM + ic16mod;
          dst[dst_index] = src[src_index];
        }
      }
    }
  } else {
    /* normal int8 deconv */
  }
  return;
}

void DeConvPackWeightSum(int8_t *weight, int32_t *weight_sum, int32_t input_zp, int32_t filter_zp, int deep16, int col4,
                         bool suppport_opt) {
  if (suppport_opt) {
    for (int c = 0; c < col4; c++) {
      int c4div = c / C4NUM, c4mod = c % C4NUM;
      int32_t value = 0;
      for (int r = 0; r < deep16; r++) {
        int r16div = r / 16, r16mod = r % 16;
        int src_index = c4div * deep16 * C4NUM + r16div * C4NUM * C16NUM + c4mod * C16NUM + r16mod;
        value += weight[src_index];
      }
      weight_sum[c] = filter_zp * input_zp * deep16 - value * input_zp;
    }
  } else {
    /* normal int8 deconv */
  }
  return;
}

void DeConvPackInputSum(const int8_t *src, int32_t *dst, int32_t filter_zp, int row4, int col16, bool suppport_opt) {
  if (suppport_opt) {
    for (int r = 0; r < row4; r++) {
      int32_t tmp_value = 0;
      for (int c = 0; c < col16; c++) {
        int r4div = r / C4NUM, r4mod = r % C4NUM, c16div = c / C16NUM, c16mod = c % C16NUM;
        int src_index = r4div * C4NUM * col16 + c16div * C16NUM * C4NUM + r4mod * C16NUM + c16mod;
        tmp_value += src[src_index];
      }
      dst[r] = tmp_value * filter_zp;
    }
  } else {
    /* normal int8 deconv */
  }
  return;
}

int DeConvInt8(const int8_t *input, const int8_t *weight, int32_t *output, int32_t *weight_sum, int32_t *input_sum,
               size_t act_row, size_t act_col, size_t act_deep, ConvParameter *conv_param,
               MATMUL_OPT_R4_FUNC matmul_func) {
  if (matmul_func != NULL) {
    matmul_func(input, weight, output, act_row, act_col, act_deep, input_sum, weight_sum);
  } else {
    /* normal int8 deconv */
  }
  return NNACL_OK;
}

int DeConvPostInt8(const int32_t *src, const int32_t *bias, int32_t *tmp, int8_t *out, int output_channel,
                   ConvParameter *conv_param, bool support_optimize) {
  int error_code = NNACL_OK;
  if (support_optimize) {
    error_code = DeConvPostInt8C4(src, bias, tmp, out, output_channel, conv_param);
  } else {
    /* normal int8 deconv post */
  }
  return error_code;
}

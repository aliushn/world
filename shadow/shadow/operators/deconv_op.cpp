#include "deconv_op.hpp"

#include "activate_op.hpp"

namespace Shadow {

void DeconvOp::Forward() {
  if (bias_term_) {
    CHECK_EQ(bottoms_size(), 3);
  } else {
    CHECK_EQ(bottoms_size(), 2);
  }

  const auto *bottom = bottoms<float>(0);
  const auto *weight = bottoms<float>(1);
  auto *top = mutable_tops<float>(0);

  CHECK_NE(bottom, top);

  int batch = bottom->shape(0), in_c = bottom->shape(1),
      in_h = bottom->shape(2), in_w = bottom->shape(3);

  CHECK_EQ(in_c % group_, 0);

  auto top_shape = bottom->shape();
  top_shape[1] = num_output_;
  top_shape[2] = deconv_out_size(in_h, kernel_size_, stride_, pad_, dilation_);
  top_shape[3] = deconv_out_size(in_w, kernel_size_, stride_, pad_, dilation_);
  top->reshape(top_shape);

  conv_in_c = num_output_, conv_out_c = in_c;

  conv_out_spatial_dim_ = bottom->count(2);
  out_spatial_dim_ = top->count(2);
  kernel_dim_ = kernel_size_ * kernel_size_ * conv_in_c / group_;

  weight_offset_ = conv_out_c * kernel_dim_ / group_;
  col_offset_ = kernel_dim_ * conv_out_spatial_dim_;
  output_offset_ = conv_out_c * conv_out_spatial_dim_ / group_;

  Blas::Set(top->count(), 0, top->mutable_data(), 0);

#if defined(USE_CUDNN)
  if (use_cudnn_) {
    cudnn::setConvolution2dDesc<float>(&conv_desc_, pad_, pad_, stride_,
                                       stride_, dilation_, dilation_, group_);
    cudnn::setTensor4dDesc<float>(&bottom_desc_, batch, in_c, in_h, in_w);
    cudnn::setTensor4dDesc<float>(&top_desc_, batch, num_output_, top_shape[2],
                                  top_shape[3]);
    cudnn::setFilter4dDesc<float>(&filter_desc_, conv_out_c, conv_in_c / group_,
                                  kernel_size_, kernel_size_);
    if (bias_term_) {
      cudnn::setTensor4dDesc<float>(&bias_desc_, 1, num_output_, 1, 1);
    }

    size_t workspace_limit_bytes = group_ == 1 ? 64 * 1024 * 1024 : 0;

    CUDNN_CHECK(cudnnGetConvolutionBackwardDataAlgorithm(
        cudnnHandle_t(op_ws_->Ctx()->cudnn_handle()), filter_desc_,
        bottom_desc_, conv_desc_, top_desc_,
        CUDNN_CONVOLUTION_BWD_DATA_SPECIFY_WORKSPACE_LIMIT,
        workspace_limit_bytes, &bwd_data_algo_));

    CUDNN_CHECK(cudnnGetConvolutionBackwardDataWorkspaceSize(
        cudnnHandle_t(op_ws_->Ctx()->cudnn_handle()), filter_desc_,
        bottom_desc_, conv_desc_, top_desc_, bwd_data_algo_,
        &workspace_bwd_size_));

    if (workspace_bwd_size_ > 0) {
      op_ws_->GrowTempBuffer(static_cast<int>(workspace_bwd_size_),
                             sizeof(unsigned char));
      workspace_ = op_ws_->CreateTempBlob<unsigned char>(
          {static_cast<int>(workspace_bwd_size_)}, op_name_ + "_workspace");
    }

    auto *workspace_ptr =
        workspace_bwd_size_ > 0 ? workspace_->mutable_data() : nullptr;
    CUDNN_CHECK(cudnnConvolutionBackwardData(
        cudnnHandle_t(op_ws_->Ctx()->cudnn_handle()),
        cudnn::dataType<float>::one, filter_desc_, weight->data(), bottom_desc_,
        bottom->data(), conv_desc_, bwd_data_algo_, workspace_ptr,
        workspace_bwd_size_, cudnn::dataType<float>::zero, top_desc_,
        top->mutable_data()));
    if (bias_term_) {
      CUDNN_CHECK(cudnnAddTensor(
          cudnnHandle_t(op_ws_->Ctx()->cudnn_handle()),
          cudnn::dataType<float>::one, bias_desc_, bottoms<float>(2)->data(),
          cudnn::dataType<float>::one, top_desc_, top->mutable_data()));
    }
    if (activate_type_ == 1) {
      Vision::Activate(top->mutable_data(), top->count(), activate_type_);
    }
    return;
  }
#endif

  int temp_count = kernel_dim_ * group_ * conv_out_spatial_dim_;
  if (bias_term_) {
    temp_count += out_spatial_dim_;
  }
  op_ws_->GrowTempBuffer(temp_count, sizeof(float));
  col_image_ = op_ws_->CreateTempBlob<float>(
      {kernel_dim_ * group_, conv_out_spatial_dim_}, op_name_ + "_col_image");
  if (bias_term_) {
    biases_multiplier_ = op_ws_->CreateTempBlob<float>(
        {out_spatial_dim_}, op_name_ + "_biases_multiplier");
    Blas::Set(out_spatial_dim_, 1, biases_multiplier_->mutable_data(), 0);
  }
  int top_num = top->num(), bottom_num = bottom->num();
  for (int b = 0; b < batch; ++b) {
    for (int g = 0; g < group_; ++g) {
      Blas::BlasSgemm(
          1, 0, kernel_dim_, conv_out_spatial_dim_, conv_out_c / group_, 1,
          weight->data(), weight_offset_ * g, bottom->data(),
          b * bottom_num + output_offset_ * g, 0, col_image_->mutable_data(),
          col_offset_ * g, op_ws_->Ctx()->blas_handle());
    }
    Vision::Col2Im(col_image_->data(), top->shape(), b * top_num, kernel_size_,
                   stride_, pad_, dilation_, bottom->shape(),
                   top->mutable_data());
    if (bias_term_) {
      Blas::BlasSgemm(0, 0, num_output_, out_spatial_dim_, 1, 1,
                      bottoms<float>(2)->data(), 0, biases_multiplier_->data(),
                      0, 1, top->mutable_data(), b * top_num,
                      op_ws_->Ctx()->blas_handle());
    }
  }
  if (activate_type_ == 1) {
    Vision::Activate(top->mutable_data(), top->count(), activate_type_);
  }
}

REGISTER_OPERATOR(Deconv, DeconvOp);

namespace Vision {

#if !defined(USE_CUDA)
// check for 0 <= a < b
inline bool check_border(int a, int b) {
  return static_cast<unsigned>(a) < static_cast<unsigned>(b);
}

template <typename T>
void Col2Im(const T *col_data, const VecInt &in_shape, int offset,
            int kernel_size, int stride, int pad, int dilation,
            const VecInt &out_shape, T *in_data) {
  in_data += offset;
  int in_c = in_shape[1], in_h = in_shape[2], in_w = in_shape[3];
  int out_h = out_shape[2], out_w = out_shape[3];
  int spatial_dim = in_h * in_w;
  for (int k_c = 0; k_c < in_c; ++k_c, in_data += spatial_dim) {
    for (int k_s = 0; k_s < kernel_size * kernel_size; ++k_s) {
      int k_h = k_s / kernel_size;
      int k_w = k_s % kernel_size;
      int im_row = -pad + k_h * dilation;
      for (int h = 0; h < out_h; ++h, im_row += stride) {
        if (check_border(im_row, in_h)) {
          int im_col = -pad + k_w * dilation;
          for (int w = 0; w < out_w; ++w, ++col_data, im_col += stride) {
            if (check_border(im_col, in_w)) {
              in_data[im_row * in_w + im_col] += *(col_data);
            }
          }
        } else {
          col_data += out_w;
        }
      }
    }
  }
}

template void Col2Im(const float *col_data, const VecInt &in_shape, int offset,
                     int kernel_size, int stride, int pad, int dilation,
                     const VecInt &out_shape, float *in_data);
#endif

}  // namespace Vision

}  // namespace Shadow

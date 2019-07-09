/*******************************************************************************
* Copyright 2019 Intel Corporation
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#include "common/c_types_map.hpp"
#include "common/math_utils.hpp"
#include "common/mkldnn_thread.hpp"
#include "common/mkldnn_traits.hpp"
#include "common/type_helpers.hpp"

#include "ocl/ref_inner_product.hpp"

namespace mkldnn {
namespace impl {
namespace ocl {

template <data_type_t src_type, data_type_t wei_type, data_type_t dst_type,
        data_type_t acc_type>
status_t ref_inner_product_fwd_t<src_type, wei_type, dst_type,
        acc_type>::execute_forward(const exec_ctx_t &ctx) const {

    compute::compute_stream_t *compute_stream
            = utils::downcast<compute::compute_stream_t *>(ctx.stream());

    auto &src = CTX_IN_STORAGE(MKLDNN_ARG_SRC);
    auto &weights = CTX_IN_STORAGE(MKLDNN_ARG_WEIGHTS);
    auto &bias = CTX_IN_STORAGE(MKLDNN_ARG_BIAS);
    auto &dst = CTX_OUT_STORAGE(MKLDNN_ARG_DST);

    const auto &jip = ker_->jip;

    auto eltwise_alpha = pd()->eltwise_alpha();
    auto eltwise_beta = pd()->eltwise_beta();
    auto sum_scale = pd()->sum_scale();
    const float *output_scales = pd()->attr()->output_scales_.scales_;

    compute::kernel_arg_list_t arg_list;
    arg_list.set(0, src);
    arg_list.set(1, weights);
    arg_list.set(2, bias);
    arg_list.set(3, dst);
    arg_list.set(4, eltwise_alpha);
    arg_list.set(5, eltwise_beta);
    arg_list.set(6, sum_scale);
    arg_list.set(7, output_scales[0]);

    auto nd_range = compute::nd_range_t({ jip.mb * jip.oc });
    status_t status = compute_stream->parallel_for(nd_range, kernel_, arg_list);

    return status;
}

template <data_type_t diff_src_type, data_type_t wei_type,
        data_type_t diff_dst_type, data_type_t acc_type>
status_t ref_inner_product_bwd_data_t<diff_src_type, wei_type, diff_dst_type,
        acc_type>::execute_backward_data(const exec_ctx_t &ctx) const {

    compute::compute_stream_t *compute_stream
            = utils::downcast<compute::compute_stream_t *>(ctx.stream());

    auto &diff_dst = CTX_IN_STORAGE(MKLDNN_ARG_DIFF_DST);
    auto &weights = CTX_IN_STORAGE(MKLDNN_ARG_WEIGHTS);
    auto &diff_src = CTX_OUT_STORAGE(MKLDNN_ARG_DIFF_SRC);

    const auto &jip = ker_->jip;

    compute::kernel_arg_list_t arg_list;
    arg_list.set(0, diff_src);
    arg_list.set(1, weights);
    arg_list.set(2, diff_dst);

    auto nd_range = compute::nd_range_t(
            { jip.mb * jip.ic * jip.id * jip.ih * jip.iw });
    status_t status = compute_stream->parallel_for(nd_range, kernel_, arg_list);

    return status;
}

template <impl::data_type_t data_type>
status_t ref_inner_product_bwd_weights_t<data_type>::execute_backward_weights(
        const exec_ctx_t &ctx) const {

    compute::compute_stream_t *compute_stream
            = utils::downcast<compute::compute_stream_t *>(ctx.stream());

    auto &src = CTX_IN_STORAGE(MKLDNN_ARG_SRC);
    auto &diff_dst = CTX_IN_STORAGE(MKLDNN_ARG_DIFF_DST);
    auto &diff_weights = CTX_OUT_STORAGE(MKLDNN_ARG_DIFF_WEIGHTS);
    auto &diff_bias = CTX_OUT_STORAGE(MKLDNN_ARG_DIFF_BIAS);

    const auto &jip = ker_->jip;

    compute::kernel_arg_list_t arg_list;
    arg_list.set(0, src);
    arg_list.set(1, diff_weights);
    arg_list.set(2, diff_bias);
    arg_list.set(3, diff_dst);

    auto nd_range = compute::nd_range_t(
            { jip.oc * jip.ic * jip.ih * jip.iw * jip.id });
    status_t status = compute_stream->parallel_for(nd_range, kernel_, arg_list);

    return status;
}

using namespace data_type;

template struct ref_inner_product_fwd_t<s8, s8, s8, s32>;
template struct ref_inner_product_fwd_t<s8, s8, u8, s32>;
template struct ref_inner_product_fwd_t<s8, s8, s32, s32>;
template struct ref_inner_product_fwd_t<u8, s8, s8, s32>;
template struct ref_inner_product_fwd_t<u8, s8, u8, s32>;
template struct ref_inner_product_fwd_t<u8, s8, s32, s32>;

template struct ref_inner_product_fwd_t<bf16, bf16, bf16, f32>;
template struct ref_inner_product_bwd_data_t<bf16, bf16, bf16, f32>;
template struct ref_inner_product_bwd_weights_t<bf16>;

template struct ref_inner_product_fwd_t<f32>;
template struct ref_inner_product_fwd_t<f16>;
template struct ref_inner_product_bwd_data_t<f32, f32, f32, f32>;
template struct ref_inner_product_bwd_weights_t<f32>;

} // namespace ocl
} // namespace impl
} // namespace mkldnn

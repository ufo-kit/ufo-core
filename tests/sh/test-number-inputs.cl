kernel void test_inputs64(global float *a0, global float *a1, global float *a2, global float *a3, global float *a4,
                          global float *a5, global float *a6, global float *a7, global float *a8, global float *a9,
                          global float *a10, global float *a11, global float *a12, global float *a13,
                          global float *a14, global float *a15, global float *a16, global float *a17,
                          global float *a18, global float *a19, global float *a20, global float *a21,
                          global float *a22, global float *a23, global float *a24, global float *a25,
                          global float *a26, global float *a27, global float *a28, global float *a29,
                          global float *a30, global float *a31, global float *a32, global float *a33,
                          global float *a34, global float *a35, global float *a36, global float *a37,
                          global float *a38, global float *a39, global float *a40, global float *a41,
                          global float *a42, global float *a43, global float *a44, global float *a45,
                          global float *a46, global float *a47, global float *a48, global float *a49,
                          global float *a50, global float *a51, global float *a52, global float *a53,
                          global float *a54, global float *a55, global float *a56, global float *a57,
                          global float *a58, global float *a59, global float *a60, global float *a61,
                          global float *a62, global float *a63, global float *result) {
    size_t idx = get_global_id(1) * get_global_size(0) + get_global_id(0);
    result[idx] = a0[idx] + a1[idx] + a2[idx] + a3[idx] + a4[idx] + a5[idx] + a6[idx] + a7[idx] + a8[idx] + a9[idx] +
                  a10[idx] + a11[idx] + a12[idx] + a13[idx] + a14[idx] + a15[idx] + a16[idx] + a17[idx] + a18[idx] +
                  a19[idx] + a20[idx] + a21[idx] + a22[idx] + a23[idx] + a24[idx] + a25[idx] + a26[idx] + a27[idx] +
                  a28[idx] + a29[idx] + a30[idx] + a31[idx] + a32[idx] + a33[idx] + a34[idx] + a35[idx] + a36[idx] +
                  a37[idx] + a38[idx] + a39[idx] + a40[idx] + a41[idx] + a42[idx] + a43[idx] + a44[idx] + a45[idx] +
                  a46[idx] + a47[idx] + a48[idx] + a49[idx] + a50[idx] + a51[idx] + a52[idx] + a53[idx] + a54[idx] +
                  a55[idx] + a56[idx] + a57[idx] + a58[idx] + a59[idx] + a60[idx] + a61[idx] + a62[idx] + a63[idx];
}

kernel void test_inputs32(global float *a0, global float *a1, global float *a2, global float *a3, global float *a4,
                          global float *a5, global float *a6, global float *a7, global float *a8, global float *a9,
                          global float *a10, global float *a11, global float *a12, global float *a13,
                          global float *a14, global float *a15, global float *a16, global float *a17,
                          global float *a18, global float *a19, global float *a20, global float *a21,
                          global float *a22, global float *a23, global float *a24, global float *a25,
                          global float *a26, global float *a27, global float *a28, global float *a29,
                          global float *a30, global float *a31, global float *result) {
    size_t idx = get_global_id(1) * get_global_size(0) + get_global_id(0);
    result[idx] = a0[idx] + a1[idx] + a2[idx] + a3[idx] + a4[idx] + a5[idx] + a6[idx] + a7[idx] + a8[idx] + a9[idx] +
                  a10[idx] + a11[idx] + a12[idx] + a13[idx] + a14[idx] + a15[idx] + a16[idx] + a17[idx] + a18[idx] +
                  a19[idx] + a20[idx] + a21[idx] + a22[idx] + a23[idx] + a24[idx] + a25[idx] + a26[idx] + a27[idx] +
                  a28[idx] + a29[idx] + a30[idx] + a31[idx];
}
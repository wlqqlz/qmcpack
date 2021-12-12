//////////////////////////////////////////////////////////////////////////////////////
// This file is distributed under the University of Illinois/NCSA Open Source License.
// See LICENSE file in top directory for details.
//
// Copyright (c) 2021 QMCPACK developers.
//
// File developed by: Peter Doak, doakpw@ornl.gov, Oak Ridge National Laboratory
//
// File created by: Peter Doak, doakpw@ornl.gov, Oak Ridge National Laboratory
//////////////////////////////////////////////////////////////////////////////////////

#include "catch.hpp"

#include <memory>
#include <iostream>
#include <vector>
#include "CUDA/CUDAruntime.hpp"
#include "CUDA/cuBLAS.hpp"
#include "CUDA/CUDAfill.hpp"
#include "CUDA/CUDAallocator.hpp"
#include "Utilities/for_testing/MatrixAccessor.hpp"
#include "Utilities/for_testing/checkMatrix.hpp"
#include "detail/CUDA/cuBLAS_LU.hpp"

/** \file
 *
 *  These are unit tests for the low level LU factorization used by the full inversion and
 *  calculation of log determinant for dirac determinants. Fundamental testing of these kernels
 *  requires full knowledge of the memory layout and data movement, As such OhmmsMatrices and
 *  custom allocators are not used.  They have their own unit tests (Hopefully!) This is also documentation
 *  of how these calls expect the memory handed to them to look.  Please leave this intact.
 *  Someday those container abstractions will change, if inversion breaks and this stil works you
 *  will have a fighting chance to know how to change these routines or fix the bug you introduced in the
 *  higher level abstractions.
 *
 *  Reference data generated by qmcpack/tests/scripts/inversion_ref.py
 */
namespace qmcplusplus
{
namespace testing
{
/** Doesn't depend on the resource management scheme thats out of scope for unit tests */
struct CUDAHandles
{
  // CUDA specific variables
  cudaStream_t hstream;
  cublasHandle_t h_cublas;

  CUDAHandles()
  {
    cudaErrorCheck(cudaStreamCreate(&hstream), "cudaStreamCreate failed!");
    cublasErrorCheck(cublasCreate(&h_cublas), "cublasCreate failed!");
    cublasErrorCheck(cublasSetStream(h_cublas, hstream), "cublasSetStream failed!");
  }

  CUDAHandles(const CUDAHandles&) : CUDAHandles() {}

  ~CUDAHandles()
  {
    cublasErrorCheck(cublasDestroy(h_cublas), "cublasDestroy failed!");
    cudaErrorCheck(cudaStreamDestroy(hstream), "cudaStreamDestroy failed!");
  }
};
} // namespace testing

/** Single double computeLogDet */
TEST_CASE("cuBLAS_LU::computeLogDet", "[wavefunction][CUDA]")
{
  auto cuda_handles = std::make_unique<testing::CUDAHandles>();
  int n             = 4;
  int lda           = 4;
  int batch_size    = 1;
  auto& hstream     = cuda_handles->hstream;

  // clang-format off
  std::vector<double, CUDAHostAllocator<double>> lu = {7.,      0.28571429,  0.71428571, 0.71428571,
                                                       5.,      3.57142857,  0.12,       -0.44,
                                                       6.,      6.28571429,  -1.04,      -0.46153846,
                                                       6.,      5.28571429,  3.08,       7.46153846};
  // clang-format on
  std::vector<double, CUDAAllocator<double>> dev_lu(16);

  std::vector<double*, CUDAHostAllocator<double*>> lus(1, nullptr);
  lus[0] = dev_lu.data();
  std::vector<double*, CUDAHostAllocator<double*>> dev_lus(1);

  using StdComp = std::complex<double>;
  std::vector<StdComp, CUDAHostAllocator<StdComp>> log_values(batch_size, 0.0);
  std::vector<StdComp, CUDAAllocator<StdComp>> dev_log_values(batch_size, 0.0);

  std::vector<int, CUDAHostAllocator<int>> pivots = {3, 3, 4, 4};
  std::vector<int, CUDAAllocator<int>> dev_pivots(4);

  // Transfers and launch kernel.
  cudaCheck(cudaMemcpyAsync(dev_lu.data(), lu.data(), sizeof(decltype(lu)::value_type) * 16, cudaMemcpyHostToDevice,
                            hstream));
  cudaCheck(cudaMemcpyAsync(dev_lus.data(), lus.data(), sizeof(double**), cudaMemcpyHostToDevice, hstream));
  cudaCheck(cudaMemcpyAsync(dev_pivots.data(), pivots.data(), sizeof(int) * 4, cudaMemcpyHostToDevice, hstream));

  // The types of the pointers passed here matter
  // Pass the C++ types
  cuBLAS_LU::computeLogDet_batched(cuda_handles->hstream, n, lda, dev_lus.data(), dev_pivots.data(),
                                   dev_log_values.data(), batch_size);

  // Copy back to the log_values
  cudaCheck(cudaMemcpyAsync(log_values.data(), dev_log_values.data(), sizeof(std::complex<double>) * 1,
                            cudaMemcpyDeviceToHost, hstream));
  cudaCheck(cudaStreamSynchronize(hstream));
  CHECK(log_values[0] == ComplexApprox(std::complex<double>{5.267858159063328, 6.283185307179586}));
}

TEST_CASE("cuBLAS_LU::computeLogDet_complex", "[wavefunction][CUDA]")
{
  auto cuda_handles = std::make_unique<testing::CUDAHandles>();
  int n             = 4;
  int lda           = 4;
  int batch_size    = 1;
  auto& hstream     = cuda_handles->hstream;

  using StdComp = std::complex<double>;
  // clang-format off
  std::vector<StdComp,
              CUDAHostAllocator<StdComp>> lu = {{8.0,                   0.5},
                                                {0.8793774319066148,    0.07003891050583658},
                                                {0.24980544747081712,   -0.0031128404669260694},
                                                {0.6233463035019455,    -0.026459143968871595},
                                                {2.0,                   0.1},
                                                {6.248249027237354,     0.2719844357976654},
                                                {0.7194170575332381,    -0.01831314754114669},
                                                {0.1212375092639108,    0.02522449751055713},
                                                {6.0,                   -0.2},
                                                {0.7097276264591441,    -0.4443579766536965},
                                                {4.999337315778741,     0.6013141870887196},
                                                {0.26158183940834034,   0.23245112532996867},
                                                {4.0,                   -0.6},
                                                {4.440466926070039,     -1.7525291828793774},
                                                {0.840192589866152,     1.5044529443071093},
                                                {1.0698651110730424,    -0.10853319738453365}};
  // clang-format on
  std::vector<StdComp, CUDAAllocator<StdComp>> dev_lu(lu.size());
  std::vector<StdComp*, CUDAHostAllocator<StdComp*>> lus(batch_size);
  lus[0] = dev_lu.data();
  std::vector<StdComp*, CUDAAllocator<StdComp*>> dev_lus(batch_size);

  std::vector<StdComp, CUDAHostAllocator<StdComp>> log_values(batch_size);
  std::vector<StdComp, CUDAAllocator<StdComp>> dev_log_values(batch_size);

  std::vector<int, CUDAHostAllocator<int>> pivots = {3, 4, 3, 4};
  std::vector<int, CUDAAllocator<int>> dev_pivots(4);

  cudaErrorCheck(cudaMemcpyAsync(dev_lu.data(), lu.data(), sizeof(double) * 32, cudaMemcpyHostToDevice, hstream),
                 "cudaMemcpyAsync failed copying log_values to device");
  cudaErrorCheck(cudaMemcpyAsync(dev_lus.data(), lus.data(), sizeof(double*), cudaMemcpyHostToDevice, hstream),
                 "cudaMemcpyAsync failed copying lus to device");
  cudaErrorCheck(cudaMemcpyAsync(dev_pivots.data(), pivots.data(), sizeof(int) * 4, cudaMemcpyHostToDevice, hstream),
                 "cudaMemcpyAsync failed copying log_values to device");

  cuBLAS_LU::computeLogDet_batched(cuda_handles->hstream, n, lda, dev_lus.data(), dev_pivots.data(),
                                   dev_log_values.data(), batch_size);

  cudaErrorCheck(cudaMemcpyAsync(log_values.data(), dev_log_values.data(), sizeof(StdComp) * batch_size,
                                 cudaMemcpyDeviceToHost, hstream),
                 "cudaMemcpyAsync failed copying log_values from device");
  cudaErrorCheck(cudaStreamSynchronize(hstream), "cudaStreamSynchronize failed!");
  CHECK(log_values[0] == ComplexApprox(StdComp{5.603777579195571, -6.1586603331188225}));
}

/** while this working is a good test, in production code its likely we want to
 *  widen the matrix M to double and thereby the LU matrix as well.
 */
TEST_CASE("cuBLAS_LU::computeLogDet_float", "[wavefunction][CUDA]")
{
  auto cuda_handles = std::make_unique<testing::CUDAHandles>();
  int n             = 4;
  int lda           = 4;
  int batch_size    = 1;
  auto& hstream     = cuda_handles->hstream;

  // clang-format off
  std::vector<float, CUDAHostAllocator<float>> lu = {7.,      0.28571429,  0.71428571, 0.71428571,
                                                       5.,      3.57142857,  0.12,       -0.44,
                                                       6.,      6.28571429,  -1.04,      -0.46153846,
                                                       6.,      5.28571429,  3.08,       7.46153846};
  // clang-format on
  std::vector<float, CUDAAllocator<float>> dev_lu(lu.size());

  std::vector<float*, CUDAHostAllocator<float*>> lus(batch_size, nullptr);
  lus[0] = dev_lu.data();
  std::vector<float*, CUDAAllocator<float*>> dev_lus(batch_size);

  using StdComp = std::complex<double>;
  std::vector<StdComp, CUDAHostAllocator<StdComp>> log_values(batch_size, 0.0);
  std::vector<StdComp, CUDAAllocator<StdComp>> dev_log_values(batch_size, 0.0);

  std::vector<int, CUDAHostAllocator<int>> pivots = {3, 3, 4, 4};
  std::vector<int, CUDAAllocator<int>> dev_pivots(4);

  // Transfer and run kernel.
  cudaCheck(cudaMemcpyAsync(dev_lu.data(), lu.data(), sizeof(float) * 16, cudaMemcpyHostToDevice, hstream));
  cudaCheck(cudaMemcpyAsync(dev_lus.data(), lus.data(), sizeof(float**), cudaMemcpyHostToDevice, hstream));
  cudaCheck(cudaMemcpyAsync(dev_pivots.data(), pivots.data(), sizeof(int) * 4, cudaMemcpyHostToDevice, hstream));

  // The types of the pointers passed here matter
  cuBLAS_LU::computeLogDet_batched(hstream, n, lda, dev_lus.data(), dev_pivots.data(), dev_log_values.data(),
                                   batch_size);

  cudaCheck(cudaMemcpyAsync(log_values.data(), dev_log_values.data(), sizeof(std::complex<double>) * batch_size,
                            cudaMemcpyDeviceToHost, hstream));
  cudaCheck(cudaStreamSynchronize(hstream));
  CHECK(log_values[0] == ComplexApprox(std::complex<double>{5.267858159063328, 6.283185307179586}));
}

TEST_CASE("cuBLAS_LU::computeLogDet(batch=2)", "[wavefunction][CUDA]")
{
  auto cuda_handles = std::make_unique<testing::CUDAHandles>();
  int n             = 4;
  int lda           = 4;
  auto& hstream     = cuda_handles->hstream;
  int batch_size    = 2;

  using StdComp = std::complex<double>;
  // clang-format off
  std::vector<StdComp,
              CUDAHostAllocator<StdComp>> lu = {{8.0,                   0.5},
                                             {0.8793774319066148,    0.07003891050583658},
                                             {0.24980544747081712,   -0.0031128404669260694},
                                             {0.6233463035019455,    -0.026459143968871595},
                                             {2.0,                   0.1},
                                             {6.248249027237354,     0.2719844357976654},
                                             {0.7194170575332381,    -0.01831314754114669},
                                             {0.1212375092639108,    0.02522449751055713},
                                             {6.0,                   -0.2},
                                             {0.7097276264591441,    -0.4443579766536965},
                                             {4.999337315778741,     0.6013141870887196},
                                             {0.26158183940834034,   0.23245112532996867},
                                             {4.0,                   -0.6},
                                             {4.440466926070039,     -1.7525291828793774},
                                             {0.840192589866152,     1.5044529443071093},
                                             {1.0698651110730424,    -0.10853319738453365}};
  std::vector<StdComp,
              CUDAHostAllocator<StdComp>> lu2 = {{8.0, 0.5},
                                                 {0.8793774319066148, 0.07003891050583658},
                                                 {0.49883268482490273, -0.01867704280155642},
                                                 {0.24980544747081712, -0.0031128404669260694},
                                                 {2.0, 0.1},
                                                 {6.248249027237354, 0.2719844357976654},
                                                 {0.800088933543564, -0.004823898651572499},
                                                 {0.2401906003014191, 0.0025474386841018853},
                                                 {3.0, -0.2},
                                                 {3.3478599221789884, -0.23424124513618677},
                                                 {0.8297816353227319, 1.3593612303468308},
                                                 {0.6377685195602139, -0.6747848919351336},
                                                 {4.0, -0.6},
                                                 {4.440466926070039, -1.7525291828793774},
                                                 {-1.5284389377713894, 1.6976073494521235},
                                                 {2.7608934839023482, -1.542084179899335}};
  // clang-format off
  std::vector<StdComp,
              CUDAHostAllocator<StdComp>> dev_lu(lu.size());
  std::vector<StdComp,
              CUDAHostAllocator<StdComp>> dev_lu2(lu2.size());
  
  std::vector<StdComp*, CUDAHostAllocator<StdComp*>> lus(batch_size);
  lus[0]                     = dev_lu.data();
  lus[1]                     = dev_lu2.data();
  std::vector<StdComp*, CUDAAllocator<StdComp*>> dev_lus(batch_size);

  std::vector<StdComp, CUDAHostAllocator<StdComp>> log_values(batch_size);
  std::vector<StdComp, CUDAAllocator<StdComp>> dev_log_values(batch_size);

  std::vector<int, CUDAHostAllocator<int>> pivots = {3, 4, 3, 4, 3, 4, 4, 4};
  std::vector<int, CUDAAllocator<int>> dev_pivots(pivots.size());

  cudaErrorCheck(cudaMemcpyAsync(dev_lu.data(), lu.data(), sizeof(decltype(lu)::value_type) * lu.size(), cudaMemcpyHostToDevice, hstream),
                 "cudaMemcpyAsync failed copying log_values to device");
  cudaErrorCheck(cudaMemcpyAsync(dev_lu2.data(), lu2.data(), sizeof(decltype(lu2)::value_type) * lu2.size(), cudaMemcpyHostToDevice, hstream),
                 "cudaMemcpyAsync failed copying log_values to device");
  cudaErrorCheck(cudaMemcpyAsync(dev_lus.data(), lus.data(), sizeof(decltype(lus)::value_type) * lus.size(), cudaMemcpyHostToDevice, hstream),
                 "cudaMemcpyAsync failed copying log_values to device");

  cudaErrorCheck(cudaMemcpyAsync(dev_pivots.data(), pivots.data(), sizeof(int) * pivots.size(), cudaMemcpyHostToDevice, hstream),
                 "cudaMemcpyAsync failed copying log_values to device");

  cuBLAS_LU::computeLogDet_batched(cuda_handles->hstream, n, lda, dev_lus.data(), dev_pivots.data(), dev_log_values.data(), batch_size);
  cudaErrorCheck(cudaMemcpyAsync(log_values.data(), dev_log_values.data(), sizeof(std::complex<double>) * 2, cudaMemcpyDeviceToHost,
                                 hstream),
                 "cudaMemcpyAsync failed copying log_values from device");
  cudaErrorCheck(cudaStreamSynchronize(hstream), "cudaStreamSynchronize failed!");

  CHECK(log_values[0] == ComplexApprox(std::complex<double>{ 5.603777579195571, -6.1586603331188225 }));
  CHECK(log_values[1] == ComplexApprox(std::complex<double>{ 5.531331998282581, -8.805487075984523  }));
}


TEST_CASE("cuBLAS_LU::getrf_batched_complex", "[wavefunction][CUDA]")
{
  auto cuda_handles = std::make_unique<testing::CUDAHandles>();
  int n             = 4;
  int lda           = 4;
  int batch_size = 1;
  auto& hstream     = cuda_handles->hstream;

  using StdComp = std::complex<double>;
  // clang-format off
  std::vector<StdComp, CUDAHostAllocator<StdComp>> M = {{2.0, 0.1}, {5.0, 0.1},  {8.0, 0.5},  {7.0, 1.0},
                                                        {5.0, 0.1}, {2.0, 0.2},  {2.0, 0.1},  {8.0, 0.5},
                                                        {7.0, 0.2}, {5.0, 1.0},  {6.0, -0.2}, {6.0, -0.2},
                                                        {5.0, 0.0}, {4.0, -0.1}, {4.0, -0.6}, {8.0, -2.0}};
  // clang-format on    
  std::vector<StdComp, CUDAAllocator<StdComp>> devM(M.size());
  std::vector<StdComp*, CUDAHostAllocator<StdComp*>> Ms(batch_size);
  std::vector<StdComp*, CUDAAllocator<StdComp*>> devMs(batch_size);
  Ms[0] = devM.data();

  std::vector<int, CUDAHostAllocator<int>> pivots = {1, 1, 1, 1};
  std::vector<int, CUDAAllocator<int>> dev_pivots(pivots.size());
  
  std::vector<int, CUDAHostAllocator<int>> infos = {1, 1, 1, 1};
  std::vector<int, CUDAAllocator<int>> dev_infos(infos.size());
  
  cudaErrorCheck(cudaMemcpyAsync(devM.data(), M.data(), sizeof(decltype(devM)::value_type) * devM.size(), cudaMemcpyHostToDevice, hstream),
                 "cudaMemcpyAsync failed copying M to device");
  cudaErrorCheck(cudaMemcpyAsync(devMs.data(), Ms.data(), sizeof(decltype(devMs)::value_type) * devMs.size(), cudaMemcpyHostToDevice, hstream),
                 "cudaMemcpyAsync failed copying Ms to device");

  cuBLAS_LU::computeGetrf_batched(cuda_handles->h_cublas, cuda_handles->hstream,  n, lda, devMs.data(), dev_pivots.data(), infos.data(), dev_infos.data(), batch_size);

  cudaErrorCheck(cudaMemcpyAsync(M.data(), devM.data(), sizeof(decltype(devM)::value_type) * devM.size(), cudaMemcpyDeviceToHost, hstream),
                 "cudaMemcpyAsync failed copying invM from device");
  cudaErrorCheck(cudaMemcpyAsync(pivots.data(), dev_pivots.data(), sizeof(int) * pivots.size(), cudaMemcpyDeviceToHost, hstream),
                 "cudaMemcpyAsync failed copying pivots from device");

  cudaErrorCheck(cudaStreamSynchronize(hstream), "cudaStreamSynchronize failed!");

  std::vector<int> real_pivot = {3, 4, 3, 4};

  auto checkArray = [](auto A, auto B, int n) {
    for (int i = 0; i < n; ++i)
    {
      CHECK(A[i] == B[i]);
    }
  };
  checkArray(real_pivot.begin(), pivots.begin(), 4);
  // clang-format off
  std::vector<StdComp> lu{{8.0,                      0.5},
                          {0.8793774319066148,       0.07003891050583658},
                          {0.24980544747081712,      -0.0031128404669260694},
                          {0.6233463035019455,       -0.026459143968871595},
                          {2.0,                      0.1},
                          {6.248249027237354,        0.2719844357976654},
                          {0.7194170575332381,       -0.01831314754114669},
                          {0.1212375092639108,       0.02522449751055713},
                          {6.0,                      -0.2},
                          {0.7097276264591441,       -0.4443579766536965},
                          {4.999337315778741,        0.6013141870887196},
                          {0.26158183940834034,      0.23245112532996867},
                          {4.0,                      -0.6},
                          {4.440466926070039,        -1.7525291828793774},
                          {0.840192589866152,        1.5044529443071093},
                          {1.0698651110730424,       -0.10853319738453365}};
  // clang-format on
  // This could actually be any container that supported the concept of
  // access via operator()(i, j) and had <T, ALLOCT> template signature
  testing::MatrixAccessor<std::complex<double>> lu_mat(lu.data(), 4, 4);
  testing::MatrixAccessor<std::complex<double>> M_mat(M.data(), 4, 4);
  auto check_matrix_result = checkMatrix(lu_mat, M_mat);
  CHECKED_ELSE(check_matrix_result.result) { FAIL(check_matrix_result.result_message); }
}

TEST_CASE("cuBLAS_LU::getrf_batched(batch=2)", "[wavefunction][CUDA]")
{
  auto cuda_handles = std::make_unique<testing::CUDAHandles>();
  int n             = 4;
  int lda           = 4;
  auto& hstream     = cuda_handles->hstream;

  int batch_size = 2;

  std::vector<double, CUDAHostAllocator<double>> M_vec{2, 5, 7, 5, 5, 2, 5, 4, 8, 2, 6, 4, 7, 8, 6, 8};
  std::vector<double, CUDAHostAllocator<double>> M2_vec{6, 5, 7, 5, 2, 2, 5, 4, 8, 2, 6, 4, 3, 8, 6, 8};
  std::vector<double, CUDAAllocator<double>> devM_vec(M_vec.size());
  std::vector<double, CUDAAllocator<double>> devM2_vec(M2_vec.size());
  std::vector<double*, CUDAHostAllocator<double*>> Ms{devM_vec.data(), devM2_vec.data()};
  std::vector<double*, CUDAAllocator<double*>> devMs(Ms.size());

  std::vector<int, CUDAHostAllocator<int>> pivots(8, -1.0);
  std::vector<int, CUDAAllocator<int>> dev_pivots(pivots.size());

  std::vector<int, CUDAHostAllocator<int>> infos(8, 1.0);
  std::vector<int, CUDAAllocator<int>> dev_infos(pivots.size());

  //Now copy the Ms
  cudaErrorCheck(cudaMemcpyAsync(devM_vec.data(), M_vec.data(), sizeof(decltype(M_vec)::value_type) * M_vec.size(),
                                 cudaMemcpyHostToDevice, hstream),
                 "cudaMemcpyAsync failed copying M to device");
  cudaErrorCheck(cudaMemcpyAsync(devM2_vec.data(), M2_vec.data(), sizeof(decltype(M2_vec)::value_type) * M2_vec.size(),
                                 cudaMemcpyHostToDevice, hstream),
                 "cudaMemcpyAsync failed copying M2 to device");
  // copy the pointer array
  cudaErrorCheck(cudaMemcpyAsync(devMs.data(), Ms.data(), sizeof(decltype(Ms)::value_type) * Ms.size(),
                                 cudaMemcpyHostToDevice, hstream),
                 "cudaMemcpyAsync failed copying Ms to device");

  cuBLAS_LU::computeGetrf_batched(cuda_handles->h_cublas, cuda_handles->hstream, n, lda, devMs.data(),
                                  dev_pivots.data(), infos.data(), dev_infos.data(), batch_size);

  // copy back the Ms, infos, pivots
  cudaErrorCheck(cudaMemcpyAsync(M_vec.data(), devM_vec.data(), sizeof(decltype(M_vec)::value_type) * M_vec.size(),
                                 cudaMemcpyDeviceToHost, hstream),
                 "cudaMemcpyAsync failed copying invM from device");
  cudaErrorCheck(cudaMemcpyAsync(M2_vec.data(), devM2_vec.data(), sizeof(decltype(M2_vec)::value_type) * M2_vec.size(),
                                 cudaMemcpyDeviceToHost, hstream),
                 "cudaMemcpyAsync failed copying invM from device");
  cudaErrorCheck(cudaMemcpyAsync(pivots.data(), dev_pivots.data(), sizeof(int) * pivots.size(), cudaMemcpyDeviceToHost,
                                 hstream),
                 "cudaMemcpyAsync failed copying pivots from device");

  cudaErrorCheck(cudaStreamSynchronize(hstream), "cudaStreamSynchronize failed!");

  // clang-format off
  std::vector<double> lu{7.,                    0.28571429,
                         0.71428571,            0.71428571,
                         5.,                    3.57142857,
                         0.12,                  -0.44,
                         6.,                    6.28571429,
                         -1.04,                 -0.46153846,
                         6.,                    5.28571429,
                         3.08,                  7.46153846};

  std::vector<double> lu2{7.0,                  0.8571428571428571,
                          0.7142857142857142,   0.7142857142857142,
                          5.0,                  -2.2857142857142856,
                          0.6874999999999998,   -0.18750000000000022,
                          6.0,                  2.8571428571428577,
                          -4.249999999999999,   -0.05882352941176502,
                          6.0,                  -2.1428571428571423,
                          5.1875,               3.617647058823531};
  // clang-format on
  std::vector<int> real_pivot{3, 3, 4, 4, 3, 3, 3, 4};

  auto checkArray = [](auto A, auto B, int n) {
    for (int i = 0; i < n; ++i)
    {
      CHECK(A[i] == Approx(B[i]));
    }
  };

  testing::MatrixAccessor<double> M_mat(M_vec.data(), 4, 4);
  testing::MatrixAccessor<double> lu_mat(lu.data(), 4, 4);
  testing::MatrixAccessor<double> M2_mat(M2_vec.data(), 4, 4);
  testing::MatrixAccessor<double> lu2_mat(lu2.data(), 4, 4);

  checkArray(real_pivot, pivots, 8);
  auto check_matrix_result = checkMatrix(lu_mat, M_mat);
  CHECKED_ELSE(check_matrix_result.result) { FAIL(check_matrix_result.result_message); }
  check_matrix_result = checkMatrix(lu2_mat, M2_mat);
  CHECKED_ELSE(check_matrix_result.result) { FAIL(check_matrix_result.result_message); }
}

TEST_CASE("cuBLAS_LU::getri_batched", "[wavefunction][CUDA]")
{
  auto cuda_handles = std::make_unique<testing::CUDAHandles>();
  int n             = 4;
  int lda           = 4;
  auto& hstream     = cuda_handles->hstream;
  int batch_size    = 1;

  // clang-format off
  std::vector<double, CUDAHostAllocator<double>> M_vec{7., 0.28571429, 0.71428571, 0.71428571,
                                                       5., 3.57142857, 0.12,       -0.44,
                                                       6., 6.28571429, -1.04,      -0.46153846,
                                                       6., 5.28571429, 3.08,       7.46153846};
  std::vector<double, CUDAAllocator<double>> devM_vec(M_vec.size());

  std::vector<double*, CUDAHostAllocator<double*>> Ms{devM_vec.data()};
  std::vector<double*, CUDAAllocator<double*>> devMs(Ms.size());

  std::vector<double, CUDAHostAllocator<double>> invM_vec{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
  std::vector<double, CUDAHostAllocator<double>> dev_invM_vec(invM_vec.size());

  std::vector<double*, CUDAHostAllocator<double*>> invMs{dev_invM_vec.data()};
  std::vector<double*, CUDAAllocator<double*>> dev_invMs(invMs.size());

  std::vector<int, CUDAHostAllocator<int>> pivots{3, 3, 4, 4};
  std::vector<int, CUDAAllocator<int>> dev_pivots(pivots.size());

  std::vector<int, CUDAHostAllocator<int>> infos(4, 1.0);
  std::vector<int, CUDAAllocator<int>> dev_infos(pivots.size());

  cudaErrorCheck(cudaMemcpyAsync(devM_vec.data(), M_vec.data(), sizeof(decltype(M_vec)::value_type) * M_vec.size(), cudaMemcpyHostToDevice, hstream),
                 "cudaMemcpyAsync failed copying M to device");
  cudaErrorCheck(cudaMemcpyAsync(devMs.data(), Ms.data(), sizeof(double*), cudaMemcpyHostToDevice, hstream),
                 "cudaMemcpyAsync failed copying Ms to device");
  cudaErrorCheck(cudaMemcpyAsync(dev_invMs.data(), invMs.data(), sizeof(double*), cudaMemcpyHostToDevice, hstream),
                 "cudaMemcpyAsync failed copying invMs to device");
  cudaErrorCheck(cudaMemcpyAsync(dev_pivots.data(), pivots.data(), sizeof(int) * 4, cudaMemcpyHostToDevice, hstream),
                 "cudaMemcpyAsync failed copying pivots to device");
  cuBLAS_LU::computeGetri_batched(cuda_handles->h_cublas, n, lda, devMs.data(), invMs.data(), dev_pivots.data(), dev_infos.data(), batch_size);

  cudaErrorCheck(cudaMemcpyAsync(invM_vec.data(), dev_invM_vec.data(), sizeof(double) * 16, cudaMemcpyDeviceToHost, hstream),
                 "cudaMemcpyAsync failed copying invM from device");
  cudaErrorCheck(cudaMemcpyAsync(infos.data(), dev_infos.data(), sizeof(int) * 4, cudaMemcpyDeviceToHost, hstream),
                 "cudaMemcpyAsync failed copying infos from device");
  cudaErrorCheck(cudaStreamSynchronize(hstream), "cudaStreamSynchronize failed!");

  // clang-format off
  std::vector<double> invA{-0.08247423, -0.26804124, 0.26804124,  0.05154639,
                           0.18556701,  -0.89690722, 0.39690722,  0.13402062,
                           0.24742268,  -0.19587629, 0.19587629,  -0.15463918,
                           -0.29896907, 1.27835052,  -0.77835052, 0.06185567};
  // clang-format on

  auto checkArray = [](auto A, auto B, int n) {
    for (int i = 0; i < n; ++i)
    {
      CHECK(A[i] == Approx(B[i]));
    }
  };

  testing::MatrixAccessor<double> invA_mat(invA.data(), 4, 4);
  testing::MatrixAccessor<double> invM_mat(invM_vec.data(), 4, 4);

  auto check_matrix_result = checkMatrix(invA_mat, invM_mat);
  CHECKED_ELSE(check_matrix_result.result) { FAIL(check_matrix_result.result_message); }
}

} // namespace qmcplusplus

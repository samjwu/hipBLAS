/* ************************************************************************
 * Copyright 2016-2020 Advanced Micro Devices, Inc.
 *
 * ************************************************************************ */

#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include "testing_common.hpp"

using namespace std;

/* ============================================================================================ */

template <typename T1, typename T2>
hipblasStatus_t testing_nrm2_batched(const Arguments& argus)
{
    bool FORTRAN = argus.fortran;
    auto hipblasNrm2BatchedFn
        = FORTRAN ? hipblasNrm2Batched<T1, T2, true> : hipblasNrm2Batched<T1, T2, false>;

    int N           = argus.N;
    int incx        = argus.incx;
    int batch_count = argus.batch_count;

    hipblasStatus_t status_1 = HIPBLAS_STATUS_SUCCESS;
    hipblasStatus_t status_2 = HIPBLAS_STATUS_SUCCESS;
    hipblasStatus_t status_3 = HIPBLAS_STATUS_SUCCESS;
    hipblasStatus_t status_4 = HIPBLAS_STATUS_SUCCESS;

    // check to prevent undefined memory allocation error
    if(N < 0 || incx < 0 || batch_count < 0)
    {
        return HIPBLAS_STATUS_INVALID_VALUE;
    }
    if(batch_count == 0)
    {
        return HIPBLAS_STATUS_SUCCESS;
    }

    int sizeX = N * incx;

    double gpu_time_used, cpu_time_used;
    double rocblas_error;

    hipblasHandle_t handle;
    hipblasCreate(&handle);

    // Naming: dX is in GPU (device) memory. hK is in CPU (host) memory, plz follow this practice
    host_vector<T1> hx_array[batch_count];
    host_vector<T2> h_cpu_result(batch_count);
    host_vector<T2> h_rocblas_result_1(batch_count);
    host_vector<T2> h_rocblas_result_2(batch_count);

    device_batch_vector<T1> bx_array(batch_count, sizeX);

    device_vector<T1*, 0, T1> dx_array(batch_count);
    device_vector<T2>         d_rocblas_result(batch_count);

    int last = batch_count - 1;
    if(!dx_array || !d_rocblas_result || (!bx_array[last] && sizeX))
    {
        hipblasDestroy(handle);
        return HIPBLAS_STATUS_ALLOC_FAILED;
    }

    // Initial Data on CPU
    srand(1);
    for(int b = 0; b < batch_count; b++)
    {
        hx_array[b] = host_vector<T1>(sizeX);

        srand(1);
        hipblas_init<T1>(hx_array[b], 1, N, incx);

        CHECK_HIP_ERROR(
            hipMemcpy(bx_array[b], hx_array[b], sizeof(T1) * sizeX, hipMemcpyHostToDevice));
    }
    CHECK_HIP_ERROR(
        hipMemcpy(dx_array, bx_array, sizeof(T1*) * batch_count, hipMemcpyHostToDevice));

    // hipblasNrm2 accept both dev/host pointer for the scalar
    status_1 = hipblasSetPointerMode(handle, HIPBLAS_POINTER_MODE_DEVICE);
    status_2 = hipblasNrm2BatchedFn(handle, N, dx_array, incx, batch_count, d_rocblas_result);

    status_3 = hipblasSetPointerMode(handle, HIPBLAS_POINTER_MODE_HOST);
    status_4 = hipblasNrm2BatchedFn(handle, N, dx_array, incx, batch_count, h_rocblas_result_1);

    if((status_1 != HIPBLAS_STATUS_SUCCESS) || (status_2 != HIPBLAS_STATUS_SUCCESS)
       || (status_3 != HIPBLAS_STATUS_SUCCESS) || (status_4 != HIPBLAS_STATUS_SUCCESS))
    {
        hipblasDestroy(handle);
        if(status_1 != HIPBLAS_STATUS_SUCCESS)
            return status_1;
        if(status_2 != HIPBLAS_STATUS_SUCCESS)
            return status_2;
        if(status_3 != HIPBLAS_STATUS_SUCCESS)
            return status_3;
        if(status_4 != HIPBLAS_STATUS_SUCCESS)
            return status_4;
    }

    CHECK_HIP_ERROR(hipMemcpy(
        h_rocblas_result_2, d_rocblas_result, sizeof(T2) * batch_count, hipMemcpyDeviceToHost));

    if(argus.unit_check || argus.norm_check)
    {
        /* =====================================================================
                    CPU BLAS
        =================================================================== */
        for(int b = 0; b < batch_count; b++)
        {
            cblas_nrm2<T1, T2>(N, hx_array[b], incx, &(h_cpu_result[b]));
        }

        if(argus.unit_check)
        {
            unit_check_nrm2<T2>(batch_count, h_cpu_result, h_rocblas_result_1, N);
            unit_check_nrm2<T2>(batch_count, h_cpu_result, h_rocblas_result_2, N);
        }

    } // end of if unit/norm check

    //  BLAS_1_RESULT_PRINT

    hipblasDestroy(handle);
    return HIPBLAS_STATUS_SUCCESS;
}

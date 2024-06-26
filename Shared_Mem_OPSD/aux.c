#include "aux.h"

void init_complex_matrix(MKL_Complex8 **matrix, size_t rows, size_t columns)
{
    (*matrix) = (MKL_Complex8 *)malloc(rows * columns * sizeof(MKL_Complex8));
    if ((*matrix) == NULL)
    {
        printf("Allocation error!\n");
        free(*matrix);
        return;
    }
}

void init_float_vector(float **v, size_t size)
{
    (*v) = (float *)malloc(size * sizeof(float));
    if ((*v) == NULL)
    {
        printf("Allocation error!\n");
        free(*v);
        return;
    }
}

void free_matrix(MKL_Complex8 *matrix)
{
    if (matrix == NULL)
    {
        printf("Matrix not found!\n");
        return;
    }

    free(matrix);
}

void show_matrix(MKL_Complex8 *matrix, size_t rows, size_t columns)
{
    if (matrix == NULL)
    {
        printf("Matrix not found!\n");
        return;
    }
    printf("\nMatriz:\n");
    for (int i = 0; i < rows; i++)
    {
        for (int j = 0; j < columns; j++)
        {
            printf("(%.2f, %.2f) ", matrix[i + j * rows].real, matrix[i + j * rows].imag);
        }
        printf("\n");
    }
    printf("\n");
}

void show_ram_allocation(size_t rows, size_t columns)
{
    printf("%.2f GB\n", rows * columns * sizeof(MKL_Complex8) / (1024.0f * 1024.0f * 1024.0f));
}

void fill(MKL_Complex8 *matrix, size_t rows, size_t columns, unsigned int seed)
{
    if (matrix == NULL)
    {
        printf("Matrix not found!\n");
        return;
    }
    
    srand(seed);

    // #pragma omp parallel for collapse(2)
    for (int i = 0; i < rows; i++)
    {
        for (int j = 0; j < columns; j++)
        {
            float random_real = (float)rand() / RAND_MAX_F;
            float scaled_real = random_real * 4 + 1;

            // float random_complex = (float)rand() / RAND_MAX_F;
            // float scaled_complex = random_real * 3 + 1;

            matrix[i + j * rows].real = scaled_real;
            // matrix[i + j * rows].real = 1.0f;
            matrix[i + j * rows].imag = 0.0f;
        }
    }
}

void compute_fft2D_column_row(MKL_Complex8 *I_t_I_w, size_t rows, size_t columns)
{

    if (I_t_I_w == NULL)
    {
        printf("Matrix not found!\n");
        return;
    }

    double start = omp_get_wtime();

    int max_threads = omp_get_max_threads();

    DFTI_DESCRIPTOR_HANDLE desc_handle_dim1 = NULL;
    DFTI_DESCRIPTOR_HANDLE desc_handle_dim2 = NULL;
    MKL_LONG status;

    status = DftiCreateDescriptor(&desc_handle_dim1, DFTI_SINGLE,
                                  DFTI_COMPLEX, 1, rows);
    status = DftiCreateDescriptor(&desc_handle_dim2, DFTI_SINGLE,
                                  DFTI_COMPLEX, 1, columns);

    MKL_LONG stride[2] = {0, rows};

    status = DftiSetValue(desc_handle_dim1, DFTI_NUMBER_OF_TRANSFORMS, columns / max_threads);
    status = DftiSetValue(desc_handle_dim1, DFTI_INPUT_DISTANCE, rows);
    status = DftiSetValue(desc_handle_dim1, DFTI_OUTPUT_DISTANCE, rows);
    status = DftiCommitDescriptor(desc_handle_dim1);

    status = DftiSetValue(desc_handle_dim2, DFTI_NUMBER_OF_TRANSFORMS, rows / max_threads);
    status = DftiSetValue(desc_handle_dim2, DFTI_INPUT_DISTANCE, 1);
    status = DftiSetValue(desc_handle_dim2, DFTI_OUTPUT_DISTANCE, 1);
    status = DftiSetValue(desc_handle_dim2, DFTI_INPUT_STRIDES, stride);
    status = DftiSetValue(desc_handle_dim2, DFTI_OUTPUT_STRIDES, stride);
    status = DftiCommitDescriptor(desc_handle_dim2);

    // Realizar as transformações FFT
    int th_c = 0;
    if (max_threads <= columns)
    {
#pragma omp parallel for
        for (th_c = 0; th_c < max_threads; th_c++)
        {
            status = DftiComputeForward(desc_handle_dim1, I_t_I_w + th_c * (columns / max_threads) * rows);
        }
        th_c = max_threads;
    }

    status = DftiFreeDescriptor(&desc_handle_dim1);

    if (columns % max_threads != 0)
    {
        DFTI_DESCRIPTOR_HANDLE desc_handle_dim_op_columns = NULL;
        status = DftiCreateDescriptor(&desc_handle_dim_op_columns, DFTI_SINGLE,
                                      DFTI_COMPLEX, 1, rows);
        status = DftiSetValue(desc_handle_dim_op_columns, DFTI_NUMBER_OF_TRANSFORMS, 1);
        status = DftiCommitDescriptor(desc_handle_dim_op_columns);

#pragma omp parallel for
        for (int th = 0; th < columns % max_threads; th++)
        {
            status = DftiComputeForward(desc_handle_dim_op_columns, I_t_I_w + th_c * (columns / max_threads) * rows + th * rows);
        }

        status = DftiFreeDescriptor(&desc_handle_dim_op_columns);
    }

    int th_r = 0;
    if (max_threads <= rows)
    {
#pragma omp parallel for
        for (th_r = 0; th_r < max_threads; th_r++)
        {
            status = DftiComputeForward(desc_handle_dim2, I_t_I_w + th_r * (rows / max_threads));
        }
        th_r = max_threads;
    }

    status = DftiFreeDescriptor(&desc_handle_dim2);

    if (rows % max_threads != 0)
    {
        DFTI_DESCRIPTOR_HANDLE desc_handle_dim_op_rows = NULL;
        status = DftiCreateDescriptor(&desc_handle_dim_op_rows, DFTI_SINGLE,
                                      DFTI_COMPLEX, 1, columns);
        status = DftiSetValue(desc_handle_dim_op_rows, DFTI_NUMBER_OF_TRANSFORMS, 1);
        status = DftiSetValue(desc_handle_dim_op_rows, DFTI_INPUT_STRIDES, stride);
        status = DftiSetValue(desc_handle_dim_op_rows, DFTI_OUTPUT_STRIDES, stride);
        status = DftiCommitDescriptor(desc_handle_dim_op_rows);

#pragma omp parallel for
        for (int th = 0; th < rows % max_threads; th++)
        {
            status = DftiComputeForward(desc_handle_dim_op_rows, I_t_I_w + th_r * (rows / max_threads) + th);
        }

        status = DftiFreeDescriptor(&desc_handle_dim_op_rows);
    }

    double end = omp_get_wtime();
    double time_spent = (end - start);
    printf("Tempo gasto no step A: %f s\n", time_spent);
}

void compute_fft2D_column_row_2(MKL_Complex8 *I_t_I_w, size_t rows, size_t columns)
{
    if (I_t_I_w == NULL)
    {
        printf("Matrix not found!\n");
        return;
    }
    
    double start = omp_get_wtime();

    int max_threads = omp_get_max_threads();

    DFTI_DESCRIPTOR_HANDLE desc_handle_dim1 = NULL;
    DFTI_DESCRIPTOR_HANDLE desc_handle_dim2 = NULL;
    MKL_LONG status;

    status = DftiCreateDescriptor(&desc_handle_dim1, DFTI_SINGLE,
                                  DFTI_COMPLEX, 1, rows);
    status = DftiCreateDescriptor(&desc_handle_dim2, DFTI_SINGLE,
                                  DFTI_COMPLEX, 1, columns);

    MKL_LONG stride[2] = {0, rows};

    status = DftiSetValue(desc_handle_dim1, DFTI_NUMBER_OF_TRANSFORMS, 1);
    status = DftiCommitDescriptor(desc_handle_dim1);

    status = DftiSetValue(desc_handle_dim2, DFTI_NUMBER_OF_TRANSFORMS, 1);
    status = DftiSetValue(desc_handle_dim2, DFTI_INPUT_STRIDES, stride);
    status = DftiSetValue(desc_handle_dim2, DFTI_OUTPUT_STRIDES, stride);
    status = DftiCommitDescriptor(desc_handle_dim2);

#pragma omp parallel for
    for (int i = 0; i < columns; i++)
    {
        status = DftiComputeForward(desc_handle_dim1, I_t_I_w + i * rows);
    }

    status = DftiFreeDescriptor(&desc_handle_dim1);

#pragma omp parallel for
    for (int i = 0; i < columns; i++)
    {
        status = DftiComputeForward(desc_handle_dim2, I_t_I_w + i);
    }

    status = DftiFreeDescriptor(&desc_handle_dim2);

    double end = omp_get_wtime();
    double time_spent = (end - start);
    printf("Tempo gasto no step A: %f s\n", time_spent);
}

void compute_periodic_border_B(MKL_Complex8 *I_t, MKL_Complex8 *B_t, size_t rows, size_t columns)
{
    if (I_t == NULL || B_t == NULL)
    {
        printf("Matrix not found!\n");
        return;
    }

    double start = omp_get_wtime();

    B_t[0].real = I_t[rows - 1].real - 2 * I_t[0].real + I_t[(columns - 1) * rows].real;
    B_t[0].imag = I_t[rows - 1].imag - 2 * I_t[0].imag + I_t[(columns - 1) * rows].imag;

    B_t[rows - 1].real = I_t[0].real - 2 * I_t[rows - 1].real + I_t[rows - 1 + (columns - 1) * rows].real;
    B_t[rows - 1].imag = I_t[0].imag - 2 * I_t[rows - 1].imag + I_t[rows - 1 + (columns - 1) * rows].imag;

    B_t[(columns - 1) * rows].real = I_t[0].real - 2 * I_t[(columns - 1) * rows].real + I_t[rows - 1 + (columns - 1) * rows].real;
    B_t[(columns - 1) * rows].imag = I_t[0].imag - 2 * I_t[(columns - 1) * rows].imag + I_t[rows - 1 + (columns - 1) * rows].imag;

    B_t[rows - 1 + (columns - 1) * rows].real = I_t[rows - 1].real - 2 * I_t[rows - 1 + (columns - 1) * rows].real + I_t[(columns - 1) * rows].real;
    B_t[rows - 1 + (columns - 1) * rows].imag = I_t[rows - 1].imag - 2 * I_t[rows - 1 + (columns - 1) * rows].imag + I_t[(columns - 1) * rows].imag;

// Primeira e última linha
#pragma omp parallel for
    for (int j = 1; j < columns - 1; j++)
    {
        B_t[j * rows].real = I_t[j * rows + rows - 1].real - I_t[j * rows].real;
        B_t[j * rows].imag = I_t[j * rows + rows - 1].imag - I_t[j * rows].imag;

        B_t[j * rows + rows - 1].real = B_t[j * rows].real * (-1);
        B_t[j * rows + rows - 1].imag = B_t[j * rows].imag * (-1);
    }

// Primeira e última coluna
#pragma omp parallel for
    for (int i = 1; i < rows - 1; i++)
    {
        B_t[i].real = I_t[i + rows * (columns - 1)].real - I_t[i].real;
        B_t[i].imag = I_t[i + rows * (columns - 1)].imag - I_t[i].imag;

        B_t[i + rows * (columns - 1)].real = B_t[i].real * (-1);
        B_t[i + rows * (columns - 1)].imag = B_t[i].imag * (-1);
    }

    double end = omp_get_wtime();
    double time_spent = (end - start);
    printf("Tempo gasto no step B: %f s\n", time_spent);
}

void compute_fft2D_of_B(MKL_Complex8 *B_t_B_w, size_t rows, size_t columns)
{
    if (B_t_B_w == NULL)
    {
        printf("Matrix not found!\n");
        return;
    }

    double start = omp_get_wtime();

    // Column-one FFT
    MKL_Complex8 *a = (MKL_Complex8 *)malloc(sizeof(MKL_Complex8));
    a->real = B_t_B_w[0].real + B_t_B_w[(columns - 1) * rows].real;
    a->imag = B_t_B_w[0].imag + B_t_B_w[(columns - 1) * rows].imag;

    DFTI_DESCRIPTOR_HANDLE desc_handle_dim1 = NULL;
    MKL_LONG status;

    status = DftiCreateDescriptor(&desc_handle_dim1, DFTI_SINGLE,
                                  DFTI_COMPLEX, 1, rows);

    status = DftiSetValue(desc_handle_dim1, DFTI_NUMBER_OF_TRANSFORMS, 1);
    status = DftiCommitDescriptor(desc_handle_dim1);
    status = DftiComputeForward(desc_handle_dim1, B_t_B_w);
    status = DftiFreeDescriptor(&desc_handle_dim1);

    // Calcular cada elemento de v usando a fórmula W^k = exp(-i * 2 * PI * k / M)
    MKL_Complex8 *v = (MKL_Complex8 *)malloc(rows * sizeof(MKL_Complex8));
    if (v == NULL)
    {
        printf("Erro ao alocar memória\n");
        return;
    }

    v[0].real = 0.00f;
    v[0].imag = 0.00f;

#pragma omp parallel for
    for (int k = 1; k < rows; k++)
    {
        float theta = -2.0f * PI * (rows - k) / rows;
        v[k].real = 1.0f - cosf(theta);
        v[k].imag = sinf(theta) * (-1);
    }

    cblas_ccopy(rows, B_t_B_w, 1, &B_t_B_w[(columns - 1) * rows], 1);
    cblas_csscal(rows, -1.0f, &B_t_B_w[(columns - 1) * rows], 1);
    cblas_caxpy(rows, a, v, 1, &B_t_B_w[(columns - 1) * rows], 1);

#pragma omp parallel for
    for (int j = 1; j < columns - 1; j++)
    {
        a->real = B_t_B_w[j * rows].real;
        a->imag = B_t_B_w[j * rows].imag;
        cblas_ccopy(rows, v, 1, &B_t_B_w[j * rows], 1);
        cblas_cscal(rows, a, &B_t_B_w[j * rows], 1);
    }

    free(v);
    //free(a);

    // Row-by-Row FFT

    int max_threads = omp_get_max_threads();

    DFTI_DESCRIPTOR_HANDLE desc_handle_dim2 = NULL;
    status = DftiCreateDescriptor(&desc_handle_dim2, DFTI_SINGLE,
                                  DFTI_COMPLEX, 1, columns);

    MKL_LONG stride[2] = {0, rows};

    status = DftiSetValue(desc_handle_dim2, DFTI_NUMBER_OF_TRANSFORMS, rows / max_threads);
    status = DftiSetValue(desc_handle_dim2, DFTI_INPUT_DISTANCE, 1);
    status = DftiSetValue(desc_handle_dim2, DFTI_OUTPUT_DISTANCE, 1);
    status = DftiSetValue(desc_handle_dim2, DFTI_INPUT_STRIDES, stride);
    status = DftiSetValue(desc_handle_dim2, DFTI_OUTPUT_STRIDES, stride);
    status = DftiCommitDescriptor(desc_handle_dim2);

    int th_r = 0;
    if (max_threads <= rows)
    {
#pragma omp parallel for
        for (th_r = 0; th_r < max_threads; th_r++)
        {
            status = DftiComputeForward(desc_handle_dim2, B_t_B_w + th_r * (rows / max_threads));
        }
        th_r = max_threads;
    }

    status = DftiFreeDescriptor(&desc_handle_dim2);

    if (rows % max_threads != 0)
    {
        DFTI_DESCRIPTOR_HANDLE desc_handle_dim_op_rows = NULL;
        status = DftiCreateDescriptor(&desc_handle_dim_op_rows, DFTI_SINGLE,
                                      DFTI_COMPLEX, 1, columns);
        status = DftiSetValue(desc_handle_dim_op_rows, DFTI_NUMBER_OF_TRANSFORMS, 1);
        status = DftiSetValue(desc_handle_dim_op_rows, DFTI_INPUT_STRIDES, stride);
        status = DftiSetValue(desc_handle_dim_op_rows, DFTI_OUTPUT_STRIDES, stride);
        status = DftiCommitDescriptor(desc_handle_dim_op_rows);

#pragma omp parallel for
        for (int th = 0; th < rows % max_threads; th++)
        {
            status = DftiComputeForward(desc_handle_dim_op_rows, B_t_B_w + th_r * (rows / max_threads) + th);
        }

        status = DftiFreeDescriptor(&desc_handle_dim_op_rows);
    }

    double end = omp_get_wtime();
    double time_spent = (end - start);
    printf("Tempo gasto no step C: %f s\n", time_spent);
}

void compute_smooth_component_S(MKL_Complex8 *B_S, size_t rows, size_t columns)
{
    if (B_S == NULL)
    {
        printf("Matrix not found!\n");
        return;
    }

    double start = omp_get_wtime();

    MKL_Complex8 aux = {B_S[0].real, B_S[0].imag};

    // #pragma omp parallel for
    for (int i = 0; i < rows; i++)
    {
        for (int j = 0; j < columns; j++)
        {
            float denom = (2.0f * cosf(2.0f * PI * i / rows) + 2.0f * cosf(2.0f * PI * j / columns) - 4.0f);
            B_S[i + j * rows].real /= denom;
            B_S[i + j * rows].imag /= denom;
        }
    }

    B_S[0].real = aux.real;
    B_S[0].imag = aux.imag;

    double end = omp_get_wtime();
    double time_spent = (end - start);
    printf("Tempo gasto no step D: %f s\n", time_spent);
}

void compute_smooth_component_S_2(MKL_Complex8 *B_S, size_t rows, size_t columns)
{
    if (B_S == NULL)
    {
        printf("Matrix not found!\n");
        return;
    }

    double start = omp_get_wtime();

    MKL_Complex8 aux = {B_S[0].real, B_S[0].imag};

    float *v1 = (float *)malloc(rows * sizeof(float));
    float *v2 = (float *)malloc(columns * sizeof(float));

    // #pragma omp parallel for
    for (int i = 0; i < rows; i++)
    {
        v1[i] = (2.0f * PI * i) / rows;
    }
    
    // #pragma omp parallel for
    for (int i = 0; i < columns; i++)
    {
        v2[i] = (2.0f * PI * (i)) / columns;
    }

    float *cos_1 = (float *)malloc(rows * sizeof(float));
    float *cos_2 = (float *)malloc(columns * sizeof(float));
    mkl_set_num_threads(8);
    vsCos(rows, v1, cos_1);
    vsCos(columns, v2, cos_2);
    cblas_sscal(rows, 2.0f, cos_1, 1);
    cblas_sscal(columns, 2.0f, cos_2, 1);
    mkl_set_num_threads(1);

    // #pragma omp parallel for
    for (int i = 0; i < rows; i++)
    {
        for (int j = 0; j < columns; j++)
        {
            float denom = cos_1[i] + cos_2[j] - 4.0f;
            B_S[i + j * rows].real /= denom;
            B_S[i + j * rows].imag /= denom;
        }
    }

    B_S[0].real = aux.real;
    B_S[0].imag = aux.imag;

    double end = omp_get_wtime();
    double time_spent = (end - start);
    printf("Tempo gasto no step D: %f s\n", time_spent);
}

void compute_periodic_component_P(MKL_Complex8 *I_w, MKL_Complex8 *S, size_t rows, size_t columns)
{
    if (I_w == NULL || S == NULL)
    {
        printf("Matrix not found!\n");
        return;
    }

    double start = omp_get_wtime();
    vcSub(rows * columns, I_w, S, I_w);
    double end = omp_get_wtime();
    double time_spent = (end - start);
    printf("Tempo gasto no step E: %f s\n", time_spent);
}

void read_binary(float *v, size_t size) {
    FILE *fp;

    fp = fopen("../bin/tests/toy2024_cp0_5m.bin", "rb");
    if (!fp) {
        perror("Error opening file ../bin/tests/toy2024_cp0_5m.bin");
        exit(1);
    }

    size_t elements_read = fread(v, sizeof(float), size, fp);
    if (elements_read != size) {
        if (feof(fp)) {
            fprintf(stderr, "Error: unexpected end of file\n");
        } else if (ferror(fp)) {
            perror("Error reading file");
        }
        fclose(fp);
        exit(1);
    }

    fclose(fp);
}

void read_matrix(MKL_Complex8 *matrix, size_t rows, size_t columns){
    float *v = NULL;
    init_float_vector(&v, rows*columns);
    read_binary(v, rows*columns);

    #pragma omp parallel for collapse(2)
    for (int i = 0; i < rows; i++)
    {
        for (int j = 0; j < columns; j++)
        {
            matrix[i + j * rows].real = v[j + i * columns];
        }
    }

    free(v);

    printf("\nMatriz:\n");
    for (int i = 0; i < 10; i++)
    {
        for (int j = 0; j < 10; j++)
        {
            printf("(%.2f + I%.2f) ", matrix[j + i * columns].real, matrix[j + i * columns].imag);
        }
        printf("\n");
    }
    printf("\n");
}


void save_complex_matrix(const char *filename, MKL_Complex8 *matrix, size_t rows, size_t columns) {
    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        perror("Erro ao abrir o arquivo para escrita");
        exit(EXIT_FAILURE);
    }
    fwrite(matrix, sizeof(MKL_Complex8), rows * columns, file);
    fclose(file);
}
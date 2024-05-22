#include "mkl_dfti.h"
#include "stdio.h"
#include "stdlib.h"
#include "complex.h"
#include "time.h"
#include "mkl.h"

#define RAND_MAX_F 2147483647.0f

int main(int argc, char const *argv[]) {
    if (argc != 3) {
        printf("Use: %s <M> <N>\n", argv[0]);
        return -1;
    }

    int M = atoi(argv[1]);
    int N = atoi(argv[2]);

    // Alocar memória dinamicamente para o array data
    _Complex float *data = (_Complex float *)malloc(M * N * sizeof(_Complex float));
    if (data == NULL) {
        printf("Erro ao alocar memória\n");
        return 1;
    }

    DFTI_DESCRIPTOR_HANDLE desc_handle_dim1 = NULL;
    DFTI_DESCRIPTOR_HANDLE desc_handle_dim2 = NULL;
    MKL_LONG status;
    clock_t start, end;
    double cpu_time_used;

    // Seed para gerar números aleatórios diferentes em cada execução
    //srand(time(NULL));

    srand(1);

    printf("\nValores de data antes da FFT:\n");
    for(int i = 0; i < M; i++){
        for(int j = 0; j < N; j++){
            float random_real = (float)rand() / RAND_MAX_F;
            // Normaliza o número para o intervalo [1, 5]
            float scaled_real = random_real * 4 + 1;

            //data[j + i*N] = scaled_real + 0.0f * I; // Row-Major Layout
            data[i + j*M] = scaled_real + 0.0f * I; // Column-Major Layout

            //printf("(%.2f, %.2f) ", crealf(data[j + i*N]), cimagf(data[j + i*N])); //Row-Major Layout
            printf("(%.2f, %.2f) ", crealf(data[i + j*M]), cimagf(data[i + j*M])); //Column-Major Layout
        }
        printf("\n");
    }
    printf("\n");

    // Criação e configuração dos descritores para as transformações

    status = DftiCreateDescriptor(&desc_handle_dim1, DFTI_SINGLE,
                              DFTI_COMPLEX, 1, M);
    status = DftiCreateDescriptor(&desc_handle_dim2, DFTI_SINGLE,
                              DFTI_COMPLEX, 1, N);

    MKL_LONG stride[2] = {0, M}; //Column-Major Layout
    
    status = DftiSetValue(desc_handle_dim1, DFTI_NUMBER_OF_TRANSFORMS, N);
    status = DftiSetValue(desc_handle_dim1, DFTI_INPUT_DISTANCE, M);
    status = DftiSetValue(desc_handle_dim1, DFTI_OUTPUT_DISTANCE, M);
    status = DftiCommitDescriptor(desc_handle_dim1);

    status = DftiSetValue(desc_handle_dim2, DFTI_NUMBER_OF_TRANSFORMS, M);
    status = DftiSetValue(desc_handle_dim2, DFTI_INPUT_DISTANCE, 1);
    status = DftiSetValue(desc_handle_dim2, DFTI_OUTPUT_DISTANCE, 1);
    status = DftiSetValue(desc_handle_dim2, DFTI_INPUT_STRIDES, stride);
    status = DftiSetValue(desc_handle_dim2, DFTI_OUTPUT_STRIDES, stride);
    status = DftiCommitDescriptor(desc_handle_dim2);

    /*MKL_LONG stride[2] = {0, N}; //Row-Major Layout
    status = DftiSetValue(desc_handle_dim1, DFTI_NUMBER_OF_TRANSFORMS, N);
    status = DftiSetValue(desc_handle_dim1, DFTI_INPUT_DISTANCE, 1);
    status = DftiSetValue(desc_handle_dim1, DFTI_OUTPUT_DISTANCE, 1);
    status = DftiSetValue(desc_handle_dim1, DFTI_INPUT_STRIDES, stride);
    status = DftiSetValue(desc_handle_dim1, DFTI_OUTPUT_STRIDES, stride);
    status = DftiCommitDescriptor(desc_handle_dim1);

    status = DftiSetValue(desc_handle_dim2, DFTI_NUMBER_OF_TRANSFORMS, M);
    status = DftiSetValue(desc_handle_dim2, DFTI_INPUT_DISTANCE, N);
    status = DftiSetValue(desc_handle_dim2, DFTI_OUTPUT_DISTANCE, N);
    status = DftiCommitDescriptor(desc_handle_dim2);
    */


    // Medir o tempo de execução da FFT
    start = clock();

    // Realizar as transformações FFT
    status = DftiComputeForward(desc_handle_dim1, data);
    status = DftiComputeForward(desc_handle_dim2, data);

    end = clock();

    cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;

    status = DftiFreeDescriptor(&desc_handle_dim1);
    status = DftiFreeDescriptor(&desc_handle_dim2);

    printf("Tempo de execução da FFT: %f segundos\n", cpu_time_used);

    printf("\nValores de data após a FFT:\n");
    for(int i = 0; i < M; i++){
        for(int j = 0; j < N; j++){
            //printf("(%.2f, %.2f) ", crealf(data[j + i * N]), cimagf(data[j + i * N])); //Row-Major Layout
            printf("(%.2f, %.2f) ", crealf(data[i + j * M]), cimagf(data[i + j * M])); //Column-Major Layout
        }
        printf("\n");
    }
    printf("\n");

    // Liberar a memória alocada dinamicamente
    free(data);

    return 0;
}
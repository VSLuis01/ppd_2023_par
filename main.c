#include <stdio.h>
#include <stdlib.h>
#include <mpi/mpi.h>

#define ROOT 0

typedef struct Aresta {
    long u;
    long v;
    long peso;
} Aresta;

MPI_Datatype MPI_Aresta;

void criarTipoAresta() {
    int blocklengths[3] = {1, 1, 1};
    MPI_Datatype types[3] = {MPI_LONG, MPI_LONG, MPI_LONG};
    MPI_Aint offsets[3];

    offsets[0] = offsetof(Aresta, u);
    offsets[1] = offsetof(Aresta, v);
    offsets[2] = offsetof(Aresta, peso);

    MPI_Type_create_struct(3, blocklengths, offsets, types, &MPI_Aresta);
    MPI_Type_commit(&MPI_Aresta);
}

void lerArquivo(int rank, int* total_vertices, int* total_arestas, Aresta** arestasLocal) {
    FILE *arquivo;

    if (rank == ROOT) {
        arquivo = fopen("super_arquivo.txt", "r");
        if (arquivo == NULL) {
            printf("Erro ao abrir o arquivo.\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        fscanf(arquivo, "%d", total_vertices);  // Lê o total de vértices
        fscanf(arquivo, "%d", total_arestas);  // Lê o total de arestas

        printf("TOTAL DE ARESTAS DO ARQUIVO: %d\n", *total_arestas);
        printf("TOTAL DE VERTICES DO ARQUIVO: %d\n", *total_vertices);

        // Distribui o número total de vértices para os demais processos
        MPI_Bcast(total_vertices, 1, MPI_INT, ROOT, MPI_COMM_WORLD);

        *arestasLocal = (Aresta *) malloc((*total_arestas) * sizeof(Aresta));

        for (int i = 0; i < *total_arestas; ++i) {
            fscanf(arquivo, "%ld%ld%ld", &(*arestasLocal)[i].u, &(*arestasLocal)[i].v, &(*arestasLocal)[i].peso);
        }
        fclose(arquivo);
    } else {
        // Processos não mestre recebem o número total de vértices
        MPI_Bcast(total_vertices, 1, MPI_INT, ROOT, MPI_COMM_WORLD);

        // Recebe numero de arestas
        MPI_Recv(total_arestas, 1, MPI_INT, ROOT, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        *arestasLocal = (Aresta *) malloc((*total_arestas) * sizeof(Aresta));
        MPI_Recv(*arestasLocal, *total_arestas, MPI_Aresta, ROOT, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
}

void distribuirArestas(int size, int total_arestas, Aresta* arestasLocal) {
    // Calcula o tamanho de "chunk" para cada processo (exceto o mestre)
    int aresta_p_processo = total_arestas / (size - 1);
    int resto = total_arestas % (size - 1);

    // Enviando a quantidade de arestas para cada processo
    for (int i = 1; i < size; ++i) {
        int quant_arestas_processo_i = (i == size - 1) ? aresta_p_processo + resto : aresta_p_processo;
        MPI_Send(&quant_arestas_processo_i, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
    }

    // Envia as arestas para os demais processos
    int offset = 0;
    for (int i = 1; i < size; i++) {
        int quant_elemento_i = (i == size - 1) ? aresta_p_processo + resto : aresta_p_processo;
        MPI_Send(&arestasLocal[offset], quant_elemento_i, MPI_Aresta, i, 0, MPI_COMM_WORLD);
        offset += quant_elemento_i;
    }
}

void imprimirArestas(int rank, int total_arestas, Aresta* arestasLocal) {
    printf("RANK: %d\n", rank);
    printf("Total de arestas: %d\n", total_arestas);
    printf("Dados lidos do arquivo:\n");
    for (int i = 0; i < total_arestas; i++) {
        printf("u: %ld, v: %ld, peso: %ld\n", arestasLocal[i].u, arestasLocal[i].v, arestasLocal[i].peso);
    }
}

int main(int argc, char *argv[]) {
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int total_vertices, total_arestas;
    Aresta *arestasLocal = NULL;

    criarTipoAresta();

    lerArquivo(rank, &total_vertices, &total_arestas, &arestasLocal);
    if (rank == ROOT) {
        distribuirArestas(size, total_arestas, arestasLocal);
    }

    // Cada processo realiza operações com seus dados locais
    if (rank != ROOT) {
        imprimirArestas(rank, total_arestas, arestasLocal);
    }

    // Libera a memória alocada
    MPI_Type_free(&MPI_Aresta);
    free(arestasLocal);
    MPI_Finalize();
    return 0;
}

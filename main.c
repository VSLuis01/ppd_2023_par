#include <mpi/mpi.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <time.h>

#define ROOT 0
#define RANGE(vertice, inferior, superior) ((vertice) >= (inferior) && (vertice) < (superior))

typedef struct node {
    struct node *raiz;
    int rank;
} UnionFindStruct;

typedef struct {
    long u;
    long v;
    long peso;
} Aresta;

FILE *arquivo; // arquivo de leitura das arestas

MPI_Datatype MPI_Aresta; // tipo de aresta do MPI

Aresta *arestas;
Aresta *arvoreGeradoraMinima;
long quantidadeArestasLocal; // quantidade de arestas desse processador

int rank, size; // identificação do processador e quantidade de processadores
UnionFindStruct unionFind; // contem todos os vertices

long totalArestas, totalVertices; // numero total de arestas e vertices

void debug(int rank, char *format, ...);

void printarArestas() {
    int i;
    printf("Proc %d local edges:\n", rank);
    for (i = 0; i < quantidadeArestasLocal; ++i) {
        printf("(%ld,%ld) = %ld\n", arestas[i].v, arestas[i].u, arestas[i].peso);
    }
}

void abortProgram(const char *mensagem) {
    perror(mensagem);

    fclose(arquivo);
    MPI_Type_free(&MPI_Aresta);
    MPI_Finalize();
    exit(1);
}

void inicializacao(int argc, char **argv) {
    // INicializando MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    // Criando o tipo de aresta MPI
    MPI_Type_contiguous(3, MPI_LONG, &MPI_Aresta);
    MPI_Type_commit(&MPI_Aresta);
}

void finalizacao() {
    MPI_Type_free(&MPI_Aresta);
    MPI_Finalize();
    free(arestas);
    free(arvoreGeradoraMinima);
}

void obterArestasVertices(const char *nomeArquivo) {
    // TODO colocar verificacoes do numero de processadores

    arquivo = fopen(nomeArquivo, "rb");

    if (arquivo == NULL) {
        abortProgram("\n[ERRO] Problema na leitura do arquivo\n");
    }

    // leitura dos vertices
    fscanf(arquivo, "%ld", &totalVertices);
    fscanf(arquivo, "%ld", &totalArestas);

    /*   if (totalVertices / size < 2) {
           abortProgram("\n[ERRO] Numero de vertices por processador deve ser pelo menos 2\n");
       }*/
}

bool intervalo(long vertice, long inferior, long superior) {
    return ((vertice >= inferior) && (vertice < superior));
}

void distribuirArestasPorProcessador() {
    long verticePorProcessador = totalVertices / size;
    long primeiroVertice = rank * verticePorProcessador;
    long ultimoVertice = (rank + 1) * verticePorProcessador;

    // ultimo processador fica com o restando dos vertices (caso tiver)
    if (rank == size - 1) {
        ultimoVertice += totalVertices % size;
        verticePorProcessador += totalVertices % size;
    }

    // alocar memória das arestas
    arestas = (Aresta *) malloc(verticePorProcessador * totalVertices * sizeof(Aresta));

    if (arestas == NULL) {
        abortProgram("\n[ERRO] erro ao alocar arestas\n");
    }

    // numero de arestas de um arvore é igual ao numero de vertices - 1
    arvoreGeradoraMinima = (Aresta *) malloc((totalVertices - 1) * sizeof(Aresta));

    if (arvoreGeradoraMinima == NULL) {
        abortProgram("\n[ERRO] erro ao alocar arestas da agm.\n");
    }

    quantidadeArestasLocal = 0;

    Aresta tmp;
    for (long i = 0; i < totalArestas; i++) {
        fscanf(arquivo, "%ld %ld %ld", &tmp.v, &tmp.u, &tmp.peso);
        if (RANGE(tmp.v, primeiroVertice, ultimoVertice) || RANGE(tmp.u, primeiroVertice, ultimoVertice)) {
            arestas[quantidadeArestasLocal] = tmp;
            quantidadeArestasLocal++;
        }
    }
    fclose(arquivo);
    MPI_Barrier(MPI_COMM_WORLD);
}

int main(int argc, char **argv) {
    const char *nomeArquivo = "arquivo.txt";
    inicializacao(argc, argv);
    obterArestasVertices(nomeArquivo);

//    printf("\n[RANK] %d - TOTAL VERTICES: %ld - TOTAL ARESTAS: %ld\n", rank, totalVertices, totalArestas);

    distribuirArestasPorProcessador();

    printarArestas();

    finalizacao();
    return 0;
}

double get_timer() {
    clock_t current_clock = clock();
    double timer = (double)current_clock / CLOCKS_PER_SEC;

    return timer;
}

void debug(int rank, char *format, ...) {
    va_list args;

    va_start(args, format);

    printf("%12.6f|%2d|", get_timer(), rank);
    vprintf(format, args);

    va_end(args);
}
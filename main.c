#include <mpi/mpi.h>
#include <stdlib.h>
#include <stdio.h>


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

int rank, size; // identificação do processador e quantidade de processadores
UnionFindStruct unionFind; // contem todos os vertices

long totalArestas, totalVertices; // numero total de arestas e vertices

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
    fclose(arquivo);
    MPI_Type_free(&MPI_Aresta);
    MPI_Finalize();
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

int main(int argc, char **argv) {
    const char* nomeArquivo = "arquivo.txt";
    inicializacao(argc, argv);
    obterArestasVertices(nomeArquivo);

    printf("\n[RANK] %d - TOTAL VERTICES: %ld - TOTAL ARESTAS: %ld\n", rank, totalVertices, totalArestas);

    finalizacao();
    return 0;
}
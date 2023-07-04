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
} Conjunto;

typedef struct {
    long u;
    long v;
    long peso;
} Aresta;

FILE *arquivo; // arquivo de leitura das arestas

MPI_Datatype MPI_Aresta; // tipo de aresta do MPI

long quantidadeArestasLocal = 0; // quantidade de arestas desse processador
Aresta *arestas;

long quantidadeArestasAGM = 0;
Aresta *arvoreGeradoraMinima;

int rank, size; // identificação do processador e quantidade de processadores
Conjunto *conjunto; // contem todos os vertices

long totalArestas, totalVertices; // numero total de arestas e vertices

void debug(char *format, ...);

int comparacaoArestas(const void *aresta1, const void *aresta2);

Conjunto *find(Conjunto *no) {
    if (no->raiz == NULL) return no;
    no->raiz = find(no->raiz);
    return no->raiz;
}

void unionConjunto(Conjunto *no1, Conjunto *no2) {
    if (no1->rank < no2->rank) {
        no1->raiz = no2;
    } else if (no1->rank > no2->rank) {
        no2->raiz = no1;
    } else {
        no1->raiz = no2;
        no1->rank += 1;
    }
}

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


void encontrarAGM() {
    qsort(arestas, quantidadeArestasLocal, sizeof(Aresta), comparacaoArestas);

    // criar estrutura do union find
    // raiz = NULL e rank = 0
    free(conjunto);
    conjunto = calloc(totalVertices, sizeof(Conjunto));

    for (long i = 0; i < totalArestas; ++i) {
        Conjunto *raizV = find(&conjunto[arestas[i].v]);
        Conjunto *raizU = find(&conjunto[arestas[i].u]);

        if (raizV != raizU) {
            arvoreGeradoraMinima[quantidadeArestasAGM] = arestas[i];
            quantidadeArestasAGM++;
            unionConjunto(raizV, raizU);
        }
    }
}

int main(int argc, char **argv) {
    const char *nomeArquivo = "arquivo.txt";
    inicializacao(argc, argv);
    obterArestasVertices(nomeArquivo);

//    printf("\n[RANK] %d - TOTAL VERTICES: %ld - TOTAL ARESTAS: %ld\n", rank, totalVertices, totalArestas);

    distribuirArestasPorProcessador();

    encontrarAGM();

    /*for (int i = 0; i < quantidadeArestasLocal; ++i) {
        debug("ANTES>>(%ld,%ld) = %ld\n", arestas[i].v, arestas[i].u, arestas[i].peso);
    }

    for (int i = 0; i < quantidadeArestasAGM; ++i) {
        debug("DEPOIS>>(%ld,%ld) = %ld\n", arvoreGeradoraMinima[i].v, arvoreGeradoraMinima[i].u, arvoreGeradoraMinima[i].peso);
    }
    printf("\n");
     */

    finalizacao();
    return 0;
}

double get_timer() {
    clock_t current_clock = clock();
    double timer = (double) current_clock / CLOCKS_PER_SEC;

    return timer;
}

void debug(char *format, ...) {
    va_list args;

    va_start(args, format);

    printf("%6.6f|%2d|", get_timer(), rank);
    vprintf(format, args);

    va_end(args);
}

int comparacaoArestas(const void *aresta1, const void *aresta2) {
    Aresta *a1 = (Aresta *) aresta1;
    Aresta *a2 = (Aresta *) aresta2;

    if (a1->peso > a2->peso) {
        return 1;
    } else if (a1->peso < a2->peso) {
        return -1;
    } else {
        return 0;
    }
}
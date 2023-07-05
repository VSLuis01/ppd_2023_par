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
Aresta *arvoreGeradoraMinima; // arestas da arvore geradora minima

int rank, size; // identificação do processador e quantidade de processadores
Conjunto *conjunto; // contem todos os vertices

long totalArestasGlobal, totalVertices; // numero total de arestas e vertices

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

void abortProgram(const char *mensagem) {
    perror(mensagem);

    fclose(arquivo);
    MPI_Type_free(&MPI_Aresta);
    MPI_Finalize();
    exit(1);
}

void inicializacao(int argc, char **argv) {
    // Inicializando MPI
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

/*Obtem o numero total de vertices e arestas*/
void obterArestasVertices(const char *nomeArquivo) {
    // TODO colocar verificacoes do numero de processadores

    arquivo = fopen(nomeArquivo, "rb");

    if (arquivo == NULL) {
        abortProgram("\n[ERRO] Problema na leitura do arquivo\n");
    }

    // leitura dos vertices
    fscanf(arquivo, "%ld", &totalVertices);
    fscanf(arquivo, "%ld", &totalArestasGlobal);

    /*   if (totalVertices / size < 2) {
           abortProgram("\n[ERRO] Numero de vertices por processador deve ser pelo menos 2\n");
       }*/
}

void distribuirArestasPorProcessador() {
    // Processo mestre lê todas as arestas do arquivo
    if (rank == ROOT) {
        arestas = malloc(totalArestasGlobal * sizeof(Aresta));

        for (int i = 0; i < totalArestasGlobal; i++) {
            fscanf(arquivo, "%ld %ld %ld", &arestas[i].u, &arestas[i].v, &arestas[i].peso);
        }

        fclose(arquivo);
    }

    // Envia a quantidade de arestas para cada processo
    int *quantidadesArestas = malloc(size * sizeof(int));
    int resto = totalArestasGlobal % size;
    int quantidadeBase = totalArestasGlobal / size;

    for (int i = 0; i < size; i++) {
        quantidadesArestas[i] = quantidadeBase;
        if (i == size - 1) {
            quantidadesArestas[i] += resto;
        }
    }
    // Calcula o deslocamento de cada processo
    int *deslocamentos = malloc(size * sizeof(int));
    deslocamentos[0] = 0;

    for (int i = 1; i < size; i++) {
        deslocamentos[i] = deslocamentos[i-1] + quantidadesArestas[i-1];
    }

    // Calcula a quantidade de arestas que cada processo irá receber
    quantidadeArestasLocal = quantidadesArestas[rank];

    // Aloca espaço para armazenar as arestas locais
    if (rank != ROOT) {
        arestas = malloc(quantidadeArestasLocal * sizeof(Aresta));
    }

    // Distribui as arestas para cada processo
    MPI_Scatterv(arestas, quantidadesArestas, deslocamentos, MPI_Aresta, arestas, quantidadeArestasLocal, MPI_Aresta, ROOT, MPI_COMM_WORLD);

    free(quantidadesArestas);
    free(deslocamentos);
}
void encontrarAGM() {
    qsort(arestas, quantidadeArestasLocal, sizeof(Aresta), comparacaoArestas);

    // Criar estrutura do Union Find
    // Raiz = NULL e Rank = 0
    free(conjunto);
    conjunto = calloc(totalVertices, sizeof(Conjunto));

    for (long i = 0; i < totalArestasGlobal; ++i) {
        Conjunto *raizV = find(&conjunto[arestas[i].v]);
        Conjunto *raizU = find(&conjunto[arestas[i].u]);
        if (raizV != raizU) {
            arvoreGeradoraMinima[quantidadeArestasAGM++] = arestas[i];
            unionConjunto(raizV, raizU);
        }
    }
}

int main(int argc, char **argv) {
    inicializacao(argc, argv);

    if (argc != 2) {
        if (rank == ROOT) {
            abortProgram("\n[ERRO] Arquivo nao encontrado.\n");
        }
    }

    obterArestasVertices(argv[1]);

    distribuirArestasPorProcessador();

    MPI_Barrier(MPI_COMM_WORLD);
    for (long i = 0; i < quantidadeArestasLocal; ++i) {
        debug("(%ld %ld) = %d\n", arestas[i].u, arestas[i].v, arestas[i].peso);
    }
    printf("\n");

//    encontrarAGM();

    /*if (rank == ROOT) {
        printf("Arvore geradora minima:\n");
        for (long i = 0; i < totalVertices - 1; ++i) {
            printf("(%d,%d) = %d\n", arvoreGeradoraMinima[i].v, arvoreGeradoraMinima[i].u, arvoreGeradoraMinima[i].peso);
        }
    }*/

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
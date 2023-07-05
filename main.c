#include <mpi/mpi.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>

#define ROOT 0

typedef struct node {
    struct node *raiz;
    int rank;
} Conjunto;

// guarda os vertices locais. Caso um vertice nao pertença a essa máquina, entao é marcado como -1
typedef struct {
    long v;
    long grau;
} Vertices;

typedef struct {
    long u;
    long v;
    long peso;
} Aresta;

FILE *arquivo; // arquivo de leitura das arestas

MPI_Datatype MPI_Aresta; // tipo de aresta do MPI

long quantidadeArestasLocal = 0; // quantidade de arestas desse processador
Aresta *arestas;
long quantidadeVerticesLocal = 0; // quantidade de vertices locais
Vertices *vertices;

long quantidadeArestasAGM = 0;
Aresta *arvoreGeradoraMinima; // arestas da arvore geradora minima

int rank, size; // identificação do processador e quantidade de processadores
Conjunto *conjunto; // contem todos os vertices

long totalArestasGlobal, totalVerticesGlobal; // numero total de arestas e vertices

void debug(char *format, ...);

int comparacaoArestas(const void *aresta1, const void *aresta2);


long hash(long v) {
    return v % totalVerticesGlobal;
}

void printVertices() {
    debug("Total vertice local: %ld\n", quantidadeVerticesLocal);
    for (int i = 0; i < totalVerticesGlobal; ++i) {
        if (vertices[i].v != -1) {
            debug("Vertice :%ld\n", vertices[i].v);
        }
    }
    printf("\n");
}

void printArestas() {
    debug("Minhas arestas:\n");
    for (int i = 0; i < quantidadeArestasLocal; ++i) {
        debug("(%ld  %ld)  =>  %ld\n", arestas[i].v, arestas[i].u, arestas[i].peso);
    }
    printf("\n");
}

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
    free(vertices);
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
    fscanf(arquivo, "%ld", &totalVerticesGlobal);
    fscanf(arquivo, "%ld", &totalArestasGlobal);

    /*   if (totalVerticesGlobal / size < 2) {
           abortProgram("\n[ERRO] Numero de vertices por processador deve ser pelo menos 2\n");
       }*/
}

// distribuir aresta entre os processos
void distribuirArestasPorProcessador() {
    // Processo mestre lê todas as arestas do arquivo
    vertices = calloc(totalVerticesGlobal, sizeof(Vertices));
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
        deslocamentos[i] = deslocamentos[i - 1] + quantidadesArestas[i - 1];
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


void compartilharVerticesAusentes() {
    // Criar um array para armazenar a quantidade de vértices de cada processo
    long *quantidadeVerticesRanks = malloc(size * sizeof(long));
    long *quantidadeArestasRanks = malloc(size * sizeof(long));

    // Coletar a quantidade de vértices e arestas de cada processo no processo raiz
    MPI_Gather(&quantidadeVerticesLocal, 1, MPI_LONG, quantidadeVerticesRanks, 1, MPI_LONG, ROOT, MPI_COMM_WORLD);
    MPI_Gather(&quantidadeArestasLocal, 1, MPI_LONG, quantidadeArestasRanks, 1, MPI_LONG, ROOT, MPI_COMM_WORLD);

    // Transmitir a quantidade de vértices e arestas de cada processo para todos os outros processos
    MPI_Bcast(quantidadeVerticesRanks, size, MPI_LONG, ROOT, MPI_COMM_WORLD);
    MPI_Bcast(quantidadeArestasRanks, size, MPI_LONG, ROOT, MPI_COMM_WORLD);

    /*debug("Eu possuo %ld vertices e %ld arestas\n", quantidadeVerticesLocal, quantidadeArestasLocal);
    for (int i = 0; i < size; ++i) {
        if (i != rank) {
            debug("A máquina %d possui: %ld vertices e %ld arestas\n", i, quantidadeVerticesRanks[i], quantidadeArestasRanks[i]);
        }
    }
    printf("\n");*/

    Aresta **arestasRanks = malloc(size * sizeof(Aresta *));

    /*Recebendo as arestas de cada rank*/
    for (int i = 0; i < size; i++) {
        if (i != rank) {
            arestasRanks[i] = malloc(quantidadeArestasRanks[i] * sizeof(Aresta));
            MPI_Send(arestas, quantidadeArestasLocal, MPI_Aresta, i, 0, MPI_COMM_WORLD);
            MPI_Recv(arestasRanks[i], quantidadeArestasRanks[i], MPI_Aresta, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    }

    /*MPI_Barrier(MPI_COMM_WORLD);
    for (int i = 0; i < size; ++i) {
        if (rank != i) {
            debug("Arestas do processo %d\n", i);
            for (int j = 0; j < quantidadeArestasRanks[i]; ++j) {
                debug("(%ld  %ld)  =>  %ld\n", arestasRanks[i][j].u, arestasRanks[i][j].v, arestasRanks[i][j].peso);
            }
        }
    }
    printf("\n");*/

    // Agora, todos os processos têm acesso à quantidade de vértices de cada máquina
    // Você pode prosseguir com o restante do seu código
    // ...

    // Liberar a memória alocada

    for (int i = 0; i < size; ++i) {
        if (i != rank) {
            free(arestasRanks[i]);
        }
    }
    free(arestasRanks);
    free(quantidadeVerticesRanks);
    free(quantidadeArestasRanks);
}


void encontrarAGM() {
    qsort(arestas, quantidadeArestasLocal, sizeof(Aresta), comparacaoArestas);

    // Criar estrutura do Union Find
    // Raiz = NULL e Rank = 0
    free(conjunto);
    conjunto = calloc(totalVerticesGlobal, sizeof(Conjunto));

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

    compartilharVerticesAusentes();


//    encontrarAGM();


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
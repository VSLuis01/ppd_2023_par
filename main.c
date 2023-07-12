#include <mpi/mpi.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <stdbool.h>
#include <math.h>

#define ROOT 0
#define RANGE(vertice, inferior, superior) ((vertice) >= (inferior) && (vertice) < (superior))

typedef struct node {
    struct node *raiz;
    int rank;
} Conjunto;

typedef struct {
    u_int64_t u;
    u_int64_t v;
    u_int64_t peso;
} Aresta;

FILE *arquivo; // arquivo de leitura das arestasLocal

MPI_Datatype MPI_Aresta; // tipo de aresta do MPI
MPI_Datatype MPI_Vertice; // tipo de vertice do MPI

u_int64_t quantidadeArestasLocal = 0; // quantidade de arestasLocal desse processador
Aresta *arestasLocal = NULL;

u_int64_t quantidadeArestasAGMLocal = 0;
Aresta *arvoreGeradoraMinimaLocal = NULL; // arestasLocal da arvore geradora minima

int quantArestasRecvAGM = 0;

u_int64_t quantArestasMergedAGM = 0;
Aresta *mergedAGM = NULL;

int rank, size; // identificação do processador e quantidade de processadores
Conjunto *conjunto = NULL; // contem todos os verticesCompleto


Aresta *arestasCompleto = NULL;
u_int64_t totalArestasGlobal, totalVerticesGlobal; // numero total de arestasLocal e verticesCompleto

void debug(char *format, ...);

int comparacaoArestas(const void *aresta1, const void *aresta2);

void printArestasGlobais() {
    u_int64_t i = 0;
    u_int64_t pesoTotal = 0;
    for (; i < totalArestasGlobal; ++i) {
        debug("%lu %lu %lu\n", arestasCompleto[i].u, arestasCompleto[i].v, arestasCompleto[i].peso);
    }
    debug("Peso parcial/total: %lu\n", pesoTotal);
    printf("\n");
}

void printArestas() {
    debug("Minhas arestasLocal: %lu\n", quantidadeArestasLocal);
    u_int64_t i = 0;
    u_int64_t pesoTotal = 0;
    for (; i < quantidadeArestasLocal; ++i) {
        debug("%lu %lu %lu\n", arestasLocal[i].u, arestasLocal[i].v, arestasLocal[i].peso);
        pesoTotal += arestasLocal[i].peso;
    }
    debug("Peso parcial/total: %lu\n", pesoTotal);
    printf("\n");
}

void printAGM() {
    u_int64_t pesoTotal = 0;
    FILE *escrita;
    escrita = fopen("pesos.txt", "w");
    debug("Quantida de arestas da AGM: %lu\n", quantidadeArestasAGMLocal);
    for (u_int64_t i = 0; i < quantidadeArestasAGMLocal; ++i) {
//        debug("(%lu  %lu)  =>  %lu\n", arvoreGeradoraMinimaLocal[i].u, arvoreGeradoraMinimaLocal[i].v, arvoreGeradoraMinimaLocal[i].peso);
        pesoTotal += arvoreGeradoraMinimaLocal[i].peso;
        fprintf(escrita, "%lu,", arvoreGeradoraMinimaLocal[i].peso);
    }
    debug("Peso parcial/total: %lu\n", pesoTotal);
    printf("\n");
    fclose(escrita);
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
    printf("(%d) %s", rank, mensagem);

    free(arestasLocal);
    free(arvoreGeradoraMinimaLocal);
    free(mergedAGM);
    free(arestasCompleto);
    MPI_Type_free(&MPI_Aresta);
    MPI_Type_free(&MPI_Vertice);
    MPI_Finalize();
    exit(1);
}

void inicializacao(int argc, char **argv) {
    // Inicializando MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    // Criando o tipo de aresta MPI
    MPI_Type_contiguous(3, MPI_UINT64_T, &MPI_Aresta);
    MPI_Type_commit(&MPI_Aresta);

    MPI_Type_contiguous(2, MPI_UINT64_T, &MPI_Vertice);
    MPI_Type_commit(&MPI_Vertice);
}

void finalizacao() {
    MPI_Type_free(&MPI_Aresta);
    MPI_Type_free(&MPI_Vertice);


    free(arestasCompleto);
    free(arestasLocal);
    free(mergedAGM);
    free(arvoreGeradoraMinimaLocal);

    MPI_Finalize();
}

/*Obtem o numero total de verticesCompleto e arestasLocal*/
void obterArestasVertices(const char *nomeArquivo) {
    arquivo = fopen(nomeArquivo, "r");

    if (arquivo == NULL) {
        abortProgram("\n[ERRO] Problema na leitura do arquivo\n");
    }

    // leitura dos verticesCompleto
    fscanf(arquivo, "%lu", &totalVerticesGlobal);
    fscanf(arquivo, "%lu", &totalArestasGlobal);

}

/**
 * Nessa implementação cada rank (processo) é responsável por ler o arquivo
 * cada rank é responsável por um intervalo de vertices
 */
void distribuirArestasPorProcessador() {
    // encontrando o intervalo de vertice de cada rank
    u_int64_t verticePorProcessador = totalVerticesGlobal / size;
    u_int64_t primeiroVertice = rank * verticePorProcessador;
    u_int64_t ultimoVertice = (rank + 1) * verticePorProcessador;

    // Último processador fica com o restante dos vértices (se houver)
    if (rank == size - 1) {
        ultimoVertice += totalVerticesGlobal % size;
    }

    // quantidade de arestas que cada processador irá receber
    u_int64_t quantidadeArestasPorProcessador = totalArestasGlobal / size;
    u_int64_t restanteArestas = totalArestasGlobal % size;
    // essa parte é um calculo inicial, pois alocar direito a quantidade de arestas possíveis (1965206 * 2769244) o malloc não aguenta
    u_int64_t quantidadeArestasInicial = quantidadeArestasPorProcessador + (rank < restanteArestas ? 1 : 0);

    // Aloca memoria para a arvore geradora local
    arvoreGeradoraMinimaLocal = (Aresta *) malloc((totalVerticesGlobal - 1) * sizeof(Aresta));
    if (arvoreGeradoraMinimaLocal == NULL) {
        abortProgram("[ERRO] erro ao alocar AGM\n");
    }

    // aloca aresta para a arvore geradora merged. 2 * (totalVerticesGlobal - 1) pois essa variavel vai receber a arvore local + a arvore de outro rank
    mergedAGM = (Aresta *) malloc(2 * (totalVerticesGlobal - 1) * sizeof(Aresta));
    if (mergedAGM == NULL) {
        abortProgram("[ERRO] erro ao alocar merged AGM\n");
    }

    // variavel com todas arestas (usada apenas para fiz de depuração)
    arestasCompleto = (Aresta *) malloc(totalArestasGlobal * sizeof(Aresta));

    // Arestas locais (do rank)
    arestasLocal = (Aresta *) malloc(quantidadeArestasInicial * sizeof(Aresta));
    if (arestasLocal == NULL) {
        abortProgram("[ERRO] erro ao alocar arestas\n");
    }

    quantidadeArestasLocal = 0;

    Aresta tmp;
    for (u_int64_t i = 0; i < totalArestasGlobal; i++) {
        fscanf(arquivo, "%lu %lu %lu", &tmp.v, &tmp.u, &tmp.peso);
        arestasCompleto[i] = tmp;
        // verifica se o vertice está dentro do intervalo calculado para cada rank (desse modo garante que cada rank tem todas as arestas necessárias para calcular a sua arvore)
        if (RANGE(tmp.v, primeiroVertice, ultimoVertice) || RANGE(tmp.u, primeiroVertice, ultimoVertice)) {
            // realoca a quantidade de arestas
            if (quantidadeArestasLocal == quantidadeArestasInicial) {
                quantidadeArestasInicial *= 2;

                arestasLocal = (Aresta *) realloc(arestasLocal, quantidadeArestasInicial * sizeof(Aresta));

                if (arestasLocal == NULL) {
                    abortProgram("\n[ERRO] erro ao relocar arestas\n");
                }
            }
            arestasLocal[quantidadeArestasLocal++] = tmp;
        }
    }

    fclose(arquivo);
    // garantir que todos os ranks já leram o arquivo
    MPI_Barrier(MPI_COMM_WORLD);
}

/***
 * Calcular a arvore gerado mínima local
 */
void encontrarAGMLocal() {
    // ordena as arestas
    qsort(arestasLocal, quantidadeArestasLocal, sizeof(Aresta), comparacaoArestas);

    // Criar estrutura do Union Find
    // Raiz = NULL e Rank = 0
    free(conjunto);
    conjunto = calloc(totalVerticesGlobal, sizeof(Conjunto));

    quantidadeArestasAGMLocal = 0;
    quantArestasMergedAGM = 0;

    for (u_int64_t i = 0; i < quantidadeArestasLocal; ++i) {
        Aresta *arestaMinima = &arestasLocal[i];

        Conjunto *raizV = find(&conjunto[arestaMinima->v]);
        Conjunto *raizU = find(&conjunto[arestaMinima->u]);
        if (raizV != raizU) {
            arvoreGeradoraMinimaLocal[quantidadeArestasAGMLocal++] = *arestaMinima;
            mergedAGM[quantArestasMergedAGM++] = *arestaMinima; // já adiciona a arvoreGeradora mínima local na variavel que vai receber as duas AGM (local e do outro rank)
            unionConjunto(raizV, raizU);
        }
    }
}

void encontrarAGMParalelo() {
    qsort(mergedAGM, quantArestasMergedAGM, sizeof(Aresta), comparacaoArestas);

    free(conjunto);
    conjunto = calloc(totalVerticesGlobal, sizeof(Conjunto));

    quantidadeArestasAGMLocal = 0;

    // Encontra a nova arvore geradora mínima, só que dessa vez perconrrendo as arestas que "mergiou"
    for (u_int64_t i = 0; i < quantArestasMergedAGM; ++i) {
        Aresta *arestaMinima = &mergedAGM[i];

        Conjunto *raizV = find(&conjunto[arestaMinima->v]);
        Conjunto *raizU = find(&conjunto[arestaMinima->u]);

        if (raizV != raizU) {
            arvoreGeradoraMinimaLocal[quantidadeArestasAGMLocal++] = *arestaMinima;

            unionConjunto(raizV, raizU);
        }
    }

    quantArestasMergedAGM = 0;
    // sobreescreve a "primeira parte" da agm mergeada com a nova agm local encontrada no loop acima. (Depois a outra metade alocada vai receber agm de outro rank).
    for (u_int64_t i = 0; i < quantidadeArestasAGMLocal; ++i) {
        Aresta arestaMerged = arvoreGeradoraMinimaLocal[i];
        mergedAGM[quantArestasMergedAGM++] = arestaMerged;
    }
}

bool ehPotenciaDeDois(int n) {
    int powerOfTwo = 1;

    while (powerOfTwo < n) {
        powerOfTwo *= 2;
    }

    return powerOfTwo == n;
}

int main(int argc, char **argv) {
    inicializacao(argc, argv);

    // Nessa implementação o código só funciona quando o número de processadores é uma potencia de dois

    /*
     Funciona dessa forma a estrutura
     0    1  2     3
      \  /    \   /
        0       2
         \     /
            0
    */
    if (!ehPotenciaDeDois(size)) {
        abortProgram("[ERRO] O numero de processadores deve ser uma potencia de 2\n");
    }

    if (argc != 2) {
        if (rank == ROOT) {
            abortProgram("\n[ERRO] Arquivo nao encontrado.\n");
        }
    }

    // Obtém o número de vertices e arestas
    obterArestasVertices(argv[1]);

    if (totalVerticesGlobal / size < 2) {
        abortProgram("[ERRO] O Numero de vertices por processador deve ser no minimo 2\n");
        fclose(arquivo);
    }

    // distribui o conjunto de arestas entre os ranks
    distribuirArestasPorProcessador();

    // Se só tem um rank, então só calcula a arvore local, pois não precisará de comunicações
    if (size == 1) {
        encontrarAGMLocal();

        printAGM();
    } else {
        int processadores = size;
        int it = 0;
        MPI_Status status;
        int source;
        int destination;


        encontrarAGMLocal();

        while (processadores > 1) {

            // determina qual grupo os ranks pertencem (remetente destinatário)
            if ((rank / (int) pow(2, it)) % 2 != 0) {
                destination = (rank - (int) pow(2, it));

                MPI_Send(arvoreGeradoraMinimaLocal, quantidadeArestasAGMLocal, MPI_Aresta, destination, 0, MPI_COMM_WORLD);
                // debug("[ITERAÇÃO: %d] Enviando a arvore para o rank %d -- SAINDO....\n", it, destination);
                break;
            } else {
                source = (rank + (int) pow(2, it)); // rank of source

                // Recebe as arestas na segunda metade do espaço alocado
                MPI_Recv(mergedAGM + quantArestasMergedAGM, totalVerticesGlobal - 1, MPI_Aresta, source, 0, MPI_COMM_WORLD, &status);
                MPI_Get_count(&status, MPI_Aresta, &quantArestasRecvAGM);
                // debug("[ITERAÇÃO: %d] Recebendo agm do rank: %d\n", it, source);
            }
            quantArestasMergedAGM += quantArestasRecvAGM;

            encontrarAGMParalelo();

            processadores /= 2; // a cada iteração a metade dos processadores saem dos calculos (são os que enviam a agm para outro rank). Isso pq metade recebe e metade envia
            it++;
        }

        MPI_Barrier(MPI_COMM_WORLD);
        if (rank == ROOT) {
            printf("\n");
            encontrarAGMParalelo();
            printAGM();
        }
    }

    finalizacao();

    return 0;
}

double get_timer() {
    clock_t current_clock = clock();
    double timer = (double) current_clock / CLOCKS_PER_SEC;

    return timer;
}

// função axuliar para depuração
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
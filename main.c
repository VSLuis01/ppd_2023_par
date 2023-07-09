#include <mpi/mpi.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#define ROOT 0
#define RECV_RANK(x, y) (((x) / (y)) % 2)

typedef struct node {
    struct node *raiz;
    int rank;
} Conjunto;

// guarda os verticesCompleto locais. Caso um vertice nao pertença a essa máquina, entao é marcado como -1
typedef struct {
    long v;
    long grau;
} Vertices;

typedef struct {
    long u;
    long v;
    long peso;
} Aresta;

FILE *arquivo; // arquivo de leitura das arestasLocal

MPI_Datatype MPI_Aresta; // tipo de aresta do MPI
MPI_Datatype MPI_Vertice; // tipo de vertice do MPI

long quantidadeArestasLocal = 0; // quantidade de arestasLocal desse processador
Aresta *arestasLocal;
Aresta *arestasCompleto;
long quantidadeVerticesLocal = 0; // quantidade de verticesCompleto locais
Vertices *verticesCompleto;
Vertices *verticesLocal;

long quantidadeArestasAGMLocal = 0;
Aresta *arvoreGeradoraMinimaLocal; // arestasLocal da arvore geradora minima

int quantArestasRecvAGM = 0;
Aresta *recvAGM;

long quantArestasMergedAGM = 0;
Aresta *mergedAGM;

int rank, size; // identificação do processador e quantidade de processadores
Conjunto *conjunto; // contem todos os verticesCompleto

long totalArestasGlobal, totalVerticesGlobal; // numero total de arestasLocal e verticesCompleto

void debug(char *format, ...);

int comparacaoArestas(const void *aresta1, const void *aresta2);


long hash(long v) {
    return v % totalVerticesGlobal;
}

void printVertices() {
    debug("Total vertice local: %ld\n", quantidadeVerticesLocal);
    for (int i = 0; i < totalVerticesGlobal; ++i) {
        if (verticesLocal[i].v != -1) {
            debug("Vertice :%ld (%ld)\n", verticesLocal[i].v, verticesLocal[i].grau);
        }
    }
    printf("\n");
}

void printArestas() {
    debug("Minhas arestasLocal: %ld\n", quantidadeArestasLocal);
    for (int i = 0; i < quantidadeArestasLocal; ++i) {
        debug("(%ld  %ld)  =>  %ld\n", arestasLocal[i].u, arestasLocal[i].v, arestasLocal[i].peso);
    }
    printf("\n");
}

void printAGM() {
    long pesoTotal = 0;
    debug("Quantida de arestas da AGM: %ld\n", quantidadeArestasAGMLocal);
    for (int i = 0; i < quantidadeArestasAGMLocal; ++i) {
        debug("(%ld  %ld)  =>  %ld\n", arvoreGeradoraMinimaLocal[i].u, arvoreGeradoraMinimaLocal[i].v,
              arvoreGeradoraMinimaLocal[i].peso);
        pesoTotal += arvoreGeradoraMinimaLocal[i].peso;
    }
    debug("Peso parcial/total: %ld\n", pesoTotal);
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
    printf("(%d) %s", rank, mensagem);

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
    MPI_Type_contiguous(3, MPI_LONG, &MPI_Aresta);
    MPI_Type_commit(&MPI_Aresta);

    MPI_Type_contiguous(2, MPI_LONG, &MPI_Vertice);
    MPI_Type_commit(&MPI_Vertice);
}

void finalizacao() {
    MPI_Type_free(&MPI_Aresta);
    MPI_Type_free(&MPI_Vertice);


    if (rank == ROOT) {
        free(arestasCompleto);
    }
    free(arestasLocal);
    free(mergedAGM);
    free(recvAGM);
    free(verticesCompleto);
    free(verticesLocal);
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
    fscanf(arquivo, "%ld", &totalVerticesGlobal);
    fscanf(arquivo, "%ld", &totalArestasGlobal);

}

// distribuir aresta entre os processos
void distribuirArestasPorProcessador() {
    // Processo mestre lê todas as arestasLocal do arquivo
    if (rank == ROOT) {
        arestasCompleto = malloc(totalArestasGlobal * sizeof(Aresta));
        verticesCompleto = calloc(totalVerticesGlobal, sizeof(Vertices));
        for (long i = 0; i < totalArestasGlobal; i++) {
            fscanf(arquivo, "%ld %ld %ld", &arestasCompleto[i].u, &arestasCompleto[i].v, &arestasCompleto[i].peso);
            // total de verticesCompleto e seus graus
            verticesCompleto[hash(arestasCompleto[i].u)].v = arestasCompleto[i].u;
            verticesCompleto[hash(arestasCompleto[i].u)].grau += 1;

            verticesCompleto[hash(arestasCompleto[i].v)].v = arestasCompleto[i].v;
            verticesCompleto[hash(arestasCompleto[i].v)].grau += 1;
        }

        fclose(arquivo);
    }

    // Calcula a quantidade de arestasLocal para cada processo
    int *quantidadesArestasPorRank = malloc(size * sizeof(int));
    int resto = totalArestasGlobal % size;
    int quantidadeBase = totalArestasGlobal / size;

    // Ultimo processo fica com o restante das arestasLocal que sobraram
    for (int i = 0; i < size; i++) {
        quantidadesArestasPorRank[i] = quantidadeBase;
        if (i == size - 1) {
            quantidadesArestasPorRank[i] += resto;
        }
    }
    // Calcula o deslocamento de cada processo
    int *deslocamentos = malloc(size * sizeof(int));
    deslocamentos[0] = 0;

    for (int i = 1; i < size; i++) {
        deslocamentos[i] = deslocamentos[i - 1] + quantidadesArestasPorRank[i - 1];
    }

    // Calcula a quantidade de arestasLocal que cada processo irá receber
    quantidadeArestasLocal = quantidadesArestasPorRank[rank];

    arestasLocal = malloc(quantidadeArestasLocal * sizeof(Aresta));

    if (arestasLocal == NULL) {
        char *mensagem = NULL;
        sprintf(mensagem, "[ERRO] Erro ao alocar as arestas locais (%d)\n", rank);
        if (rank == 0) {
            free(arestasCompleto);
            free(verticesCompleto);
        }
        abortProgram(mensagem);
    }

    // Distribui as arestasLocal para cada processo
    MPI_Scatterv(arestasCompleto, quantidadesArestasPorRank, deslocamentos, MPI_Aresta, arestasLocal,
                 quantidadeArestasLocal, MPI_Aresta, ROOT, MPI_COMM_WORLD);


    // Verificar os verticesLocal disponíveis e o grau deles;
    verticesLocal = calloc(totalVerticesGlobal, sizeof(Vertices));
    mergedAGM = malloc(2 * (totalVerticesGlobal - 1) * sizeof(Aresta));
    arvoreGeradoraMinimaLocal = malloc((totalVerticesGlobal - 1) * sizeof(Aresta));

    if (verticesLocal == NULL) {
        char *mensagem = NULL;
        sprintf(mensagem, "[ERRO] Erro ao alocar os vertices locais (%d)\n", rank);
        if (rank == 0) {
            free(arestasCompleto);
            free(verticesCompleto);
        }
        abortProgram(mensagem);
    }


    debug("Aresta %ld: %ld %ld %ld\n", quantidadeArestasLocal, arestasLocal[quantidadeArestasLocal - 1].u,
          arestasLocal[quantidadeArestasLocal - 1].v, arestasLocal[quantidadeArestasLocal - 1].peso);

    // Verificar os verticesLocal disponíveis e o grau deles;
    for (long i = 0; i < quantidadeArestasLocal; ++i) {

        if (verticesLocal[hash(arestasLocal[i].v)].v == 0) {
            quantidadeVerticesLocal++;
        }
        verticesLocal[hash(arestasLocal[i].v)].grau += 1;
        verticesLocal[hash(arestasLocal[i].v)].v = arestasLocal[i].v;

        if (verticesLocal[hash(arestasLocal[i].u)].v == 0) {
            quantidadeVerticesLocal++;
        }
        verticesLocal[hash(arestasLocal[i].u)].v = arestasLocal[i].u;
        verticesLocal[hash(arestasLocal[i].u)].grau += 1;

    }

    free(quantidadesArestasPorRank);
    free(deslocamentos);

    MPI_Barrier(MPI_COMM_WORLD);
}

void inserirAresta(Aresta novaAresta, Aresta **arestas, long *quantidadeAtual) {
    Aresta *aux = malloc((*quantidadeAtual + 1) * sizeof(Aresta));

    if (aux == NULL) {
        char *mensagem = NULL;
        sprintf(mensagem, "[ERRO] Erro ao inserir novaAresta (%d)\n", rank);
        if (rank == 0) {
            free(arestasCompleto);
            free(verticesCompleto);
        }
        free(arestasLocal);
        free(verticesLocal);
        abortProgram(mensagem);
    }

    memcpy(aux, *arestas, *quantidadeAtual * sizeof(Aresta));

    aux[*quantidadeAtual].v = novaAresta.v;
    aux[*quantidadeAtual].u = novaAresta.u;
    aux[*quantidadeAtual].peso = novaAresta.peso;
    free(*arestas);
    *arestas = aux;
    *quantidadeAtual += 1;
}

bool isInseridaRecentemente(const Aresta *arestasRecentes, long tamanho, Aresta arestaNova) {
    for (long i = 0; i < tamanho; ++i) {
        if (arestasRecentes[i].v == arestaNova.v && arestasRecentes[i].u == arestaNova.u &&
            arestasRecentes[i].peso == arestaNova.peso) {
            return true;
        }
    }
    return false;
}

void encontrarArestasFaltantes() {
    // Array para armazenar a quantidade de arestas de cada processo
    long *quantidadeArestasRanks = malloc(size * sizeof(long));

    // Coletar a quantidade de arestas e arestasLocal de cada processo no processo raiz
    MPI_Gather(&quantidadeArestasLocal, 1, MPI_LONG, quantidadeArestasRanks, 1, MPI_LONG, ROOT, MPI_COMM_WORLD);

    // Transmitir a quantidade de vértices e arestasLocal de cada processo para todos os outros processos
    MPI_Bcast(quantidadeArestasRanks, size, MPI_LONG, ROOT, MPI_COMM_WORLD);

    // matriz que possui as arestas de cada rank
    // cada linha são as arestas de um rank respectivamente
    Aresta **arestasRanks = malloc(size * sizeof(Aresta *));

    /*Recebendo as arestasLocal de cada rank*/
    for (int i = 0; i < size; i++) {
        if (i != rank) {
            arestasRanks[i] = malloc(quantidadeArestasRanks[i] * sizeof(Aresta));
            MPI_Send(arestasLocal, quantidadeArestasLocal, MPI_Aresta, i, 0, MPI_COMM_WORLD);
            MPI_Recv(arestasRanks[i], quantidadeArestasRanks[i], MPI_Aresta, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    }

    // esse vetor será utilizado para comprar se o vertice local já atingiu a quantidade maxima de ligações
    Vertices *todosVertices = NULL; // todos as vertices do arquivo
    if (rank == ROOT) {
        for (int i = 0; i < size; ++i) {
            if (i != rank) {
                MPI_Send(verticesCompleto, totalVerticesGlobal, MPI_Vertice, i, 0, MPI_COMM_WORLD);
            }
        }
    } else {
        // o rank ROOT nao aloca esse vetor pois ele ja tem essas informacoes no verticesCompleto
        todosVertices = malloc(totalVerticesGlobal * sizeof(Vertices));
        MPI_Recv(todosVertices, totalVerticesGlobal, MPI_Vertice, ROOT, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }


    long quantInseridasRecent = 1;
    long verticesPercorridos = 0; // utilizado para verificar se já percorreu a quantidade de vertices locais, evitando iterações desnecessárias
    Aresta *inseridasRecente = malloc(
            quantInseridasRecent *
            sizeof(Aresta)); // utilizado para verificar se a mesma aresta já foi inserida recentemente, evitando arestas repetidas
    for (long i = 0; i < totalVerticesGlobal && verticesPercorridos < quantidadeVerticesLocal; ++i) {
        // percorre cada vertice local verificando as conexoes faltantes
        if (verticesLocal[i].v != -1) {
            Vertices verticeLocal = verticesLocal[i];
            // caso o rank for o ROOT verifica o grau no vetor verticesCompleto ou inves do todosVertices
            Vertices verticeTodos =
                    rank != ROOT ? todosVertices[hash(verticeLocal.v)] : verticesCompleto[hash(verticeLocal.v)];

            bool possuiGrauMaximo = false; // caso o vertice possuir o grau maximo, sai dos loops e vai pro proxximo vertice
            // percorre as arestas de cada processo, caso o grau maximo do vertice nao seja atingido
            for (int processo = 0; processo < size && !possuiGrauMaximo; ++processo) {
                if (processo != rank) {
                    for (long k = 0; k < quantidadeArestasRanks[processo] && !possuiGrauMaximo; ++k) {
                        if (verticeLocal.grau == verticeTodos.grau) {
                            possuiGrauMaximo = true;
                        } else {
                            Aresta arestaProcesso = arestasRanks[processo][k];
                            // Encontrou uma aresta faltante para o vertice já que as arestas de cada processo sao disjuntas
                            // isInseridaRecentemente é utilizado para evitar triangulações (evitar inserir arestas que já existem).
                            if ((arestaProcesso.v == verticeLocal.v || arestaProcesso.u == verticeLocal.v) &&
                                !isInseridaRecentemente(inseridasRecente, quantInseridasRecent, arestaProcesso)) {
//                                debug("(%ld %ld) => %ld ARESTA FALTANTE do vertice %ld\n", arestaProcesso.u, arestaProcesso.v, arestaProcesso.peso, verticeLocal.v);
                                inserirAresta(arestaProcesso, &arestasLocal, &quantidadeArestasLocal);
                                inserirAresta(arestaProcesso, &inseridasRecente, &quantInseridasRecent);
                                verticeLocal.grau++;
                                verticesLocal[i].grau = verticeLocal.grau;
                            }
                        }
                    }
                }
            }
            verticesPercorridos++;
        }
    }

    // Liberar a memória alocada
    for (int i = 0; i < size; ++i) {
        if (i != rank) {
            free(arestasRanks[i]);
        }
    }
    free(arestasRanks);
    free(inseridasRecente);
    free(todosVertices);
    free(quantidadeArestasRanks);

    // garantir que todos processos encontrem as conexoes faltantes antes de realizar o calculo da agm
    MPI_Barrier(MPI_COMM_WORLD);
}


void encontrarAGMLocal() {
    qsort(arestasLocal, quantidadeArestasLocal, sizeof(Aresta), comparacaoArestas);

    // Criar estrutura do Union Find
    // Raiz = NULL e Rank = 0
    free(conjunto);
    conjunto = calloc(totalVerticesGlobal, sizeof(Conjunto));

    quantidadeArestasAGMLocal = 0;
    quantArestasMergedAGM = 0;
    long indexUsados = 0;
    for (long i = 0; i < quantidadeArestasLocal; ++i) {
        Aresta *arestaMinima = &arestasLocal[indexUsados++];

        Conjunto *raizV = find(&conjunto[arestaMinima->v]);
        Conjunto *raizU = find(&conjunto[arestaMinima->u]);
        if (raizV != raizU) {
            arvoreGeradoraMinimaLocal[quantidadeArestasAGMLocal++] = *arestaMinima;
            mergedAGM[quantArestasMergedAGM++] = *arestaMinima;
            unionConjunto(raizV, raizU);
        }
    }
}

void encontrarAGMParalelo() {
    qsort(mergedAGM, quantArestasMergedAGM, sizeof(Aresta), comparacaoArestas);

    free(conjunto);
    conjunto = calloc(totalVerticesGlobal, sizeof(Conjunto));

    quantidadeArestasAGMLocal = 0;
    long indicesArestaUsados = 0;

    for (int i = 0; i < quantArestasMergedAGM; ++i) {
        Aresta *arestaMinima = &mergedAGM[indicesArestaUsados++];

        Conjunto *raizV = find(&conjunto[arestaMinima->v]);
        Conjunto *raizU = find(&conjunto[arestaMinima->u]);

        if (raizV != raizU) {
            arvoreGeradoraMinimaLocal[quantidadeArestasAGMLocal++] = *arestaMinima;

            unionConjunto(raizV, raizU);
        }
    }

    quantArestasMergedAGM = 0;
    for (int i = 0; i < quantidadeArestasAGMLocal; ++i) {
        Aresta arestaMerged = arvoreGeradoraMinimaLocal[i];
        mergedAGM[quantArestasMergedAGM++] = arestaMerged;
    }
}

int main(int argc, char **argv) {
    inicializacao(argc, argv);

    if ((size & (size - 1)) != 0) {
        abortProgram("[ERRO] O numero de processadores deve ser uma potencia de 2\n");
    }

    if (argc != 2) {
        if (rank == ROOT) {
            abortProgram("\n[ERRO] Arquivo nao encontrado.\n");
        }
    }


    obterArestasVertices(argv[1]);

    if (totalVerticesGlobal / size < 2) {
        abortProgram("[ERRO] O Numero de vertices por processador deve ser no minimo 2\n");
        fclose(arquivo);
    }

    // distribui o conjunto de arestas entre os ranks
    distribuirArestasPorProcessador();


    // encontra as conexoes faltantes para cada vértice.
    // Nao é preciso esse processamento se tiver apenas um processador
    /*if (size > 1) {
        encontrarArestasFaltantes();
    }

    if (size == 1) {
        encontrarAGMLocal();

        printAGM();
    } else {
        int processadores = size;
        int it = 0;
        MPI_Status status;
        int source;
        int destination;

        recvAGM = malloc((totalVerticesGlobal - 1) * sizeof(Aresta));

        encontrarAGMLocal();

        while (processadores > 1) {

            if ((rank / (int) pow(2, it)) % 2 != 0) {
                destination = (rank - (int) pow(2, it)); // rank of destination

                MPI_Send(arvoreGeradoraMinimaLocal, quantidadeArestasAGMLocal, MPI_Aresta, destination, 0, MPI_COMM_WORLD);
                break;
            } else {
                source = (rank + (int) pow(2, it)); // rank of source

                MPI_Recv(recvAGM, totalVerticesGlobal - 1, MPI_Aresta, source, 0, MPI_COMM_WORLD, &status);
                MPI_Get_count(&status, MPI_Aresta, &quantArestasRecvAGM);
            }

            for (int i = 0; i < quantArestasRecvAGM; ++i) {
                Aresta arestaRecv = recvAGM[i];
                mergedAGM[quantArestasMergedAGM++] = arestaRecv;
            }

            encontrarAGMParalelo();

            processadores /= 2;
            it++;

        }
        if (rank == ROOT) {
            printAGM();
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
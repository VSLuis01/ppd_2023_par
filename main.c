#include <stdio.h>
#include <stdlib.h>
#include <mpi/mpi.h>


typedef struct Vertice {
    long destino;
    long peso;
    struct Vertice* prox;
} Vertice;

typedef struct ListaAdj {
    Vertice* cabeca;
} ListaAdj;

Vertice* novoNo(long destino, long peso) {
    Vertice* no = (Vertice*)malloc(sizeof(Vertice));
    no->destino = destino;
    no->peso = peso;
    no->prox = NULL;
    return no;
}

void adicionarAresta(ListaAdj* lista, long destino, long peso) {
    Vertice* novo = novoNo(destino, peso);
    novo->prox = lista->cabeca;
    lista->cabeca = novo;
}

ListaAdj* lerArquivo(const char* arquivo, long* numVertices) {
    FILE* file = fopen(arquivo, "r");
    if (file == NULL) {
        printf("Erro ao abrir o arquivo.\n");
        return NULL;
    }

    if (fscanf(file, "%ld\n", numVertices) != 1) {
        printf("Erro ao ler a quantidade de vértices do arquivo.\n");
        fclose(file);
        return NULL;
    }

    ListaAdj* listaAdj = (ListaAdj*)malloc((*numVertices) * sizeof(ListaAdj));

    long numArestas;
    if (fscanf(file, "%ld\n", &numArestas) != 1) {
        printf("Erro ao ler a quantidade de arestas do arquivo.\n");
        fclose(file);
        free(listaAdj);
        return NULL;
    }

    for (long i = 0; i < *numVertices; i++) {
        listaAdj[i].cabeca = NULL;
    }

    for (long i = 0; i < numArestas; i++) {
        long no, aresta, peso;
        if (fscanf(file, "%ld %ld %ld\n", &no, &aresta, &peso) != 3) {
            printf("Erro ao ler a linha %ld do arquivo.\n", i + 3);
            fclose(file);
            for (long j = 0; j < *numVertices; j++) {
                Vertice* atual = listaAdj[j].cabeca;
                while (atual != NULL) {
                    Vertice* prox = atual->prox;
                    free(atual);
                    atual = prox;
                }
            }
            free(listaAdj);
            return NULL;
        }

        adicionarAresta(&listaAdj[no], aresta, peso);
    }

    fclose(file);
    return listaAdj;
}

void imprimirGrafo(ListaAdj* grafo, long numVertices) {
    for (long i = 0; i < numVertices; i++) {
        Vertice* atual = grafo[i].cabeca;
        printf("Vértice %ld: ", i);
        while (atual != NULL) {
            printf("(%ld, %ld) ", atual->destino, atual->peso);
            atual = atual->prox;
        }
        printf("\n");
    }
}

void liberarGrafo(ListaAdj* grafo, long numVertices) {
    for (long i = 0; i < numVertices; i++) {
        Vertice* atual = grafo[i].cabeca;
        while (atual != NULL) {
            Vertice* prox = atual->prox;
            free(atual);
            atual = prox;
        }
    }
    free(grafo);
}

int main() {
    long numVertices;
    ListaAdj *grafo = lerArquivo("dados_entrada_paralelo.txt", &numVertices);

//    imprimirGrafo(grafo, numVertices);

    liberarGrafo(grafo, numVertices);

    return 0;
}

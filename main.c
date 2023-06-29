#include <stdio.h>
#include <stdlib.h>

typedef struct No {
    long vertice;
    long peso;
    struct No* proximo;
} No;

typedef struct GrafoAdjPonderado {
    long numVertices;
    long numArestas;
    No** lista;
} GrafoAdjPonderado;

// Função para criar um novo nó da lista de adjacência
No* criarNo(long vertice, long peso) {
    No* novoNo = (No*)malloc(sizeof(No));
    novoNo->vertice = vertice;
    novoNo->peso = peso;
    novoNo->proximo = NULL;
    return novoNo;
}

// Função para criar um grafo ponderado com o número especificado de vértices
GrafoAdjPonderado* criarGrafo(long numVertices, long numArestas) {
    GrafoAdjPonderado* grafo = (GrafoAdjPonderado*)malloc(sizeof(GrafoAdjPonderado));
    grafo->numVertices = numVertices;
    grafo->numArestas = numArestas;

    // Cria uma lista de adjacência para cada vértice
    grafo->lista = (No**)malloc(numVertices * sizeof(No*));
    long i;
    for (i = 0; i < numVertices; i++) {
        grafo->lista[i] = NULL;
    }

    return grafo;
}

// Função para adicionar uma aresta ponderada ao grafo
void adicionarAresta(GrafoAdjPonderado* grafo, long verticeOrigem, long verticeDestino, long peso) {
    // Adiciona uma aresta ponderada do vérticeOrigem para o vérticeDestino

    // Cria um novo nó para o vérticeDestino com o peso da aresta
    No* novoNo = criarNo(verticeDestino, peso);

    // Insere o nó no início da lista de adjacência do vérticeOrigem
    novoNo->proximo = grafo->lista[verticeOrigem];
    grafo->lista[verticeOrigem] = novoNo;

    // Como o grafo é não direcionado, também adicionamos uma aresta do vérticeDestino para o vérticeOrigem
    novoNo = criarNo(verticeOrigem, peso);
    novoNo->proximo = grafo->lista[verticeDestino];
    grafo->lista[verticeDestino] = novoNo;
}

// Função para exibir o grafo ponderado
void printGrafo(GrafoAdjPonderado* grafo) {
    long i;
    printf("Número de vértices: %ld, número de arestas: %ld\n", grafo->numVertices, grafo->numArestas);
    printf("Lista de adjacência do grafo:\n");
    for (i = 0; i < grafo->numVertices; i++) {
        No* atual = grafo->lista[i];
        printf("Vértice %ld: ", i);
        while (atual != NULL) {
            printf("(%ld, peso-%ld) -> ", atual->vertice, atual->peso);
            atual = atual->proximo;
        }
        printf("NULL\n");
    }
}

// Função para liberar a memória alocada pelo grafo
void deleteGrafo(GrafoAdjPonderado* grafo) {
    if (grafo) {
        long i;
        for (i = 0; i < grafo->numVertices; i++) {
            No* atual = grafo->lista[i];
            while (atual != NULL) {
                No* temp = atual;
                atual = atual->proximo;
                free(temp);
            }
        }
        free(grafo->lista);
        free(grafo);
    }
}

GrafoAdjPonderado* lerArquivoGrafo(const char* nomeArquivo) {
    FILE* arquivo = fopen(nomeArquivo, "r");
    if (arquivo == NULL) {
        printf("Erro ao abrir o arquivo.\n");
        return NULL;
    }

    // Leitura do número de nós e arestas
    long numVertices, numArestas;
    fscanf(arquivo, "%ld%ld", &numVertices, &numArestas);  // Removido o caractere '\n' após %ld


    // Criação do grafo com o número de nós lido
    GrafoAdjPonderado* grafo = criarGrafo(numVertices, numArestas);

    // Leitura das arestas e adição ao grafo
    long verticeOrigem, verticeDestino, peso;
    while (fscanf(arquivo, "%ld%ld%ld", &verticeOrigem, &verticeDestino, &peso) != EOF) {
        adicionarAresta(grafo, verticeOrigem, verticeDestino, peso);
    }

    fclose(arquivo);
    return grafo;
}


int main() {
    const char* nomeArquivo = "dados_entrada_paralelo.txt";
    GrafoAdjPonderado* grafo = lerArquivoGrafo(nomeArquivo);

    if (grafo != NULL) {
        printGrafo(grafo);
        deleteGrafo(grafo);
    }

    return 0;
}
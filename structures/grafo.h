//
// Created by luis on 26/05/23.
//

#ifndef PPD_2023_SEQ_GRAFO_H
#define PPD_2023_SEQ_GRAFO_H
typedef struct {
    int v; /*Vértice 1*/
    int w; /*Vértice 2*/
    int peso;
} Aresta;

typedef struct {
    int v;
    int grau;
    int componente;
} Vertice;

typedef struct {
    int V; /*Número de vertices no grafo*/
    int A; /*Número de arestas no grafo*/
    Vertice *vertices; /*Conjunto de vertices*/
    Aresta *arestas; /*Conjunto de arestas*/
} Grafo;

/**
 * @brief Constrói uma arvore geradora mínima a partir do grafo de entrada.
 * @param grafo
 * @return Arvore geradora minima F (V, E')
 */
Grafo *arvoreGeradoraMinima(Grafo grafo);

/**
 * @brief Inicializa o grafo. Aloca memória para o grafo e para o conjunto de arestas e inicializa o número de vértices e arestas
 * @return Ponteiro para o grafo
 */
Grafo *inicializaGrafo();

/**
 * @brief Inicializa o grafo com um número de vertices
 * @param numVertices
 * @return
 */
Grafo *inicializaGrafoComVertice(int numVertices);

/**
 * Insere a aresta de forma dinamica.
 * @param grafo
 * @param v
 * @param w
 * @param peso
 * @param somarGrau flag para somar o grau do vertice
 * @return 1 caso inserido com sucesso, 0 caso contrario
 */
int inserirAresta(Grafo *grafo, int v, int w, int peso, int somarGrau);

/**
 * @brief Insere vertices de forma dinamica.
 * @attention Essa funcao eh melhor utilizada quando o grafo eh iniciado com a funcao Grafo *inicializaGrafo();
 * @attention Nessa funcao, os indices do conjunto de vertices NAO sao iguais ao valor do vertice.
 * @param grafo ponteiro para o grafo
 * @param v vertice
 * @param w vertice
 */
void inserirVertice(Grafo *grafo, int v);

/**
 * @brief Insere o vertice diretamente no seu indice,
 * @attention Essa funcao so funciona quando o grafo eh iniciado com a funcao Grafo* inicializaGrafoComVertice(int numVertices);
 * @attention Nessa funcao, os indices do conjunto de vertices sao iguais ao (valor do vertice - 1).
 * @param grafo
 * @param v
 */
void inserirVerticeDireto(Grafo *grafo, int v);

/**
 * @brief Libera a memória alocada para o grafo e para o conjunto de arestas
 * @param grafo Ponteiro para o grafo
 */
void deleteGrafo(Grafo *grafo);


#endif //PPD_2023_SEQ_GRAFO_H

//
// Created by luis on 27/05/23.
//

#include "sdl_utils.h"
#include <math.h>
#include <unistd.h>
#include <string.h>
#include <linux/limits.h>


#define FONT_SIZE 12

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Event event;

TTF_Font *fonte;
SDL_Surface *superficieTexto;
SDL_Texture *texturaTexto;

typedef struct {
    int v; /*Vértice 1*/
    int w; /*Vértice 2*/
    int peso;
} Aresta_SDL;

int seed = 16;

/**
 * @brief Inicializa a janela do SDL (window e renderer)
 * @param width comprimento da janela
 * @param height altura da janela
 */
void sdlInitWindow(int width, int height) {
    // Inicialização do SDL
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    // Criação da janela
    window = SDL_CreateWindow("Arvore Geradora Mínima Sequencial", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, 0);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
}

/**
 * @brief Destrói a janela do SDL (renderer e window)
 */
void sdlDestroyWindow() {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

/**
 * @brief Verifica se o evento é de fechar a janela.
 * @return
 */
int sdlEventClosedWindow() {
    return event.type == SDL_QUIT;
}

int sdlEventKeyDown() {
    return event.type == SDL_KEYDOWN;
}

SDL_KeyCode sdlGetKeyCode() {
    return event.key.keysym.sym;
}

void sdlClearRender() {
    // Limpa o renderer
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
}

void sdlDraw() {
    // Atualiza a janela
    SDL_RenderPresent(renderer);
}

/**
 * @brief Verifica se há eventos na fila do SDL
 * @return 1 se há eventos, 0 caso contrário.
 */
int sdlPullEvent() {
    return SDL_PollEvent(&event);
}

void destroyTexto() {
    // Limpar recursos
    SDL_FreeSurface(superficieTexto);
    SDL_DestroyTexture(texturaTexto);
    TTF_CloseFont(fonte);
}

int initTexto(const char *texto, int tamanhoFonte) {
    // Renderizar texto
    char pathFont[PATH_MAX]; // Tamanho máximo do caminho
    if (getcwd(pathFont, sizeof(pathFont)) != NULL) {
        strcat(pathFont, "/opensans.ttf");
        fonte = TTF_OpenFont(pathFont, tamanhoFonte);

        if (fonte == NULL) {
            perror("Erro ao iniciar fonte");
            return 0;
        }

        SDL_Color corTexto = {255, 255, 255, 255};
        superficieTexto = TTF_RenderText_Solid(fonte, texto, corTexto);

        if (superficieTexto == NULL) {
            perror("Erro ao criar textura");
            return 0;
        }

        texturaTexto = SDL_CreateTextureFromSurface(renderer, superficieTexto);

        return 1;
    } else {
        perror("Erro ao obter o diretório atual");
        return 0;
    }

}

/**
 * @brief Renderiza um círculo no renderer
 * @param pos posicao do centro do círculo na janela
 * @param raio raio do circulo
 * @param color cor do circulo
 */
void sdlRenderizarCirculo(SDL_Point pos, int raio, SDL_Colour color, const char *label) {
    // Configura a cor do renderer
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    int x, y;
    for (y = -raio; y <= raio; y++) {
        for (x = -raio; x <= raio; x++) {
            // Verifica se o ponto está dentro do círculo
            if (x * x + y * y <= raio * raio) {
                // Desenha o ponto no renderer
                SDL_RenderDrawPoint(renderer, pos.x + x, pos.y + y);
            }
        }
    }

    if (initTexto(label, FONT_SIZE)) {
        // Definir a posição do label
        int posX = pos.x - superficieTexto->w / 2;
        int posY = pos.y - superficieTexto->h / 2;

        // Renderizar o label
        SDL_Rect destinoTexto = {posX, posY, superficieTexto->w, superficieTexto->h};
        SDL_RenderCopy(renderer, texturaTexto, NULL, &destinoTexto);
    }

    destroyTexto();
}

/**
 * @brief Renderiza uma linha no renderer
 * @param p1 ponto inicial da linha (extremo 1)
 * @param p2 ponto final da linha (extremo 2)
 * @param color cor da linha
 */
void sdlRenderizarLinha(SDL_Point p1, SDL_Point p2, SDL_Colour color, const char *label) {
    // Configura a cor do renderer
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);


    // Desenha a linha utilizando SDL_RenderDrawLine
    SDL_RenderDrawLine(renderer, p1.x, p1.y, p2.x, p2.y);

    // Calcula a posição central da linha
    float centerX = (p1.x + p2.x) / 2;
    float centerY = (p1.y + p2.y) / 2;

    if (initTexto(label, FONT_SIZE)) {
        int textoWidth, textoHeight;
        TTF_SizeText(fonte, label, &textoWidth, &textoHeight);

        // Define o deslocamento horizontal e vertical do texto
        int offsetX = textoWidth / 2;
        int offsetY = textoHeight / 2;

        // Renderiza o texto centralizado com deslocamento
        SDL_Rect destinoTexto = {centerX - offsetX, centerY - offsetY, textoWidth, textoHeight};
        SDL_RenderCopy(renderer, texturaTexto, NULL, &destinoTexto);
    }
    destroyTexto();
}

/**
 * Função para gerar um valor unico para cada vertice
 * @param x
 * @return
 */
int hash(int x) {
    x = ((x >> seed) ^ x) * 0x45d9f3b;
    x = ((x >> seed) ^ x) * 0x45d9f3b;
    x = (x >> seed) ^ x;
    return x;
}

void desenharGrafo(const void* arestas, int numArestas, int numVertices) {
    Aresta_SDL *aresta = (Aresta_SDL*) arestas;

    // Determine a largura e altura da janela
    int larguraJanela, alturaJanela;
    int raioVertice = 10;
    SDL_GetRendererOutputSize(renderer, &larguraJanela, &alturaJanela);


    // Calcule o raio máximo permitido com base nas dimensões da janela
    int raioMaximo = (larguraJanela < alturaJanela ? larguraJanela : alturaJanela) / 3;

    // Calcule as coordenadas do centro da janela
    int centerX = larguraJanela / 2;
    int centerY = alturaJanela / 2;

    for (int i = 0; i < numArestas; ++i) {
        // Gere um raio aleatório para o vértice atual
        int raioV = (hash(aresta[i].v) % raioMaximo) + 150;
        int raioW = (hash(aresta[i].w) % raioMaximo) + 150;

        // Calcule o ângulo de separação entre os vértices
        double anguloSeparacao = 2 * M_PI / numVertices;

        // Calcule o ângulo atual para o vértice atual

        //Desenhar vertice V
        double anguloV = aresta[i].v * anguloSeparacao;
        char labelV[10];
        sprintf(labelV, "%d", aresta[i].v);

        // Calcule as coordenadas do vértice com base no raio e no ângulo
        int posXVerticeV = centerX + raioV * cos(anguloV);
        int posYVerticeV = centerY + raioV * sin(anguloV);


        //Desenhar vertice W
        double anguloW = aresta[i].w * anguloSeparacao;
        char labelW[10];
        sprintf(labelW, "%d", aresta[i].w);

        // Calcule as coordenadas do vértice com base no raio e no ângulo
        int posXVerticeW = centerX + raioW * cos(anguloW);
        int posYVerticeW = centerY + raioW * sin(anguloW);


        //Desenhar aresta
        char labelAresta[10];
        sprintf(labelAresta, "%d", aresta[i].peso);
        sdlRenderizarLinha((SDL_Point) {posXVerticeV, posYVerticeV},
                           (SDL_Point) {posXVerticeW, posYVerticeW},
                           (SDL_Colour) {0, 0, 255, 255},
                           labelAresta);


        // Desenhe o vértice V na posição calculada
        sdlRenderizarCirculo((SDL_Point) {posXVerticeV, posYVerticeV}, raioVertice, (SDL_Colour) {255, 0, 0, 255}, labelV);

        // Desenhe o vértice W na posição calculada
        sdlRenderizarCirculo((SDL_Point) {posXVerticeW, posYVerticeW}, raioVertice, (SDL_Colour) {255, 0, 0, 255}, labelW);
    }
}

void desenharGrafoMatrizAdj(int **matrizAdj, int numVertices) {

}

void changeSeed(int sum) {
    seed += sum;
}

void setSeed(int newSeed) {
    seed = newSeed;
}




//
// Created by luis on 27/05/23.
//

#ifndef PPD_2023_SEQ_SDL_UTILS_H
#define PPD_2023_SEQ_SDL_UTILS_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "../structures/grafo.h"

// Função de renderização do grafo
typedef void (*RenderFunction)(Grafo*);

/**
 * @brief Inicializa a janela do SDL (window e renderer)
 * @param width comprimento da janela
 * @param height altura da janela
 */
void sdlInitWindow(int width, int height);

/**
 * @brief Destrói a janela do SDL (renderer e window)
 */
void sdlDestroyWindow();

/**
 * @brief Verifica se o evento é de fechar a janela.
 * @return
 */
int sdlEventClosedWindow();


int sdlEventKeyDown();

SDL_KeyCode sdlGetKeyCode();

void changeSeed(int sum);

void setSeed(int seed);

/**
 * @brief Limpa o renderer
 */
void sdlClearRender();

/**
 * @brief Atualiza a janela
 */
void sdlDraw();

/**
 * @brief Verifica se há eventos na fila do SDL
 * @return 1 se há eventos, 0 caso contrário.
 */
int sdlPullEvent();

/**
 * @brief Renderiza um círculo no renderer
 * @param pos posicao do centro do círculo na janela
 * @param raio raio do circulo
 * @param color cor do circulo
 */
void sdlRenderizarCirculo(SDL_Point pos, int raio, SDL_Colour color, const char* label);

/**
 * @brief Renderiza uma linha no renderer
 * @param p1 ponto inicial da linha (extremo 1)
 * @param p2 ponto final da linha (extremo 2)
 * @param color cor da linha
 */
void sdlRenderizarLinha(SDL_Point p1, SDL_Point p2, SDL_Colour color, const char* label);

void desenharGrafo(Grafo* grafo);

void renderizarGrafo(Grafo* grafo, RenderFunction renderFunction);

#endif //PPD_2023_SEQ_SDL_UTILS_H

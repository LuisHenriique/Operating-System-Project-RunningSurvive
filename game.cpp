// game.cpp
// Compile: g++ -std=c++17 game.cpp -pthread -lncurses -o semaphore_game
// Run: ./game

#include <iostream>
#include <thread>
#include <semaphore.h>
#include <vector>
#include <mutex>
#include <chrono>
#include <ncurses.h> // Biblioteca para interface textual (TUI)
#include <unistd.h>

using namespace std;

// --- CONFIGURAÇÃO ---
// Tamanho do Mapa: 21 Linhas x 60 Caracteres (+1 para o terminador nulo \0)
const int LIN = 21; 
const int COL = 61; 

// --- RECURSOS COMPARTILHADOS (Shared Resources) ---
// MAPA BASE: Representa o cenário estático.
// O design "Funil" força os jogadores a passarem pela ponte central.
// Isso garante que o conceito de Exclusão Mútua (Semáforo) seja testado.
char base_map[LIN][COL] = {
    "############################################################",
    "#F     #                                            #      #", // F = Bandeira (Alvo) ou Ponto de Partida
    "###### # ########################################## # #### #",
    "#      # #                                        # #      #",
    "# ###### # ###################################### # ###### #",
    "#        #                                        #        #",
    "######## ####################  #################### ########",
    "#                            CC                            #", // Entrada do Funil
    "############################ CC ############################", // PAREDE SÓLIDA
    "#                            CC                            #", // A PONTE (Região Crítica)
    "# ########################## CC ########################## #", // PAREDE SÓLIDA
    "#                            CC                            #", // A PONTE
    "############################ CC ############################", // PAREDE SÓLIDA
    "#                            CC                            #", // Saída da Ponte
    "######## ####################  #################### ########",
    "#        #                                        #        #",
    "# ###### # ###################################### # ###### #",
    "#      # #                                        # #      #",
    "###### # ########################################## # #### #",
    "#      #                                            #     F#", 
    "############################################################"
};

// Visualização atual do mapa (O que é desenhado na tela frame a frame)
char map_view[LIN][COL];

// Estrutura que define o estado de cada jogador
struct Player {
    int x, y;       // Posição atual
    char symbol;    // '1' ou '2'
    char direction; // Direção do movimento ('u', 'd', 'l', 'r')
};

// --- VARIÁVEIS GLOBAIS E SINCRONIZAÇÃO ---
mutex mtx_map;       // Mutex para proteger o desenho do mapa (evita "glitches" visuais)
sem_t sem_RC;        // SEMÁFORO: Controla o acesso à Região Crítica (A Ponte 'C')
bool playing = true; // Variável de controle do loop principal
string winner_msg = "";

// --- FUNÇÕES ---

// Inicializa a biblioteca NCurses (Melhor que system("clear"))
void init_interface() {
    initscr();             // Inicia modo ncurses
    cbreak();              // Desabilita buffer de linha
    noecho();              // Não mostra teclas digitadas
    curs_set(0);           // Oculta o cursor piscante
    nodelay(stdscr, TRUE); // Leitura de teclado não-bloqueante (importante para threads!)
    keypad(stdscr, TRUE);  // Habilita setas do teclado
    start_color();         // Habilita cores
    
    // Definição de Pares de Cores (Texto, Fundo)
    init_pair(1, COLOR_CYAN, COLOR_BLACK);   // Jogador 1
    init_pair(2, COLOR_RED, COLOR_BLACK);    // Jogador 2
    init_pair(3, COLOR_GREEN, COLOR_BLACK);  // Paredes
    init_pair(4, COLOR_YELLOW, COLOR_BLACK); // Região Crítica (Ponte)
    init_pair(5, COLOR_MAGENTA, COLOR_BLACK);// Bandeiras
}

void close_interface() {
    endwin(); // Encerra o ncurses e restaura o terminal
}

// Função responsável por desenhar o mapa na tela
void draw_map() {
    // Bloqueamos o mutex para garantir que o mapa não mude enquanto estamos desenhando
    unique_lock<mutex> lock(mtx_map);
    
    for (int i = 0; i < LIN; i++) {
        for (int j = 0; j < COL; j++) {
            char cell = map_view[i][j];
            
            // Aplica cores baseadas no caractere
            if (cell == '1') attron(COLOR_PAIR(1));
            else if (cell == '2') attron(COLOR_PAIR(2));
            else if (cell == '#') attron(COLOR_PAIR(3));
            else if (cell == 'C') attron(COLOR_PAIR(4));
            else if (cell == 'F') attron(COLOR_PAIR(5));
            
            mvaddch(i, j, cell); // Move cursor e imprime caractere
            
            // Desliga as cores
            attroff(COLOR_PAIR(1));
            attroff(COLOR_PAIR(2));
            attroff(COLOR_PAIR(3));
            attroff(COLOR_PAIR(4));
            attroff(COLOR_PAIR(5));
        }
    }
    refresh(); // Atualiza a tela real
}

// Verifica se o movimento é válido (não é parede nem fora do mapa)
bool allow_move(int nx, int ny) {
    if (nx < 0 || nx >= LIN || ny < 0 || ny >= COL) return false;
    if (map_view[nx][ny] == '#') return false; 
    return true;
}

// Lógica principal de movimento e controle de concorrência
void move_player(Player &p) {
    int nx = p.x;
    int ny = p.y;

    // Calcula nova posição baseada na direção
    switch (p.direction) {
        case 'u': nx--; break;
        case 'd': nx++; break;
        case 'l': ny--; break;
        case 'r': ny++; break;
        default: return;
    }

    if (!allow_move(nx, ny)) return;

    // INÍCIO DA SEÇÃO CRÍTICA DE DADOS (Acesso à matriz map_view)
    unique_lock<mutex> lock(mtx_map);

    // Colisão simples: não pode andar onde o outro jogador está
    if (map_view[nx][ny] == '1' || map_view[nx][ny] == '2') return;

    char next_base = base_map[nx][ny];
    char current_base = base_map[p.x][p.y];

    // --- LÓGICA DO SEMÁFORO (Região Crítica do Jogo) ---
    // Se o jogador tenta entrar na ponte ('C') vindo de fora
    if (next_base == 'C' && current_base != 'C') {
        // Tenta adquirir o semáforo (Down). 
        // sem_trywait não bloqueia a thread, retorna erro se estiver ocupado.
        if (sem_trywait(&sem_RC) != 0) {
            return; // BLOQUEADO: A ponte está ocupada por outro jogador!
        }
    }
    // ----------------------------------------------------

    // VERIFICAÇÃO DE VITÓRIA
    // Jogador 1 vence se chegar na bandeira 'F' do lado direito (nx > 15)
    if (p.symbol == '1' && next_base == 'F' && nx > 15) {
        playing = false;
        winner_msg = "PLAYER 1 WENCEU!";
    }
    // Jogador 2 vence se chegar na bandeira 'F do lado esquerdo (nx < 5)
    else if (p.symbol == '2' && next_base == 'F' && nx < 5) {
        playing = false;
        winner_msg = "PLAYER 2 VENCEU!";
    }

    // Detecta se o jogador acabou de sair da ponte
    bool just_exited_critical = (current_base == 'C' && next_base != 'C');

    // ATUALIZAÇÃO VISUAL
    map_view[p.x][p.y] = current_base; // Restaura o chão original (apaga rastro)
    p.x = nx;
    p.y = ny;
    map_view[p.x][p.y] = p.symbol;     // Desenha jogador na nova posição

    lock.unlock(); 
    // FIM DA SEÇÃO CRÍTICA DE DADOS

    // Libera o semáforo (Up) se o jogador saiu da ponte
    if (just_exited_critical) {
        sem_post(&sem_RC); 
    }
}

// Função executada pelas Threads dos jogadores
void thread_player(Player &p) {
    while (playing) {
        if (p.direction != ' ') {
            move_player(p);
            p.direction = ' '; // Reseta direção para andar passo-a-passo
            this_thread::sleep_for(chrono::milliseconds(100)); // Velocidade do movimento
        } else {
             // Pequeno delay para não consumir 100% da CPU
             this_thread::sleep_for(chrono::milliseconds(10));
        }
    }
}

int main() {
    // 1. Copia o Mapa Base para a Visualização
    for (int i = 0; i < LIN; i++) {
        for (int j = 0; j < COL; j++) {
            map_view[i][j] = base_map[i][j];
        }
    }

    // 2. Inicializa o Semáforo
    // Valor 1 = Mutex (Exclusão Mútua), apenas 1 thread na ponte por vez
    sem_init(&sem_RC, 0, 1); 

    // 3. Configura Jogadores
    // P1 começa no Topo Esquerdo (1, 1)
    Player p1 = {1, 1, '1', ' '}; 
    
    // P2 começa na Base Direita (19, 58)
    Player p2 = {19, 58, '2', ' '}; 

    map_view[p1.x][p1.y] = '1';
    map_view[p2.x][p2.y] = '2';

    // 4. Inicia Interface Gráfica (NCurses)
    init_interface();

    // 5. Inicia as Threads independentes
    thread t1(thread_player, ref(p1));
    thread t2(thread_player, ref(p2));

    // 6. Loop Principal (Captura de Input e Renderização)
    while (playing) {
        draw_map();

        int ch = getch(); // Leitura de teclado (ncurses)
        if (ch != ERR) {
            switch(ch) {
                // Controles P1 (WASD)
                case 'w': p1.direction = 'u'; break;
                case 's': p1.direction = 'd'; break;
                case 'a': p1.direction = 'l'; break;
                case 'd': p1.direction = 'r'; break;
                
                // Controles P2 (Setas)
                case KEY_UP:    p2.direction = 'u'; break;
                case KEY_DOWN:  p2.direction = 'd'; break;
                case KEY_LEFT:  p2.direction = 'l'; break;
                case KEY_RIGHT: p2.direction = 'r'; break;
                
                case 'q': playing = false; break; // Sair
            }
        }
        this_thread::sleep_for(chrono::milliseconds(30)); // ~30 FPS
    }

    // 7. Encerramento e Limpeza
    t1.join();
    t2.join();
    close_interface();
    sem_destroy(&sem_RC);

    cout << "\n===========================\n";
    cout << "   " << winner_msg << "   \n";
    cout << "===========================\n";

    return 0;
}
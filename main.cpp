#include <iostream>
#include <thread>
#include <semaphore.h>
#include <vector>
#include <mutex>
#include <chrono>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
using namespace std;

// Estrutura de posição do jogador
struct Player
{
    int x, y;       // x-linhas y-colunas
    char symbol;    // simbolo do player
    char direction; // ultima direção selecionada
};
/*Prototipos de funções*/
void draw_map();
bool allow_move(int nx, int ny);
int kbhit();
void thread_player(Player &p);
void move_player(Player &p);
void main_game_loop();

// Global shared states
const int LIN = 10;
const int COL = 30;
char map[LIN][COL] = {
    "############################", // # = Parede do jogo
    "#                          #", // C = Região Crítica (Passagem)
    "########  #  ########  #####", // 1 = Jogador 1
    "#      #  #  #      #  #   #", // 1 = Jogador 1
    "#  #####  #  #      #  #   #", // F = Bandeira
    "#         #  #      #  #   #",
    "#  #######   ###############",
    "#         CCCC            F#",
    "############################"};

// Variáveis globais
mutex mtx_map;       //  Evita condição de corrida ao desenhar o mapa
bool playing = true; // Jogo está rodando

void draw_map()
{
    /*Desenha o mapa no terminal*/
    system("clear");
    for (int i = 0; i < LIN; i++)
    {
        cout << map[i] << endl;
    }
}

/*Verifica se é possível se mover para a próxima posição*/
bool allow_move(int nx, int ny)
{
    return (map[nx][ny] != '#' && nx >= 0 && ny >= 0 && nx < LIN && ny < COL);
}

// posições iniciais de cada jogador
Player p1 = {1, 1, '1', ' '};
Player p2 = {1, 26, '2', ' '};

int kbhit()
{
    /*Função responsável por realizar as leituras das teclas no teclado nao bloqueante*/
    struct termios oldt, newt;
    int ch;
    int oldf;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    if (ch != EOF)
    {
        ungetc(ch, stdin);
        return 1;
    }
    return 0;
}

void thread_player(Player &p)
{
    // espera a entrada de comando (lida pela main_function e compartilhada)
    // move o jogador se for válido
    // antes de entrar na rc usa semafora
    // ao sair libera
    while (playing)
    {
        if (p.direction != ' ')
        {
            move_player(p);
            p.direction = ' ';
            this_thread::sleep_for(chrono::milliseconds(100)); // poe para dormir para 1 seg
        }
    }
}
void move_player(Player &p)
{
    int nx = p.x, ny = p.y;
    switch (p.direction)
    {
    case 'u':
        nx--;
        break;
    case 'd':
        nx++;
        break;
    case 'l':
        ny--;
        break;
    case 'r':
        ny++;
        break;
    default:
        break;
    }

    // Testa se o novo movimento está dentro dos limites do jogo
    if (!allow_move(nx, ny))
        return;

    // Mutex aqui para evitar condição de disputa ao desenhar no mapa
    lock_guard<mutex> lock(mtx_map); // trava automaticamente
    if (map[nx][ny] == '1' or map[nx][ny] == '2')
    {
        return;
    }

    // Apaga a posição anterior apenas se era ele mesmo, evita sobreposições entre os jogadores.
    if (map[p.x][p.y] == p.symbol)
        map[p.x][p.y] = ' '; // jogador foi movido, espaço anterior fica em branco.

    if (map[nx][ny] == 'F')
        playing = false; // sinaliza que o jogo se encerrou, temos um vencedor.

    p.x = nx, p.y = ny;       // Atualiza com as novas posições
    map[p.x][p.y] = p.symbol; // move o player
    draw_map();

    // Se o jogo encerrou, imprime o vencedor.
    if (!playing)
    {
        cout << "\nJogador " << p.symbol << " venceu!\n";
    }
}
// Thread principal do jogo
void main_game_loop()
{
    // inicia o mapa
    // cria as threads dos players
    // le teclado dos 2 players
    // atualiza o mapa a cada movimento
    // desenha o mapa na tela
    // encerra quando F é conquistado
    while (playing)
    {
        draw_map();
        if (kbhit())
        {
            char key = getchar();
            switch (key)
            {
            case 'w':
                p1.direction = 'u';
                break;
            case 's':
                p1.direction = 'd';
                break;
            case 'd':
                p1.direction = 'r';
                break;
            case 'a':
                p1.direction = 'l';
                break;
            case 'i':
                p2.direction = 'u';
                break;
            case 'k':
                p2.direction = 'd';
                break;
            case 'j':
                p2.direction = 'l';
                break;
            case 'l':
                p2.direction = 'r';
                break;

            default:
                break;
            }
        }
        this_thread::sleep_for(chrono::milliseconds(50));
    }
}

int main()
{
    // Coloca os jogadores nas posições inicias
    map[p1.x][p1.y] = '1';
    map[p2.x][p2.y] = '2';

    thread player_1(thread_player, ref(p1));
    thread player_2(thread_player, ref(p2));
    thread game(main_game_loop);

    game.join();
    player_1.join();
    player_2.join();

    return 0;
}
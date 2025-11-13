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
char base_map[LIN][COL]; // Será a cópia do labirinto no estado atual, é uma camada fixa que contém os dados originais do mapa
char map[LIN][COL] =
    {
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
mutex mtx_map;       //  Evita condição de corrida ao desenhar o mapa - mutex (Garante que apenas umas das 3 threads opere por vez)
bool playing = true; // Jogo está rodando
sem_t sem_RC;        // semáforo para  Região Crítica (RC)

void draw_map()
{ /*Função responsável por imprimir o mapa no terminal.*/

    system("clear"); // Limpa tela do terminal
    for (int i = 0; i < LIN; i++)
    {
        cout << map[i] << endl;
    }
}

bool allow_move(int nx, int ny)
{
    /*Função responsável por verificar se é possível se mover para a próxima posição indicada.*/
    return (map[nx][ny] != '#' && nx >= 0 && ny >= 0 && nx < LIN && ny < COL);
}

int kbhit()
{
    /*Função responsável por realizar as leituras das teclas no teclado de maneira não bloqueante*/
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

// posições iniciais de cada jogador
Player p1 = {1, 1, '1', ' '};
Player p2 = {1, 26, '2', ' '};

void thread_player(Player &p)
{
    // Verifica se o usuário digitou algo no teclado, com isso será capturado p/ p.direction
    // Coloca e verifica os movimentos, após isso zera o buffer de direção para evitar de mover sem parar
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
    /*Função responsável por movimentar o jodador, se atendando a região crítica.*/

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
        return;
    }

    // Verifica se o novo movimento é permitido
    if (!allow_move(nx, ny))
        return;

    // Garante a exclusão mútua no acesso ao mapa, utilizamos unique_lock pois podemos liberar o mutex e pegar novamente depois
    unique_lock<mutex> lock(mtx_map);

    // evita sobreposição imediata entre os jogadores
    if (map[nx][ny] == '1' || map[nx][ny] == '2')
        return;

    // capture o que havia na célula anterior (base) para checar saída depois
    char prev_base = base_map[p.x][p.y];

    // Verifica se o player está fora da RC e vai entrar no movimento seguinte
    if (base_map[nx][ny] == 'C' && base_map[p.x][p.y] != 'C')
    {
        // Tenta entrar na RC, sem bloquear a thread caso RC esteja ocupada.
        if (sem_trywait(&sem_RC) != 0)
            return; // RC está ocupada, nao deixa entrar, mas ainda tem os movimentos livres, nao congela.
    }

    // restaura a célula anterior ao seu valor de base (C, F ou ' '), essa mudança é feita com o mutex travado
    map[p.x][p.y] = base_map[p.x][p.y];

    // atualiza posição do jogador
    p.x = nx;
    p.y = ny;

    // marca o jogador na nova célula
    map[p.x][p.y] = p.symbol;

    // redesenha
    draw_map();

    // detecta saída da RC: se prev_base era 'C' e a base da atual não é 'C'
    if (prev_base == 'C' && base_map[p.x][p.y] != 'C')
    {
        // liberamos semáforo - já não precisamos do mutex para isso,
        // mas como ainda temos lock, liberamos depois de postar
        // (não há risco de deadlock porque sem_post não bloqueia)
        sem_post(&sem_RC); // Incrementa valor do semáforo
    }

    // se chegou na bandeira (base == 'F'), encerra
    if (base_map[p.x][p.y] == 'F')
    {
        playing = false;
        cout << "\nJogador " << p.symbol << " venceu!\n";
    }
    // lock é liberado ao sair do escopo
}

// Thread principal do jogo
void main_game_loop()
{
    // inicia o mapa
    // le teclado dos 2 players
    // Exibe mapa
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
            case 'b':
                // Tecla 'b' para encerrar o jogo no brute_force
                playing = false;
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
    // copy estado incial do jogo
    for (int i = 0; i < LIN; i++)
    {
        for (int j = 0; j < COL; j++)
        {
            base_map[i][j] = map[i][j];
        }
    }

    sem_init(&sem_RC, 0, 1); // apenas um jogador por vez pode ter acesso a região crítica

    // Coloca os jogadores nas posições inicias
    map[p1.x][p1.y] = '1';
    map[p2.x][p2.y] = '2';

    thread player_1(thread_player, ref(p1));
    thread player_2(thread_player, ref(p2));
    thread game(main_game_loop);

    game.join();
    player_1.join();
    player_2.join();

    sem_destroy(&sem_RC); // destrói região crítica criada
    return 0;
}
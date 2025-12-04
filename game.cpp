// Arquivo game.cpp
// Como compilar: g++ -std=c++17 game.cpp -pthread -lncurses -o semaphore_game
// Run: ./game

/*
NOME: Gabriel de Araujo Lima​ ​NUSP: 14571376​
NOME: Luis Henrique Ponciano dos Santos ​ NUSP: 15577760
NOME: Gabriel Demba​ ​NUSP: 15618344
NOME: Wiltord N M NUSP: 15595392
*/

/*
 * ======================================================================================
 * DOCUMENTAÇÃO DO SISTEMA OPERACIONAL (THREADS E SEMÁFOROS)
 * ======================================================================================
 * Este jogo utiliza conceitos de programação concorrente para gerenciar dois jogadores
 * simultaneamente. Abaixo estão descritos os mecanismos de sincronização utilizados:
 * * 1. THREADS (std::thread):
 * - Cada jogador é controlado por uma thread independente (t1 e t2).
 * - Isso permite que o processamento de movimento de cada jogador ocorra em paralelo,
 * sem que um bloqueie a execução do outro ou da thread principal (main).
 * Assim as threads sao
 * Main Thread (Thread principal): Responsável por iniciar o jogo, lidar com a interface gráfica (usando ncurses), 
 * capturar a entrada do usuário (movimentos dos jogadores) e coordenar a execução do jogo.
 * t1 (Thread do Jogador 1): Controla o movimento e a lógica do Jogador 1, realizando as ações do jogador
 * no mapa em paralelo com o Jogador 2.
 * * 2. SEMÁFORO (sem_t):
 * - Utilizado para controlar o acesso à "Ponte" (marcada com 'C' no mapa).
 * - A ponte é uma Região Crítica Lógica do jogo: apenas um jogador pode estar nela
 * por vez. O semáforo garante a Exclusão Mútua neste trecho.
 * * 3. MUTEX (std::mutex):
 * - Utilizado para proteger a Região Crítica de Dados (a matriz map_view).
 * - Garante que a escrita na matriz (mover jogador) e a leitura (desenhar mapa)
 * não aconteçam ao mesmo tempo, prevenindo condições de corrida e "glitches" visuais.
 * ======================================================================================
 */

/*
 * =========================================================================================================
 * EXPLICACAO NO CODIGO FONTE DO PROJETO OS PONTOS DE IMPLEMENTAÇÃO DAS THREADS E DO SEMÁFORO
 * =========================================================================================================
 *
 * 1. ONDE AS THREADS FORAM UTILIZADAS E SUA NECESSIDADE
 * ---------------------------------------------------------------------------------------------------------
 * - Identificação: Foram criadas duas threads de trabalho (workers): 't1' (Player 1) e 't2' (Player 2).
 * - [cite_start]Necessidade: O uso de múltiplas threads é essencial para simular o "Tempo Real"[cite: 8]. Sem elas,
 * o jogo seria baseado em turnos (sequencial), onde um jogador precisaria esperar o outro mover.
 * - Função Específica: A função 'thread_player' encapsula o ciclo de vida de cada jogador, processando
 * inputs de direção e executando a lógica de movimento independentemente.
 * - Inicialização: Utilizamos 'std::thread' (C++ Standard), que abstrai a criação via 'pthread_create'.
 * As threads iniciam imediatamente ao serem instanciadas na função 'main'.
 * - Finalização: O ciclo termina quando a flag global 'playing' se torna false. O encerramento seguro
 * é garantido pela chamada de 'join()' na main, aguardando que t1 e t2 completem suas tarefas.
 *
 * 2. COMO AS THREADS INTERAGEM ENTRE SI
 * ---------------------------------------------------------------------------------------------------------
 * - Dados Compartilhados: As threads compartilham a matriz 'map_view' (visualização do jogo), a matriz
 * 'base_map' (lógica imutável) e a variável de controle 'playing'.
 * - Comunicação: A comunicação é indireta (via estado compartilhado). Um jogador "sabe" onde o outro
 * está ao ler a matriz 'map_view'.
 * - Evitando Condições de Corrida: O acesso à matriz visual é protegido por um Mutex ('mtx_map'). Isso
 * impede que uma thread desenhe a tela enquanto outra está atualizando uma posição (rasgo de tela).
 *
 * 3. IMPLEMENTAÇÃO E JUSTIFICATIVA DOS SEMÁFOROS
 * ---------------------------------------------------------------------------------------------------------
 * - Semáforo Utilizado: 'sem_RC' (tipo sem_t da biblioteca semaphore.h).
 * - Justificativa ("O Porquê"): O mapa possui um estreitamento (Ponte) marcado com 'C'. Esta é uma
 * [cite_start]região onde fisicamente (pela lógica do jogo) apenas UM jogador cabe por vez[cite: 8].
 * - Tipo: Semáforo Binário (Inicializado com 1). Escolhido pois o recurso "Ponte" tem capacidade
 * unitária. Se a ponte fosse mais larga, usaríamos um semáforo contador (ex: valor 2).
 *
 * 4. SEÇÕES CRÍTICAS PROTEGIDAS
 * ---------------------------------------------------------------------------------------------------------
 * - Região Crítica de DADOS: O acesso às variáveis globais (map_view) dentro de 'move_player'.
 * Protegida por 'std::unique_lock<std::mutex>'.
 * -> Risco Evitado: Corrupção de memória e inconsistência visual (glitches).
 * - Região Crítica LÓGICA: A travessia da ponte ('C'). Protegida por 'sem_trywait' e 'sem_post'.
 * -> Risco Evitado: Dois jogadores ocupando a mesma coordenada física no estreitamento.
 *
 * 5. COORDENAÇÃO DO FLUXO PELOS SEMÁFOROS
 * ---------------------------------------------------------------------------------------------------------
 * - Sincronização de Movimento: O semáforo atua como um "porteiro". Quando um jogador tenta entrar na
 * ponte, executa 'sem_trywait'.
 * - Controle de Recursos Limitados: A ponte é tratada como um recurso finito. Se o semáforo for 0,
 * o recurso está esgotado. O jogador é barrado (movimento cancelado) até que o recurso seja liberado.
 * - Fluxo: Entrada na Ponte -> Decrementa Semáforo. Saída da Ponte -> Incrementa Semáforo ('sem_post').
 *
 * 6. PROBLEMAS DE CONCORRÊNCIA EVITADOS
 * ---------------------------------------------------------------------------------------------------------
 * - Race Conditions (Condição de Corrida): Evitadas no desenho do mapa pelo uso de Mutex.
 * - Inconsistência de Estado: Evitada garantindo que, se um jogador está na ponte, o estado do
 * semáforo reflete "ocupado", impedindo sobreposição lógica.
 * - Deadlocks: Evitados pelo uso de 'sem_trywait' (não-bloqueante) ao invés de 'sem_wait'. Se a
 * ponte estiver ocupada, a thread não "dorme" segurando o Mutex; ela apenas aborta o movimento
 * e tenta novamente no próximo ciclo, mantendo a fluidez do jogo.
 *
 * 7. DESTAQUES DA IMPLEMENTAÇÃO (VIDE CÓDIGO)
 * ---------------------------------------------------------------------------------------------------------
 * - Criação de Threads: Linhas contendo 'thread t1(...)' na função main.
 * - Entrada na RC: Uso de 'sem_trywait(&sem_RC)' dentro da função 'move_player'.
 * - Saída da RC: Uso de 'sem_post(&sem_RC)' após detectar saída da célula 'C'.
 * - Exclusão Mútua: Blocos 'unique_lock<mutex> lock(mtx_map)' em 'draw_map' e 'move_player'.
 *
 * 8. RELAÇÃO ENTRE DESIGN DO JOGO E SISTEMAS OPERACIONAIS
 * ---------------------------------------------------------------------------------------------------------
 * - Contexto: O jogo simula o "Problema do Produtor-Consumidor" ou "Acesso a Recurso Único" (como
 * uma impressora). Os jogadores são PROCESSOS e a ponte é o RECURSO COMPARTILHADO não-preemptível.
 * - Paralelismo: O jogo exige paralelismo para que a competição seja justa (tempo real), tal qual
 * um SO escalona múltiplos processos para dar a ilusão de simultaneidade.
 *
 */

/* Importar bibliotecas utilizadas */
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

// --- RECURSOS COMPARTILHADOS 
/* Matriz constante que serve de modelo para o mapa (chão, paredes, ponte) */
char base_map[LIN][COL] = {
    "############################################################",
    "#F     #                                            #      #", // F = Bandeira (Alvo) ou Ponto de Partida
    "###### ############################################## #### #",
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
    "###### ############################################## #### #",
    "#      #                                            #     F#", 
    "############################################################"
};

/* Matriz visual compartilhada entre as threads e a main para desenho */
char map_view[LIN][COL];

/* Estrutura que define o estado de cada jogador */
struct Player {
    int x, y;       // Posição atual
    char symbol;    // '1' ou '2'
    char direction; // Direção do movimento ('u', 'd', 'l', 'r')
};

// --- VARIÁVEIS GLOBAIS E SINCRONIZAÇÃO ---

/* Define o Mutex para proteção de dados compartilhados (Exclusão Mútua) */
mutex mtx_map;       // Instancia objeto mutex para proteger acesso à map_view. No jogo, evita condição de corrida no desenho.

/* Define o Semáforo para controle da Região Crítica Lógica (A Ponte) */
sem_t sem_RC;        // Declara variável semáforo para controlar acesso à ponte. No jogo, limita o acesso a 1 jogador na região crítica.

bool playing = true; // Define flag de controle para gerenciar loop principal. No jogo, mantém a execução até ordem de parada.
string winner_msg = "";

// --- FUNÇÕES ---

/* Inicializa a biblioteca ncurses e configura cores/teclado */
void init_interface() {
    initscr();            // Inicia ncurses para preparar o terminal. No jogo, permite desenho gráfico avançado.
    cbreak();             // Desativa buffer de linha para ler input imediatamente. No jogo, torna o controle responsivo.
    noecho();             // Desativa eco para não mostrar tecla digitada. No jogo, mantém a tela limpa.
    curs_set(0);          // Oculta cursor para remover o piscar do terminal. No jogo, melhora visual da interface.
    nodelay(stdscr, TRUE);// Ativa não-bloqueio para que input não trave o loop. No jogo, permite execução em tempo real.
    keypad(stdscr, TRUE); // Ativa teclado especial para ler setas. No jogo, permite controle intuitivo.
    start_color();        // Inicia sistema de cores para habilitar RGB. No jogo, permite diferenciar os jogadores.
    
    init_pair(1, COLOR_CYAN, COLOR_BLACK);   // Define par de cor 1 para configurar P1. No jogo, identifica visualmente o Jogador 1.
    init_pair(2, COLOR_RED, COLOR_BLACK);    // Define par de cor 2 para configurar P2. No jogo, identifica visualmente o Jogador 2.
    init_pair(3, COLOR_GREEN, COLOR_BLACK);  // Define par de cor 3 para configurar paredes. No jogo, dá contraste ao mapa.
    init_pair(4, COLOR_YELLOW, COLOR_BLACK); // Define par de cor 4 para configurar ponte. No jogo, destaca a Região Crítica.
    init_pair(5, COLOR_MAGENTA, COLOR_BLACK);// Define par de cor 5 para configurar bandeira. No jogo, destaca o objetivo.
}

/* Encerra a interface gráfica corretamente */
void close_interface() {
    endwin(); // Finaliza ncurses para restaurar o terminal original. No jogo, evita que terminal fique "bugado" ao sair.
}

/* Função responsável por desenhar o estado atual do jogo na tela */
void draw_map() {
    /* Início da Seção Crítica de Leitura: Bloqueia acesso simultâneo ao mapa */
    unique_lock<mutex> lock(mtx_map); // Adquire lock do mutex para bloquear escrita no mapa. No jogo, garante desenho consistente sem falhas.
    
    for (int i = 0; i < LIN; i++) {
        for (int j = 0; j < COL; j++) {
            char cell = map_view[i][j];
            
            if (cell == '1') attron(COLOR_PAIR(1));      // Ativa cor P1 para preparar texto. No jogo, visualização do Jogador 1.
            else if (cell == '2') attron(COLOR_PAIR(2)); // Ativa cor P2 para preparar texto. No jogo, visualização do Jogador 2.
            else if (cell == '#') attron(COLOR_PAIR(3)); // Ativa cor parede para preparar texto. No jogo, visualização de obstáculo.
            else if (cell == 'C') attron(COLOR_PAIR(4)); // Ativa cor ponte para preparar texto. No jogo, visualização da Região Crítica.
            else if (cell == 'F') attron(COLOR_PAIR(5)); // Ativa cor fim para preparar texto. No jogo, visualização do objetivo.
            
            mvaddch(i, j, cell); // Imprime caractere na posição para atualizar buffer. No jogo, constrói o quadro atual.
            
            attroff(COLOR_PAIR(1)); // Desativa cor para limpar atributo. No jogo, evita contaminação de cores.
            attroff(COLOR_PAIR(2)); // Desativa cor para limpar atributo. No jogo, evita contaminação de cores.
            attroff(COLOR_PAIR(3)); // Desativa cor para limpar atributo. No jogo, evita contaminação de cores.
            attroff(COLOR_PAIR(4)); // Desativa cor para limpar atributo. No jogo, evita contaminação de cores.
            attroff(COLOR_PAIR(5)); // Desativa cor para limpar atributo. No jogo, evita contaminação de cores.
        }
    }
    refresh(); // Atualiza tela real para transferir buffer. No jogo, o usuário vê o quadro desenhado.
    /* O destrutor do unique_lock libera o mutex automaticamente aqui (RAII) */
}

/* Verifica se o movimento para (nx, ny) é válido */
bool allow_move(int nx, int ny) {
    if (nx < 0 || nx >= LIN || ny < 0 || ny >= COL) return false; // Checa limites para validar coordenadas. No jogo, impede segfault ou saída do mapa.
    if (map_view[nx][ny] == '#') return false; // Checa parede para verificar colisão sólida. No jogo, impede atravessar obstáculos.
    return true; // Retorna true para confirmar validade. No jogo, permite o movimento.
}

/* Lógica principal de movimento, colisão e sincronização */
void move_player(Player &p) {
    int nx = p.x; // Cria cópia local de X para cálculo. No jogo, prepara nova posição.
    int ny = p.y; // Cria cópia local de Y para cálculo. No jogo, prepara nova posição.

    /* Calcula a nova coordenada baseada na direção */
    switch (p.direction) {
        case 'u': nx--; break; // Decrementa X para mover para cima. No jogo, define destino do movimento.
        case 'd': nx++; break; // Incrementa X para mover para baixo. No jogo, define destino do movimento.
        case 'l': ny--; break; // Decrementa Y para mover para esquerda. No jogo, define destino do movimento.
        case 'r': ny++; break; // Incrementa Y para mover para direita. No jogo, define destino do movimento.
        default: return; // Caso padrão para ignorar input inválido. No jogo, não faz nada.
    }

    if (!allow_move(nx, ny)) return; // Chama validação para verificar permissão. No jogo, aborta se for parede.

    /* Entrada na Seção Crítica de DADOS: Solicita acesso exclusivo à matriz */
    unique_lock<mutex> lock(mtx_map); // Adquire mutex mtx_map para entrar em exclusão mútua. No jogo, garante que ninguém mexa no mapa agora.

    char next_base = base_map[nx][ny];      // Lê terreno futuro para lógica. No jogo, identifica se é ponte ou chão.
    char current_base = base_map[p.x][p.y]; // Lê terreno atual para lógica. No jogo, usado para apagar rastro.

    // --- LÓGICA DO SEMÁFORO (Região Crítica do Jogo) ---
    /* Verifica se está entrando na Ponte ('C') vindo de fora */
    if (next_base == 'C' && current_base != 'C') {
        /* Operação WAIT (TryWait) NO SEMÁFORO: Tenta entrar na RC Lógica */
        if (sem_trywait(&sem_RC) != 0) { // Tenta decrementar semáforo para verificar disponibilidade. No jogo, se falhar, jogador é impedido de entrar.
            return; // Retorna erro para cancelar função. No jogo, o personagem "bate" na entrada e espera.
        }
    }
    // ----------------------------------------------------

    /* Verificação de Vitória */
    int mid_col = COL / 2; // Calcula meio do mapa para definir fronteira. No jogo, separa os lados de vitória.
    if (p.symbol == '1' && next_base == 'F' && ny > mid_col) { // Verifica condição P1 para checar alvo. No jogo, define fim da partida.
        playing = false; // Seta flag false para sinalizar parada. No jogo, encerra o loop principal.
        winner_msg = "PLAYER 1 VENCEU!"; // Define string para output. No jogo, anuncia o vencedor.
    }
    else if (p.symbol == '2' && next_base == 'F' && ny < mid_col) { // Verifica condição P2 para checar alvo. No jogo, define fim da partida.
        playing = false; // Seta flag false para sinalizar parada. No jogo, encerra o loop principal.
        winner_msg = "PLAYER 2 VENCEU!"; // Define string para output. No jogo, anuncia o vencedor.
    }

    /* Flag auxiliar para saber se deve liberar o semáforo depois */
    bool just_exited_critical = (current_base == 'C' && next_base != 'C'); // Avalia saída para lógica booleana. No jogo, true se saiu da ponte agora.

    /* Atualização Visual do Mapa (Memória Compartilhada) */
    // Só restauramos a célula antiga se ela ainda contém o símbolo deste jogador.
    if (map_view[p.x][p.y] == p.symbol) { 
        map_view[p.x][p.y] = current_base; // Restaura chão para limpar posição. No jogo, apaga o rastro do jogador.
    }
    p.x = nx; // Atualiza struct X para efetivar valor. No jogo, jogador muda de posição lógica.
    p.y = ny; // Atualiza struct Y para efetivar valor. No jogo, jogador muda de posição lógica.
    
    // Desenha jogador na nova posição (isso pode sobrescrever o outro jogador: último escritor vence - permite ultrapassar)
    map_view[p.x][p.y] = p.symbol; // Escreve na matriz para renderizar. No jogo, atualiza a posição visual.

    lock.unlock(); 
    /* Saída da Seção Crítica de DADOS: Libera o mutex manualmente para permitir desenho */

    if (just_exited_critical) {
        /* Operação POST (Signal) NO SEMÁFORO: Libera a RC Lógica */
        sem_post(&sem_RC); // Incrementa semáforo para sinalizar "livre". No jogo, permite que outro jogador entre na ponte.
    }
}

// FUNÇÃO DA THREAD
/* Loop de execução de cada jogador */
void thread_player(Player &p) {
    while (playing) { // Verifica flag global para manter thread. No jogo, define a vida útil do jogador.
        if (p.direction != ' ') { // Checa input para verificar intenção. No jogo, evita processamento inútil se parado.
            move_player(p); // Chama função lógica para tentar mover. No jogo, executa as regras de movimento.
            p.direction = ' '; // Reseta direção para consumir input. No jogo, aguarda nova tecla.
            this_thread::sleep_for(chrono::milliseconds(100)); // Executa sleep para controlar velocidade. No jogo, define o ritmo/dificuldade.
        } else {
             this_thread::sleep_for(chrono::milliseconds(10)); // Executa sleep curto para evitar CPU 100%. No jogo, economiza recursos enquanto ocioso.
        }
    }
}

/* Função main a seguir para coordenar os comandos do jogo */
int main() {
    /* Inicializa o mapa visual com a base estática */
    for (int i = 0; i < LIN; i++) {
        for (int j = 0; j < COL; j++) {
            map_view[i][j] = base_map[i][j]; // Copia bytes para clonar mapa. No jogo, prepara estado inicial do cenário.
        }
    }

    /* Inicialização do Semáforo POSIX */
    sem_init(&sem_RC, 0, 1); // Cria semáforo com valor 1 para definir escopo. No jogo, garante Exclusão Mútua na ponte.

    Player p1 = {1, 1, '1', ' '};   // Instancia P1 para criar objeto. No jogo, define posição inicial esquerda.
    Player p2 = {19, 58, '2', ' '}; // Instancia P2 para criar objeto. No jogo, define posição inicial direita.

    map_view[p1.x][p1.y] = '1'; // Escreve na matriz para atualizar P1. No jogo, mostra P1 na largada.
    map_view[p2.x][p2.y] = '2'; // Escreve na matriz para atualizar P2. No jogo, mostra P2 na largada.

    init_interface(); // Inicia ncurses para configurar TUI. No jogo, entra no modo gráfico textual.

    /* Criação e Disparo das Threads (Paralelismo) */
    thread t1(thread_player, ref(p1)); // Cria thread t1 para rodar função. No jogo, P1 começa a existir em paralelo.
    thread t2(thread_player, ref(p2)); // Cria thread t2 para rodar função. No jogo, P2 começa a existir em paralelo.

    /* Loop Principal (Main Thread) - Cuida do Input e Desenho */
    while (playing) { // Loop while para manter engine. No jogo, verifica se partida continua.
        draw_map(); // Chama draw_map para renderizar. No jogo, atualiza tela para usuário.

        int ch = getch(); // Lê teclado para capturar input. No jogo, controla os personagens.
        if (ch != ERR) { // Verifica buffer para ver se há tecla. No jogo, processa comando se houver.
            switch(ch) {
                case 'w': p1.direction = 'u'; break; // Seta direção para registrar comando. No jogo, P1 vai para cima.
                case 's': p1.direction = 'd'; break; // Seta direção para registrar comando. No jogo, P1 vai para baixo.
                case 'a': p1.direction = 'l'; break; // Seta direção para registrar comando. No jogo, P1 vai para esquerda.
                case 'd': p1.direction = 'r'; break; // Seta direção para registrar comando. No jogo, P1 vai para direita.
                
                case KEY_UP:    p2.direction = 'u'; break; // Seta direção para registrar comando. No jogo, P2 vai para cima.
                case KEY_DOWN:  p2.direction = 'd'; break; // Seta direção para registrar comando. No jogo, P2 vai para baixo.
                case KEY_LEFT:  p2.direction = 'l'; break; // Seta direção para registrar comando. No jogo, P2 vai para esquerda.
                case KEY_RIGHT: p2.direction = 'r'; break; // Seta direção para registrar comando. No jogo, P2 vai para direita.
                
                case 'q': playing = false; break; // Altera flag para encerrar. No jogo, sai do programa.
            }
        }
        this_thread::sleep_for(chrono::milliseconds(30)); // Frame limiter para controlar FPS. No jogo, evita flickering excessivo.
    }

    /* Sincronização Final: Aguarda o término das threads */
    t1.join(); // Join na t1 para bloquear main. No jogo, garante fim ordenado de P1.
    t2.join(); // Join na t2 para bloquear main. No jogo, garante fim ordenado de P2.
    close_interface(); // Fecha ncurses para limpar recursos. No jogo, restaura terminal.
    
    /* Destruição do recurso do Semáforo */
    sem_destroy(&sem_RC); // Destrói semáforo para liberar memória. No jogo, evita vazamento de recursos.

    cout << "\n===========================\n"; // Print stream para mostrar texto. No jogo, feedback pós-jogo.
    cout << "   " << winner_msg << "   \n"; // Print stream para mostrar vencedor. No jogo, exibe resultado.
    cout << "===========================\n"; // Print stream para mostrar texto. No jogo, feedback pós-jogo.

    return 0; // Retorna 0 para finalizar main. No jogo, programa encerra com sucesso.
}
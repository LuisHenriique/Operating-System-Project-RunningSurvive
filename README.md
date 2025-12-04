
---

# RunningSurvive — Projeto de Sistemas Operacionais

## Visão Geral do Projeto

**RunningSurvive** é um jogo desenvolvido para a disciplina **SSC0140 — Sistemas Operacionais I**.
O objetivo principal é demonstrar, na prática, conceitos essenciais de **Concorrência**, **Threads** e **Semáforos (Exclusão Mútua)**.

### Objetivo do Jogo

Dois jogadores (rodando em **threads independentes**) começam em lados opostos do mapa.
Para vencer, o jogador deve atravessar o mapa e alcançar a bandeira (`F`) localizada no lado adversário.

### O Desafio

O mapa possui uma parede central sólida, exceto por uma passagem estreita — **a ponte**.

* Essa ponte é uma **Região Crítica**: somente *um jogador por vez* pode entrar.
* Um **Semáforo** controla o acesso.
* Se o Jogador 1 estiver na ponte, o Jogador 2 fica automaticamente bloqueado até o recurso ser liberado — mostrando claramente o conceito de **Exclusão Mútua** estudado em SO.

---

## Pré-requisitos (Linux)

Este projeto usa **NCurses** para exibir o jogo no terminal.

Instale as dependências de desenvolvimento:

```bash
sudo apt-get update
sudo apt-get install build-essential libncurses5-dev libncursesw5-dev
```
## Compatibilidade e Execução em Outros Sistemas

Este projeto foi desenvolvido nativamente para **Linux** (padrão POSIX), utilizando as bibliotecas `ncurses` e `pthread`. Abaixo estão as instruções para execução em Windows e MacOS.

### Windows (Via WSL)
Este código **não** compila nativamente no CMD ou PowerShell do Windows, pois utiliza cabeçalhos exclusivos do Linux (<ncurses.h>, <semaphore.h>).
Para rodar no Windows, é **obrigatório** o uso do **WSL (Windows Subsystem for Linux)**.

1. Abra o terminal do WSL (Ubuntu/Debian).
2. Instale as dependências: `sudo apt-get install build-essential libncurses5-dev libncursesw5-dev`
3. Compile e rode normalmente usando `make` e `./game`.

### MacOS
O MacOS é baseado em Unix e suporta nativamente as bibliotecas utilizadas.

1. Certifique-se de ter as ferramentas de linha de comando instaladas (`xcode-select --install`).
2. Compile usando o Makefile fornecido (`make`).
3. **Nota:** Caso ocorra erro de biblioteca não encontrada, instale via Homebrew: `brew install ncurses`.

---

## Como Compilar

Você pode compilar usando o **Makefile** (recomendado) ou manualmente.

### Opção 1: Usando Make (Recomendado)

Na pasta do projeto:

```bash
make
```

### Opção 2: Compilação Manual

Caso queira compilar diretamente:

```bash
g++ game.cpp -o game -lncurses -pthread
```

---

## Como Executar

Após compilar, o executável `game` será gerado. Para rodá-lo:

```bash
./game
```

---

## Controles

### Jogador 1 (Ciano)

* `W` — Cima
* `S` — Baixo
* `A` — Esquerda
* `D` — Direita

### Jogador 2 (Vermelho)

* Setas do Teclado:
  `↑` Cima, `↓` Baixo, `←` Esquerda, `→` Direita

### Sair do Jogo

* Pressione `Q`

---


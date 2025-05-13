#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Estrutura de Página
typedef struct {
    int presente;        // 1 se a página está na memória, 0 caso contrário
    int frame;          // Número do frame onde a página está alocada (-1 se não alocada)
    int tempo_carga;    // Instante em que a página foi carregada (para FIFO)
} Pagina;

// Estrutura de Processo
typedef struct {
    int pid;            // Identificador do processo
    int tamanho;        // Tamanho do processo em bytes
    int num_paginas;    // Número de páginas do processo
    Pagina *tabela_paginas; // Tabela de páginas do processo
} Processo;

// Estrutura da Memória Física
typedef struct {
    int num_frames;     // Número total de frames
    int *frames;        // Array de frames (pid << 16 | num_pagina)
    int *tempo_carga;   // Tempo de carregamento de cada frame
} MemoriaFisica;

// Estrutura do Simulador
typedef struct {
    int tempo_atual;    // Contador de tempo da simulação
    int tamanho_pagina; // Tamanho da página em bytes
    int tamanho_memoria_fisica; // Tamanho da memória física em bytes
    int num_processos;  // Número de processos
    Processo *processos; // Array de processos
    MemoriaFisica memoria; // Memória física
    // Estatísticas
    int total_acessos;  // Total de acessos à memória
    int page_faults;    // Total de page faults
    int algoritmo;      // 0=FIFO, 3=RANDOM
} Simulador;

// Inicializa o simulador
Simulador *inicializar_simulador(int tamanho_pagina, int tamanho_memoria_fisica) {
    Simulador *sim = (Simulador *)malloc(sizeof(Simulador));
    sim->tempo_atual = 0;
    sim->tamanho_pagina = tamanho_pagina;
    sim->tamanho_memoria_fisica = tamanho_memoria_fisica;
    sim->num_processos = 0;
    sim->processos = NULL;
    sim->total_acessos = 0;
    sim->page_faults = 0;
    sim->algoritmo = 0; // Default: FIFO

    // Inicializa memória física
    sim->memoria.num_frames = tamanho_memoria_fisica / tamanho_pagina;
    sim->memoria.frames = (int *)calloc(sim->memoria.num_frames, sizeof(int));
    sim->memoria.tempo_carga = (int *)calloc(sim->memoria.num_frames, sizeof(int));
    for (int i = 0; i < sim->memoria.num_frames; i++) {
        sim->memoria.frames[i] = -1; // Frame vazio
    }

    return sim;
}

// Cria um novo processo
Processo *criar_processo(Simulador *sim, int tamanho_processo) {
    Processo *proc = (Processo *)malloc(sizeof(Processo));
    proc->pid = sim->num_processos + 1;
    proc->tamanho = tamanho_processo;
    proc->num_paginas = (tamanho_processo + sim->tamanho_pagina - 1) / sim->tamanho_pagina;
    proc->tabela_paginas = (Pagina *)calloc(proc->num_paginas, sizeof(Pagina));

    for (int i = 0; i < proc->num_paginas; i++) {
        proc->tabela_paginas[i].presente = 0;
        proc->tabela_paginas[i].frame = -1;
        proc->tabela_paginas[i].tempo_carga = -1;
    }

    sim->num_processos++;
    sim->processos = (Processo *)realloc(sim->processos, sim->num_processos * sizeof(Processo));
    sim->processos[sim->num_processos - 1] = *proc;
    return proc;
}

// Extrai página e deslocamento
void extrair_pagina_deslocamento(Simulador *sim, int endereco_virtual, int *pagina, int *deslocamento) {
    *pagina = endereco_virtual / sim->tamanho_pagina;
    *deslocamento = endereco_virtual % sim->tamanho_pagina;
}

// Verifica se a página está presente
int verificar_pagina_presente(Simulador *sim, int pid, int pagina) {
    for (int i = 0; i < sim->num_processos; i++) {
        if (sim->processos[i].pid == pid) {
            return sim->processos[i].tabela_paginas[pagina].presente;
        }
    }
    return 0;
}

// Carrega uma página na memória
int carregar_pagina(Simulador *sim, int pid, int pagina) {
    // Procura frame livre
    int frame_livre = -1;
    for (int i = 0; i < sim->memoria.num_frames; i++) {
        if (sim->memoria.frames[i] == -1) {
            frame_livre = i;
            break;
        }
    }

    // Se não há frame livre, usa algoritmo de substituição
    if (frame_livre == -1) {
        if (sim->algoritmo == 0) {
            frame_livre = substituir_pagina_fifo(sim);
        } else if (sim->algoritmo == 3) {
            frame_livre = substituir_pagina_random(sim);
        }
    }

    // Carrega a página no frame
    for (int i = 0; i < sim->num_processos; i++) {
        if (sim->processos[i].pid == pid) {
            sim->processos[i].tabela_paginas[pagina].presente = 1;
            sim->processos[i].tabela_paginas[pagina].frame = frame_livre;
            sim->processos[i].tabela_paginas[pagina].tempo_carga = sim->tempo_atual;
            sim->memoria.frames[frame_livre] = (pid << 16) | pagina;
            sim->memoria.tempo_carga[frame_livre] = sim->tempo_atual;
            break;
        }
    }

    return frame_livre;
}

// Substituição FIFO
int substituir_pagina_fifo(Simulador *sim) {
    int frame_mais_antigo = 0;
    int menor_tempo = sim->memoria.tempo_carga[0];

    for (int i = 1; i < sim->memoria.num_frames; i++) {
        if (sim->memoria.tempo_carga[i] < menor_tempo) {
            menor_tempo = sim->memoria.tempo_carga[i];
            frame_mais_antigo = i;
        }
    }

    // Remove a página antiga
    int pid_antigo = sim->memoria.frames[frame_mais_antigo] >> 16;
    int pagina_antiga = sim->memoria.frames[frame_mais_antigo] & 0xFFFF;
    for (int i = 0; i < sim->num_processos; i++) {
        if (sim->processos[i].pid == pid_antigo) {
            sim->processos[i].tabela_paginas[pagina_antiga].presente = 0;
            sim->processos[i].tabela_paginas[pagina_antiga].frame = -1;
            sim->processos[i].tabela_paginas[pagina_antiga].tempo_carga = -1;
            break;
        }
    }

    printf("Tempo t=%d: Substituindo Página %d do Processo %d no Frame %d (FIFO)\n",
           sim->tempo_atual, pagina_antiga, pid_antigo, frame_mais_antigo);
    return frame_mais_antigo;
}

// Substituição Random
int substituir_pagina_random(Simulador *sim) {
    int frame = rand() % sim->memoria.num_frames;

    // Remove a página antiga
    int pid_antigo = sim->memoria.frames[frame] >> 16;
    int pagina_antiga = sim->memoria.frames[frame] & 0xFFFF;
    for (int i = 0; i < sim->num_processos; i++) {
        if (sim->processos[i].pid == pid_antigo) {
            sim->processos[i].tabela_paginas[pagina_antiga].presente = 0;
            sim->processos[i].tabela_paginas[pagina_antiga].frame = -1;
            sim->processos[i].tabela_paginas[pagina_antiga].tempo_carga = -1;
            break;
        }
    }

    printf("Tempo t=%d: Substituindo Página %d do Processo %d no Frame %d (RANDOM)\n",
           sim->tempo_atual, pagina_antiga, pid_antigo, frame);
    return frame;
}

// Traduz endereço virtual para físico
int traduzir_endereco(Simulador *sim, int pid, int endereco_virtual) {
    int pagina, deslocamento;
    extrair_pagina_deslocamento(sim, endereco_virtual, &pagina, &deslocamento);

    if (!verificar_pagina_presente(sim, pid, pagina)) {
        printf("Tempo t=%d: [PAGE FAULT] Página %d do Processo %d não está na memória física!\n",
               sim->tempo_atual, pagina, pid);
        sim->page_faults++;
        int frame = carregar_pagina(sim, pid, pagina);
        printf("Tempo t=%d: Carregando Página %d do Processo %d no Frame %d\n",
               sim->tempo_atual, pagina, pid, frame);
        exibir_memoria_fisica(sim);
        return (frame * sim->tamanho_pagina) + deslocamento;
    } else {
        for (int i = 0; i < sim->num_processos; i++) {
            if (sim->processos[i].pid == pid) {
                int frame = sim->processos[i].tabela_paginas[pagina].frame;
                printf("Tempo t=%d: Endereço Virtual (P%d): %d -> Página: %d -> Frame: %d -> Endereço Físico: %d\n",
                       sim->tempo_atual, pid, endereco_virtual, pagina, frame,
                       (frame * sim->tamanho_pagina) + deslocamento);
                return (frame * sim->tamanho_pagina) + deslocamento;
            }
        }
    }
    return -1;
}

// Exibe estado da memória física
void exibir_memoria_fisica(Simulador *sim) {
    printf("Tempo t=%d\nEstado da Memória Física:\n", sim->tempo_atual);
    for (int i = 0; i < sim->memoria.num_frames; i++) {
        printf("--------\n");
        if (sim->memoria.frames[i] == -1) {
            printf("| ---- |\n");
        } else {
            int pid = sim->memoria.frames[i] >> 16;
            int pagina = sim->memoria.frames[i] & 0xFFFF;
            printf("| P%d-%d |\n", pid, pagina);
        }
    }
    printf("--------\n");
}

// Exibe estatísticas
void exibir_estatisticas(Simulador *sim) {
    printf("======== ESTATÍSTICAS DA SIMULAÇÃO ========\n");
    printf("Total de acessos à memória: %d\n", sim->total_acessos);
    printf("Total de page faults: %d\n", sim->page_faults);
    printf("Taxa de page faults: %.2f%%\n", (sim->page_faults * 100.0) / sim->total_acessos);
    printf("Algoritmo: %s\n", sim->algoritmo == 0 ? "FIFO" : "RANDOM");
}

// Executa a simulação
void executar_simulacao(Simulador *sim, int algoritmo) {
    sim->algoritmo = algoritmo;
    printf("===== SIMULADOR DE PAGINAÇÃO =====\n");
    printf("Tamanho da página: %d bytes (%d KB)\n", sim->tamanho_pagina, sim->tamanho_pagina / 1024);
    printf("Tamanho da memória física: %d bytes (%d KB)\n", sim->tamanho_memoria_fisica, sim->tamanho_memoria_fisica / 1024);
    printf("Número de frames: %d\n", sim->memoria.num_frames);
    printf("Algoritmo de substituição: %s\n", algoritmo == 0 ? "FIFO" : "RANDOM");
    printf("======== INÍCIO DA SIMULAÇÃO ========\n");

    // Sequência de acessos de exemplo
    int acessos[][2] = {
        {1, 1111}, {1, 4444}, {2, 2000}, {3, 3000}, {1, 1111},
        {2, 5000}, {3, 7000}, {1, 4444}, {2, 2000}, {3, 12000}
    };
    int num_acessos = 10;

    for (int i = 0; i < num_acessos; i++) {
        sim->total_acessos++;
        sim->tempo_atual++;
        traduzir_endereco(sim, acessos[i][0], acessos[i][1]);
    }

    exibir_estatisticas(sim);
}

// Função principal
int main() {
    srand(time(NULL));
    Simulador *sim = inicializar_simulador(4096, 16384);

    // Cria 3 processos com 4 páginas cada
    criar_processo(sim, 16384);
    criar_processo(sim, 16384);
    criar_processo(sim, 16384);

    // Executa simulação com FIFO
    executar_simulacao(sim, 0);

    // Reinicializa para Random
    sim->tempo_atual = 0;
    sim->total_acessos = 0;
    sim->page_faults = 0;
    for (int i = 0; i < sim->memoria.num_frames; i++) {
        sim->memoria.frames[i] = -1;
        sim->memoria.tempo_carga[i] = 0;
    }
    for (int i = 0; i < sim->num_processos; i++) {
        for (int j = 0; j < sim->processos[i].num_paginas; j++) {
            sim->processos[i].tabela_paginas[j].presente = 0;
            sim->processos[i].tabela_paginas[j].frame = -1;
            sim->processos[i].tabela_paginas[j].tempo_carga = -1;
        }
    }

    // Executa simulação com Random
    executar_simulacao(sim, 3);

    // Libera memória
    for (int i = 0; i < sim->num_processos; i++) {
        free(sim->processos[i].tabela_paginas);
    }
    free(sim->processos);
    free(sim->memoria.frames);
    free(sim->memoria.tempo_carga);
    free(sim);

    return 0;
}

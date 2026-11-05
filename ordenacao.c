/*
 * Competição de Ordenação: O Desafio dos Milissegundos
 * Disciplina: Estrutura de Dados
 *
 * Estratégia híbrida, escolhida automaticamente pela faixa dos dados:
 *   - Counting Sort, quando a faixa (max - min) é pequena em relação a N
 *     (é o caso da base da competição: 10 milhões de valores entre 1 e 2.500.000).
 *   - Radix Sort LSD, caso contrário (descrito abaixo).
 *
 * Radix Sort LSD (Least Significant Digit) com base 2^11:
 *   - Complexidade O(N * k), onde k = número de passadas (geralmente 2 ou 3).
 *   - Não faz comparações: distribui os números por "dígitos" de 11 bits.
 *   - Para inteiros, é mais rápido que Quick Sort / Merge Sort (O(N log N)).
 *   - O número de passadas é calculado pela faixa real dos dados (max - min),
 *     então bases com valores pequenos ordenam com menos passadas.
 *
 * Fluxo do programa:
 *   1. Lê o arquivo inteiro para a memória (NÃO entra no tempo).
 *   2. Converte o texto em vetor de inteiros (NÃO entra no tempo).
 *   3. Mede SOMENTE a ordenação com clock().
 *   4. Verifica se o vetor ficou em ordem e se nenhum elemento foi perdido.
 *
 * Compilação: gcc -O3 -march=native -o ordenacao ordenacao.c
 * Execução:   ./ordenacao ListahAhSerOrdenada.txt
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#define BITS_DIGITO 11                    /* tamanho de cada "dígito" do radix */
#define TAM_BALDE   (1 << BITS_DIGITO)    /* 2048 baldes por passada */
#define MASCARA     (TAM_BALDE - 1)

/* ------------------------------------------------------------------ */
/* Leitura do arquivo inteiro para um buffer (fora da medição de tempo) */
/* ------------------------------------------------------------------ */
static char *ler_arquivo(const char *caminho, size_t *tamanho)
{
    FILE *f = fopen(caminho, "rb");
    if (!f) { perror("Erro ao abrir arquivo"); return NULL; }

    fseek(f, 0, SEEK_END);
    long tam = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = (char *)malloc((size_t)tam + 1);
    if (!buf) { fclose(f); fprintf(stderr, "Memoria insuficiente\n"); return NULL; }

    size_t lidos = fread(buf, 1, (size_t)tam, f);
    fclose(f);
    buf[lidos] = '\0';
    *tamanho = lidos;
    return buf;
}

/* ------------------------------------------------------------------ */
/* Converte o texto em vetor de inteiros de 64 bits.                   */
/* Aceita números separados por quebra de linha, espaço, vírgula etc.  */
/* ------------------------------------------------------------------ */
static int64_t *converter_numeros(const char *buf, size_t tam, size_t *total)
{
    size_t capacidade = tam / 2 + 1;      /* limite superior de números possíveis */
    int64_t *v = (int64_t *)malloc(capacidade * sizeof(int64_t));
    if (!v) { fprintf(stderr, "Memoria insuficiente\n"); return NULL; }

    size_t n = 0;
    const char *p = buf, *fim = buf + tam;
    while (p < fim) {
        /* pula tudo que não é dígito nem sinal de menos */
        while (p < fim && !((*p >= '0' && *p <= '9') || *p == '-')) p++;
        if (p >= fim) break;

        int negativo = 0;
        if (*p == '-') {
            negativo = 1; p++;
            if (p >= fim || *p < '0' || *p > '9') continue; /* '-' solto */
        }
        int64_t x = 0;
        while (p < fim && *p >= '0' && *p <= '9') { x = x * 10 + (*p - '0'); p++; }
        v[n++] = negativo ? -x : x;
    }
    *total = n;
    return v;
}

/* ------------------------------------------------------------------ */
/* Radix Sort LSD                                                      */
/* Os valores são deslocados por (valor - min), então negativos também */
/* funcionam e só processamos os bits realmente usados.                */
/* ------------------------------------------------------------------ */
static void ordenarVetor(int64_t *v, size_t n)
{
    if (n < 2) return;

    /* 1) Descobre mínimo e máximo para saber quantos bits são necessários */
    int64_t min = v[0], max = v[0];
    for (size_t i = 1; i < n; i++) {
        if (v[i] < min) min = v[i];
        if (v[i] > max) max = v[i];
    }
    uint64_t faixa = (uint64_t)max - (uint64_t)min;

    /* ---- Caminho rápido: COUNTING SORT ----
     * Se a faixa de valores é pequena (no máximo ~4x o número de elementos),
     * basta contar quantas vezes cada valor aparece e reescrever o vetor.
     * Complexidade O(N + faixa), com só 2 varreduras e nenhuma comparação.
     * Na base da competição: 10 milhões de valores entre 1 e 2.500.000. */
    if (faixa < (uint64_t)n * 4 && faixa < (1ULL << 28)) {
        size_t tam = (size_t)faixa + 1;
        uint32_t *conta = (uint32_t *)calloc(tam, sizeof(uint32_t));
        if (conta) {
            for (size_t i = 0; i < n; i++) conta[v[i] - min]++;   /* conta ocorrências */
            size_t pos = 0;
            for (size_t valor = 0; valor < tam; valor++) {        /* reescreve em ordem */
                int64_t x = (int64_t)valor + min;
                for (uint32_t c = conta[valor]; c > 0; c--) v[pos++] = x;
            }
            free(conta);
            return;
        }
        /* se faltar memória, cai no Radix Sort abaixo */
    }

    int bits = 0;
    while (bits < 64 && (faixa >> bits)) bits++;
    int passadas = (bits + BITS_DIGITO - 1) / BITS_DIGITO;
    if (passadas == 0) return;            /* todos os elementos são iguais */

    /* 2) Vetores auxiliares (chave deslocada sem sinal) */
    uint64_t *a = (uint64_t *)malloc(n * sizeof(uint64_t));
    uint64_t *b = (uint64_t *)malloc(n * sizeof(uint64_t));
    size_t *cont = (size_t *)malloc((size_t)passadas * TAM_BALDE * sizeof(size_t));
    if (!a || !b || !cont) { fprintf(stderr, "Memoria insuficiente\n"); exit(1); }
    memset(cont, 0, (size_t)passadas * TAM_BALDE * sizeof(size_t));

    /* 3) Uma única leitura monta os histogramas de TODAS as passadas */
    for (size_t i = 0; i < n; i++) {
        uint64_t k = (uint64_t)v[i] - (uint64_t)min;
        a[i] = k;
        for (int p = 0; p < passadas; p++)
            cont[p * TAM_BALDE + ((k >> (p * BITS_DIGITO)) & MASCARA)]++;
    }

    /* 4) Transforma contagens em posições iniciais (soma prefixa) */
    for (int p = 0; p < passadas; p++) {
        size_t *c = cont + p * TAM_BALDE, soma = 0;
        for (int d = 0; d < TAM_BALDE; d++) { size_t t = c[d]; c[d] = soma; soma += t; }
    }

    /* 5) Distribuição estável, alternando entre os vetores a e b */
    for (int p = 0; p < passadas; p++) {
        size_t *c = cont + p * TAM_BALDE;
        int desloc = p * BITS_DIGITO;
        for (size_t i = 0; i < n; i++) {
            uint64_t k = a[i];
            b[c[(k >> desloc) & MASCARA]++] = k;
        }
        uint64_t *t = a; a = b; b = t;
    }

    /* 6) Desfaz o deslocamento e devolve ao vetor original */
    for (size_t i = 0; i < n; i++) v[i] = (int64_t)(a[i] + (uint64_t)min);

    free(a); free(b); free(cont);
}

/* ------------------------------------------------------------------ */
/* Verificações de corretude                                           */
/* ------------------------------------------------------------------ */
static uint64_t soma_controle(const int64_t *v, size_t n)
{
    uint64_t s = 0;
    for (size_t i = 0; i < n; i++) s += (uint64_t)v[i];
    return s;
}

int main(int argc, char *argv[])
{
    const char *arquivo = (argc > 1) ? argv[1] : "ListahAhSerOrdenada.txt";

    /* ===== Leitura (NÃO cronometrada) ===== */
    size_t tam;
    char *buf = ler_arquivo(arquivo, &tam);
    if (!buf) return 1;

    size_t n;
    int64_t *vetor = converter_numeros(buf, tam, &n);
    free(buf);
    if (!vetor) return 1;

    printf("Arquivo: %s\n", arquivo);
    printf("Elementos lidos: %zu\n", n);

    uint64_t soma_antes = soma_controle(vetor, n);

    /* ===== Ordenação (SOMENTE isto é cronometrado) ===== */
    clock_t inicio = clock();
    ordenarVetor(vetor, n);
    clock_t fim = clock();

    double tempo_gasto = ((double)(fim - inicio)) / CLOCKS_PER_SEC;
    printf("Tempo de ordenacao: %f segundos (%f ms)\n", tempo_gasto, tempo_gasto * 1000.0);

    /* ===== Verificação (fora do tempo) ===== */
    int ordenado = 1;
    size_t repetidos = 0;
    for (size_t i = 1; i < n; i++) {
        if (vetor[i - 1] > vetor[i]) { ordenado = 0; break; }
        if (vetor[i - 1] == vetor[i]) repetidos++;
    }
    int sem_perda = (soma_controle(vetor, n) == soma_antes);

    printf("Vetor em ordem crescente: %s\n", ordenado ? "SIM" : "NAO");
    printf("Sem perda de elementos: %s\n", sem_perda ? "SIM" : "NAO");
    printf("Valores repetidos: %zu\n", repetidos);
    if (n > 0) printf("Menor: %lld | Maior: %lld\n", (long long)vetor[0], (long long)vetor[n - 1]);

    /* Para salvar o resultado, rode com: ./ordenacao arquivo.txt --salvar */
    if (argc > 2 && strcmp(argv[2], "--salvar") == 0) {
        FILE *out = fopen("ListaOrdenada.txt", "w");
        if (out) {
            for (size_t i = 0; i < n; i++) fprintf(out, "%lld\n", (long long)vetor[i]);
            fclose(out);
            printf("Resultado salvo em ListaOrdenada.txt\n");
        }
    }

    free(vetor);
    return (ordenado && sem_perda) ? 0 : 2;
}

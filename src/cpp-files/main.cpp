#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Rabbit state structure
typedef struct {
    uint32_t x[8]; // State variables
    uint32_t c[8]; // Counter variables
    uint32_t carry;
} rabbit_ctx;

// Nonlinear G function
#define G_FUNC(u) ({ \
    uint64_t sq = (uint64_t)(u) * (uint64_t)(u); \
    (uint32_t)(sq ^ (sq >> 32)); \
})

// Left rotation macro
#define ROTL32(x, n) ((x << n) | (x >> (32 - n)))

extern "C" void rabbit_key_setup_(rabbit_ctx *ctx, const uint8_t key[16]);
extern "C" void rabbit_crypt_(rabbit_ctx *ctx, uint8_t *data, size_t len);



// Rabbit key setup function
void rabbit_key_setup(rabbit_ctx *ctx, const uint8_t key[16]) {
    uint32_t k0 = ((uint32_t *)key)[0], k1 = ((uint32_t *)key)[1];
    uint32_t k2 = ((uint32_t *)key)[2], k3 = ((uint32_t *)key)[3];

    ctx->x[0] = k0;
    ctx->x[2] = k1;
    ctx->x[4] = k2;
    ctx->x[6] = k3;
    
    ctx->x[1] = (k3 << 16) | (k2 >> 16);
    ctx->x[3] = (k0 << 16) | (k3 >> 16);
    ctx->x[5] = (k1 << 16) | (k0 >> 16);
    ctx->x[7] = (k2 << 16) | (k1 >> 16);
    
    // // TILL HERE
    ctx->c[0] = ROTL32(k2, 16);
    ctx->c[2] = ROTL32(k3, 16);
    ctx->c[4] = ROTL32(k0, 16);
    ctx->c[6] = ROTL32(k1, 16);
    ctx->c[1] = (k0 & 0xFFFF0000) | (k1 & 0x0000FFFF);
    ctx->c[3] = (k1 & 0xFFFF0000) | (k2 & 0x0000FFFF);
    ctx->c[5] = (k2 & 0xFFFF0000) | (k3 & 0x0000FFFF);
    ctx->c[7] = (k3 & 0xFFFF0000) | (k0 & 0x0000FFFF);

    ctx->carry = 0;

    for (int i = 0; i < 4; i++) {
        // Perform four iterations to mix the state
        uint32_t g[8];

        for (int j = 0; j < 8; j++) {
            uint32_t prev_c = ctx->c[j];
            ctx->c[j] += 0x4D34D34D + ctx->carry;
            ctx->carry = ctx->c[j] < prev_c;
        }

        for (int j = 0; j < 8; j++)
            g[j] = G_FUNC(ctx->x[j] + ctx->c[j]);

        for (int j = 0; j < 8; j++)
            ctx->x[j] = g[j] ^ ROTL32(g[(j + 7) % 8], 16) ^ ROTL32(g[(j + 6) % 8], 24);
    }
}

// Rabbit keystream generator (produces 16 bytes)
void rabbit_generate_keystream(rabbit_ctx *ctx, uint8_t keystream[16]) {
    uint32_t g[8], next_x[8];

    for (int j = 0; j < 8; j++) {
        uint32_t prev_c = ctx->c[j];
        ctx->c[j] += 0x4D34D34D + ctx->carry;
        ctx->carry = ctx->c[j] < prev_c;
    }

    for (int j = 0; j < 8; j++)
        g[j] = G_FUNC(ctx->x[j] + ctx->c[j]);

    for (int j = 0; j < 8; j++)
        next_x[j] = g[j] ^ ROTL32(g[(j + 7) % 8], 16) ^ ROTL32(g[(j + 6) % 8], 24);

    uint32_t k0 = next_x[0] ^ (next_x[5] >> 16);
    uint32_t k1 = next_x[2] ^ (next_x[7] >> 16);
    uint32_t k2 = next_x[4] ^ (next_x[1] >> 16);
    uint32_t k3 = next_x[6] ^ (next_x[3] >> 16);

    ((uint32_t *)keystream)[0] = k0;
    ((uint32_t *)keystream)[1] = k1;
    ((uint32_t *)keystream)[2] = k2;
    ((uint32_t *)keystream)[3] = k3;
}

// Encrypt or decrypt using Rabbit (XORs keystream with data)
void rabbit_crypt(rabbit_ctx *ctx, uint8_t *data, size_t len) {
    uint8_t keystream[16];

    for (size_t i = 0; i < len; i += 16) {
        rabbit_generate_keystream(ctx, keystream);

        for (size_t j = 0; j < 16 && i + j < len; j++)
            data[i + j] ^= keystream[j];
    }
}

// Example usage
int main1() {
    uint8_t key[16] = {0x91, 0x28, 0xA6, 0x13, 0x64, 0x53, 0xB2, 0xAF,
                       0xD3, 0x21, 0xF1, 0x6A, 0x76, 0xB4, 0x8C, 0xE3};
    uint8_t plaintext[] = "Hello, Rabbit Cipher!";
    size_t len = strlen((char *)plaintext);

    rabbit_ctx ctx;
    rabbit_key_setup(&ctx, key);
    printf("%d-",ctx.x[0]);
    printf("%d-",ctx.x[1]);
    printf("%d-",ctx.x[2]);
    printf("%d-",ctx.x[3]);
    printf("%d-",ctx.x[4]);
    printf("%d-",ctx.x[5]);
    printf("%d-",ctx.x[6]);
    printf("%d-",ctx.x[7]);

    printf("%d-",ctx.c[0]);
    printf("%d-",ctx.c[1]);
    printf("%d-",ctx.c[2]);
    printf("%d-",ctx.c[3]);
    printf("%d-",ctx.c[4]);
    printf("%d-",ctx.c[5]);
    printf("%d-",ctx.c[6]);
    printf("%d-",ctx.c[7]);
    printf("\n");
    // printf("Original: %s\n", plaintext);

    // rabbit_crypt(&ctx, plaintext, len);
    // printf("Encrypted: ");
    // for (size_t i = 0; i < len; i++) printf("%02X ", plaintext[i]);
    // printf("\n");

    // rabbit_key_setup(&ctx, key); // Reinitialize for decryption
    // rabbit_crypt(&ctx, plaintext, len);
    // printf("Decrypted: %s\n", plaintext);

    return 0;
}
int main2() {
    uint8_t key[16] = {0x91, 0x28, 0xA6, 0x13, 0x64, 0x53, 0xB2, 0xAF,
                       0xD3, 0x21, 0xF1, 0x6A, 0x76, 0xB4, 0x8C, 0xE3};
    uint8_t plaintext[] = "Hello, Rabbit Cipher!";
    size_t len = strlen((char *)plaintext);

    rabbit_ctx ctx;
    rabbit_key_setup_(&ctx, key);

    printf("%d-",ctx.x[0]);
    printf("%d-",ctx.x[1]);
    printf("%d-",ctx.x[2]);
    printf("%d-",ctx.x[3]);
    printf("%d-",ctx.x[4]);
    printf("%d-",ctx.x[5]);
    printf("%d-",ctx.x[6]);
    printf("%d-",ctx.x[7]);
    printf("%d-",ctx.c[0]);
    printf("%d-",ctx.c[1]);
    printf("%d-",ctx.c[2]);
    printf("%d-",ctx.c[3]);
    printf("%d-",ctx.c[4]);
    printf("%d-",ctx.c[5]);
    printf("%d-",ctx.c[6]);
    printf("%d-",ctx.c[7]);
    printf("\n");

    // printf("Original: %s\n", plaintext);

    // rabbit_crypt_(&ctx, plaintext, len);
    // printf("Encrypted: ");
    // for (size_t i = 0; i < len; i++) printf("%02X ", plaintext[i]);
    // printf("\n");

    // rabbit_key_setup_(&ctx, key); // Reinitialize for decryption
    // rabbit_crypt_(&ctx, plaintext, len);
    // printf("Decrypted: %s\n", plaintext);

    return 0;
}

int main(){
	printf("****************CPP****************\n");
	main1();
    printf("****************ASM****************\n");
	main2();
	
}
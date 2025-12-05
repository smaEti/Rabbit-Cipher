# Rabbit Cipher — C++ & ARM64 Implementation

## Overview
This repository contains a from-scratch implementation of the Rabbit stream cipher written in portable C++ alongside a hand-crafted AArch64 (ARMv8) assembly port. The project was produced for an undergraduate architecture course where the assignment was to translate a working C implementation into optimized assembly while preserving bit-for-bit parity. The resulting binary can be cross-compiled for ARM and executed locally through `qemu-aarch64`.

Rabbit is a synchronous stream cipher that generates 128-bit keystream blocks from a 128-bit secret key. Encryption and decryption are identical operations that XOR the keystream with plaintext/ciphertext bytes. The cipher emphasizes a compact state (only 513 bits) and high throughput, which is reflected in both the C++ and assembly code paths implemented here.

## Repository Structure
| Path | Description |
| --- | --- |
| `src/cpp-files/main.cpp` | Reference C++ implementation of Rabbit along with a simple demo that encrypts/decrypts a short string using both the high-level and assembly entry points. |
| `src/asm-files/assembly.s` | Full ARM64 translation of the key schedule, keystream generator, and XOR loop. Functions follow the AArch64 ELF ABI so they can be called directly from C++. |
| `Makefile` | Cross-compilation pipeline that builds C++ and assembly objects with `aarch64-linux-gnu` toolchains, links them into a single binary, and exposes helpers such as `run`, `debug`, and `clean`. |

## Building & Running
1. **Install prerequisites** (package names are for Debian/Ubuntu-based systems):
   ```bash
   sudo apt install build-essential qemu-user aarch64-linux-gnu-g++ aarch64-linux-gnu-binutils gdb-multiarch
   ```
2. **Build the project** (produces an ARM64 binary in `bin`):
   ```bash
   make all
   ```
3. **Run inside QEMU**:
   ```bash
   make run
   ```
   The demo prints both the C++ and assembly executions so you can verify identical ciphertext and recovered plaintext.
4. **Optional debugging**: launch QEMU in GDB server mode via `make debug`, then attach with `make connect` in another terminal to single-step either implementation.
5. **Clean intermediates**:
   ```bash
   make clean
   ```

> **Note:** The Makefile assumes the presence of `/usr/aarch64-linux-gnu` runtime libraries. Adjust `EMULATION_LIB_PATH` if your sysroot lives elsewhere.

## Interfacing C++ and Assembly
- The shared state structure `rabbit_ctx` is defined in C++ as:
  ```c
  typedef struct {
      uint32_t x[8];
      uint32_t c[8];
      uint32_t carry;
  } rabbit_ctx;
  ```
- `extern "C"` declarations expose the assembly routines (`rabbit_key_setup_`, `rabbit_generate_keystream_`, `rabbit_crypt_`) so the C++ driver can invoke them without C++ name mangling.
- Both implementations operate on identical memory layouts. The assembly file loads/stores state fields using fixed offsets that mirror the struct layout (e.g., `x[0]` at byte 0, `c[0]` at byte 32, `carry` at byte 64), ensuring strict ABI compatibility.

## Algorithm Breakdown
Rabbit keeps track of eight 32-bit state words (`x[0..7]`), eight 32-bit counter words (`c[0..7]`), and a single carry bit. Each round updates the counters, applies a non-linear squaring function, mixes results through rotations, and extracts four 32-bit keystream words.

### Key Setup (Initialization Rounds)
1. **Key partitioning**: The 128-bit key is split into four 32-bit words `k0..k3`. These words are interleaved and rotated to populate the initial `x` vector, while rotated/interwoven versions populate the `c` counters. This layout injects diffusion before any rounds execute.
2. **Counter constant**: Every counter update adds the fixed `0x4D34D34D`. This constant is derived from Rabbit's specification and guarantees full-period behavior when combined with the carry bit.
3. **State mixing**: Four warm-up iterations run immediately after seeding. Each iteration performs the same steps as the normal keystream generator to decorrelate the initial state from raw key material before keystream bits are emitted.

### Counter System & Carry Propagation
- Counters are updated with modular addition (`c[j] += 0x4D34D34D + carry`).
- The carry flag captures overflow from each addition and becomes the input carry for the next counter, effectively forming a ripple adder across the eight counters. This mechanism provides non-linearity through modular wraparound.

### Non-linear `G` Function
- For each index `j`, the cipher computes `g[j] = ((x[j] + c[j])^2 mod 2^64) XOR (((x[j] + c[j])^2) >> 32)`.
- Squaring introduces quadratic terms while mixing low/high halves via XOR collapses 64-bit entropy back into 32 bits. Both the C++ macro and assembly use 64-bit intermediates to avoid overflow.

### State Update with Rotations
- New state words are derived via:
  ```
  x[j] = g[j]
        ^ ROTL32(g[(j+7) mod 8], 16)
        ^ ROTL32(g[(j+6) mod 8], 24)
  ```
- The rotations by 16 and 24 bits provide intra-word diffusion and ensure every new word depends on three different `g` values. The assembly mirrors this using `LSL`/`LSR` plus `ORR` to emulate cyclic shifts.

### Keystream Extraction
- After producing `next_x`, the cipher emits four 32-bit words:
  ```
  k0 = next_x[0] ^ (next_x[5] >> 16)
  k1 = next_x[2] ^ (next_x[7] >> 16)
  k2 = next_x[4] ^ (next_x[1] >> 16)
  k3 = next_x[6] ^ (next_x[3] >> 16)
  ```
- These words are concatenated into 16 keystream bytes. Encryption/decryption simply XORs these bytes with the message buffer, so the process is symmetric.

## Data Structures & Control Flow Highlights
| Component | Role |
| --- | --- |
| `rabbit_ctx` | Holds the entire cipher state. Arrays are small enough to stay in registers/stack, which helps performance in both C++ and assembly paths. |
| Counter loop | Implemented as a for-loop in C++ and an indexed loop in assembly (`loop_j1`). The consistent stride and offsets make it easy to unroll or pipeline on real hardware. |
| Temporary `g[8]` | Stored on the stack in assembly and on the C++ stack array. Provides staging so that all `g[j]` values are available before computing the new `x` words, matching the algorithm's synchronous nature. |
| XOR keystream loop | Outer loop advances in 16-byte blocks, inner loop XORs byte-by-byte until the last partial block is handled. The assembly version uses nested loops with bounds checks to avoid overruns. |

## Demo Output
Running `make run` prints something similar to:
```
****************CPP****************
Original: Hello, Rabbit Cipher!
Encrypted: 3A 41 ...
Decrypted: Hello, Rabbit Cipher!
****************ASM****************
Original: Hello, Rabbit Cipher!
Encrypted: 3A 41 ...
Decrypted: Hello, Rabbit Cipher!
```
The ciphertext bytes match across both implementations, proving the assembly translation is functionally equivalent to the C++ reference.

## Extending the Project
- Swap `plaintext`/`key` in `main.cpp` to experiment with new vectors.
- Integrate the assembly functions into larger ARM projects (e.g., bare-metal firmware) by reusing `rabbit_ctx` and the provided prototypes.
- Profile the assembly on actual ARM hardware to explore instruction-level optimizations such as unrolling or NEON-based XORs.

## References
- Original Rabbit specification: B. Boesen et al., "The Rabbit Stream Cipher" (ECRYPT eSTREAM Phase 3).
- ARM Architecture Reference Manual for A-profile architecture (for instruction semantics used in `assembly.s`).

//
// M.AKIMOTO
// Modified by ChatGPT based on user request
// 2025/05/25
// gcc -O2 -Wall -o octadiskbitcheck octadiskbitcheck.c
//

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

void print_usage(const char* prog) {
    printf("Usage: %s vdif/raw read_bytes [loop] [skip] [bit]\n", prog);
    printf("  vdif/raw   : Input .vdif or .raw file (required)\n");
    printf("  read bytes : Number of bytes to read per loop (required)\n");
    printf("  loop       : Number of loops (optional, default=1)\n");
    printf("  skip bytes : Bytes to skip before reading (optional, default=0)\n");
    printf("  bit levels : 1-4 bit (optional, default=4)\n");
    printf("\n");
    printf("NOTE\n");
    printf("  - If no arguments are given, usage is printed.\n");
    printf("  - If only the filename is given, the file size is printed.\n");
}

int main(int argc, char* argv[]) {
    if (argc == 1) {
        print_usage(argv[0]);
        return 0;
    } else if (argc == 2) {
        FILE* f = fopen(argv[1], "rb");
        if (!f) {
            perror("Error opening file");
            return 1;
        }
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fclose(f);
        printf("File: %s\nSize: %ld bytes\n", argv[1], size);
        return 0;
    }

    if (argc < 3) {
        fprintf(stderr, "Error: Insufficient arguments.\n");
        print_usage(argv[0]);
        return 1;
    }

    const char* filename = argv[1];
    size_t read_bytes = atoi(argv[2]);
    size_t loop_count  = 1;   // default
    size_t skip_bytes  = 0;   // default
    int bitwidth = 4;         // default

    if (argc >= 4) loop_count = atoi(argv[3]);
    if (argc >= 5) skip_bytes = atoi(argv[4]);
    if (argc >= 6) bitwidth = atoi(argv[5]);

    if (bitwidth <= 0 || bitwidth > 5) {
        fprintf(stderr, "Error: Bit level must be between 1 and 4\n");
        return 1;
    }

    FILE* file = fopen(filename, "rb");
    if (!file) {
        perror("Error opening file");
        return 1;
    }

    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    rewind(file);

    size_t total_required = skip_bytes + read_bytes * loop_count;
    if (total_required > (size_t)filesize) {
        size_t max_loop = (filesize - skip_bytes) / read_bytes;
        if (max_loop == 0) {
            fprintf(stderr, "Error: Not enough data to skip and read.\n");
            fclose(file);
            return 1;
        }
        loop_count = max_loop;
        fprintf(stderr, "Warning: Adjusted loop count to %zu due to file size\n", loop_count);
    }

    fseek(file, skip_bytes, SEEK_SET);
    uint8_t* buffer = malloc(read_bytes);
    if (!buffer) {
        perror("Memory allocation failed");
        fclose(file);
        return 1;
    }

    printf("# readbytes=%zu\n", read_bytes);
    printf("# loop=%zu\n", loop_count);
    printf("# bit=%d\n", bitwidth);
    size_t i, j, loop;
    for (loop = 0; loop < loop_count; ++loop) {
        size_t read = fread(buffer, 1, read_bytes, file);
        if (read < read_bytes) break;

        size_t count[256] = {0};  // Max for 8-bit width
        size_t maxval = (1 << bitwidth);
        size_t total_symbols = 0;

        for (i = 0; i < read_bytes * 8; i += bitwidth) {
            size_t byte_idx = i / 8;
            int bit_offset = i % 8;
            if (bit_offset + bitwidth <= 8) {
                uint8_t val = (buffer[byte_idx] >> bit_offset) & ((1 << bitwidth) - 1);
                count[val]++;
                total_symbols++;
            } else {
                if (byte_idx + 1 >= read_bytes) break;
                uint16_t span = buffer[byte_idx] | (buffer[byte_idx + 1] << 8);
                uint8_t val = (span >> bit_offset) & ((1 << bitwidth) - 1);
                count[val]++;
                total_symbols++;
            }
        }

        printf("Hist : ");
        for (j = 0; j < maxval; ++j) {
            double percent = 100.0 * count[j] / total_symbols;
            printf("%.1f ", percent);
        }
        printf("\n");
    }

    free(buffer);
    fclose(file);
    return 0;
}

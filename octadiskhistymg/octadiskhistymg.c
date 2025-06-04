//
// Made by M.AKIMOTO
// 2025/05/26
// gcc -O2 -Wall -o octadiskhistymg octadiskhistymg.c
//

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

void print_usage(const char* prog) {
    printf("Usage: %s --ifile FILE --readbyte N --loop N [--skip N] [--gnuplot]\n", prog);
    printf("  --ifile     : Input .vdif or .raw file (required)\n");
    printf("  --readbyte  : Bytes to read per loop (required)\n");
    printf("  --loop      : Number of times to read and analyze (required)\n");
    printf("  --skip      : Bytes to skip before reading (optional)\n");
    printf("  --gnuplot   : Generate .csv and .gplt for plotting with gnuplot (optional)\n");
}

int ends_with(const char* str, const char* suffix) {
    size_t len_str = strlen(str);
    size_t len_suffix = strlen(suffix);
    return len_str >= len_suffix && strcmp(str + len_str - len_suffix, suffix) == 0;
}

int main(int argc, char* argv[]) {
    const char* filename = NULL;
    size_t read_bytes = 0, skip_bytes = 0, loop_count = 0;
    int generate_gnuplot = 0;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--ifile") == 0 && i + 1 < argc) {
            filename = argv[++i];
        } else if (strcmp(argv[i], "--readbyte") == 0 && i + 1 < argc) {
            read_bytes = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--skip") == 0 && i + 1 < argc) {
            skip_bytes = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--loop") == 0 && i + 1 < argc) {
            loop_count = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--gnuplot") == 0) {
            generate_gnuplot = 1;
        } else {
            fprintf(stderr, "Error: Unknown argument '%s'\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    if (!filename) {
        fprintf(stderr, "Error: --ifile is required.\n");
        print_usage(argv[0]);
        return 1;
    }

    if (!(ends_with(filename, ".vdif") || ends_with(filename, ".raw"))) {
        fprintf(stderr, "Error: Input file must have .vdif or .raw extension.\n");
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

    if (read_bytes == 0 || loop_count == 0) {
        printf("File: %s\nSize: %ld bytes\n", filename, filesize);
        fclose(file);
        return 0;
    }

    if (read_bytes % 4 != 0) {
        fprintf(stderr, "Error: --readbyte must be a multiple of 4 (1 sample = 4 bytes)\n");
        fclose(file);
        return 1;
    }

    size_t total_required = skip_bytes + read_bytes * loop_count;
    if (total_required > filesize) {
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

    FILE* csv = NULL;
    char csv_filename[512], gplt_filename[520];
    if (generate_gnuplot) {
        snprintf(csv_filename, sizeof(csv_filename), "%s.hist.csv", filename);
        csv = fopen(csv_filename, "w");
        if (!csv) {
            perror("Error creating CSV file");
            free(buffer);
            fclose(file);
            return 1;
        }
    }

    for (size_t loop = 0; loop < loop_count; ++loop) {
    
        size_t read = fread(buffer, 1, read_bytes, file);
        
        if (read < read_bytes) break;

        size_t count[4] = {0};
        size_t samples = read_bytes / 4;

	for (size_t i = 0; i < read_bytes; i += 4) {
	
	    uint32_t word = buffer[i] |
		           (buffer[i + 1] << 8) |
		           (buffer[i + 2] << 16) |
		           (buffer[i + 3] << 24);

	    for (int k = 0; k < 32; k += 2) {
		count[(word >> k) & 0x03]++;
	    }
	}


        size_t total = samples * 16;
        double percent[4];
        for (int i = 0; i < 4; ++i) {
            percent[i] = 100.0 * count[i] / total;
        }

        printf("(Loop %zu) Hist: %.1f %.1f %.1f %.1f\n",
               loop+1, percent[0], percent[1], percent[2], percent[3]);

        if (csv) {
            fprintf(csv, "%zu,%.2f,%.2f,%.2f,%.2f\n",
                    loop, percent[0], percent[1], percent[2], percent[3]);
        }
    }

    if (csv) {
        fclose(csv);
        snprintf(gplt_filename, sizeof(gplt_filename), "%s.gplt", csv_filename);
        FILE* gplt = fopen(gplt_filename, "w");
        if (gplt) {
            fprintf(gplt,
                "set datafile separator \",\"\n"
                "set xlabel \"loop\"\n"
                "set ylabel \"bit histogram (%%)\"\n"
                "set title \"2-bit Histogram per Loop\"\n"
                "set grid\n"
                "set key outside\n"
                "set terminal png size 800,600\n"
                "set output \"%s.png\"\n"
                "plot \"%s\" using 1:2 title \"0\" with lines, \\\n"
                "     \"%s\" using 1:3 title \"1\" with lines, \\\n"
                "     \"%s\" using 1:4 title \"2\" with lines, \\\n"
                "     \"%s\" using 1:5 title \"3\" with lines\n",
                csv_filename, csv_filename, csv_filename, csv_filename, csv_filename);
            fclose(gplt);
            printf("\n");
            printf("Make CSV: %s\n", csv_filename);
            printf("Execute : gnuplot %s\n", gplt_filename);
        } else {
            perror("Error creating gnuplot script");
        }
    }

    free(buffer);
    fclose(file);
    return 0;
}


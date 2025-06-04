//
// M.AKIMOTO
// Modified by ChatGPT based on user request
// 2025/05/27 
// gcc -O2 -Wall -fopenmp -o octadiskbandpass octadiskbandpass.c -lfftw3f -lm 
//

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <omp.h>
#include <fftw3.h>
#include <unistd.h>

#define BASE_RATE_MB 256
#define BASE_RATE_BYTES (BASE_RATE_MB * 1024 * 1024)
#define MAX_PATH 1024
#define BANDWIDTH_MHZ 511.0f

void print_usage(const char* prog) {
    printf("Usage: %s file.vdif fft_length integration_time [--recorder octadisk|vsrec] [--cpu N]\n", prog);
    printf("  file.vdif           : Input VDIF file\n");
    printf("  fft_length          : FFT length (power of 2)\n");
    printf("  integration_time    : Integration time in seconds (can be fractional)\n");
    printf("  --recorder          : Optional, 'octadisk' (default) or 'vsrec'\n");
    printf("  --cpu               : Optional, number of CPU threads (default=3)\n");
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        print_usage(argv[0]);
        return 1;
    }

    const char* filename = argv[1];
    int fft_len = atoi(argv[2]);
    float integration_time = atof(argv[3]);
    int recorder_type = 0;
    int cpu_threads = 3;

    for (int i = 4; i < argc; ++i) {
        if (strcmp(argv[i], "--recorder") == 0 && i + 1 < argc) {
            recorder_type = (strcmp(argv[++i], "vsrec") == 0) ? 1 : 0;
        } else if (strcmp(argv[i], "--cpu") == 0 && i + 1 < argc) {
            cpu_threads = atoi(argv[++i]);
        }
    }

    FILE* f = fopen(filename, "rb");
    if (!f) {
        perror("Error opening file");
        return 1;
    }

    char datname[MAX_PATH];
    snprintf(datname, sizeof(datname), "%s.dat", filename);
    FILE* fout = fopen(datname, "w");
    if (!fout) {
        perror("Error opening output .dat file");
        fclose(f);
        return 1;
    }

    omp_set_num_threads(cpu_threads);
    size_t seconds = (size_t)integration_time;
    float frac = integration_time - seconds;

    size_t full_sec_bytes = BASE_RATE_BYTES;
    size_t frac_bytes = (size_t)(BASE_RATE_BYTES * frac);
    size_t total_bytes = seconds * full_sec_bytes + frac_bytes;
    size_t max_symbols = total_bytes * 4;
    size_t num_ffts = max_symbols / fft_len;

    float* power_spectrum = calloc(fft_len / 2, sizeof(float));
    if (!power_spectrum) {
        perror("calloc failed");
        fclose(f);
        fclose(fout);
        return 1;
    }

    float* in = fftwf_alloc_real(fft_len);
    float* out = fftwf_alloc_real(fft_len);
    fftwf_plan plan = fftwf_plan_r2r_1d(fft_len, in, out, FFTW_R2HC, FFTW_MEASURE);

    size_t done_ffts = 0;
    uint32_t* wordbuf = malloc(BASE_RATE_BYTES);
    if (!wordbuf) {
        perror("malloc wordbuf failed");
        return 1;
    }

    for (size_t sec = 0; sec < (size_t)ceil(integration_time); ++sec) {
        size_t to_read = (sec == seconds) ? frac_bytes : full_sec_bytes;
        size_t read_words = to_read / 4;
        size_t r = fread(wordbuf, 1, read_words, f);
        if (r != read_words) break;

        uint8_t* symbols = malloc(read_words * 16);
        #pragma omp parallel for
        for (size_t i = 0; i < read_words; i++) {
            for (int j = 0; j < 16; j++) {
                symbols[i * 16 + j] = (wordbuf[i] >> (2 * j)) & 0x3;
            }
        }

        size_t sympos = 0;
        while (sympos + fft_len <= read_words * 16 && done_ffts < num_ffts) {
            for (int i = 0; i < fft_len; ++i) in[i] = (float)(symbols[sympos + i]);
            fftwf_execute(plan);
            out[0] = 0.0f;
            #pragma omp parallel for
            for (int i = 0; i < fft_len / 2; ++i) {
   
                power_spectrum[i] += out[i] * out[i];
            }
            sympos += fft_len;
            ++done_ffts;
        }
        free(symbols);
    }

    float freq_res = BANDWIDTH_MHZ / (fft_len / 2.0);
    for (int i = 0; i < (fft_len / 2); ++i) {
        int j = (fft_len / 2) - 1 - i;  // スペクトル反転
        float freq = i * freq_res;
        fprintf(fout, "%f\t%f\n", freq, power_spectrum[j] / done_ffts);
    }
    fclose(fout);

    char epsname[MAX_PATH];
    snprintf(epsname, sizeof(epsname), "%s.eps", filename);
    FILE* gplot = popen("gnuplot", "w");
    if (gplot) {
        fprintf(gplot, "set terminal postscript eps enhanced color\n");
        fprintf(gplot, "set output '%s'\n", epsname);
        fprintf(gplot, "set xlabel 'Frequency (MHz)'\n");
        fprintf(gplot, "set ylabel 'Power'\n");
        fprintf(gplot, "set xrange [0:511]\n");
        fprintf(gplot, "set yrange [0:]\n");
        fprintf(gplot, "set title 'Power Spectrum'\n");
        fprintf(gplot, "plot '%s' using 1:2 title 'Spectrum'\n", datname);
        pclose(gplot);
    } else {
        fprintf(stderr, "Failed to run gnuplot\n");
    }

    free(power_spectrum);
    fftwf_destroy_plan(plan);
    fftwf_free(in);
    fftwf_free(out);
    free(wordbuf);
    fclose(f);
    return 0;
}


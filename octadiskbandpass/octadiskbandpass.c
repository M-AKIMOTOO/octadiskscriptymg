//
// M.AKIMOTO
// 2025/06/10
// gcc -O2 -Wall -o octadiskbandpass octadiskbandpass.c -lfftw3 -lm
// 

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fftw3.h>
#include <math.h>
#include <string.h>

#define BANDWIDTH (512e6) // 512 MHz

int8_t decode_2bit(uint8_t data, int shift) {
    uint8_t val = (data >> shift) & 0x03;
    switch (val) {
        case 0: return -3;
        case 1: return -1;
        case 2: return 1;
        case 3: return 3;
        default: return 0; 
    }
}

int main(int argc, char *argv[]) {
    if (argc < 4 || argc > 5) {
        fprintf(stderr, "Usage: %s <vdif_file> <fft_points> <integration_time_sec> [moving_avg_points]\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *filename = argv[1];
    int fft_points = atoi(argv[2]);
    int integration_time = atoi(argv[3]);
    int moving_avg_points = (argc == 5) ? atoi(argv[4]) : 32;

    // ファイル名準備
    char pngfile[256], scriptfile[256], specfile[256], avgfile[256];
    snprintf(pngfile, sizeof(pngfile),   "%s.png", filename);
    snprintf(scriptfile, sizeof(scriptfile), "%s.gplt", filename);
    snprintf(specfile, sizeof(specfile), "%s.spec.dat", filename);
    snprintf(avgfile, sizeof(avgfile), "%s.avg.dat", filename);

    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror("Failed to open file");
        return EXIT_FAILURE;
    }

    size_t samples_per_sec = (size_t)BANDWIDTH * 2; // 2bit/sample
    size_t bytes_per_fft = fft_points / 4;
    size_t total_ffts = samples_per_sec / fft_points * integration_time;

    uint8_t *buffer = malloc(bytes_per_fft);
    double *input = fftw_malloc(sizeof(double) * fft_points);
    fftw_complex *output = fftw_malloc(sizeof(fftw_complex) * (fft_points/2+1));
    double *spectrum = calloc(fft_points/2+1, sizeof(double));

    fftw_plan plan = fftw_plan_dft_r2c_1d(fft_points, input, output, FFTW_MEASURE);

    for (size_t fft_count = 0; fft_count < total_ffts; fft_count++) {
        if (fread(buffer, 1, bytes_per_fft, fp) != bytes_per_fft) {
            fprintf(stderr, "Unexpected end of file\n");
            break;
        }

        double sum = 0.0;
        for (size_t i = 0, samp_idx = 0; i < bytes_per_fft; i++) {
            for (int shift = 6; shift >= 0; shift -= 2) {
                input[samp_idx] = decode_2bit(buffer[i], shift);
                sum += input[samp_idx];
                samp_idx++;
            }
        }

        double mean = sum / fft_points;
        for (int i = 0; i < fft_points; i++)
            input[i] -= mean;

        input[0] = 0;
        fftw_execute(plan);
        
 

        for (int i = 0; i < fft_points / 2; i++)
            spectrum[i] += output[i][0]*output[i][0] + output[i][1]*output[i][1];
    }

    for (int i = 0; i < fft_points / 2; i++)
        spectrum[i] /= (total_ffts * fft_points);

    // スペクトルデータ保存
    FILE *fspec = fopen(specfile, "w");
    FILE *favg  = fopen(avgfile, "w");
    double freq_res = BANDWIDTH / (fft_points / 2);
    int i = 0;
    int j = 0;

    for (i = 1; i < fft_points / 2; i++)
        fprintf(fspec, "%.10f %.10f\n", i * freq_res/1e6, spectrum[i]);

    for (i = 1; i < (fft_points / 2) - moving_avg_points; i++) {
        double avg1 = 0.0;
        double avg2 = 0.0;
        for (j = 0; j < moving_avg_points; j++) {
            avg1 += ((i+j) * freq_res/1e6);
            avg2 += spectrum[i + j];
        }
        avg1 /= moving_avg_points;
        avg2 /= moving_avg_points;
        fprintf(favg, "%.10f %.10f\n", avg1, avg2);
    }
    fclose(fspec);
    fclose(favg);

    // gnuplotスクリプト書き出し
    FILE *gpf = fopen(scriptfile, "w");
    fprintf(gpf, "set xlabel 'Frequency [MHz]'\n");
    fprintf(gpf, "set ylabel 'Power'\n");
    fprintf(gpf, "set xrange [0:%f]\n", BANDWIDTH/1e6);
    fprintf(gpf, "set title 'Spectrum (%s)'\n", filename);
    fprintf(gpf, "plot \\\n");
    fprintf(gpf, "  '%s' w l title 'FFT=%d,  int=%ds',\\\n", specfile, fft_points, integration_time);
    fprintf(gpf, "  '%s' w l title 'Moving Avg (%d pts)'\n", avgfile, moving_avg_points);
    fclose(gpf);

    // gnuplot同時起動
    FILE *gp = popen("gnuplot", "w");
    fprintf(gp, "set terminal png size 1200,800\n");
    fprintf(gp, "set output '%s'\n", pngfile);
    fprintf(gp, "load '%s'\n", scriptfile);
    pclose(gp);

    fftw_destroy_plan(plan);
    fftw_free(input);
    fftw_free(output);
    free(spectrum);
    free(buffer);
    fclose(fp);
    
    printf("Make %s\n", pngfile);
    printf("     %s\n", scriptfile);
    printf("     %s\n", specfile);
    printf("     %s\n", avgfile);

    return EXIT_SUCCESS;
}


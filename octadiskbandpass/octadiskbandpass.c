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

double decode_2bit(uint8_t data, int shift) {
    uint8_t val = (data >> shift) & 0x03;
    // val は 0, 1, 2, 3 のいずれかの値を取る
    // これらの値を0中心にするため、平均値 1.5 を引く
    // 0 -> 0 - 1.5 = -1.5
    // 1 -> 1 - 1.5 = -0.5
    // 2 -> 2 - 1.5 =  0.5
    // 3 -> 3 - 1.5 =  1.5
    switch (val) {
        case 0: return -1.5;
        case 1: return -0.5;
        case 2: return  0.5;
        case 3: return  1.5;
        default: return 0.0; // 通常このケースには到達しないはず
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

    // Check if fft_points is a multiple of 16, so bytes_per_fft is a multiple of 4.
    // This is important for the 32-bit word processing logic below.
    if (fft_points % 16 != 0) {
        fprintf(stderr, "Warning: fft_points (%d) is not a multiple of 16.\n", fft_points);
        fprintf(stderr, "  This means bytes_per_fft (%zu) is not a multiple of 4.\n", bytes_per_fft);
        fprintf(stderr, "  The primary decoding loop processes data in 32-bit (4-byte) words.\n");
        fprintf(stderr, "  Any remaining bytes will be processed using a fallback byte-wise MSB-first method.\n");
    }

    for (size_t fft_count = 0; fft_count < total_ffts; fft_count++) {
        if (fread(buffer, 1, bytes_per_fft, fp) != bytes_per_fft) {
            if (feof(fp)) {
                fprintf(stderr, "Reached end of file sooner than expected. Processed %zu FFTs out of %zu planned.\n", fft_count, total_ffts);
            } else {
                perror("Error reading from file");
            }
            total_ffts = fft_count; // Adjust total_ffts to what was actually processed
            if (fft_count == 0) { // No data processed, cleanup and exit
                 fftw_destroy_plan(plan);
                 fftw_free(input);
                 fftw_free(output);
                 free(spectrum);
                 free(buffer);
                 fclose(fp);
                 // Consider removing partially created output files if desired
                 fprintf(stderr, "Error: No FFTs processed due to file read issue.\n");
                 return EXIT_FAILURE;
            }
            break;
        }

        double sum = 0.0;
        size_t samp_idx = 0;

        // Process data in 32-bit (4-byte) words from LSB
        size_t num_full_words = bytes_per_fft / 4;
        for (size_t word_i = 0; word_i < num_full_words; ++word_i) {
            uint32_t current_word_le = 0;
            // Construct a little-endian 32-bit word from 4 bytes in the buffer.
            // buffer[word_i*4 + 0] is the first byte of the word in the stream (LSB of LE word)
            current_word_le  = (uint32_t)buffer[word_i*4 + 0];
            current_word_le |= (uint32_t)buffer[word_i*4 + 1] << 8;
            current_word_le |= (uint32_t)buffer[word_i*4 + 2] << 16;
            current_word_le |= (uint32_t)buffer[word_i*4 + 3] << 24;

            // Decode 2-bit samples from the LSB of the little-endian word upwards
            for (int bit_s = 0; bit_s < 32; bit_s += 2) { // bit_s is the bit shift for the 2-bit sample
                if (samp_idx < fft_points) {
                    uint8_t two_bit_val = (current_word_le >> bit_s) & 0x03;
                    // Directly use the mapping from decode_2bit function
                    switch (two_bit_val) {
                        case 0: input[samp_idx] = -1.5; break;
                        case 1: input[samp_idx] = -0.5; break;
                        case 2: input[samp_idx] =  0.5; break;
                        case 3: input[samp_idx] =  1.5; break;
                        default: input[samp_idx] = 0.0; // Should not happen
                    }
                    sum += input[samp_idx];
                    samp_idx++;
                } else {
                    break; // Should not happen if fft_points is a multiple of 16
                }
            }
        }

        // Handle remaining bytes if bytes_per_fft is not a multiple of 4
        // using the original MSB-first byte-wise decoding method.
        size_t remaining_bytes_start_idx = num_full_words * 4;
        for (size_t byte_i = remaining_bytes_start_idx; byte_i < bytes_per_fft; byte_i++) {
            for (int shift = 6; shift >= 0; shift -= 2) { // MSB-first within the byte
                if (samp_idx < fft_points) {
                    input[samp_idx] = decode_2bit(buffer[byte_i], shift);
                    sum += input[samp_idx];
                    samp_idx++;
                } else {
                    break;
                }
            }
        }

        double mean = sum / fft_points;
        for (int i = 0; i < fft_points; i++) {
            input[i] -= mean;
        }

        input[0] = 0.0; // Explicitly zero DC component after mean subtraction
        fftw_execute(plan);

        // Accumulate power spectrum (from DC up to Nyquist)
        int j = 0;
        for (int i = 0; i <= fft_points / 2; i++) {
            j = fft_points / 2 - i;
            if (i == 0 || i == fft_points / 2) { // DC or Nyquist (real components)
                spectrum[j] += output[i][0] * output[i][0] / (total_ffts * (double)fft_points);
            } else { // Other frequencies (complex components)
                spectrum[j] += (output[i][0] * output[i][0] + output[i][1] * output[i][1]) / (total_ffts * (double)fft_points);
            }
        }
    }

    // スペクトルデータ保存
    FILE *fspec = fopen(specfile, "w");
    FILE *favg  = fopen(avgfile, "w");
    double freq_res = BANDWIDTH / (fft_points / 2);
    int i = 0;
    int j = 0;

    // Output spectrum data from DC (index 0) up to Nyquist (index fft_points/2)
    // Original code started from i=1, excluding DC.
    for (i = 0; i <= fft_points / 2; i++) {
        fprintf(fspec, "%.10f %.10f\n", i * freq_res/1e6, spectrum[i]);
    }
    // Moving average calculation (original logic started from i=1)
    for (i = 0; i < (fft_points / 2) - moving_avg_points  ; i++) { // Adjusted loop bound for consistency if spectrum includes Nyquist
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

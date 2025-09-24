#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "save_to_file.h"
#include "storage.h"

void save_psd_to_csv(double* f, double* Pxx_dB, int length, const char* filename) {
    mkdir("Outputs", 0777);
    save_to_file(f, Pxx_dB, length, filename);
    printf("💾 PSD guardada en %s\n", filename);
}

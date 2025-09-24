#ifndef CAPTURE_H
#define CAPTURE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <complex.h>

// Captura IQ en formato CS8 desde HackRF
int capture_signal(long samples_to_xfer_max, uint64_t central_frequency_mhz);

// Convierte archivo CS8 → vector de IQ complejos
complex double* convert_cs8(const char* filename, size_t* N);

#endif

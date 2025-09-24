/**
 * @file cs8_to_iq.h
 * @brief Declaración de funciones para convertir datos CS8 a formato IQ.
 *
 * Este archivo contiene la declaración de la función `cargar_cs8`,
 * utilizada para cargar datos binarios en formato CS8 y convertirlos
 * en números complejos.
 */

#ifndef PROCESS_CS8
#define PROCESS_CS8

#include <stdint.h>
#include <complex.h>
#include <stddef.h>

/**
 * @brief Carga datos IQ desde un archivo binario con formato CS8.
 * 
 * @param filename Nombre del archivo binario que contiene los datos en formato CS8.
 * @param num_samples Puntero a una variable donde se almacenará el número de muestras leídas.
 * 
 * @return Un puntero a un arreglo de números complejos (`complex double`), 
 *         que contiene las muestras IQ. Retorna `NULL` en caso de error.
 * 
 * @note Es responsabilidad del usuario liberar la memoria asignada para el arreglo devuelto.
 */
complex double* cargar_cs8(const char* filename, size_t* num_samples);

#endif  // PROCESS_CS8

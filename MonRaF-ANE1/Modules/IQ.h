/**
 * @file IQ.h
 * @brief Declaración de funciones relacionadas con la lectura de archivos CS8, manejo de bandas y creación de vectores IQ.
 * 
 * Este archivo contiene las declaraciones de funciones utilizadas para trabajar con archivos CS8, cargar bandas de frecuencias y construir vectores de datos complejos (IQ).
 */

#ifndef IQ_H
#define IQ_H

#include <stdint.h>
#include <complex.h>

#define MAX_BAND_SIZE 50 ///< Tamaño máximo para el buffer de bandas

/**
 * @enum BANDS
 * @brief Enumeración de las bandas disponibles.
 * 
 * Estas son las bandas de frecuencia que se pueden cargar desde los archivos CSV correspondientes. Cada valor representa una banda distinta.
 */
typedef enum BANDS {
    VHF1, ///< Banda VHF1
    VHF2, ///< Banda VHF2
    VHF3, ///< Banda VHF3
    VHF4, ///< Banda VHF4
    UHF1,
    UHF1_2,
    UHF1_3,
    UHF1_4, ///< Banda UHF1
    UHF2_1, ///< Banda UHF2
    UHF2_2,
    UHF2_3,
    UHF2_4, ///< Banda UHF2
    UHF2_5,
    UHF2_6,
    UHF2_7, ///< Banda UHF2
    UHF2_8,
    UHF2_9,
    UHF2_10, ///< Banda UHF2
    UHF2_11,
    UHF2_12,
    UHF2_13,
    UHF3, ///< Banda UHF3
    UHF3_1,
    UHF3_2,
    UHF3_3,
    UHF3_4,
    UHF3_5,
    SHF1, ///< Banda SHF1
    SHF2,  ///< Banda SHF2
    SHF2_2,
    SHF2_3,
    SHF2_4,
    SHF2_5,
    SHF2_6,
    SHF2_7
    ,
} bands_t;

/**
 * @brief Lee un archivo con extensión CS8 y carga sus datos en un buffer.
 * 
 * Esta función abre un archivo CS8, lee su contenido y lo almacena en un arreglo de enteros de 8 bits (`int8_t`).
 * 
 * @param file_sample El número del archivo a leer, representado como un número entero sin signo.
 * @param file_size Puntero donde se almacenará el tamaño del archivo leído.
 * 
 * @return Un puntero al arreglo de datos leídos desde el archivo. Retorna `NULL` si hubo algún error al abrir o leer el archivo.
 * 
 * @note El archivo debe estar ubicado en el mismo directorio que el ejecutable y debe tener una extensión `.cs8`.
 */
int8_t* read_CS8(uint8_t file_sample, size_t* file_size);

/**
 * @brief Elimina un archivo CS8 dado.
 * 
 * Esta función elimina un archivo CS8 de acuerdo con el número de muestra proporcionado.
 * 
 * @param file_sample El número del archivo a eliminar.
 * 
 * @return No tiene valor de retorno. Imprime un mensaje indicando si la eliminación fue exitosa o no.
 */
void delete_CS8(uint8_t file_sample);

void delete_JSON(uint8_t file_json);

/**
 * @brief Carga las frecuencias y anchos de banda de un archivo CSV correspondiente a una banda seleccionada.
 * 
 * Esta función carga los valores de frecuencias y anchos de banda desde un archivo CSV, el cual depende de la banda seleccionada.
 * Los datos se almacenan en dos arreglos proporcionados como parámetros.
 * 
 * @param bands La banda de frecuencias a cargar (VHF1, VHF2, UHF1, etc.).
 * @param frequencies Puntero a un arreglo donde se almacenarán las frecuencias de la banda.
 * @param bandwidths Puntero a un arreglo donde se almacenarán los anchos de banda de la banda.
 * 
 * @return El número de filas leídas desde el archivo CSV, o 0 si ocurrió un error al abrir el archivo.
 */
int load_bands(uint8_t bands, double* frequencies, double* bandwidths);

uint16_t load_bands_tdt(char* channel, char* city, int *modulation);

/**
 * @brief Convierte un arreglo de datos en formato CS8 a un vector de números complejos IQ.
 * 
 * Esta función toma un arreglo de datos de 8 bits y los convierte en números complejos representados como `complex double`.
 * Los datos se interpretan como pares consecutivos de valores, uno para la parte I (In-phase) y otro para la parte Q (Quadrature).
 * 
 * @param rawVector Un puntero al arreglo de datos en formato CS8.
 * @param length La longitud del arreglo `rawVector`.
 * @param num_samples Puntero donde se almacenará el número de muestras convertidas a números complejos.
 * 
 * @return Un puntero a un arreglo de números complejos `complex double`, representando el vector IQ.
 * 
 * @note La longitud del arreglo debe ser par, ya que cada par de valores será interpretado como una muestra compleja.
 */
complex double* Vector_BIN(int8_t *rawVector, size_t length, size_t* num_samples);

#endif // IQ_H
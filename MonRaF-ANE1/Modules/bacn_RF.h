/**
 * @file bacn_RF.h
 * @brief Definiciones y prototipos para el manejo del dispositivo HackRF.
 * 
 * Este archivo contiene constantes, enumeraciones y funciones utilizadas para 
 * la configuración y adquisición de datos mediante HackRF.
 */

#ifndef BACN_RF_H
#define BACN_RF_H

#include <libhackrf/hackrf.h>

/**
 * @def DEFAULT_SAMPLE_RATE_HZ
 * @brief Tasa de muestreo predeterminada en Hz.
 * 
 * Valor por defecto: 20 MHz.
 */
#define DEFAULT_SAMPLE_RATE_HZ (20000000)
#define DEFAULT_SAMPLE_RATE_TDT (6500000)

/**
 * @def DEFAULT_CENTRAL_FREQ_HZ
 * @brief Frecuencia central predeterminada en Hz.
 * 
 * Valor por defecto: 10 MHz.
 */
#define DEFAULT_CENTRAL_FREQ_HZ (10000000)

/**
 * @def DEFAULT_SAMPLES_TO_XFER_MAX
 * @brief Número máximo de muestras a transferir por defecto.
 * 
 * Valor por defecto: 20M muestras.
 */
// #define DEFAULT_SAMPLES_TO_XFER_MAX (20000000)

// extern long samples_to_xfer_max; // se define en test_capture.c----------------
#define DEFAULT_SAMPLES_TDT_XFER_MAX (6500000)

/**
 * @def FD_BUFFER_SIZE
 * @brief Tamaño del búfer de transmisión.
 * 
 * Valor por defecto: 8 KB.
 */
#define FD_BUFFER_SIZE (8 * 1024)

/**
 * @enum transceiver_mode_t
 * @brief Enumeración de los modos de operación del transceptor.
 */
typedef enum {
	TRANSCEIVER_MODE_OFF = 0, /**< Transceptor apagado. */
	TRANSCEIVER_MODE_RX = 1,  /**< Modo de recepción (RX). */
	TRANSCEIVER_MODE_TDT = 2, /**< Modo TDT. */
} transceiver_mode_t;

/**
 * @brief Detiene el bucle principal.
 * 
 * Esta función se utiliza para finalizar la ejecución del programa de forma segura.
 */
void stop_main_loop(void);

/**
 * @brief Callback para manejar los datos recibidos del HackRF.
 * 
 * @param transfer Estructura de transferencia de HackRF que contiene los datos recibidos.
 * @return int Devuelve 0 si el procesamiento fue exitoso, -1 en caso de error.
 */
int rx_callback(hackrf_transfer* transfer);

/**
 * @brief Manejador de la señal SIGINT (Ctrl+C).
 * 
 * @param signum Número de la señal capturada.
 */
void sigint_callback_handler(int signum);

/**
 * @brief Manejador de la señal SIGALRM.
 * 
 * Este manejador se ejecuta cuando se captura la señal SIGALRM.
 */
void sigalrm_callback_handler();

/**
 * @brief Configura y ejecuta la adquisición de muestras con HackRF.
 * 
 * @param lo_freq Frecuencia inicial en Hz.
 * @param hi_freq Frecuencia final en Hz.
 * @param transceiver_mode Modo de operación del transceptor.
 * @param lna_gain Ganancia del amplificador de bajo ruido (LNA) en dB.
 * @param vga_gain Ganancia del amplificador de ganancia variable (VGA) en dB.
 * @return int Devuelve el número de frecuencias procesadas o -1 en caso de error.
 */
int getSamples(uint8_t bands, long samples_to_xfer_max, transceiver_mode_t transceiver_mode, uint16_t lna_gain, uint16_t vga_gain, uint16_t centralFrec, bool is_second_sample);

#endif // BACN_RF_H

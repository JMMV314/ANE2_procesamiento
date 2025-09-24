/**
 * @file bacn_RF.c
 * @brief Implementación de captura de datos IQ con HackRF.
 *
 * Este archivo contiene la implementación de funciones para manejar la captura de datos IQ 
 * utilizando un dispositivo HackRF. Incluye manejo de señales, configuración del dispositivo 
 * y almacenamiento de datos en archivos binarios.
 */

#include <libhackrf/hackrf.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include "bacn_RF.h"
#include "IQ.h"
#include "../Drivers/bacn_gpio.h"

// long samples_to_xfer_max=20000000; // valor por defecto (20M)---------------------

/** @brief Variable para controlar la finalización del bucle principal. */
static volatile bool do_exit = false;

/** @brief Archivo para almacenar los datos de salida. */
FILE* file = NULL;

/** @brief Contador de bytes transferidos. */
volatile uint32_t byte_count = 0;

/** @brief Tamaño del buffer de streaming. */
uint64_t stream_size = 0;

/** @brief Puntero al inicio del buffer circular de streaming. */
uint32_t stream_head = 0;

/** @brief Puntero al final del buffer circular de streaming. */
uint32_t stream_tail = 0;

/** @brief Número de streams descartados. */
uint32_t stream_drop = 0;

/** @brief Buffer para almacenamiento temporal de datos de streaming. */
uint8_t* stream_buf = NULL;

/** @brief Flag para limitar la cantidad de muestras transferidas. */
bool limit_num_samples = true;

/** @brief Bytes restantes para transferir. */
size_t bytes_to_xfer = 0;

/** @brief Frecuencia inicial para el barrido. */
int64_t lo_freq = 0;

/** @brief Frecuencia final para el barrido. */
int64_t hi_freq = 0;

/** @brief Dispositivo HackRF activo. */
static hackrf_device* device = NULL;

extern int64_t central_freq[60];

extern uint8_t getData;

/**
 * @brief Detiene el bucle principal de captura.
 */
void stop_main_loop(void)
{
	do_exit = true;
	kill(getpid(), SIGALRM);
}


int rx_callback(hackrf_transfer* transfer)
{
	size_t bytes_to_write;
	size_t bytes_written;

	if (file == NULL) {
		stop_main_loop();
		return -1;
	}

	/* Determina cuántos bytes escribir */
	bytes_to_write = transfer->valid_length;

	/* Actualiza el conteo de bytes */
	byte_count += transfer->valid_length;
	
	if (limit_num_samples) {
		if (bytes_to_write >= bytes_to_xfer) {
			bytes_to_write = bytes_to_xfer;
		}
		bytes_to_xfer -= bytes_to_write;
	}

	/* Escribe los datos directamente en el archivo si no hay búfer de transmisión */
	if (stream_size == 0) {
		bytes_written = fwrite(transfer->buffer, 1, bytes_to_write, file);
		if ((bytes_written != bytes_to_write) ||
		    (limit_num_samples && (bytes_to_xfer == 0))) {
			stop_main_loop();
			fprintf(stderr, "Total Bytes: %u\n",byte_count);
			return -1;
		} else {
			return 0;
		}
	}

	if ((stream_size - 1 + stream_head - stream_tail) % stream_size <
	    bytes_to_write) {
		stream_drop++;
	} else {
		if (stream_tail + bytes_to_write <= stream_size) {
			memcpy(stream_buf + stream_tail,
			       transfer->buffer,
			       bytes_to_write);
		} else {
			memcpy(stream_buf + stream_tail,
			       transfer->buffer,
			       (stream_size - stream_tail));
			memcpy(stream_buf,
			       transfer->buffer + (stream_size - stream_tail),
			       bytes_to_write - (stream_size - stream_tail));
		};
		__atomic_store_n(
			&stream_tail,
			(stream_tail + bytes_to_write) % stream_size,
			__ATOMIC_RELEASE);
	}
	return 0;
}


void sigint_callback_handler(int signum)
{
	fprintf(stderr, "Caught signal %d\n", signum);
	do_exit = true;
}

/**
 * @brief Manejador para la señal SIGALRM.
 */
void sigalrm_callback_handler()
{
}


int getSamples(uint8_t central_freq_Rx_MHz, transceiver_mode_t transceiver_mode, uint16_t lna_gain, uint16_t vga_gain, uint16_t centralFrec_TDT, bool is_second_sample)
{
    int result = 0;
	
	int64_t lo_freq = 0;
	int64_t hi_freq = 0;
	//bool is_second_sample;

	uint8_t tSample = 0;
	int64_t FreqTDT;
	uint64_t byte_count_now = 0;
	char path[20];
	
	if (transceiver_mode == TRANSCEIVER_MODE_TDT) { 
		FreqTDT = centralFrec_TDT * 1000000;
		fprintf(stderr, "central frequency: %lu\n", FreqTDT);
		tSample = 1;
	} else {
		// Definir rangos de frecuencia por banda
		lo_freq = (central_freq_Rx_MHz - 10) * 1000000;
		hi_freq = (central_freq_Rx_MHz + 10) * 1000000;

		// switch (bands) {
		// 	case VHF1: lo_freq = 88000000; hi_freq = 108000000; break;
		// 	case VHF2: lo_freq = 137000000; hi_freq = 157000000; break;
		// 	case VHF3: lo_freq = 148000000; hi_freq = 168000000; break;
		// 	case VHF4: lo_freq = 154000000; hi_freq = 174000000; break;
		// 	case UHF1: lo_freq = 400000000; hi_freq = 420000000; break;
		// 	case UHF1_2: lo_freq = 420000000; hi_freq = 440000000; break;
		// 	case UHF1_3: lo_freq = 440000000; hi_freq = 460000000; break;
		// 	case UHF1_4: lo_freq = 450000000; hi_freq = 470000000; break;
		// 	case UHF2_1: lo_freq = 470000000; hi_freq = 490000000; break;
		// 	case UHF2_2: lo_freq = 488000000; hi_freq = 508000000; break;
		// 	case UHF2_3: lo_freq = 506000000; hi_freq = 526000000; break;
		// 	case UHF2_4: lo_freq = 524000000; hi_freq = 544000000; break;
		// 	case UHF2_5: lo_freq = 542000000; hi_freq = 562000000; break;
		// 	case UHF2_6: lo_freq = 560000000; hi_freq = 580000000; break;
		// 	case UHF2_7: lo_freq = 578000000; hi_freq = 598000000; break;
		// 	case UHF2_8: lo_freq = 596000000; hi_freq = 616000000; break;
		// 	case UHF2_9: lo_freq = 614000000; hi_freq = 634000000; break;
		// 	case UHF2_10: lo_freq = 632000000; hi_freq = 652000000; break;
		// 	case UHF2_11: lo_freq = 650000000; hi_freq = 670000000; break;
		// 	case UHF2_12: lo_freq = 668000000; hi_freq = 688000000; break;
		// 	case UHF2_13: lo_freq = 678000000; hi_freq = 698000000; break;
		// 	case UHF3: lo_freq = 1708000000; hi_freq = 1728000000; break;
		// 	case UHF3_1: lo_freq = 1735000000; hi_freq = 1755000000; break;
		// 	case UHF3_2: lo_freq = 1805000000; hi_freq = 1825000000; break;
		// 	case UHF3_3: lo_freq = 1848000000; hi_freq = 1868000000; break;
		// 	case UHF3_4: lo_freq = 1868000000; hi_freq = 1888000000; break;
		// 	case UHF3_5: lo_freq = 1877000000; hi_freq = 1897000000; break;
		// 	case SHF1: lo_freq = 2550000000; hi_freq = 2570000000; break;
		// 	case SHF2: lo_freq = 3295000000; hi_freq = 3315000000; break;
		// 	case SHF2_2: lo_freq = 3338000000; hi_freq = 3358000000; break;
		// 	case SHF2_3: lo_freq = 3375000000; hi_freq = 3395000000; break;
		// 	case SHF2_4: lo_freq = 3444000000; hi_freq = 3464000000; break;
		// 	case SHF2_5: lo_freq = 3538000000; hi_freq = 3558000000; break;
		// 	case SHF2_6: lo_freq = 3550000000; hi_freq = 3570000000; break;
		// 	case SHF2_7: lo_freq = 3580000000; hi_freq = 3600000000; break;
		// 	default: return 0;
		// }
		
		if (is_second_sample) {
			lo_freq += 2000000;
			hi_freq += 2000000;
		}

		tSample = (hi_freq - lo_freq)/DEFAULT_SAMPLE_RATE_HZ;
				
		central_freq[0] = lo_freq + DEFAULT_CENTRAL_FREQ_HZ;
		fprintf(stderr, "central frequency: %lu\n", central_freq[0]);
		
		for(uint8_t i=1; i<tSample; i++) {
			central_freq[i] = central_freq[0] + DEFAULT_SAMPLE_RATE_HZ;
			fprintf(stderr, "central frequency: %lu\n", central_freq[i]); 
		}
	}

	if(lo_freq > 999999999) {
		switch_ANTENNA(RF1);
	} else {
		switch_ANTENNA(RF2);
	}

	result = hackrf_init();
	if (result != HACKRF_SUCCESS) {
		fprintf(stderr,
			"hackrf_init() failed: %s (%d)\n",
			hackrf_error_name(result),
			result);
		return -1;
	}

	// Configura manejadores de señales
	signal(SIGINT, &sigint_callback_handler);
	signal(SIGILL, &sigint_callback_handler);
	signal(SIGFPE, &sigint_callback_handler);
	signal(SIGSEGV, &sigint_callback_handler);
	signal(SIGTERM, &sigint_callback_handler);
	signal(SIGABRT, &sigint_callback_handler);

	signal(SIGALRM, &sigalrm_callback_handler);

	fprintf(stderr, "Device initialized\r\n");

	for(uint8_t i=0; i<tSample; i++)
	{		
		byte_count_now = 0;
		if (transceiver_mode == TRANSCEIVER_MODE_TDT) { 
			bytes_to_xfer = DEFAULT_SAMPLES_TDT_XFER_MAX * 2ull;
		} else {
			// bytes_to_xfer = DEFAULT_SAMPLES_TO_XFER_MAX * 2ull;
			bytes_to_xfer = samples_to_xfer_max * 2ull;
		}	

		memset(path, 0, 20);
		sprintf(path, "Samples/%d", i);
		file = fopen(path, "wb");
	
		if (file == NULL) {
			fprintf(stderr, "Failed to open file: %s\n", path);
			return -1;
		}
		/* Change file buffer to have bigger one to store or read data on/to HDD */
		result = setvbuf(file, NULL, _IOFBF, FD_BUFFER_SIZE);
		if (result != 0) {
			fprintf(stderr, "setvbuf() failed: %d\n", result);
			return -1;
		}

		fprintf(stderr,"Start Acquisition\n");

		result = hackrf_open(&device);
		if (result != HACKRF_SUCCESS) {
			fprintf(stderr,
				"hackrf_open() failed: %s (%d)\n",
				hackrf_error_name(result),
				result);
			return -1;
		}

		if (transceiver_mode == TRANSCEIVER_MODE_TDT) { 
			result = hackrf_set_sample_rate(device, DEFAULT_SAMPLE_RATE_TDT);
			if (result != HACKRF_SUCCESS) {
				fprintf(stderr,
					"hackrf_set_sample_rate() failed: %s (%d)\n",
					hackrf_error_name(result),
					result);
				return -1;
			}
		} else {
			result = hackrf_set_sample_rate(device, DEFAULT_SAMPLE_RATE_HZ);
			if (result != HACKRF_SUCCESS) {
				fprintf(stderr,
					"hackrf_set_sample_rate() failed: %s (%d)\n",
					hackrf_error_name(result),
					result);
				return -1;
			}
		}	
		

		result = hackrf_set_hw_sync_mode(device, 0);
		if (result != HACKRF_SUCCESS) {
			fprintf(stderr,
				"hackrf_set_hw_sync_mode() failed: %s (%d)\n",
				hackrf_error_name(result),
				result);
			return -1;
		}

		if (transceiver_mode == TRANSCEIVER_MODE_TDT) { 
			result = hackrf_set_freq(device, FreqTDT);
		if (result != HACKRF_SUCCESS) {
			fprintf(stderr,
				"hackrf_set_freq() failed: %s (%d)\n",
				hackrf_error_name(result),
				result);
			return -1;
		}	
		} else {
			result = hackrf_set_freq(device, central_freq[i]);
			if (result != HACKRF_SUCCESS) {
				fprintf(stderr,
					"hackrf_set_freq() failed: %s (%d)\n",
					hackrf_error_name(result),
					result);
				return -1;
			}	
		}	
		

		if (transceiver_mode == TRANSCEIVER_MODE_RX) {
			result = hackrf_set_vga_gain(device, 0);
			result |= hackrf_set_lna_gain(device, 0);
			result |= hackrf_start_rx(device, rx_callback, NULL);
		} else {
			result = hackrf_set_vga_gain(device, vga_gain);
			result |= hackrf_set_lna_gain(device, lna_gain);
			result |= hackrf_start_rx(device, rx_callback, NULL);
		}

		if (result != HACKRF_SUCCESS) {
			fprintf(stderr,
				"hackrf_start_rx() failed: %s (%d)\n",
				hackrf_error_name(result),
				result);
			return -1;
		}

		// Wait for SIGALRM from interval timer, or another signal.
		pause();

		/* Read and reset both totals at approximately the same time. */
		byte_count_now = byte_count;
		byte_count = 0;

		if (!((byte_count_now == 0))) {
			fprintf(stderr, "Name file RDY: %d\n", i);				
		}

		if ((byte_count_now == 0)) {
			fprintf(stderr,
				"Couldn't transfer any bytes for one second.\n");
			break;
		}	

		// Stop interval timer.
		result = hackrf_is_streaming(device);
		if (do_exit) {
			fprintf(stderr, "Exiting...\n");
		} else {
			fprintf(stderr,
				"Exiting... device_is_streaming() result: %s (%d)\n",
				hackrf_error_name(result),
				result);
		}

		result = hackrf_stop_rx(device);
		if (result != HACKRF_SUCCESS) {
			fprintf(stderr,
				"stop_rx() failed: %s (%d)\n",
				hackrf_error_name(result),
				result);
		} else {
			fprintf(stderr, "stop_rx() done\n");
		}		

		if (file != NULL) {
			if (file != stdin) {
				fflush(file);
			}
			if ((file != stdout) && (file != stdin)) {
				fclose(file);
				file = NULL;
				fprintf(stderr, "fclose() done\n");
			}
		}

		result = hackrf_close(device);
		if (result != HACKRF_SUCCESS) {
			fprintf(stderr,
				"device_close() failed: %s (%d)\n",
				hackrf_error_name(result),
				result);
		} else {
			fprintf(stderr, "device_close() done\n");
		}			
	}

	if (device != NULL) 
	{
		hackrf_exit();
		fprintf(stderr, "device_exit() done\n");
	}

	fprintf(stderr, "exit\n");
	return 0;
}

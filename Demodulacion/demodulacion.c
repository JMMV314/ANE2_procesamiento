#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <complex.h>
#include <string.h>
#include <libhackrf/hackrf.h>
#include <fftw3.h>
#include <portaudio.h>
#include <pthread.h>
#include <unistd.h>
#include <termios.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// --- CONFIGURACIÓN ---
#define SAMPLE_RATE 2000000.0  
#define FREQ_CENTER 101200000  
#define AUDIO_RATE 48000.0
#define CIRCULAR_BUFFER_SIZE (4 * 1024 * 1024) 
#define PROCESS_BLOCK_SIZE 32768 

// Configuración PFB
#define PFB_M 1024              // Tamaño FFT
#define PFB_T 4                 // Taps por canal
#define KAISER_BETA 8.0         
#define PFB_L (PFB_M * PFB_T)   

#define UDP_IP "127.0.0.1"
#define UDP_PORT 9999

// --- ESTRUCTURAS ---
typedef struct {
    float complex *buffer;
    volatile size_t head;
    volatile size_t tail;
} ring_buffer_t;

// Contexto PFB Persistente
typedef struct {
    double *h;              
    double *poly[PFB_T];    
    fftw_complex *fft_in;
    fftw_complex *fft_out;
    fftw_plan plan;
    double *p_out_accum;    
    float *udp_buffer;      
} pfb_context_t;

// Globales
ring_buffer_t ring;
pfb_context_t pfb_ctx;
int audio_enabled = 0;
hackrf_device* dev = NULL;
volatile int keep_running = 1;

// Ganancias Controlables
volatile int current_lna = 32;
volatile int current_vga = 20;

// Red
int sockfd;
struct sockaddr_in server_addr;

// --- UTILS TECLADO (NON-BLOCKING) ---
struct termios orig_termios;
void reset_terminal_mode() { tcsetattr(0, TCSANOW, &orig_termios); }
void set_conio_terminal_mode() {
    struct termios new_termios; tcgetattr(0, &orig_termios);
    memcpy(&new_termios, &orig_termios, sizeof(new_termios));
    atexit(reset_terminal_mode); cfmakeraw(&new_termios);
    tcsetattr(0, TCSANOW, &new_termios);
}
int kbhit() {
    struct timeval tv = { 0L, 0L }; fd_set fds; FD_ZERO(&fds);
    FD_SET(0, &fds); return select(1, &fds, NULL, NULL, &tv);
}
int getch() {
    int r; unsigned char c;
    if ((r = read(0, &c, sizeof(c))) < 0) return r; else return c;
}

// Hilo para leer teclado y ajustar ganancias
void* input_worker(void* arg) {
    while(keep_running) {
        if (kbhit()) {
            char c = getch();
            if (c == 3 || c == 'q') { keep_running = 0; break; } 
            
            int changed = 0;
            if (c == 'w' || c == 'W') { if(current_lna < 40) current_lna += 8; changed = 1; }
            else if (c == 's' || c == 'S') { if(current_lna > 0) current_lna -= 8; changed = 1; }
            else if (c == 'd' || c == 'D') { if(current_vga < 62) current_vga += 2; changed = 1; }
            else if (c == 'a' || c == 'A') { if(current_vga > 0) current_vga -= 2; changed = 1; }

            if (changed && dev) {
                hackrf_set_lna_gain(dev, current_lna);
                hackrf_set_vga_gain(dev, current_vga);
                printf("\r[CONTROL] LNA: %2d dB | VGA: %2d dB   ", current_lna, current_vga);
                fflush(stdout);
            }
        }
        usleep(20000); 
    }
    return NULL;
}

// --- MATEMÁTICAS PFB Y SHIFT ---

static double bessi0(double x) {
    double sum = 1.0, y = x * x / 4.0, t = y; int k = 1;
    while (t > 1e-12) { sum += t; k++; t *= y / (k * k); }
    return sum;
}

static void generate_kaiser_proto(double* h, int len, double beta) {
    double denom = bessi0(beta);
    for (int n = 0; n < len; n++) {
        double x = 2.0 * n / (len - 1) - 1.0;
        h[n] = bessi0(beta * sqrt(1 - x * x)) / denom;
    }
}

// --- CORRECCIÓN CRÍTICA: FFT SHIFT ---
// Ordena el array para que vaya de -BW/2 a +BW/2
void fft_shift_float(float* data, int n) {
    int half = n / 2;
    float temp;
    for (int i = 0; i < half; i++) {
        temp = data[i];
        data[i] = data[i + half];
        data[i + half] = temp;
    }
}

// --- INIT & PROCESS ---

void init_pfb() {
    pfb_ctx.h = (double*)malloc(PFB_L * sizeof(double));
    for(int t=0; t<PFB_T; t++) pfb_ctx.poly[t] = (double*)malloc(PFB_M * sizeof(double));
    pfb_ctx.fft_in  = fftw_alloc_complex(PFB_M);
    pfb_ctx.fft_out = fftw_alloc_complex(PFB_M);
    pfb_ctx.p_out_accum = (double*)malloc(PFB_M * sizeof(double));
    pfb_ctx.udp_buffer = (float*)malloc(PFB_M * sizeof(float));

    generate_kaiser_proto(pfb_ctx.h, PFB_L, KAISER_BETA);
    for (int t = 0; t < PFB_T; t++) 
        for (int m = 0; m < PFB_M; m++) 
            pfb_ctx.poly[t][m] = pfb_ctx.h[t * PFB_M + m];

    pfb_ctx.plan = fftw_plan_dft_1d(PFB_M, pfb_ctx.fft_in, pfb_ctx.fft_out, FFTW_FORWARD, FFTW_ESTIMATE);
}

void process_pfb_and_send(float complex *input_chunk, int n_samples) {
    memset(pfb_ctx.p_out_accum, 0, PFB_M * sizeof(double));
    
    // Calculamos cuántos bloques PFB caben en el chunk de audio
    int blocks = (n_samples - PFB_L) / PFB_M;
    if (blocks <= 0) return;

    for (int b = 0; b < blocks; b++) {
        memset(pfb_ctx.fft_in, 0, PFB_M * sizeof(fftw_complex));
        for (int t = 0; t < PFB_T; t++) {
            size_t offset = b * PFB_M + t * PFB_M; 
            for (int m = 0; m < PFB_M; m++) {
                // Convertir float complex a double complex
                double r = crealf(input_chunk[offset + m]);
                double i = cimagf(input_chunk[offset + m]);
                pfb_ctx.fft_in[m] += (r + I*i) * pfb_ctx.poly[t][m];
            }
        }
        fftw_execute(pfb_ctx.plan);
        for (int k = 0; k < PFB_M; k++) {
            double mag2 = creal(pfb_ctx.fft_out[k])*creal(pfb_ctx.fft_out[k]) + cimag(pfb_ctx.fft_out[k])*cimag(pfb_ctx.fft_out[k]);
            pfb_ctx.p_out_accum[k] += mag2;
        }
    }
    
    // Promedio y Logaritmo
    double scale = 1.0 / (blocks * SAMPLE_RATE * PFB_M); 
    for (int k = 0; k < PFB_M; k++) {
        pfb_ctx.udp_buffer[k] = (float)(10.0 * log10(pfb_ctx.p_out_accum[k] * scale + 1e-20)); 
    }

    // --- APLICAR CORRECCIÓN DE ORDEN ---
    //fft_shift_float(pfb_ctx.udp_buffer, PFB_M);

    sendto(sockfd, pfb_ctx.udp_buffer, PFB_M * sizeof(float), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
}

// --- CALLBACK & MAIN LOOP ---

int rx_callback(hackrf_transfer* transfer) {
    for(int i = 0; i < transfer->valid_length; i += 2) {
        float i_val = (float)((int8_t*)transfer->buffer)[i] / 128.0f;
        float q_val = (float)((int8_t*)transfer->buffer)[i+1] / 128.0f;
        ring.buffer[ring.head] = i_val + I * q_val;
        size_t next = (ring.head + 1) % CIRCULAR_BUFFER_SIZE;
        if (next == ring.tail) ring.tail = (ring.tail + 1) % CIRCULAR_BUFFER_SIZE; // Push tail if full
        ring.head = next;
    }
    return 0;
}

void process_block(float complex *input, int length, PaStream *stream) {
    // 1. PSD VISUAL (PFB)
    process_pfb_and_send(input, length);

    // 2. AUDIO (FM Demod)
    if (audio_enabled && stream) {
        int decim = 10;
        int audio_len = length / decim;
        static float *audio_out = NULL; 
        if (!audio_out) audio_out = malloc(length * sizeof(float)); // Simple alloc once
        
        static float complex prev = 0;
        float ratio = 200000.0 / AUDIO_RATE; 
        int out_idx = 0;

        for(int i=0; i<audio_len; i++) {
            float complex curr = input[i * decim];
            float demod = cargf(curr * conjf(prev)) * 0.15f; 
            prev = curr;
            
            if (i % (int)ratio == 0 && out_idx < length) {
                audio_out[out_idx++] = demod;
            }
        }
        Pa_WriteStream(stream, audio_out, out_idx);
    }
}

int main() {
    // Setup Socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    server_addr.sin_family = AF_INET; server_addr.sin_port = htons(UDP_PORT);
    server_addr.sin_addr.s_addr = inet_addr(UDP_IP);

    init_pfb();

    // Setup Buffer & HackRF
    ring.buffer = (float complex*)malloc(CIRCULAR_BUFFER_SIZE * sizeof(float complex));
    hackrf_init(); hackrf_open(&dev);
    hackrf_set_sample_rate(dev, SAMPLE_RATE);
    hackrf_set_freq(dev, FREQ_CENTER);
    hackrf_set_lna_gain(dev, current_lna); 
    hackrf_set_vga_gain(dev, current_vga);
    hackrf_set_amp_enable(dev, 0);

    // Setup Audio
    Pa_Initialize(); PaStream *stream = NULL;
    Pa_OpenDefaultStream(&stream, 0, 1, paFloat32, AUDIO_RATE, paFramesPerBufferUnspecified, NULL, NULL);
    if(stream) Pa_StartStream(stream); audio_enabled = (stream != NULL);

    hackrf_start_rx(dev, rx_callback, NULL);
    float complex *proc_buf = malloc(PROCESS_BLOCK_SIZE * sizeof(float complex));

    // Setup Teclado
    set_conio_terminal_mode();
    pthread_t kb_thread;
    pthread_create(&kb_thread, NULL, input_worker, NULL);

    printf("\n--- RADIO PFB FINAL ---\n");
    printf("Controles: [W/S] LNA | [A/D] VGA | [Q] Salir\n");

    while(keep_running) {
        size_t available = (ring.head >= ring.tail) ? (ring.head - ring.tail) : (CIRCULAR_BUFFER_SIZE - ring.tail + ring.head);
        
        // --- LOGICA DE LATENCIA (CATCH-UP) ---
        if (available > CIRCULAR_BUFFER_SIZE * 0.7) {
             printf("O"); fflush(stdout);
             size_t saltar = available - PROCESS_BLOCK_SIZE; 
             ring.tail = (ring.tail + saltar) % CIRCULAR_BUFFER_SIZE;
             available = PROCESS_BLOCK_SIZE;
        }

        if (available >= PROCESS_BLOCK_SIZE) {
            for(int i=0; i<PROCESS_BLOCK_SIZE; i++) {
                proc_buf[i] = ring.buffer[ring.tail];
                ring.tail = (ring.tail + 1) % CIRCULAR_BUFFER_SIZE;
            }
            process_block(proc_buf, PROCESS_BLOCK_SIZE, stream);
        } else {
            usleep(500);
        }
    }

    // Cleanup
    reset_terminal_mode();
    hackrf_stop_rx(dev); hackrf_close(dev); hackrf_exit();
    if(stream) Pa_CloseStream(stream); Pa_Terminate();
    free(pfb_ctx.h); free(ring.buffer); free(proc_buf);
    return 0;
}
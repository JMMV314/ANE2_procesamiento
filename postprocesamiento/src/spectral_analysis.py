# **Servicio único**: ruido + picos + potencia + ocupación (OBW 99%, −XdB)

import math
from typing import Dict, List

import numpy as np

from .spectrum_frame import SpectrumFrame


def estimate_noise_floor(frame: SpectrumFrame) -> float: #Estima un piso de ruido. Toma la moda con tolerancia +-3, y si hay más de 8 emisiones, usa la media, si no, la mediana
    """Estimar de forma básica el piso de ruido global del espectro provisto.

    Args:
        frame: Ventana espectral sobre la cual se requiere inferir el nivel de ruido.

    Returns:
        Valor en dBm representativo del piso de ruido. La implementación futura definirá
        el enfoque (promedio, percentil, etc.) y servirá como entrada para detectar
        emisiones o calcular SNR.

    Uso previsto: escenarios donde no se necesite un método robusto o como fallback
    para las variantes avanzadas de estimación.
    """

    x = np.asarray(frame.amplitudes_dbm, dtype=float)

    # Modo por histograma -> ventana ±3 dB alrededor del modo -> media
    bins = min(256, max(32, int(np.sqrt(x.size)))) #
    counts, edges = np.histogram(x, bins=bins)
    i = int(np.argmax(counts))
    mode = (edges[i] + edges[i + 1]) / 2.0 #Moda

    win_db = 6.0
    mask = (x >= mode - win_db / 2) & (x <= mode + win_db / 2) #Umbral en la moda +-3dBs
    if mask.sum() >= 8: #Si hay más de 8 emisiones
        return float(x[mask].mean())
    # Fallback: mediana
    return float(np.median(x))

def estimate_noise_floor_robust(
    frame: SpectrumFrame, method: str = "median") -> float: # Se deja para MC
    """Calcular el piso de ruido mediante técnicas robustas seleccionables.

    Args:
        frame: Datos espectrales en los que se medirá el ruido de fondo.
        method: Estrategia a utilizar (p. ej. ``"median"`` para median-of-minima,
            ``"percentile"`` para k-percentile o ``"histogram"`` para enfoques basados
            en histogramas).

    Returns:
        Nivel de ruido estimado en dBm conforme al método elegido, pensado para soportar
        señales con interferencias o emisiones fuertes cercanas.

    Uso previsto: reemplazo del estimador simple cuando se requiera resiliencia a picos
    espurios o entornos con alta variabilidad.
    """

    x = np.asarray(frame.amplitudes_dbm, dtype=float)
    m = (method or "median").lower()

    if m == "median":
        return float(np.median(x))

    if m == "percentile":
        # Percentil bajo robusto frente a colas/picos
        return float(np.percentile(x, 25.0))

    if m == "histogram":
        bins = min(256, max(32, int(np.sqrt(x.size))))
        counts, edges = np.histogram(x, bins=bins)
        i = int(np.argmax(counts))
        return float((edges[i] + edges[i + 1]) / 2.0)

    # Fallback robusto (t-Student IRLS liviano)
    mu = np.median(x)
    r = x - mu
    mad = np.median(np.abs(r)) + 1e-12
    sigma2 = (1.4826 * mad) ** 2
    nu = 6.0
    tol = 1e-5
    itmax = 50

    for _ in range(itmax):
        tval = (r * r) / max(sigma2, 1e-18)
        w = (nu + 1.0) / (nu + tval)
        wsum = np.sum(w) + 1e-18
        mu_new = float(np.sum(w * x) / wsum)
        r = x - mu_new
        tval = (r * r) / max(sigma2, 1e-18)
        w = (nu + 1.0) / (nu + tval)
        wsum = np.sum(w) + 1e-18
        sigma2_new = float(np.sum(w * r * r) / wsum)
        if (abs(mu_new - mu) <= tol * (abs(mu) + 1e-12)) and \
           (abs(sigma2_new - sigma2) <= tol * (sigma2 + 1e-12)):
            mu, sigma2 = mu_new, sigma2_new
            break
        mu, sigma2 = mu_new, max(1e-18, sigma2_new)
    return float(mu)

def detect_peak_bins(frame: SpectrumFrame) -> List[int]:
    """Localizar bins que funcionen como picos iniciales para análisis detallados.

    Args:
        frame: Espectro sobre el cual se identificarán máximos locales.

    Returns:
        Lista ordenada de índices de bins considerados picos. La lógica contemplará
        suavizado, comparación con el ruido y separación mínima entre picos.

    Uso previsto: alimentar mediciones como OBW, ancho a −XdB o potencia de canal a
    partir de la posición del pico principal.
    """

    y_dbm = np.asarray(frame.amplitudes_dbm, dtype=float)
    N = y_dbm.size

    # --- Suavizado ligero para reducir ruido puntual sin borrar emisiones estrechas ---
    kernel = np.array([0.25, 0.5, 0.25], dtype=float)
    y_smooth = np.convolve(y_dbm, kernel, mode="same")

    # Piso de ruido (mediana) para referencia de SNR
    nf = estimate_noise_floor(frame)

    # --- Umbral de semilla: un poco por encima del piso ---
    try:
        # Usamos adaptive_threshold pero con n_sigma bajo para no perder emisiones
        thr_seed = adaptive_threshold(frame, n_sigma=3.5) #Revisar que hace conceptualmente
    except Exception:
        # Fallback simple si algo falla: NF + 2 dB
        thr_seed = nf + 2.0

    # --- SNR mínimo para considerar un máximo como emisión real ---
    min_snr_db = 0.73 # el pico debe estar al menos 2 dB sobre el piso global
    # min_snr_db = 0  # el pico debe estar al menos 2 dB sobre el piso global

    cand: List[int] = [] #Lista de candidatos

    # Recorremos segmentos donde la señal suavizada supera el umbral de semilla
    i = 0
    while i < N:
        if y_smooth[i] > thr_seed:
            s = i #Frecuencia inicial de pico
            while i < N and y_smooth[i] > thr_seed:
                i += 1
            e = i #Frecuencia final de pico

            if e > s:
                # En cada segmento, tomamos el máximo en la señal original
                seg = y_dbm[s:e]
                pk_local = int(np.argmax(seg) + s)
                amp = y_dbm[pk_local] #Extrae el valor ed potencia maximo del pico

                # Filtro por SNR mínimo
                if amp - nf >= min_snr_db:
                    cand.append(pk_local) #Agregar pico a los candidatos
        else:
            i += 1

    if len(cand) <= 1:
        return sorted(cand)

    # --- No-maximum suppression: imponer separación mínima entre picos ---
    # Ordenamos por amplitud descendente (picos más fuertes primero)
    cand = sorted(cand, key=lambda idx: y_dbm[idx], reverse=True)

    # Separación mínima en bins (~0.1 % del span, pero nunca menos de 3 bins)
    min_sep_bins = max(3, int(0.001 * N))

    kept: List[int] = []
    taken = np.zeros(N, dtype=np.bool_)
    for idx in cand:
        left = max(0, idx - min_sep_bins)
        right = min(N, idx + min_sep_bins)
        if not taken[left:right].any():
            kept.append(idx)
            taken[left:right] = True

    return sorted(kept)

def measure_emission_power( #Calcula la potencia integrada, revisar metricas
    frame: SpectrumFrame, f_center_hz: float, metric: str) -> Dict[str, float]:
    """Calcular métricas de potencia y ocupación para una emisión centrada en ``f_center_hz``.

    Args:
        frame: Medición espectral que contiene la emisión de interés.
        f_center_hz: Frecuencia objetivo alrededor de la cual se localizará el pico
            principal para integrar potencia y calcular anchuras.
        metric: Variante a aplicar (p. ej. ``"obw"`` para potencia acumulada, ``"xdb``
            para anchos a −XdB); permitirá reutilizar la función en distintos reportes.

    Returns:
        Diccionario con métricas específicas (potencia integrada, OBW 99 %, anchos a
        3/10/26 dB, etc.). La implementación decidirá qué claves se rellenan según
        ``metric`` y cómo se formatean los valores.

    Uso previsto: centralizar el cómputo de métricas de emisiones detectadas a fin de
    generar reportes de conformidad u optimización de espectro.
    """

    y_dbm = np.asarray(frame.amplitudes_dbm, dtype=float)
    N = y_dbm.size
    if hasattr(frame, "freq_hz") and frame.freq_hz is not None:
        f = np.asarray(frame.freq_hz, dtype=float)
    else:
        f = np.linspace(float(frame.f_start_hz), float(frame.f_stop_hz), N)

    if hasattr(frame, "bin_hz") and frame.bin_hz:
        df = float(frame.bin_hz)
    else:
        df = float((f[-1] - f[0]) / max(1, N - 1))

    # Pico más cercano al centro solicitado
    peak_idx = int(np.argmin(np.abs(f - float(f_center_hz))))

    # Potencia integrada en ventana local (aprox) alrededor del pico
    p_w_hz = 10 ** ((y_dbm - 30.0) / 10.0)  # dBm/Hz -> W/Hz
    K = max(1, int(0.01 * N))               # ±1% del total de bins como ventana simple
    sl = slice(max(0, peak_idx - K), min(N, peak_idx + K + 1))
    p_w = p_w_hz[sl].sum() * max(df, 1.0)
    out: Dict[str, float] = {"power_dBm": 10.0 * np.log10(max(p_w, 1e-18)) + 30.0}

    m = (metric or "").lower()
    if m == "obw":
        out["obw_percent"] = 99.0  # el ancho exacto lo obtienes con measure_obw en tests
    elif m == "xdb":
        out["xdb_ref_dB"] = 3.0    # el ancho exacto lo obtienes con measure_bandwidth_xdb
    return out

def measure_bandwidth_xdb(
    frame: SpectrumFrame, peak_idx: int, x_db: float = 3.0
) -> float:
    """
    Determina el ancho de banda a -x dB respecto al pico usando:
      - Suavizado ligero
      - Cruces exactos por interpolación lineal
      - Detección robusta de las dos primeras intersecciones reales

    Esto es SIGNIFICATIVAMENTE más estable frente a ruido
    y funciona bien en señales reales FM/AM/LTE/WiFi.
    """

    y = np.asarray(frame.amplitudes_dbm, dtype=float)
    N = len(y)

    if hasattr(frame, "freq_hz") and frame.freq_hz is not None:
        f = np.asarray(frame.freq_hz, dtype=float)
    else:
        f = np.linspace(frame.f_start_hz, frame.f_stop_hz, N)

    pk = int(np.clip(peak_idx, 0, N - 1))
    peak_val = y[pk]
    thr = peak_val - float(x_db)

    # -------------------------------------------------------
    # 1) SUAVIZADO LIGERO (sin destruir estructura)
    # -------------------------------------------------------
    kernel = np.array([0.25, 0.5, 0.25])
    y_s = np.convolve(y, kernel, mode="same")

    # -------------------------------------------------------
    # 2) Buscamos todos los cruces de y_s con el umbral
    #    f_cross = f[i] + (thr - y_s[i]) * (f[i+1]-f[i])/(y_s[i+1]-y_s[i])
    # -------------------------------------------------------
    fL = None
    fR = None

    for i in range(N - 1):
        yi, yj = y_s[i], y_s[i+1]

        # Si hay cruce (el umbral está entre yi y yj)
        if (yi - thr) == 0:
            # cruce exacto en bin
            fc = f[i]
        elif (yi - thr) * (yj - thr) < 0:
            # Interpolación lineal del cruce
            t = (thr - yi) / (yj - yi)
            t = np.clip(t, 0.0, 1.0)
            fc = f[i] + t * (f[i+1] - f[i])
        else:
            continue

        # Primer cruce a la izquierda del pico → fL
        if fL is None and fc < f[pk]:
            fL = fc

        # Primer cruce a la derecha del pico → fR
        if fR is None and fc > f[pk]:
            fR = fc

        if fL is not None and fR is not None:
            break

    # -------------------------------------------------------
    # 3) Casos degenerados
    # -------------------------------------------------------
    if fL is None:
        fL = f[0]
    if fR is None:
        fR = f[-1]

    bw = max(0.0, float(fR - fL))

    # Candado físico: no más grande que el span total
    return min(bw, float(f[-1] - f[0]))

def measure_obw(frame: SpectrumFrame,peak_idx: int,percentile: float = 99.0,x_db_window: float = 3.0,) -> float:
    """
    'OBW' definido como un porcentaje del ANCHO DE BANDA de la emisión
    alrededor del pico indicado, NO basado en potencia acumulada.

    Es decir:
        OBW = (percentile / 100) * BW_xdB

    donde BW_xdB es el ancho de banda a -x_db_window dB respecto del pico.

    Args:
        frame: Captura espectral con la emisión a analizar.
        peak_idx: Índice del pico de la emisión.
        percentile: Porcentaje del ancho de banda total de la emisión
            (p.ej. 99 => 99% del ancho de banda).
        x_db_window: Nivel relativo (en dB) para definir el ancho de banda
            de referencia (típicamente 3 dB).

    Returns:
        Ancho de banda en Hz correspondiente al porcentaje solicitado
        del BW a -x_db_window dB.
    """
    # Usamos tu función existente de BW a -XdB
    bw_total = measure_bandwidth_xdb(frame, peak_idx, x_db=x_db_window)

    if bw_total <= 0:
        return 0.0

    frac = float(percentile) / 100.0
    obw = bw_total * frac
    return float(obw)

def measure_channel_power(
    frame: SpectrumFrame, f_center: float, bw: float
) -> float:
    """
    Integrar la potencia contenida dentro de un canal centrado en ``f_center``.

    Se asume que las amplitudes están en PSD [dBm/Hz].
    La integración se hace en el dominio lineal (W/Hz) usando la
    regla del trapecio sobre el eje de frecuencias real.

    Args:
        frame: Datos espectrales que cubren el canal objetivo.
        f_center: Frecuencia central del canal a integrar.
        bw: Ancho de banda del canal (en Hz) que define los límites de integración.

    Returns:
        Potencia total del canal en dBm (señal+ruido) integrada sobre BW.
        Si no hay suficientes puntos dentro del canal, devuelve -inf.
    """

    y_dbm = np.asarray(frame.amplitudes_dbm, dtype=float)
    N = y_dbm.size
    if N < 2:
        return float("-inf")

    # Eje de frecuencias
    if getattr(frame, "freq_hz", None) is not None:
        f = np.asarray(frame.freq_hz, dtype=float)
    else:
        f = np.linspace(float(frame.f_start_hz), float(frame.f_stop_hz), N)

    # Validación ancho de banda
    f_center = float(f_center)
    bw = float(bw)
    if bw <= 0.0:
        return float("-inf")

    # Límites del canal, recortados al span disponible
    span_min = float(f[0])
    span_max = float(f[-1])
    fL = max(f_center - bw / 2.0, span_min)
    fR = min(f_center + bw / 2.0, span_max)

    if fR <= fL:
        return float("-inf")

    # Seleccionar muestras dentro del canal
    mask = (f >= fL) & (f <= fR)
    if mask.sum() < 2:
        return float("-inf")

    f_sel = f[mask]
    y_sel_dbm = y_dbm[mask]

    # dBm/Hz -> W/Hz en dominio lineal
    p_w_per_hz = 10.0 ** ((y_sel_dbm - 30.0) / 10.0)

    # Integración en frecuencia (W) usando trapecios
    p_w = float(np.trapz(p_w_per_hz, f_sel))

    if p_w <= 0.0:
        return float("-inf")

    # W -> dBm
    return 10.0 * np.log10(p_w) + 30.0

def compute_snr(frame: SpectrumFrame, peaks: List[int]) -> Dict[int, float]:
    """Calcular la relación señal/ruido (SNR) para cada pico o emisión identificada.

    Args:
        frame: Espectro de referencia para extraer potencias de señal y ruido.
        peaks: Lista de índices de bins que representan las señales a evaluar.

    Returns:
        Diccionario que asocia cada índice de pico con su SNR en dB, calculado a partir
        de la potencia de la señal frente al piso de ruido estimado.

    Uso previsto: cuantificar calidad de señales detectadas y alimentar decisiones de
    demodulación, asignación de espectro o validación de enlaces.
    """

    y_dbm = np.asarray(frame.amplitudes_dbm, dtype=float)
    nf = float(np.median(y_dbm))

    out: Dict[int, float] = {}
    for pk in peaks:
        idx = int(max(0, min(len(y_dbm) - 1, int(pk))))
        out[idx] = float(y_dbm[idx] - nf)
    return out

def adaptive_threshold(frame: SpectrumFrame, n_sigma: float = 3.0) -> float:
    """Generar un umbral dinámico a partir de estadísticos del piso de ruido.

    Args:
        frame: Medición espectral usada para estimar ruido y desviación estándar.
        n_sigma: Factor multiplicativo sobre la desviación estándar para posicionar el
            umbral por encima del piso de ruido.

    Returns:
        Nivel de umbral en dB que distinguirá ruido de señales significativas.

    Uso previsto: módulo auxiliar para detección automática de emisiones o filtrado de
    picos falsos mediante técnicas adaptativas.
    """
    x = np.asarray(frame.amplitudes_dbm, dtype=float)

    med = np.median(x)
    mad = np.median(np.abs(x - med)) + 1e-12
    sigma = 1.4826 * mad

    thr = float(med + float(n_sigma) * sigma)
    return thr

def find_emission_span(frame: SpectrumFrame,peak_idx: int,margin_db: float = 0.5) -> tuple[int, int]:
    """
    Devuelve los índices [L, R] que delimitan la emisión alrededor de un pico dado.

    Se expande desde `peak_idx` hacia la izquierda y derecha mientras la señal
    esté por encima de (NF + margin_db).
    """
    y_dbm = np.asarray(frame.amplitudes_dbm, dtype=float)
    N = y_dbm.size
    pk = int(max(0, min(N - 1, int(peak_idx))))

    # Piso de ruido global
    nf = estimate_noise_floor(frame)
    thr = nf + float(margin_db)

    # Izquierda
    L = pk
    while L > 0 and y_dbm[L] > thr:
        L -= 1

    # Derecha
    R = pk
    while R < N - 1 and y_dbm[R] > thr:
        R += 1

    # Aseguramos que L < R y recortamos dentro de [0, N-1]
    L = max(0, L)
    R = min(N - 1, R)
    if R <= L:
        # fallback mínimo de 3 bins
        L = max(0, pk - 1)
        R = min(N - 1, pk + 1)

    return L, R

def slice_spectrum_frame(frame: SpectrumFrame, L: int, R: int) -> SpectrumFrame:
    """
    Crea un SpectrumFrame nuevo recortado a los índices [L, R].
    """
    L = int(L)
    R = int(R)
    amps = frame.amplitudes_dbm[L:R+1]
    freqs = frame.freq_hz[L:R+1]
    return SpectrumFrame(
        amplitudes_dbm=amps,
        f_start_hz=float(freqs[0]),
        f_stop_hz=float(freqs[-1]),
        freq_hz=freqs,
        bin_hz=frame.bin_hz,  # misma resolución
    )

def estimate_ber_in_band_mqam(frame: SpectrumFrame,f_start_hz: float,f_stop_hz: float,M: int) -> Dict[str, float]:
    """
    Estimar BER teórico M-QAM dentro de una banda dada [f_start_hz, f_stop_hz].

    Flujo:
      1) Se toma el centro: f_c = (f_start_hz + f_stop_hz)/2.
      2) Se define una sub-banda fija de ±3 MHz alrededor de f_c.
      3) Se recorta el SpectrumFrame a esa sub-banda.
      4) Dentro de esa porción se detectan picos y se elige el pico principal.
      5) Se calcula el SNR de ese pico en la sub-banda.
      6) A partir del SNR se estima el BER M-QAM (canal AWGN).

    Args:
        frame: Espectro completo.
        f_start_hz: Frecuencia inicial nominal de la banda.
        f_stop_hz: Frecuencia final nominal de la banda.
        M: Orden de modulación QAM (4, 16, 64, ...).

    Returns:
        Diccionario con:
          - "f_center_hz"     : centro nominal de la banda
          - "band_start_hz"   : inicio efectivo de la sub-banda usada
          - "band_stop_hz"    : fin efectivo de la sub-banda usada
          - "snr_db"          : SNR estimado en esa sub-banda
          - "ber_est"         : BER teórico estimado
    """
    # Eje de frecuencias completo
    f = np.asarray(frame.freq_hz, dtype=float)
    if f.size == 0:
        raise ValueError("SpectrumFrame no contiene frecuencias.")

    # 1) Centro de la banda nominal
    f_start_hz = float(f_start_hz)
    f_stop_hz = float(f_stop_hz)
    f_center = 0.5 * (f_start_hz + f_stop_hz)

    # 2) Definir sub-banda fija ±3 MHz alrededor del centro
    half_bw = 3e6  # 3 MHz a cada lado
    fL = f_center - half_bw
    fR = f_center + half_bw

    # 3) Ajustar a los límites reales del espectro
    fL = max(fL, f[0])
    fR = min(fR, f[-1])
    if fR <= fL:
        raise ValueError("La sub-banda calculada queda vacía dentro del espectro.")

    # Encontrar índices [L, R] que cubren esa sub-banda
    mask = (f >= fL) & (f <= fR)
    idx = np.where(mask)[0]
    if idx.size == 0:
        raise ValueError("No hay bins dentro de la sub-banda especificada.")

    L = int(idx[0])
    R = int(idx[-1])

    # 4) Crear un sub_frame solo con esa porción del espectro
    sub_frame = slice_spectrum_frame(frame, L, R)

    # 5) Detectar picos dentro de la sub-banda
    local_peaks = detect_peak_bins(sub_frame)
    if len(local_peaks) == 0:
        raise ValueError("No se detectaron picos en la sub-banda recortada.")

    # Elegimos como pico principal el de mayor amplitud local
    y_sub = np.asarray(sub_frame.amplitudes_dbm, dtype=float)
    pk_local = max(local_peaks, key=lambda i: y_sub[i])

    # SNR en la sub-banda (usando compute_snr sobre el sub-frame)
    snr_dict_sub = compute_snr(sub_frame, [pk_local])
    snr_db = float(snr_dict_sub[pk_local])

    # 6) SNR -> BER M-QAM (fórmula teórica en canal AWGN)
    M = int(M)
    if M < 4:
        raise ValueError("M debe ser >= 4 para M-QAM cuadrada.")

    k = math.log2(M)
    snr_lin = 10.0 ** (snr_db / 10.0)

    # Argumento de Q()
    x = math.sqrt(3.0 * k * snr_lin / (M - 1.0))
    Qx = 0.5 * math.erfc(x / math.sqrt(2.0))

    Ps = 2.0 * (1.0 - 1.0 / math.sqrt(M)) * Qx  # prob. de símbolo erróneo
    Pb = Ps / k                                 # BER aproximado

    # Limitar rango
    Pb = float(min(max(Pb, 0.0), 0.5))

    return {
        "f_center_hz": f_center,
        "band_start_hz": fL,
        "band_stop_hz": fR,
        "snr_db": snr_db,
        "ber_est": Pb,
    }


def estimate_mer_from_ber_mqam(ber: float, M: int) -> float:
    """
    Estimar MER (dB) a partir de un BER teórico para M-QAM cuadrada.

    Se usa una aproximación tipo:
        MER_dB ≈ 10*log10(1 / BER)

    Esto interpreta MER como relación señal-a-error en la constelación.
    Es una aproximación práctica cuando solo se dispone de BER y el
    modelo teórico subyacente es M-QAM en AWGN.

    Args:
        ber: BER teórico (0 < ber < 0.5).
        M: Orden de la modulación (no se usa directamente aquí,
           pero se mantiene por coherencia de interfaz).

    Returns:
        MER aproximado en dB.
    """
    ber = float(ber)
    if ber <= 0.0:
        return 99.0  # límite superior arbitrario
    if ber >= 0.5:
        return 0.0   # enlace destruido

    mer_db = 10.0 * math.log10(1.0 / ber)
    return float(mer_db)

def measure_emission_parameters(
    frame: SpectrumFrame,
    fc: float,
    xdb: float = 3.0,
    obw_percent: float = 99.0
) -> Dict[str, float]:
    """
    Mide los parámetros principales de una emisión en torno a una frecuencia central dada.

    Usa internamente:
      - measure_bandwidth_xdb  : ancho a -XdB
      - measure_obw            : OBW como % del BW_xdB
      - measure_channel_power  : integración de potencia sobre BW u OBW
      - estimate_noise_floor_robust : piso de ruido (para SNR)
      - compute_snr            : SNR en el pico

    Retorna:
        {
          "fc_hz": frecuencia central efectiva (Hz, bin del pico más cercano)
          "power_dbm": potencia integrada de la emisión (dBm) sobre BW_xdB,
          "bandwidth_xdb_hz": ancho de banda a -XdB (Hz),
          "obw_hz": ancho de banda equivalente al obw_percent % (Hz),
          "channel_power_dbm": potencia dentro del OBW (dBm),
          "snr_db": SNR estimado en el pico (dB),
        }
    """

    # --- Espectro y eje de frecuencias ---
    y_dbm = np.asarray(frame.amplitudes_dbm, dtype=float)
    N = y_dbm.size

    if frame.freq_hz is not None:
        f = np.asarray(frame.freq_hz, dtype=float)
    else:
        f = np.linspace(float(frame.f_start_hz), float(frame.f_stop_hz), N)

    if frame.bin_hz:
        df = float(frame.bin_hz)
    else:
        df = float((f[-1] - f[0]) / max(1, N - 1))

    # --- Pico más cercano a fc ---
    fc = float(fc)
    pk = int(np.argmin(np.abs(f - fc)))
    pk = int(np.clip(pk, 0, N - 1))
    fc_eff = float(f[pk])

    # --- Ancho a -XdB (usa tu función robusta) ---
    bw_xdb = measure_bandwidth_xdb(frame, pk, x_db=xdb)
    if bw_xdb <= 0.0:
        return {
            "fc_hz": fc_eff,
            "power_dbm": float("-inf"),
            "bandwidth_xdb_hz": 0.0,
            "obw_hz": 0.0,
            "channel_power_dbm": float("-inf"),
            "snr_db": float("nan"),
        }

    # --- OBW como % del BW_xdB (usa tu función measure_obw) ---
    obw_hz = measure_obw(
        frame,
        peak_idx=pk,
        percentile=obw_percent,
        x_db_window=xdb,
    )
    if obw_hz <= 0.0:
        obw_hz = bw_xdb * float(obw_percent) / 100.0

    # --- Piso de ruido SOLO para SNR ---
    nf_dbm = estimate_noise_floor_robust(frame, method="histogram")

    # ---------------------------------------
    # 1) Potencia sobre BW_xdB (señal + ruido)
    # ---------------------------------------
    total_bw_dbm = measure_channel_power(frame, f_center=fc_eff, bw=bw_xdb)
    if np.isneginf(total_bw_dbm):
        power_dbm = float("-inf")
    else:
        power_dbm = float(total_bw_dbm)

    # ---------------------------------------
    # 2) Potencia de canal dentro del OBW
    # ---------------------------------------
    total_ch_dbm = measure_channel_power(frame, f_center=fc_eff, bw=obw_hz)
    if np.isneginf(total_ch_dbm):
        channel_power_dbm = float("-inf")
    else:
        channel_power_dbm = float(total_ch_dbm)

    # --- SNR usando compute_snr (sobre el frame actual) ---
    snr_dict = compute_snr(frame, [pk])
    snr_db = float(snr_dict.get(pk, float("nan")))

    return {
        "fc_hz": fc_eff,
        "power_dbm": power_dbm,
        "bandwidth_xdb_hz": bw_xdb,
        "obw_hz": obw_hz,
        "channel_power_dbm": channel_power_dbm,
        "snr_db": snr_db,
    }

import numpy as np
import matplotlib.pyplot as plt
import os

# --- 1. TUS FUNCIONES ORIGINALES (Tal cual las enviaste) ---
def comparar_psds(psd_ref_dbm, psd_sens_dbm, alpha=0.05):
    # Validación básica
    if len(psd_ref_dbm) != len(psd_sens_dbm):
        raise ValueError(f"Longitudes distintas: Ref={len(psd_ref_dbm)}, Sens={len(psd_sens_dbm)}")

    psd_ref_dbm  = np.asarray(psd_ref_dbm,  dtype=float)
    psd_sens_dbm = np.asarray(psd_sens_dbm, dtype=float)

    # dB -> mW
    psd1_lin = 10.0 ** (psd_ref_dbm  / 10.0)
    psd2_lin = 10.0 ** (psd_sens_dbm / 10.0)

    # Energía total
    total_pwr1 = np.sum(psd1_lin)
    total_pwr2 = np.sum(psd2_lin)
    diff_db = 10.0 * np.log10(total_pwr2 / total_pwr1)

    # PDFs y CDFs
    pdf1 = psd1_lin / total_pwr1
    pdf2 = psd2_lin / total_pwr2
    cdf1 = np.cumsum(pdf1)
    cdf2 = np.cumsum(pdf2)

    # KS
    ks_stat_D = float(np.max(np.abs(cdf1 - cdf2)))
    n = len(psd_ref_dbm)
    critical_value = float(1.36 / np.sqrt(n))

    # p-value aprox
    def ks_p_value(D, n):
        if D <= 0.0: return 1.0
        x = -2.0 * (D ** 2) * n
        p = 0.0
        for k in range(1, 101):
            term = (-1) ** (k - 1) * np.exp(x * (k * k))
            p += term
            if abs(term) < 1e-10: break
        return float(max(min(2.0 * p, 1.0), 0.0))

    p_value = ks_p_value(ks_stat_D, n)
    is_different = ks_stat_D > critical_value
    ks_msg = "Distribuciones espectrales DISTINTAS" if is_different else "Distribuciones espectrales SIMILARES"

    # MAE y Correlación
    mae_db = float(np.mean(np.abs(psd_sens_dbm - psd_ref_dbm)))
    corr_matrix = np.corrcoef(psd_ref_dbm, psd_sens_dbm)
    corr_coeff = float(corr_matrix[0, 1])

    if corr_coeff >= 0.99: corr_cat = "Prácticamente idénticas (ρ≈1.00)"
    elif corr_coeff >= 0.90: corr_cat = "Muy similar (0.90–0.99)"
    elif corr_coeff >= 0.70: corr_cat = "Parcialmente similar (0.70–0.90)"
    else: corr_cat = "Formas diferentes (<0.70)"

    return {
        "ks_statistic_D": round(ks_stat_D, 6),
        "critical_value": round(critical_value, 6),
        "p_value":       round(p_value, 6),
        "reject_null":   is_different,
        "power_diff_db": round(diff_db, 3),
        "mae_db":        round(mae_db, 3),
        "corr_coeff":    round(corr_coeff, 6),
        "corr_category": corr_cat,
        "message":       ks_msg,
    }

def graficar_ks_espectral(freqs, psd1_dbm, psd2_dbm):
    # Recálculo rápido para graficar
    psd1_lin = 10 ** (psd1_dbm / 10.0)
    psd2_lin = 10 ** (psd2_dbm / 10.0)
    cdf1 = np.cumsum(psd1_lin / np.sum(psd1_lin))
    cdf2 = np.cumsum(psd2_lin / np.sum(psd2_lin))
    
    diff_abs = np.abs(cdf1 - cdf2)
    idx_max_d = np.argmax(diff_abs)
    freq_at_max = freqs[idx_max_d]
    val_d = diff_abs[idx_max_d]

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 8), sharex=True)

    ax1.plot(freqs, psd1_dbm, label='Ref', color='blue', alpha=0.7)
    ax1.plot(freqs, psd2_dbm, label='Sensor', color='orange', alpha=0.7)
    ax1.set_title("1. Dominio Físico: PSD (dBm)")
    ax1.set_ylabel("Potencia (dBm)")
    ax1.legend()
    ax1.grid(True, linestyle=':', alpha=0.6)
    ax1.axvline(freq_at_max, color='red', linestyle=':', alpha=0.5)

    ax2.plot(freqs, cdf1, label='CDF Ref', color='blue')
    ax2.plot(freqs, cdf2, label='CDF Sensor', color='orange', linestyle='--')
    ax2.fill_between(freqs, cdf1, cdf2, color='gray', alpha=0.2)
    
    y_min, y_max = min(cdf1[idx_max_d], cdf2[idx_max_d]), max(cdf1[idx_max_d], cdf2[idx_max_d])
    ax2.plot([freq_at_max, freq_at_max], [y_min, y_max], color='red', linewidth=2, label=f'KS D={val_d:.4f}')
    
    ax2.set_title("2. Dominio Estadístico: CDF Acumulada")
    ax2.set_ylabel("Probabilidad Acumulada")
    ax2.set_xlabel("Frecuencia (Hz)")
    ax2.legend()
    ax2.grid(True, linestyle=':', alpha=0.6)

    plt.tight_layout()
    plt.show()

# --- 2. NUEVA FUNCIÓN PARA LEER CSV ---
def leer_espectro_csv(ruta_archivo):
    """
    Lee un CSV ignorando header y comentarios (#).
    Retorna: (frecuencias, potencias_dbm)
    """
    try:
        # np.loadtxt es robusto:
        # - comments='#': ignora todo lo que esté después de un #
        # - delimiter=',': asume separación por comas (cambiar si es ; o tab)
        # - skiprows=1: ignora la primera línea (el header)
        data = np.loadtxt(ruta_archivo, delimiter=',', comments='#', skiprows=1)
        
        # Columna 0: Frecuencia, Columna 1: Potencia
        freqs = data[:, 0]
        power = data[:, 1]
        
        return freqs, power
    except Exception as e:
        print(f"Error leyendo {ruta_archivo}: {e}")
        return None, None

# --- 3. BLOQUE PRINCIPAL DE EJECUCIÓN ---

# Define aquí tus archivos
archivo_ref = f'Calibración/extraccion/Samples/keysight_20251218_072409.csv'
archivo_sens = f'Calibración/extraccion/Samples/hackrf_20251218_072409_psd_pfb.csv'

# ¿Quieres generar archivos de prueba para ver si funciona? (Pon True si no tienes los archivos aún)
GENERAR_PRUEBA = True 

if GENERAR_PRUEBA:
    # Creamos dos archivos dummy para probar el código
    f = np.linspace(100e6, 110e6, 500)
    p_ref = -100 + 50 * np.exp(-0.5 * ((f - 105e6) / 1e6)**2) # Señal gaussiana limpia
    p_sens = p_ref - 3 + np.random.normal(0, 1, 500)          # Señal con offset y ruido
    
    np.savetxt(archivo_ref, np.column_stack((f, p_ref)), header="Freq,Power", delimiter=",", comments="# ")
    np.savetxt(archivo_sens, np.column_stack((f, p_sens)), header="Freq,Power", delimiter=",", comments="# ")
    print("Archivos de prueba generados.")

# 1. Cargar datos
print(f"Cargando {archivo_ref}...")
freq_ref, psd_ref = leer_espectro_csv(archivo_ref)

print(f"Cargando {archivo_sens}...")
freq_sens, psd_sens = leer_espectro_csv(archivo_sens)

if freq_ref is not None and freq_sens is not None:
    
    # 2. Verificar compatibilidad
    # Para comparar punto a punto, las frecuencias deben ser idénticas.
    # Aquí hacemos un chequeo simple de longitud y límites.
    if len(freq_ref) != len(freq_sens):
        print("¡ALERTA! Los archivos tienen diferente cantidad de puntos.")
        print(f"Ref: {len(freq_ref)}, Sens: {len(freq_sens)}")
        # Opcional: Podrías interpolar aquí si fuera necesario.
    elif not np.allclose(freq_ref, freq_sens):
        print("¡ALERTA! Las frecuencias no coinciden exactamente entre archivos.")
    else:
        # 3. Ejecutar comparación
        print("\n--- Resultados del Análisis ---")
        resultados = comparar_psds(psd_ref, psd_sens)
        
        # Imprimir bonito
        for k, v in resultados.items():
            print(f"{k}: {v}")
            
        # 4. Graficar
        print("\nGenerando gráfico...")
        graficar_ks_espectral(freq_ref, psd_ref, psd_sens)
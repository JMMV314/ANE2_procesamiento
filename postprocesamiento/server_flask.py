from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple, Union

from flask import Flask, request, jsonify

from src.processor import process_input

app = Flask(__name__)

# ---------------------------
# Helpers
# ---------------------------

META_KEYS = {
    "frame", "json_path", "cumplimiento", "picos", "lic", "corr", "dane", "municipio"
}

def parse_picos_arg(picos_arg: Optional[Union[str, List[Any]]]) -> List[float]:
    if picos_arg is None:
        return []
    if isinstance(picos_arg, list):
        out: List[float] = []
        for x in picos_arg:
            try:
                out.append(float(x))
            except Exception:
                pass
        return out

    s = str(picos_arg).strip()
    if s == "":
        return []

    # JSON list style: "[92.86, 95.9]"
    if s.startswith("[") and s.endswith("]"):
        try:
            arr = json.loads(s)
            if isinstance(arr, list):
                return [float(x) for x in arr]
        except Exception:
            pass

    # CSV style: "92.86,95.9"
    parts = [p.strip() for p in s.split(",") if p.strip() != ""]
    out2: List[float] = []
    for p in parts:
        try:
            out2.append(float(p))
        except Exception:
            pass
    return out2

def normalize_dane(d: Optional[Any]) -> Optional[str]:
    if d is None:
        return None
    s = str(d).strip()
    if s == "":
        return None
    # Si llega como "11001.0" por conversiones, lo limpiamos
    if s.replace(".", "", 1).isdigit() and "." in s:
        try:
            s2 = str(int(float(s)))
            return s2
        except Exception:
            return s
    return s

def is_wrapper_object(body: Any) -> bool:
    """Wrapper: dict que contiene metadatos (cumplimiento/dane/picos/etc)."""
    if not isinstance(body, dict):
        return False
    return any(k in body for k in META_KEYS)

def load_frame_from_json_path(json_path: str) -> Any:
    p = Path(json_path)
    with p.open("r", encoding="utf-8") as f:
        return json.load(f)

def get_defaults() -> Tuple[Optional[str], Optional[str], bool]:
    """
    Defaults del servidor (se setean al arrancar).
    - default_lic_csv: ruta CSV licencias
    - default_corr_csv: ruta CSV correcciones
    - allow_json_path: permitir que el cliente mande json_path (NO recomendado en prod)
    """
    return (
        app.config.get("DEFAULT_LIC_CSV"),
        app.config.get("DEFAULT_CORR_CSV"),
        bool(app.config.get("ALLOW_JSON_PATH", False)),
    )


# ---------------------------
# Routes
# ---------------------------

@app.get("/health")
def health():
    return jsonify({"ok": True})

@app.post("/analyze")
def analyze():
    """
    Recomendado (modo ANE/sensores):
    {
      "frame": { ...JSON COMPLETO DE MEDICIÓN... },
      "cumplimiento": 1,
      "dane": "11001",
      "picos": [92.86, 95.9]    // opcional
    }

    Compatibilidad local (NO recomendado en prod):
    {
      "json_path": "C:\\ruta\\archivo.json",
      "cumplimiento": 1,
      "dane": "11001"
    }
    -> solo si el servidor se arrancó con --allow-json-path
    """
    try:
        body = request.get_json(force=True, silent=False)

        default_lic_csv, default_corr_csv, allow_json_path = get_defaults()

        # 1) Determinar frame + meta
        frame_json: Any
        meta: Dict[str, Any]

        if is_wrapper_object(body) and isinstance(body, dict) and "frame" in body:
            meta = body
            frame_json = body["frame"]

        elif is_wrapper_object(body) and isinstance(body, dict) and "json_path" in body:
            if not allow_json_path:
                return jsonify({
                    "error": "Por seguridad, este servidor no permite 'json_path'. Envía el JSON completo en 'frame'."
                }), 400
            meta = body
            frame_json = load_frame_from_json_path(str(body["json_path"]))

        else:
            # Si mandan el JSON "crudo" (solo frame), aceptamos,
            # y leemos metadatos desde query params.
            frame_json = body
            meta = {
                "cumplimiento": request.args.get("cumplimiento", 0),
                "picos": request.args.get("picos", None),
                "dane": request.args.get("dane", None),
                "municipio": request.args.get("municipio", None),
                "lic": request.args.get("lic", None),
                "corr": request.args.get("corr", None),
            }

        # 2) Meta params
        try:
            cumplimiento = int(meta.get("cumplimiento", 0))
        except Exception:
            cumplimiento = 0

        picos_list = parse_picos_arg(meta.get("picos", None))

        # rutas: si no vienen, usar defaults del servidor
        corr_csv_path = meta.get("corr", None) or default_corr_csv
        lic_csv_path  = meta.get("lic", None)  or default_lic_csv

        # compatibilidad municipio/dane
        dane_filtro = normalize_dane(meta.get("dane", None))
        municipio_filtro = meta.get("municipio", None)

        # Si mandan municipio y no dane, y municipio es numérico => tratar como dane
        if dane_filtro is None and municipio_filtro is not None:
            m = str(municipio_filtro).strip()
            if m.isdigit():
                dane_filtro = m
                municipio_filtro = None

        # 3) Regla: all_emissions (cumplimiento=0 y sin picos) => ignorar licencias
        if cumplimiento == 0 and len(picos_list) == 0:
            lic_csv_path = None

        # 4) Construir payload como espera process_input()
        payload = [frame_json, picos_list, cumplimiento]

        out = process_input(
            payload,
            corr_csv_path=corr_csv_path,
            licencia_csv_path=lic_csv_path,
            dane_filtro=dane_filtro,
            municipio_filtro=municipio_filtro,
        )

        return jsonify(out)

    except Exception as e:
        return jsonify({"error": str(e)}), 500


# ---------------------------
# Entrypoint
# ---------------------------

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", type=str, default="127.0.0.1")
    ap.add_argument("--port", type=int, default=8000)

    # Defaults del servidor (para no mandarlos en cada request)
    ap.add_argument(
        "--lic-default",
        type=str,
        default=os.environ.get("ANE_LIC_CSV", ""),
        help="Ruta default del CSV de licencias (o env ANE_LIC_CSV).",
    )
    ap.add_argument(
        "--corr-default",
        type=str,
        default=os.environ.get("ANE_CORR_CSV", ""),
        help="Ruta default del CSV de correcciones (o env ANE_CORR_CSV).",
    )

    # Seguridad: por defecto NO permitimos que el cliente envíe rutas del servidor
    ap.add_argument(
        "--allow-json-path",
        action="store_true",
        help="(Solo dev) Permite que el cliente mande json_path. NO usar en prod.",
    )

    args = ap.parse_args()

    app.config["DEFAULT_LIC_CSV"] = args.lic_default.strip() or None
    app.config["DEFAULT_CORR_CSV"] = args.corr_default.strip() or None
    app.config["ALLOW_JSON_PATH"] = bool(args.allow_json_path)

    print(f"[SERVER] http://{args.host}:{args.port}")
    if app.config["DEFAULT_LIC_CSV"]:
        print(f"[SERVER] DEFAULT_LIC_CSV = {app.config['DEFAULT_LIC_CSV']}")
    if app.config["DEFAULT_CORR_CSV"]:
        print(f"[SERVER] DEFAULT_CORR_CSV = {app.config['DEFAULT_CORR_CSV']}")
    print(f"[SERVER] allow_json_path = {app.config['ALLOW_JSON_PATH']}")

    app.run(host=args.host, port=args.port, debug=False)

if __name__ == "__main__":
    main()
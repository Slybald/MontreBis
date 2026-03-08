#!/usr/bin/env python3
"""
Diagramme de Flots de Données (DFD Niveau 1) — Montre Connectée nRF5340
========================================================================
Méthodologie : « Les Techniques de l'Ingénieur »
  DFD = Vision « données » du système :
    - Processus (bulles numérotées) : transformations de données
    - Dépôts de données : stockages intermédiaires partagés
    - Entités externes : sources et puits de données
    - Flux de données (flèches nommées) : données échangées

  Étape 2 de l'analyse préliminaire (entre Bords du modèle et DFE).

Style reproduit d'après les schémas du cours N. Richard (M1 Communicants)
  → Figure 4 - Diagramme de flots de données

Dépendances :
  pip install graphviz Pillow
  brew install graphviz   (macOS)
  sudo apt install graphviz  (Linux)

Exécution :
  python3 diagramme_flot_donnees.py
  → génère diagramme_flot_donnees.png (16:9)
"""

from graphviz import Digraph
from PIL import Image
Image.MAX_IMAGE_PIXELS = None          # suppress DecompressionBombWarning

FONT = "Helvetica"

# Résolution cible 16:9
TARGET_W = 3840
TARGET_H = 2160


def build_diagram() -> Digraph:
    """Construit et renvoie le DFD Niveau 1 de la montre connectée."""

    dot = Digraph("DFD_Montre", format="png")

    # ---- Paramètres globaux ----
    dot.attr(
        rankdir="LR",
        bgcolor="white",
        dpi="300",
        pad="0.4",
        nodesep="0.7",
        ranksep="1.8",
        splines="spline",
        fontname=FONT,
        fontsize="22",
        label=(
            "\nFIGURE 4 – Diagramme de flots de données"
            " (DFD Niveau 1)\n"
            "Montre Connectée (nRF5340)\n"
        ),
        labelloc="b",
        labeljust="c",
    )
    dot.attr("node", fontname=FONT, fontsize="13", margin="0.2,0.15")
    dot.attr("edge", fontname=FONT, fontsize="10")

    # ==================================================================
    # Styles réutilisables
    # ==================================================================

    # Entités externes — sources (jaune)
    ext_src = dict(
        shape="box", style="rounded,filled",
        fillcolor="#fff2cc", color="#d6b656",
        width="1.7", height="0.65",
    )
    # Entités externes — puits (bleu)
    ext_sink = dict(
        shape="box", style="rounded,filled",
        fillcolor="#dae8fc", color="#6c8ebf",
        width="1.7", height="0.65",
    )
    # Entités externes — bidirectionnelles (vert clair)
    ext_bidir = dict(
        shape="box", style="rounded,filled",
        fillcolor="#d5e8d4", color="#82b366",
        width="1.7", height="0.65",
    )
    # Processus / transformations (ellipses vertes — convention DFD)
    proc = dict(
        shape="ellipse", style="filled",
        fillcolor="#d9ead3", color="#6aa84f",
        width="2.4", height="1.1",
        fontsize="12",
    )
    # Dépôts de données (double bordure orange — convention DFD)
    dstore = dict(
        shape="box", style="filled",
        fillcolor="#fce5cd", color="#e69138",
        peripheries="2",
        width="2.4", height="0.6",
        fontsize="11",
    )

    # Styles de flèches
    data_n = dict(penwidth="2.0", arrowsize="0.9", color="#333333")
    data_1 = dict(penwidth="1.0", arrowsize="0.9", color="#333333")

    # ==================================================================
    # COLONNE 1 — Entités externes sources (gauche)
    # Ordre vertical : capteurs I2C en haut, IHM au milieu, RTC en bas
    # ==================================================================
    dot.node("HTS221",  "HTS221\n(Temp / Hum)",       **ext_src)
    dot.node("LPS22HH", "LPS22HH\n(Pression)",        **ext_src)
    dot.node("LIS2MDL", "LIS2MDL\n(Magnétomètre)",    **ext_src)
    dot.node("LSM6DSO", "LSM6DSO\n(Acc / Gyro)",      **ext_src)
    dot.node("FT6206",  "Écran Tactile\n(FT6206)",    **ext_src)
    dot.node("Boutons", "Boutons\nUtilisateur",        **ext_src)
    dot.node("RV8263",  "RV-8263-C8\n(RTC)",           **ext_bidir)

    with dot.subgraph() as s:
        s.attr(rank="same")
        for n in ("HTS221", "LPS22HH", "LIS2MDL", "LSM6DSO",
                  "FT6206", "Boutons", "RV8263"):
            s.node(n)

    # Forcer l'ordre vertical des entrées
    for a, b in [("HTS221", "LPS22HH"), ("LPS22HH", "LIS2MDL"),
                 ("LIS2MDL", "LSM6DSO"), ("LSM6DSO", "FT6206"),
                 ("FT6206", "Boutons"), ("Boutons", "RV8263")]:
        dot.edge(a, b, style="invis")

    # ==================================================================
    # COLONNE 2 — Processus d'acquisition (P1, P5)
    # ==================================================================
    dot.node("P1", "1\nSENSORS Thread (P7)\nAcquérir Capteurs", **proc)
    dot.node("P5", "5\nCONTROLLER\nSync Heure RTC",             **proc)

    with dot.subgraph() as s:
        s.attr(rank="same")
        s.node("P1")
        s.node("P5")
    dot.edge("P1", "P5", style="invis")

    # ==================================================================
    # COLONNE 3 — Dépôts de données (D1, D2, D3, D4)
    # ==================================================================
    dot.node("D1", "D1 — shared_sensors\n(k_mutex protégé)",          **dstore)
    dot.node("D2", "D2 — Données Heure\n(volatile, time_offset)",     **dstore)
    dot.node("D3", "D3 — Event Bus\n(k_msgq, 16 msgs)",               **dstore)
    dot.node("D4", "D4 — UI Action Flags\n(atomic_t)",                **dstore)

    with dot.subgraph() as s:
        s.attr(rank="same")
        s.node("D1")
        s.node("D2")
        s.node("D3")
        s.node("D4")
    dot.edge("D1", "D2", style="invis")
    dot.edge("D2", "D3", style="invis")
    dot.edge("D3", "D4", style="invis")

    # ==================================================================
    # COLONNE 4 — Processus de traitement (P2, P4, P6)
    # ==================================================================
    dot.node("P2", "2\nCONTROLLER\nCalculer Orientation", **proc)
    dot.node("P4", "4\nCONTROLLER\nCommuniquer BLE",      **proc)
    dot.node("P6", "6\nJournaliser\n(multi-thread)",      **proc)

    with dot.subgraph() as s:
        s.attr(rank="same")
        s.node("P2")
        s.node("P4")
        s.node("P6")
    dot.edge("P2", "P4", style="invis")
    dot.edge("P4", "P6", style="invis")

    # ==================================================================
    # COLONNE 5 — Processus central IHM (P3 seul)
    # ==================================================================
    dot.node("P3", "3\nDISPLAY Thread (P9)\nAffichage IHM", **proc)

    # ==================================================================
    # COLONNE 6 — Entités externes puits / bidirectionnelles (droite)
    # ==================================================================
    dot.node("ILI9341",    "Écran TFT\nILI9341",        **ext_sink)
    dot.node("Smartphone", "Smartphone\n(Central BLE)", **ext_bidir)
    dot.node("Console",    "Terminal\nConsole",           **ext_sink)

    with dot.subgraph() as s:
        s.attr(rank="same")
        for n in ("ILI9341", "Smartphone", "Console"):
            s.node(n)
    dot.edge("ILI9341", "Smartphone", style="invis")
    dot.edge("Smartphone", "Console", style="invis")

    # ==================================================================
    # LÉGENDE
    # ==================================================================
    legend_html = """<
    <TABLE BORDER="1" CELLBORDER="0" CELLSPACING="3" CELLPADDING="5"
           BGCOLOR="white" COLOR="#999999">
      <TR><TD COLSPAN="2" ALIGN="CENTER"><B><FONT POINT-SIZE="13">Légende DFD</FONT></B></TD></TR>
      <TR>
        <TD BGCOLOR="#fff2cc" BORDER="1" COLOR="#d6b656">&nbsp;&nbsp;&nbsp;</TD>
        <TD ALIGN="LEFT"><FONT POINT-SIZE="10">Entité externe (source)</FONT></TD>
      </TR>
      <TR>
        <TD BGCOLOR="#dae8fc" BORDER="1" COLOR="#6c8ebf">&nbsp;&nbsp;&nbsp;</TD>
        <TD ALIGN="LEFT"><FONT POINT-SIZE="10">Entité externe (puits)</FONT></TD>
      </TR>
      <TR>
        <TD BGCOLOR="#d5e8d4" BORDER="1" COLOR="#82b366">&nbsp;&nbsp;&nbsp;</TD>
        <TD ALIGN="LEFT"><FONT POINT-SIZE="10">Entité bidirectionnelle</FONT></TD>
      </TR>
      <TR>
        <TD BGCOLOR="#d9ead3" BORDER="1" COLOR="#6aa84f">&nbsp;&#x25CB;&nbsp;</TD>
        <TD ALIGN="LEFT"><FONT POINT-SIZE="10">Processus (n°)</FONT></TD>
      </TR>
      <TR>
        <TD BGCOLOR="#fce5cd" BORDER="1" COLOR="#e69138">&nbsp;&#x25A1;&nbsp;</TD>
        <TD ALIGN="LEFT"><FONT POINT-SIZE="10">Dépôt de données</FONT></TD>
      </TR>
      <TR>
        <TD><FONT POINT-SIZE="14"><B>&#x21D2;</B></FONT></TD>
        <TD ALIGN="LEFT"><FONT POINT-SIZE="10">Flux de données (n bits)</FONT></TD>
      </TR>
      <TR>
        <TD><FONT POINT-SIZE="14">&#x2192;</FONT></TD>
        <TD ALIGN="LEFT"><FONT POINT-SIZE="10">Flux de données (1 bit)</FONT></TD>
      </TR>
    </TABLE>
    >"""
    dot.node("legend", legend_html, shape="none")

    with dot.subgraph() as s:
        s.attr(rank="same")
        s.node("ILI9341")
        s.node("legend")

    # ==================================================================
    # FLUX DE DONNÉES
    # ==================================================================

    # ---- Capteurs → P1 (Acquisition via I2C1) ----
    dot.edge("HTS221",  "P1", label="  I2C (T, H)  ",       weight="8", **data_n)
    dot.edge("LPS22HH", "P1", label="  I2C (P)  ",          weight="8", **data_n)
    dot.edge("LIS2MDL", "P1", label="  I2C (Mag)  ",        weight="8", **data_n)
    dot.edge("LSM6DSO", "P1", label="  I2C (Acc, Gyr)  ",   weight="8", **data_n)

    # ---- P1 → D1 (stockage mesures, snapshot protégé par mutex) ----
    dot.edge(
        "P1",
        "D1",
        label="  sensor_snapshot\n  (k_mutex_lock/unlock)  ",
        weight="8",
        **data_n,
    )

    # ---- D1 → consommateurs ----
    dot.edge("D1", "P2",
             label="  Acc X, Y  ",
             weight="4", **data_n)

    dot.edge("D1", "P4",
             label="  T/H, P, Acc/Gyr  ",
             weight="4", **data_n)

    dot.edge(
        "D1",
        "P3",
        label="  sensor_snapshot\n  (lecture mutex)  ",
        weight="2",
        minlen="2",
        **data_n,
    )

    # ---- P2 → P3 (résultat orientation) ----
    dot.edge("P2", "P3",
             label="  orientation\n  (0°/90°/270°)  ",
             weight="4", **data_1)

    # ---- Tactile → P3 (interaction directe LVGL) ----
    dot.edge(
        "FT6206",
        "P3",
        label="  position tactile (x,y)  ",
        weight="2",
        minlen="4",
        **data_n,
    )

    # ---- Boutons → D3 (événements via raw_input_q → INPUT_MGR → event_bus) ----
    dot.edge(
        "Boutons",
        "D3",
        label="  EVT_BTN_*  ",
        weight="2",
        minlen="4",
        **data_1,
    )

    # ---- RV-8263 ↔ P5 (lecture / écriture heure I2C2) ----
    dot.edge("RV8263", "P5",
             label="  date/heure I2C (R/W)  ",
             dir="both", weight="8", **data_n)

    # ---- P5 → D2 (stockage heure synchronisée) ----
    dot.edge("P5", "D2",
             label="  heure synchronisée  ",
             weight="8", **data_n)

    # ---- D2 → P3 (heure pour affichage) ----
    dot.edge("D2", "P3",
             label="  date, heure  ",
             weight="2", minlen="2", **data_n)

    # ---- P3 → ILI9341 (affichage LVGL via SPI) ----
    dot.edge(
        "P3",
        "ILI9341",
        label="  trames SPI\n  affichage LVGL  ",
        weight="8",
        **data_n,
    )

    # ---- P4 → Smartphone (notifications GATT) ----
    dot.edge(
        "P4",
        "Smartphone",
        label="  notifications GATT\n  (Env, Press, Motion)  ",
        weight="6",
        **data_n,
    )

    # ---- Smartphone → D3 (événement EVT_BLE_TIME_SYNC) ----
    dot.edge(
        "Smartphone",
        "D3",
        label="  EVT_BLE_TIME_SYNC\n  (event_post_val)  ",
        weight="6",
        **data_n,
    )

    # ---- Event Bus → processus CONTROLLER (P2, P4, P5) ----
    dot.edge(
        "D3",
        "P2",
        label="  struct event_msg\n  (EVT_BTN_*, ... )  ",
        weight="2",
        **data_n,
    )

    dot.edge(
        "D3",
        "P4",
        label="  struct event_msg\n  (EVT_BLE_TIME_SYNC)  ",
        weight="2",
        **data_n,
    )

    dot.edge(
        "D3",
        "P5",
        label="  struct event_msg\n  (EVT_TICK_TIME, ...)  ",
        weight="2",
        **data_n,
    )

    # ---- P4 → P5 (données sync heure reçues via BLE) ----
    dot.edge(
        "P4",
        "P5",
        label="  time_offset\n  flag_update_rtc  ",
        constraint="false",
        **data_n,
    )

    # ---- Flags UI : CONTROLLER → D4 → DISPLAY Thread ----
    dot.edge(
        "P2",
        "D4",
        label="  UI_ACT_*  ",
        weight="2",
        **data_1,
    )

    dot.edge(
        "D4",
        "P3",
        label="  UI_ACT_*  ",
        weight="2",
        **data_1,
    )

    # ---- Flux log : P1, P4 → P6 → Console ----
    dot.edge("P1", "P6",
             label="  log capteurs  ",
             constraint="false", **data_1)

    dot.edge("P4", "P6",
             label="  log BLE  ",
             constraint="false", **data_1)

    dot.edge("P6", "Console",
             label="  LOG_INF / LOG_ERR  ",
             weight="6", **data_1)

    return dot


# ---------------------------------------------------------------------------
def fit_to_16_9(path: str) -> None:
    """Étire l'image pour remplir exactement le canvas 16:9 (sans espace blanc)."""
    img = Image.open(path)
    w, h = img.size
    print(f"  Taille brute graphviz : {w}×{h} px  (ratio {w/h:.2f})")

    img_out = img.resize((TARGET_W, TARGET_H), Image.LANCZOS)
    img_out.save(path)
    print(f"  → Étiré à {TARGET_W}×{TARGET_H} px (16:9, zéro espace blanc)")


def main() -> None:
    diagram = build_diagram()
    output_path = diagram.render("diagramme_flot_donnees", cleanup=True)
    print(f"Diagramme de flots de données généré : {output_path}")
    fit_to_16_9(output_path)


if __name__ == "__main__":
    main()

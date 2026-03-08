#!/usr/bin/env python3
"""
Diagramme de Flots de Donnees (DFD Niveau 1) — Montre Connectee  [SOMBRE]
==========================================================================
Methodologie : « Les Techniques de l'Ingenieur »
  DFD = Vision « donnees » du systeme :
    - Processus (bulles) : transformations de donnees
    - Depots de donnees : stockages intermediaires partages
    - Entites externes : sources et puits de donnees
    - Flux de donnees (fleches nommees) : donnees echangees

Style sombre reproduit d'apres les schemas du cours N. Richard (M1 Communicants)
  -> Figure 4 - Diagramme de flots de donnees

Dependances :
  pip install graphviz Pillow
  brew install graphviz   (macOS)

Execution :
  python3 dfd_projet_sombre.py
  -> genere dfd_projet_sombre.png (16:9)
"""

from graphviz import Digraph
from PIL import Image
Image.MAX_IMAGE_PIXELS = None          # suppress DecompressionBombWarning

FONT = "Helvetica"

# Resolution cible 16:9
TARGET_W = 3840
TARGET_H = 2160

# Palette sombre
BG       = "#1a1a2e"
FG       = "#e0e0e0"
BORDER   = "#555555"
NODE_BG  = "#ffffff"
NODE_FG  = "#000000"


def build_diagram() -> Digraph:
    """Construit et renvoie le DFD Niveau 1 (sombre)."""

    dot = Digraph("DFD_Montre_Sombre", format="png")

    # ---- Parametres globaux ----
    dot.attr(
        rankdir="LR",
        bgcolor=BG,
        dpi="300",
        pad="0.4",
        nodesep="0.7",
        ranksep="1.6",
        splines="spline",
        fontname=FONT,
        fontsize="28",
        fontcolor=FG,
        label="\nDIAGRAMME DE FLUX DE DONNEES\n",
        labelloc="t",
        labeljust="c",
    )
    dot.attr("node", fontname=FONT, fontsize="12", fontcolor=NODE_FG,
             margin="0.2,0.15")
    dot.attr("edge", fontname=FONT, fontsize="10", fontcolor=FG)

    # ==================================================================
    # Styles reutilisables
    # ==================================================================

    # Entites externes — sources (blanc sur fond sombre)
    ext_src = dict(
        shape="box", style="rounded,filled",
        fillcolor=NODE_BG, color=BORDER,
        width="1.7", height="0.65",
    )
    # Entites externes — puits (blanc)
    ext_sink = dict(
        shape="box", style="rounded,filled",
        fillcolor=NODE_BG, color=BORDER,
        width="1.7", height="0.65",
    )
    # Processus / transformations (ellipses blanches)
    proc = dict(
        shape="ellipse", style="filled",
        fillcolor=NODE_BG, color=BORDER,
        width="2.2", height="1.0",
        fontsize="11",
    )
    # Depot de donnees (losange / parallelogramme blanc)
    dstore = dict(
        shape="box", style="filled",
        fillcolor=NODE_BG, color=BORDER,
        peripheries="2",
        width="2.4", height="0.6",
        fontsize="11",
    )

    # Styles de fleches
    data_n = dict(penwidth="2.0", arrowsize="0.9", color=FG)
    data_1 = dict(penwidth="1.0", arrowsize="0.9", color=FG)

    # ==================================================================
    # COLONNE 1 — Entites externes sources (gauche)
    # ==================================================================
    dot.node("LSM6DSO", "Accelerometer\nGyroscope\n(LSM6DSO)", **ext_src)
    dot.node("LIS2MDL", "Magnetometer\n(LIS2MDL)",             **ext_src)
    dot.node("HTS221",  "Temperature\nHumidity\n(HTS221)",     **ext_src)
    dot.node("LPS22HH", "Pressure\n(LPS22HH)",                 **ext_src)
    dot.node("RV8263",  "Time\nDate\n(RTC RV-8263-C8)",         **ext_src)

    with dot.subgraph() as s:
        s.attr(rank="same")
        for n in ("LSM6DSO", "LIS2MDL", "HTS221", "LPS22HH", "RV8263"):
            s.node(n)
    for a, b in [("LSM6DSO", "LIS2MDL"), ("LIS2MDL", "HTS221"),
                 ("HTS221", "LPS22HH"), ("LPS22HH", "RV8263")]:
        dot.edge(a, b, style="invis")

    # Entrees IHM (en bas)
    dot.node("Boutons",     "Boutons\nSW0-SW3",           **ext_src)
    dot.node("Touchscreen", "Touchscreen",                 **ext_src)

    # ==================================================================
    # COLONNE 2 — Bus I2C + Handlers entree
    # ==================================================================
    dot.node("I2C1", "I2C1", shape="box", style="filled",
             fillcolor=NODE_BG, color=BORDER, width="1.0", height="0.5")
    dot.node("I2C2", "I2C2", shape="box", style="filled",
             fillcolor=NODE_BG, color=BORDER, width="1.0", height="0.5")

    dot.node("P_Acq", "Acquisition\ntask", **proc)
    dot.node("P_Btn", "Buttons\nhandler",  **proc)
    dot.node("P_Tch", "Touch screen\nhandler", **proc)

    # ==================================================================
    # COLONNE 3 — Depot de donnees central + traitement
    # ==================================================================
    dot.node("D_SYS", "struct\ns_system_data", **dstore)
    dot.node("P_Pas", "Traitement du nombre\nde pas", **proc)

    # ==================================================================
    # COLONNE 4 — Taches de mise a jour (sorties)
    # ==================================================================
    dot.node("P_LED",  "LEDs\nhandler",             **proc)
    dot.node("P_Term", "Terminal\nupdate task",      **proc)
    dot.node("P_Scrn", "Screen\nupdate task",        **proc)
    dot.node("P_Comm", "Communication\nupdate task", **proc)

    with dot.subgraph() as s:
        s.attr(rank="same")
        for n in ("P_LED", "P_Term", "P_Scrn", "P_Comm"):
            s.node(n)
    dot.edge("P_LED", "P_Term", style="invis")
    dot.edge("P_Term", "P_Scrn", style="invis")
    dot.edge("P_Scrn", "P_Comm", style="invis")

    # ==================================================================
    # COLONNE 5 — Sorties physiques externes (droite)
    # ==================================================================
    dot.node("LEDs_out",    "LED 1-4\nGPIO",        **ext_sink)
    dot.node("Hyperterm",   "Hyperterminal",         **ext_sink)
    dot.node("Screen_out",  "Screen",                **ext_sink)
    dot.node("STM32",       "STM32",                 **ext_sink)

    with dot.subgraph() as s:
        s.attr(rank="same")
        for n in ("LEDs_out", "Hyperterm", "Screen_out", "STM32"):
            s.node(n)

    # ==================================================================
    # MCU boundary (cluster)
    # ==================================================================
    # (Les noeuds internes sont deja crees; le cluster visuel est
    #  assure par le layout LR et le fond sombre commun.)

    # ==================================================================
    # LEGENDE
    # ==================================================================
    legend_html = """<
    <TABLE BORDER="1" CELLBORDER="0" CELLSPACING="3" CELLPADDING="5"
           BGCOLOR="#1a1a2e" COLOR="#555555">
      <TR>
        <TD><FONT POINT-SIZE="14" COLOR="#e0e0e0">&#x2192;</FONT></TD>
        <TD ALIGN="LEFT"><FONT POINT-SIZE="10" COLOR="#e0e0e0">1 bit</FONT></TD>
      </TR>
      <TR>
        <TD><FONT POINT-SIZE="14" COLOR="#e0e0e0"><B>&#x21D2;</B></FONT></TD>
        <TD ALIGN="LEFT"><FONT POINT-SIZE="10" COLOR="#e0e0e0">1 mot</FONT></TD>
      </TR>
      <TR>
        <TD><FONT POINT-SIZE="14" COLOR="#e0e0e0">&#x21E2;</FONT></TD>
        <TD ALIGN="LEFT"><FONT POINT-SIZE="10" COLOR="#e0e0e0">Une interruption</FONT></TD>
      </TR>
    </TABLE>
    >"""
    dot.node("legend", legend_html, shape="none")

    # ==================================================================
    # FLUX DE DONNEES
    # ==================================================================

    # ---- Capteurs → I2C → Acquisition task ----
    for sensor in ("LSM6DSO", "LIS2MDL", "HTS221", "LPS22HH"):
        dot.edge(sensor, "I2C1", dir="both", weight="8", **data_n)
    dot.edge("RV8263", "I2C2", dir="both", weight="8", **data_n)

    dot.edge("I2C1", "P_Acq", weight="8", **data_n)
    dot.edge("I2C2", "P_Acq", weight="8", **data_n)

    # ---- Acquisition task → struct s_system_data ----
    dot.edge("P_Acq", "D_SYS",
             label="  sensor data  ",
             weight="8", **data_n)

    # ---- struct s_system_data → Traitement du nombre de pas (boucle) ----
    dot.edge("D_SYS", "P_Pas",
             label="  accel data  ",
             weight="4", **data_n)
    dot.edge("P_Pas", "D_SYS",
             label="  step count  ",
             constraint="false", **data_n)

    # ---- struct s_system_data → Taches de sortie ----
    dot.edge("D_SYS", "P_LED",
             label="  status flags  ",
             weight="4", **data_1)
    dot.edge("D_SYS", "P_Term",
             label="  all sensor data  ",
             weight="4", **data_n)
    dot.edge("D_SYS", "P_Scrn",
             label="  display data  ",
             weight="4", **data_n)
    dot.edge("D_SYS", "P_Comm",
             label="  BLE payload  ",
             weight="4", **data_n)

    # ---- Handlers entree → struct s_system_data ----
    dot.edge("Boutons", "P_Btn",
             weight="6", **data_1)
    dot.edge("P_Btn", "D_SYS",
             label="  button events  ",
             weight="4", minlen="2", **data_1)

    dot.edge("Touchscreen", "P_Tch",
             weight="6", **data_n)
    dot.edge("P_Tch", "D_SYS",
             label="  touch coords  ",
             weight="4", minlen="2", **data_n)

    # ---- Taches de sortie → Sorties externes ----
    dot.edge("P_LED",  "LEDs_out",
             label="  GPIO  ",
             weight="6", **data_1)
    dot.edge("P_Term", "Hyperterm",
             label="  USB Virtual Com  ",
             weight="6", **data_n)
    dot.edge("P_Scrn", "Screen_out",
             label="  LCD shield  ",
             weight="6", **data_n)
    dot.edge("P_Comm", "STM32",
             label="  BLE\n  Communication  ",
             weight="6", **data_n)

    return dot


# ---------------------------------------------------------------------------
def fit_to_16_9(path: str) -> None:
    """Etire l'image pour remplir exactement le canvas 16:9 (sans espace blanc)."""
    img = Image.open(path)
    w, h = img.size
    print(f"  Taille brute graphviz : {w}x{h} px  (ratio {w/h:.2f})")

    img_out = img.resize((TARGET_W, TARGET_H), Image.LANCZOS)
    img_out.save(path)
    print(f"  -> Etire a {TARGET_W}x{TARGET_H} px (16:9, zero espace blanc)")


def main() -> None:
    diagram = build_diagram()
    output_path = diagram.render("dfd_projet_sombre", cleanup=True)
    print(f"DFD genere : {output_path}")
    fit_to_16_9(output_path)


if __name__ == "__main__":
    main()

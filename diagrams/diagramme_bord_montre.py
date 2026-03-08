#!/usr/bin/env python3
"""
Diagramme de bord (Bords du modèle) — Montre Connectée nRF5340
===============================================================
Méthodologie : « Les Techniques de l'Ingénieur »
  Bords du modèle = Entrées physiques → Périphériques internes → µP
                     µP → Périphériques internes → Sorties physiques

Style reproduit d'après les schémas du cours N. Richard (M1 Communicants).

Dépendances :
  pip install graphviz Pillow
  brew install graphviz   (macOS)
  sudo apt install graphviz  (Linux)

Exécution :
  python3 diagramme_bord_montre.py
  → génère diagramme_bord_montre.png (16:9)
"""

from graphviz import Digraph
from PIL import Image
Image.MAX_IMAGE_PIXELS = None          # suppress DecompressionBombWarning

FONT = "Helvetica"

# Résolution cible 16:9
TARGET_W = 3840
TARGET_H = 2160


def build_diagram() -> Digraph:
    """Construit et renvoie le graphe Digraph du diagramme de bord."""

    dot = Digraph("Bords_du_Modele_Montre", format="png")

    # ---- Paramètres globaux ----
    dot.attr(
        rankdir="LR",
        bgcolor="white",
        dpi="300",
        pad="0.3",
        nodesep="1.6",
        ranksep="0.9",
        splines="spline",
        fontname=FONT,
        fontsize="22",
        label=(
            "\nFIGURE 3 – Architecture système multitâche préemptive\n"
            "Bords du modèle\n"
            "Montre Connectée (nRF5340)\n"
        ),
        labelloc="b",
        labeljust="c",
    )
    dot.attr("node", fontname=FONT, fontsize="14", margin="0.25,0.2")
    dot.attr("edge", fontname=FONT, fontsize="12")

    # ==================================================================
    # Styles réutilisables
    # ==================================================================
    ext_in = dict(
        shape="box", style="rounded,filled",
        fillcolor="#fff2cc", color="#d6b656",
        width="1.8", height="0.7",
    )
    ext_out = dict(
        shape="box", style="rounded,filled",
        fillcolor="#dae8fc", color="#6c8ebf",
        width="1.8", height="0.7",
    )
    periph = dict(
        shape="box", style="filled",
        fillcolor="white", color="#333333",
        width="1.8", height="0.7",
    )
    # Styles de flèches (légende Techniques de l'Ingénieur)
    sg_n = dict(penwidth="2.5", arrowsize="1.0", color="#333333")
    sg_1 = dict(penwidth="1.0", arrowsize="1.0", color="#333333")
    irq  = dict(style="dashed", penwidth="1.0", arrowsize="1.0",
                color="#555555")

    # ==================================================================
    # COLONNE 1 — Entrées physiques externes
    # ==================================================================
    dot.node("HTS221",    "HTS221",                  **ext_in)
    dot.node("LPS22HH",   "LPS22HH",                 **ext_in)
    dot.node("LIS2MDL",   "LIS2MDL",                  **ext_in)
    dot.node("LSM6DSO",   "LSM6DSO",                  **ext_in)
    dot.node("EcranTact", "Écran Tactile\n(FT6206)",  **ext_in)
    dot.node("Boutons",   "Boutons\nUtilisateur",      **ext_in)
    dot.node("RV8263",    "RV-8263-C8",                **ext_in)

    # Aligner verticalement les entrées (même rang = même colonne en LR)
    with dot.subgraph() as s:
        s.attr(rank="same")
        for n in ("HTS221", "LPS22HH", "LIS2MDL", "LSM6DSO",
                  "EcranTact", "Boutons", "RV8263"):
            s.node(n)

    # ==================================================================
    # µControleur (nRF5340) — cadre pointillé
    # ==================================================================
    with dot.subgraph(name="cluster_uc") as uc:
        uc.attr(
            label="µControleur (nRF5340)",
            style="dashed",
            color="#444444",
            fontsize="16",
            fontname=f"{FONT} Bold",
            penwidth="2.0",
        )

        # -- Périphériques internes côté entrée --
        uc.node("I2C1",      "I2C1\n(arduino_i2c)", **periph)
        uc.node("GPIO_EXTI", "GPIO / EXTI",          **periph)
        uc.node("I2C2",      "I2C2",                 **periph)

        # Ordre vertical dans le cluster (invisible)
        uc.edge("I2C1",      "GPIO_EXTI", style="invis")
        uc.edge("GPIO_EXTI", "I2C2",      style="invis")

        # -- µP central — grand bloc vert --
        uc.node(
            "uP", "\nµP\n(4 Threads\nPréemptifs)\n",
            shape="box", style="filled",
            fillcolor="#b7d7a8", color="#6aa84f",
            width="2.0", height="6.0", fixedsize="true",
            fontsize="26", fontname=f"{FONT} Bold",
        )

        # -- Périphériques internes côté sortie --
        uc.node("SPI",       "SPI\n(arduino_spi)", **periph)
        uc.node("BLE_Radio", "Radio\nBLE",          **periph)
        uc.node("UART",      "UART",                **periph)

        # Ordre vertical dans le cluster (invisible)
        uc.edge("SPI",       "BLE_Radio", style="invis")
        uc.edge("BLE_Radio", "UART",      style="invis")

    # ==================================================================
    # COLONNE 5 — Sorties physiques externes
    # ==================================================================
    dot.node("ILI9341",    "Écran TFT\nILI9341",       **ext_out)
    dot.node("Smartphone", "Smartphone\n(Central BLE)",
             shape="box", style="rounded,filled",
             fillcolor="#d5e8d4", color="#82b366",
             width="1.4", height="0.6")
    dot.node("Console",    "Terminal\nConsole",          **ext_out)

    # Aligner verticalement les sorties
    with dot.subgraph() as s:
        s.attr(rank="same")
        for n in ("ILI9341", "Smartphone", "Console"):
            s.node(n)

    # ==================================================================
    # LÉGENDE
    # ==================================================================
    legend_html = """<
    <TABLE BORDER="1" CELLBORDER="0" CELLSPACING="4" CELLPADDING="6"
           BGCOLOR="white" COLOR="#999999">
      <TR><TD COLSPAN="2" ALIGN="CENTER"><B><FONT POINT-SIZE="13">Légende</FONT></B></TD></TR>
      <TR>
        <TD><FONT POINT-SIZE="16">&#x2192;</FONT></TD>
        <TD ALIGN="LEFT"><FONT POINT-SIZE="12">Sg d'un Bit</FONT></TD>
      </TR>
      <TR>
        <TD><FONT POINT-SIZE="16"><B>&#x21D2;</B></FONT></TD>
        <TD ALIGN="LEFT"><FONT POINT-SIZE="12">Sg de n Bits</FONT></TD>
      </TR>
      <TR>
        <TD><FONT POINT-SIZE="16">&#x21E2;</FONT></TD>
        <TD ALIGN="LEFT"><FONT POINT-SIZE="12">Interruption / événement</FONT></TD>
      </TR>
    </TABLE>
    >"""
    dot.node("legend", legend_html, shape="none")

    with dot.subgraph() as s:
        s.attr(rank="same")
        s.node("ILI9341")
        s.node("legend")

    # ==================================================================
    # FLUX — Entrées physiques → Périphériques d'entrée
    # ==================================================================

    for sensor in ("HTS221", "LPS22HH", "LIS2MDL", "LSM6DSO"):
        dot.edge(sensor, "I2C1", dir="both", weight="8", **sg_n)

    dot.edge("EcranTact", "I2C1", dir="both", weight="8", **sg_n)
    dot.edge("Boutons", "GPIO_EXTI", weight="8", **sg_1)
    dot.edge("RV8263", "I2C2", label="  Date/Heure  ", weight="8", **sg_n)

    # ==================================================================
    # FLUX — Périphériques d'entrée → µP
    # ==================================================================

    dot.edge("I2C1", "uP",
             label="  Données capteurs\n  + Tactile  ",
             weight="8", **sg_n)

    dot.edge("GPIO_EXTI", "uP",
             label="  IRQ_Button  ",
             weight="8", **irq)

    dot.edge("I2C2", "uP",
             label="  Date/Heure  ",
             weight="8", **sg_n)

    # ==================================================================
    # FLUX — µP → Périphériques de sortie
    # ==================================================================

    dot.edge("uP", "SPI",
             label="  Affichage  ",
             weight="8", **sg_n)

    dot.edge("uP", "BLE_Radio",
             label="  Notif capteurs  ",
             weight="8", **sg_n)

    dot.edge("uP", "UART",
             label="  Log  ",
             weight="8", **sg_1)

    dot.edge("BLE_Radio", "uP",
             label="  Sync heure  ",
             constraint="false", **irq)

    dot.edge("uP", "I2C2",
             label="  MAJ Heure\n  (sync BLE)  ",
             constraint="false", **sg_n)

    # ==================================================================
    # FLUX — Périphériques de sortie → Sorties physiques
    # ==================================================================

    dot.edge("SPI", "ILI9341",
             label="  Affichage  ",
             weight="8", **sg_n)

    dot.edge("BLE_Radio", "Smartphone",
             dir="both",
             weight="8", **sg_n)

    dot.edge("UART", "Console",
             label="  Affichage  ",
             weight="8", **sg_1)

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
    output_path = diagram.render("diagramme_bord_montre", cleanup=True)
    print(f"Diagramme de bord généré : {output_path}")
    fit_to_16_9(output_path)


if __name__ == "__main__":
    main()

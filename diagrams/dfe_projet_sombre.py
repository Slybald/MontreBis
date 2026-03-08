#!/usr/bin/env python3
"""
Diagramme de Flot d'Evenements (DFE) — Montre Connectee  [SOMBRE]
===================================================================
Methodologie : « Les Techniques de l'Ingenieur »
  DFE = Chaines de commande :
    - Identifier les commandes de controle (Start&Go, Start/Stop...)
    - Identifier les signaux de controle (IRQ, flags) -> changent l'etat de la ME
    - La ME centralise la gestion des ordres et la reception des signaux

Style sombre reproduit d'apres les schemas du cours N. Richard (M1 Communicants)
  -> Figure 5 - Diagramme de flot d'evenements

Dependances :
  pip install graphviz Pillow
  brew install graphviz   (macOS)

Execution :
  python3 dfe_projet_sombre.py
  -> genere dfe_projet_sombre.png (16:9)
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
    """Construit et renvoie le DFE (sombre)."""

    dot = Digraph("DFE_Montre_Sombre", format="png")

    # ---- Parametres globaux ----
    dot.attr(
        rankdir="LR",
        bgcolor=BG,
        dpi="300",
        pad="0.3",
        nodesep="1.2",
        ranksep="0.8",
        splines="spline",
        fontname=FONT,
        fontsize="28",
        fontcolor=FG,
        label="\nDIAGRAMME DE FLOT\nD'EVENEMENT\n",
        labelloc="t",
        labeljust="c",
    )
    dot.attr("node", fontname=FONT, fontsize="12", fontcolor=NODE_FG,
             margin="0.2,0.15")
    dot.attr("edge", fontname=FONT, fontsize="10", fontcolor=FG)

    # ==================================================================
    # Styles reutilisables
    # ==================================================================
    periph = dict(
        shape="box", style="filled",
        fillcolor=NODE_BG, color=BORDER,
        width="1.6", height="0.6",
    )
    task_block = dict(
        shape="box", style="rounded,filled",
        fillcolor=NODE_BG, color=BORDER,
        width="2.0", height="0.65",
    )

    # Styles de fleches
    sg_n    = dict(penwidth="2.0", arrowsize="0.9", color=FG)
    sg_1    = dict(penwidth="1.0", arrowsize="0.9", color=FG)
    irq     = dict(style="dashed", penwidth="1.0", arrowsize="0.9",
                   color=FG)
    startgo = dict(style="dashed", penwidth="1.5", arrowsize="0.9",
                   color=FG)

    # ==================================================================
    # MCU boundary (cluster)
    # ==================================================================
    with dot.subgraph(name="cluster_mcu") as mcu:
        mcu.attr(
            label="MCU",
            style="dashed",
            color=FG,
            fontsize="14",
            fontcolor=FG,
            fontname=f"{FONT} Bold",
            penwidth="2.0",
            labeljust="r",
            labelloc="b",
        )

        # ---- COLONNE 1 — Sources d'evenements (Timers) ----
        mcu.node("TIM1", "Timer 1\n(1Hz, 1s)",              **periph)
        mcu.node("TIM2", "Timer 2\n(100Hz, 10ms)",          **periph)
        mcu.node("TIM3", "Timer 3 channel 1\n(5Hz, 200ms)", **periph)
        mcu.node("TIM4", "Timer 3 channel 2\n(25Hz, 40ms)", **periph)

        mcu.edge("TIM1", "TIM2", style="invis")
        mcu.edge("TIM2", "TIM3", style="invis")
        mcu.edge("TIM3", "TIM4", style="invis")

        # ---- COLONNE 1 — Sources d'evenements (Boutons) ----
        mcu.node("BTN1", "Button 1\nGPIO", **periph)
        mcu.node("BTN2", "Button 2\nGPIO", **periph)
        mcu.node("BTN3", "Button 3\nGPIO", **periph)
        mcu.node("BTN4", "Button 4\nGPIO", **periph)

        mcu.edge("BTN1", "BTN2", style="invis")
        mcu.edge("BTN2", "BTN3", style="invis")
        mcu.edge("BTN3", "BTN4", style="invis")

        # ---- Touchscreen ----
        mcu.node("Touchscreen", "Touchscreen", **periph)
        mcu.node("SPI1", "SPI", shape="plaintext", fontcolor=FG,
                 fontsize="11")

        # ==============================================================
        # MACHINE D'ETATS — noeud central (gros cercle)
        # ==============================================================
        mcu.node(
            "ME", "MACHINE\nD'ETATS",
            shape="circle", style="filled",
            fillcolor="#2a2a44", color=FG,
            width="2.8", height="2.8", fixedsize="true",
            fontsize="20", fontcolor=FG,
            fontname=f"{FONT} Bold",
        )

        # ---- COLONNE 3 — Taches pilotees (droite / bas) ----
        mcu.node("T_LED",  "LEDs\nhandler",             **task_block)
        mcu.node("T_Term", "Terminal\nupdate",           **task_block)
        mcu.node("T_Scrn", "Screen\nupdate",             **task_block)
        mcu.node("T_Comm", "Communication\nupdate task", **task_block)

        mcu.edge("T_LED", "T_Term", style="invis")
        mcu.edge("T_Term", "T_Scrn", style="invis")
        mcu.edge("T_Scrn", "T_Comm", style="invis")

        # ---- Taches bas ----
        mcu.node("T_Tch",  "Touch screen\nhandler",   **task_block)
        mcu.node("T_Acq",  "Acquisition\ntask",       **task_block)
        mcu.node("T_Btn",  "Buttons\nhandler",        **task_block)
        mcu.node("T_Pas",  "Traitement du nombre\nde pas", **task_block)

    # ==================================================================
    # Entrees externes (en dehors du cluster)
    # ==================================================================
    dot.node("SW0", "SW0", shape="plaintext", fontcolor=FG, fontsize="11")
    dot.node("SW1", "SW1", shape="plaintext", fontcolor=FG, fontsize="11")
    dot.node("SW2", "SW2", shape="plaintext", fontcolor=FG, fontsize="11")
    dot.node("SW3", "SW3", shape="plaintext", fontcolor=FG, fontsize="11")

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
    # FLUX — IRQ Timers → Machine d'Etats
    # ==================================================================
    dot.edge("TIM1", "ME", label="  MY_TIM_1_IRQ  ", weight="6", **irq)
    dot.edge("TIM2", "ME", label="  MY_TIM_2_IRQ  ", weight="6", **irq)
    dot.edge("TIM3", "ME", label="  MY_TIM_3_CC0_IRQ  ", weight="6", **irq)
    dot.edge("TIM4", "ME", label="  MY_TIM_3_CC1_IRQ  ", weight="6", **irq)

    # ---- IRQ Boutons → Machine d'Etats ----
    dot.edge("SW0", "BTN1", weight="6", **sg_1)
    dot.edge("SW1", "BTN2", weight="6", **sg_1)
    dot.edge("SW2", "BTN3", weight="6", **sg_1)
    dot.edge("SW3", "BTN4", weight="6", **sg_1)

    dot.edge("BTN1", "ME", label="  MY_BT_1_IRQ  ", weight="6", **irq)
    dot.edge("BTN2", "ME", label="  MY_BT_2_IRQ  ", weight="6", **irq)
    dot.edge("BTN3", "ME", label="  MY_BT_3_IRQ  ", weight="6", **irq)
    dot.edge("BTN4", "ME", label="  MY_BT_4_IRQ  ", weight="6", **irq)

    # ---- IRQ Touchscreen → Machine d'Etats ----
    dot.edge("Touchscreen", "SPI1", weight="6", **sg_n)
    dot.edge("SPI1", "ME", label="  MY_TCH_IRQ  ", weight="6", **irq)

    # ==================================================================
    # FLUX — Machine d'Etats → Taches (Start&Go)
    # ==================================================================
    dot.edge("ME", "T_LED",  label="  S&G  ", weight="4", **startgo)
    dot.edge("ME", "T_Term", label="  S&G  ", weight="4", **startgo)
    dot.edge("ME", "T_Scrn", label="  S&G  ", weight="4", **startgo)
    dot.edge("ME", "T_Comm", label="  S&G  ", weight="4", **startgo)

    dot.edge("ME", "T_Acq",  label="  S&G  ", weight="4", **startgo)
    dot.edge("ME", "T_Tch",  label="  S&G  ", weight="4", **startgo)
    dot.edge("ME", "T_Btn",  label="  S&G  ", weight="4", **startgo)

    # ---- Start/Stop specifique pour traitement pas ----
    dot.edge("ME", "T_Pas",
             label="  Start/Stop  ",
             style="dashed", penwidth="2.0", arrowsize="0.9", color=FG,
             weight="4")

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
    output_path = diagram.render("dfe_projet_sombre", cleanup=True)
    print(f"DFE genere : {output_path}")
    fit_to_16_9(output_path)


if __name__ == "__main__":
    main()

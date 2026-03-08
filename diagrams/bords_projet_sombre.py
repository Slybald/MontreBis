#!/usr/bin/env python3
"""
Diagramme de bord (Bords du modele) — Montre Connectee nRF5340  [SOMBRE]
=========================================================================
Methodologie : « Les Techniques de l'Ingenieur »
  Bords du modele = Entrees physiques -> Peripheriques internes -> uP
                     uP -> Peripheriques internes -> Sorties physiques

Style sombre reproduit d'apres les schemas du cours N. Richard (M1 Communicants).

Dependances :
  pip install graphviz Pillow
  brew install graphviz   (macOS)

Execution :
  python3 bords_projet_sombre.py
  -> genere bords_projet_sombre.png (16:9)
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
GREEN_BG = "#b7d7a8"
GREEN_BD = "#6aa84f"


def build_diagram() -> Digraph:
    """Construit et renvoie le graphe Digraph du diagramme de bord (sombre)."""

    dot = Digraph("Bords_du_Modele", format="png")

    # ---- Parametres globaux ----
    dot.attr(
        rankdir="LR",
        bgcolor=BG,
        dpi="300",
        pad="0.3",
        nodesep="0.35",
        ranksep="0.7",
        splines="polyline",
        fontname=FONT,
        fontsize="28",
        fontcolor=FG,
        label="\nBORDS DU MODELE\n",
        labelloc="t",
        labeljust="c",
    )
    dot.attr("node", fontname=FONT, fontsize="11", fontcolor=NODE_FG,
             margin="0.15,0.10")
    dot.attr("edge", fontname=FONT, fontsize="9", fontcolor=FG)

    # ==================================================================
    # Styles reutilisables
    # ==================================================================
    ext_in = dict(
        shape="box", style="rounded,filled",
        fillcolor=NODE_BG, color=BORDER,
        width="1.5", height="0.55",
    )
    periph = dict(
        shape="box", style="filled",
        fillcolor=NODE_BG, color=BORDER,
        width="1.3", height="0.5",
    )
    ext_out = dict(
        shape="box", style="rounded,filled",
        fillcolor=NODE_BG, color=BORDER,
        width="1.3", height="0.5",
    )
    out_label = dict(shape="plaintext", fontcolor=FG, fontsize="11")

    # Styles de fleches
    sg_n = dict(penwidth="2.0", arrowsize="0.8", color=FG)
    sg_1 = dict(penwidth="1.0", arrowsize="0.8", color=FG)
    irq  = dict(style="dashed", penwidth="1.0", arrowsize="0.8", color=FG)

    # ==================================================================
    # COLONNE 1 — Entrees physiques externes (capteurs, a gauche)
    # ==================================================================
    dot.node("LSM6DSO", "Accelerometer\nGyroscope\n(LSM6DSO)", **ext_in)
    dot.node("LIS2MDL", "Magnetometer\n(LIS2MDL)",             **ext_in)
    dot.node("HTS221",  "Temperature\nHumidity\n(HTS221)",     **ext_in)
    dot.node("LPS22HH", "Pressure\n(LPS22HH)",                 **ext_in)
    dot.node("RV8263",  "Time\nDate\n(RTC RV-8263-C8)",         **ext_in)

    with dot.subgraph() as s:
        s.attr(rank="same")
        for n in ("LSM6DSO", "LIS2MDL", "HTS221", "LPS22HH", "RV8263"):
            s.node(n)
    for a, b in [("LSM6DSO", "LIS2MDL"), ("LIS2MDL", "HTS221"),
                 ("HTS221", "LPS22HH"), ("LPS22HH", "RV8263")]:
        dot.edge(a, b, style="invis")

    # Labels boutons
    dot.node("SW0", "SW0", **out_label)
    dot.node("SW1", "SW1", **out_label)
    dot.node("SW2", "SW2", **out_label)
    dot.node("SW3", "SW3", **out_label)
    dot.node("Touchscreen", "Touchscreen", **ext_in)

    # ==================================================================
    # uControleur — cadre pointille MCU
    # ==================================================================
    with dot.subgraph(name="cluster_mcu") as mcu:
        mcu.attr(
            label="MCU",
            style="dashed",
            color=FG,
            fontsize="12",
            fontcolor=FG,
            fontname=f"{FONT} Bold",
            penwidth="2.0",
            labeljust="r",
            labelloc="b",
        )

        # -- Timers (col interne gauche-haut) --
        mcu.node("TIM1", "Timer 1\n(1Hz, 1s)",              **periph)
        mcu.node("TIM2", "Timer 2\n(100Hz, 10ms)",          **periph)
        mcu.node("TIM3", "Timer 3 channel 1\n(5Hz, 200ms)", **periph)
        mcu.node("TIM4", "Timer 3 channel 2\n(25Hz, 40ms)", **periph)

        # -- I2C --
        mcu.node("I2C1", "I2C1", **periph)
        mcu.node("I2C2", "I2C2", **periph)

        # -- Boutons GPIO --
        mcu.node("BTN1", "Button 1\nGPIO", **periph)
        mcu.node("BTN2", "Button 2\nGPIO", **periph)
        mcu.node("BTN3", "Button 3\nGPIO", **periph)
        mcu.node("BTN4", "Button 4\nGPIO", **periph)

        # -- SPI --
        mcu.node("SPI1", "SPI", **periph)

        # -- MPU central --
        mcu.node(
            "MPU", "\nMPU\n",
            shape="box", style="filled",
            fillcolor=BG, color=FG,
            width="2.0", height="5.5", fixedsize="true",
            fontsize="22", fontcolor=FG,
            fontname=f"{FONT} Bold",
        )

        # -- LEDs GPIO (droite MPU) --
        mcu.node("LED1", "LED 1\nGPIO", **periph)
        mcu.node("LED2", "LED 2\nGPIO", **periph)
        mcu.node("LED3", "LED 3\nGPIO", **periph)
        mcu.node("LED4", "LED 4\nGPIO", **periph)

        # -- Sorties com --
        mcu.node("USB_COM", "USB\nVirtual Com",   **periph)
        mcu.node("LCD",     "LCD shield",          **periph)
        mcu.node("BLE_COM", "BLE\nCommunication", **periph)

        # Ordres invisibles pour forcer la verticalite
        mcu.edge("TIM1", "TIM2", style="invis")
        mcu.edge("TIM2", "TIM3", style="invis")
        mcu.edge("TIM3", "TIM4", style="invis")
        mcu.edge("LED1", "LED2", style="invis")
        mcu.edge("LED2", "LED3", style="invis")
        mcu.edge("LED3", "LED4", style="invis")
        mcu.edge("USB_COM", "LCD", style="invis")
        mcu.edge("LCD", "BLE_COM", style="invis")
        mcu.edge("BTN1", "BTN2", style="invis")
        mcu.edge("BTN2", "BTN3", style="invis")
        mcu.edge("BTN3", "BTN4", style="invis")

    # ==================================================================
    # COLONNE — Sorties physiques externes
    # ==================================================================
    dot.node("led0", "led0", **out_label)
    dot.node("led1", "led1", **out_label)
    dot.node("led2", "led2", **out_label)
    dot.node("led3", "led3", **out_label)
    dot.node("Hyperterm", "Hyperterminal", **out_label)
    dot.node("Screen",    "Screen",        **out_label)
    dot.node("STM32",     "STM32",         **out_label)

    # ==================================================================
    # LEGENDE (coin haut-droit)
    # ==================================================================
    legend_html = """<
    <TABLE BORDER="1" CELLBORDER="0" CELLSPACING="3" CELLPADDING="4"
           BGCOLOR="#1a1a2e" COLOR="#555555">
      <TR>
        <TD><FONT POINT-SIZE="12" COLOR="#e0e0e0">&#x2192;</FONT></TD>
        <TD ALIGN="LEFT"><FONT POINT-SIZE="9" COLOR="#e0e0e0">1 bit</FONT></TD>
      </TR>
      <TR>
        <TD><FONT POINT-SIZE="12" COLOR="#e0e0e0"><B>&#x21D2;</B></FONT></TD>
        <TD ALIGN="LEFT"><FONT POINT-SIZE="9" COLOR="#e0e0e0">1 mot</FONT></TD>
      </TR>
      <TR>
        <TD><FONT POINT-SIZE="12" COLOR="#e0e0e0">&#x21E2;</FONT></TD>
        <TD ALIGN="LEFT"><FONT POINT-SIZE="9" COLOR="#e0e0e0">Une interruption</FONT></TD>
      </TR>
    </TABLE>
    >"""
    dot.node("legend", legend_html, shape="none")

    with dot.subgraph() as s:
        s.attr(rank="same")
        s.node("led0")
        s.node("legend")

    # ==================================================================
    # FLUX — Capteurs → I2C → MPU
    # ==================================================================
    for sensor in ("LSM6DSO", "LIS2MDL", "HTS221", "LPS22HH"):
        dot.edge(sensor, "I2C1", dir="both", weight="8", **sg_n)
    dot.edge("RV8263", "I2C2", weight="8", **sg_n)

    dot.edge("I2C1", "MPU", weight="8", **sg_n)
    dot.edge("I2C2", "MPU", weight="8", **sg_n)

    # ---- Timers → MPU (IRQ) ----
    dot.edge("TIM1", "MPU", label="MY_TIM_1_IRQ", weight="6", **irq)
    dot.edge("TIM2", "MPU", label="MY_TIM_2_IRQ", weight="6", **irq)
    dot.edge("TIM3", "MPU", label="MY_TIM_3_IRQ", weight="6", **irq)
    dot.edge("TIM4", "MPU", label="MY_TIM_4_IRQ", weight="6", **irq)

    # ---- Boutons → GPIO → MPU (IRQ) ----
    dot.edge("SW0", "BTN1", weight="6", **sg_1)
    dot.edge("SW1", "BTN2", weight="6", **sg_1)
    dot.edge("SW2", "BTN3", weight="6", **sg_1)
    dot.edge("SW3", "BTN4", weight="6", **sg_1)

    dot.edge("BTN1", "MPU", label="MY_BT_1_IRQ", weight="6", **irq)
    dot.edge("BTN2", "MPU", label="MY_BT_2_IRQ", weight="6", **irq)
    dot.edge("BTN3", "MPU", label="MY_BT_3_IRQ", weight="6", **irq)
    dot.edge("BTN4", "MPU", label="MY_BT_4_IRQ", weight="6", **irq)

    # ---- Touchscreen → SPI → MPU (IRQ) ----
    dot.edge("Touchscreen", "SPI1", label="SPI", weight="6", **sg_n)
    dot.edge("SPI1", "MPU", label="MY_TCH_IRQ", weight="6", **irq)

    # ==================================================================
    # FLUX — MPU → Sorties
    # ==================================================================
    dot.edge("MPU", "LED1", label="my_led_1", weight="6", **sg_1)
    dot.edge("MPU", "LED2", label="my_led_2", weight="6", **sg_1)
    dot.edge("MPU", "LED3", label="my_led_3", weight="6", **sg_1)
    dot.edge("MPU", "LED4", label="my_led_4", weight="6", **sg_1)

    dot.edge("MPU", "USB_COM", weight="6", **sg_n)
    dot.edge("MPU", "LCD",     weight="6", **sg_n)
    dot.edge("MPU", "BLE_COM", weight="6", **sg_n)

    # ---- Peripheriques → Sorties externes ----
    dot.edge("LED1", "led0", weight="6", **sg_1)
    dot.edge("LED2", "led1", weight="6", **sg_1)
    dot.edge("LED3", "led2", weight="6", **sg_1)
    dot.edge("LED4", "led3", weight="6", **sg_1)

    dot.edge("USB_COM", "Hyperterm", weight="6", **sg_n)
    dot.edge("LCD",     "Screen",    weight="6", **sg_n)
    dot.edge("BLE_COM", "STM32",     weight="6", **sg_n)

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
    output_path = diagram.render("bords_projet_sombre", cleanup=True)
    print(f"Diagramme de bord genere : {output_path}")
    fit_to_16_9(output_path)


if __name__ == "__main__":
    main()

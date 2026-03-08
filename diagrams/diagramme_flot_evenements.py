#!/usr/bin/env python3
"""
Diagramme de Flot d'Événements (DFE) — Montre Connectée nRF5340
================================================================
Méthodologie : « Les Techniques de l'Ingénieur »
  DFE = Chaînes de commande :
    - Identifier les commandes de contrôle (Start&Go, Start/Stop…)
    - Identifier les signaux de contrôle (IRQ, flags) → changent l'état de la ME
    - La ME centralise la gestion des ordres et la réception des signaux

Style reproduit d'après les schémas du cours N. Richard (M1 Communicants)
  → Figure 5 - Diagramme de flot d'événements

Dépendances :
  pip install graphviz Pillow
  brew install graphviz   (macOS)
  sudo apt install graphviz  (Linux)

Exécution :
  python3 diagramme_flot_evenements.py
  → génère diagramme_flot_evenements.png (16:9)
"""

from graphviz import Digraph
from PIL import Image
Image.MAX_IMAGE_PIXELS = None          # suppress DecompressionBombWarning

FONT = "Helvetica"

# Résolution cible 16:9
TARGET_W = 3840
TARGET_H = 2160


def build_diagram() -> Digraph:
    """Construit et renvoie le DFE de la montre connectée."""

    dot = Digraph("DFE_Montre", format="png")

    # ---- Paramètres globaux ----
    dot.attr(
        rankdir="LR",
        bgcolor="white",
        dpi="300",
        pad="0.3",
        nodesep="1.4",
        ranksep="0.8",
        splines="spline",
        fontname=FONT,
        fontsize="22",
        label=(
            "\nFIGURE 5 – Diagramme de flot d'événements\n"
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
    ext_in = dict(
        shape="box", style="rounded,filled",
        fillcolor="#fff2cc", color="#d6b656",
        width="1.7", height="0.65",
    )
    ext_out = dict(
        shape="box", style="rounded,filled",
        fillcolor="#dae8fc", color="#6c8ebf",
        width="1.7", height="0.65",
    )
    periph_in = dict(
        shape="box", style="filled",
        fillcolor="white", color="#333333",
        width="1.6", height="0.65",
    )
    # Blocs de traitement internes au µP (vert clair)
    task_block = dict(
        shape="box", style="rounded,filled",
        fillcolor="#d9ead3", color="#6aa84f",
        width="2.2", height="0.7",
    )
    # Blocs de données intermédiaires (parallélogramme, jaune pâle)
    data_block = dict(
        shape="parallelogram", style="filled",
        fillcolor="#fce5cd", color="#e69138",
        width="1.8", height="0.55",
        fontsize="11",
    )
    # Périphériques internes côté sortie
    periph_out = dict(
        shape="box", style="filled",
        fillcolor="white", color="#333333",
        width="1.6", height="0.65",
    )

    # Styles de flèches
    sg_n    = dict(penwidth="2.0", arrowsize="0.9", color="#333333")
    sg_1    = dict(penwidth="1.0", arrowsize="0.9", color="#333333")
    irq     = dict(style="dashed", penwidth="1.0", arrowsize="0.9",
                   color="#cc0000")
    startgo = dict(style="dashed", penwidth="1.5", arrowsize="0.9",
                   color="#274e13")

    # ==================================================================
    # COLONNE 1 — Entrées physiques externes
    # ==================================================================
    dot.node("HTS221",     "HTS221",                 **ext_in)
    dot.node("LPS22HH",    "LPS22HH",                **ext_in)
    dot.node("LIS2MDL",    "LIS2MDL",                 **ext_in)
    dot.node("LSM6DSO",    "LSM6DSO",                 **ext_in)
    dot.node("EcranTact",  "Écran Tactile\n(FT6206)", **ext_in)
    dot.node("Boutons",    "Boutons\nUtilisateur",     **ext_in)
    dot.node("RV8263",     "RV-8263-C8",               **ext_in)

    with dot.subgraph() as s:
        s.attr(rank="same")
        for n in ("HTS221", "LPS22HH", "LIS2MDL", "LSM6DSO",
                  "EcranTact", "Boutons", "RV8263"):
            s.node(n)

    # ==================================================================
    # COLONNE 2 — Périphériques internes côté entrée
    # ==================================================================
    dot.node("I2C1",       "I2C1\n(arduino_i2c)",  **periph_in)
    dot.node("GPIO_EXTI",  "GPIO / EXTI",           **periph_in)
    dot.node("I2C2",       "I2C2",                  **periph_in)
    dot.node("Timer250",   "k_timer\n(250 ms)",     **periph_in)
    dot.node("Timer10",    "k_timer\n(1 s)",        **periph_in)

    with dot.subgraph() as s:
        s.attr(rank="same")
        for n in ("I2C1", "GPIO_EXTI", "I2C2", "Timer250", "Timer10"):
            s.node(n)

    # ==================================================================
    # µControleur (nRF5340) — bloc MP étendu
    # ==================================================================
    with dot.subgraph(name="cluster_mp") as mp:
        mp.attr(
            label="MP — Matériel Programmé (nRF5340)",
            style="dashed",
            color="#444444",
            fontsize="16",
            fontname=f"{FONT} Bold",
            penwidth="2.0",
        )

        # ---- CONTROLLER : Thread FSM central (P5) ----
        mp.node(
            "ME", "CONTROLLER (P5)\nFSM : INIT/IDLE/\nACTIVE/SLEEP",
            shape="ellipse", style="filled",
            fillcolor="#cfe2f3", color="#3c78d8",
            width="2.8", height="1.5",
            fontsize="14", fontname=f"{FONT} Bold",
        )

        # ---- Threads dédiés ----
        mp.node(
            "INPUT_MGR",
            "INPUT_MGR (P3)\nFiltre & traduit\nentrées brutes",
            **task_block,
        )

        mp.node(
            "T_Capteurs",
            "SENSORS Thread (P7)\nLecture I2C",
            **task_block,
        )

        mp.node(
            "T_IHM",
            "DISPLAY Thread (P9)\nLVGL lv_task_handler",
            **task_block,
        )

        # ---- Traitements internes (dans CONTROLLER) ----
        mp.node(
            "T_Rotation",
            "Calcul Auto-Rotation\n(accéléromètre → 90°/270°)",
            **task_block,
        )

        mp.node(
            "T_MAJ_RTC",
            "MAJ Heure RTC\n(rtc_set_time après\nEVT_BLE_TIME_SYNC)",
            **task_block,
        )

        mp.node(
            "T_BLE_Notif",
            "Traitement BLE\n(Notifications GATT)",
            **task_block,
        )

        mp.node(
            "T_Log",
            "Log Console\n(LOG_INF, LOG_ERR)",
            **task_block,
        )

        # ---- Blocs de données intermédiaires ----
        mp.node(
            "D_Capteurs",
            "shared_sensors\n(k_mutex protégé)",
            **data_block,
        )

        mp.node(
            "D_Heure",
            "Tab Heure\n(time_offset, synced)",
            **data_block,
        )

        mp.node(
            "RAW_Q",
            "raw_input_q\n(k_msgq)",
            **data_block,
        )

        mp.node(
            "EVENT_BUS",
            "event_bus\n(k_msgq)",
            **data_block,
        )

        mp.node(
            "UI_FLAGS",
            "ui_action_flags\n(atomic_t)",
            **data_block,
        )

        # Ordre vertical interne approximatif
        mp.edge("INPUT_MGR", "ME", style="invis")
        mp.edge("ME", "T_Capteurs", style="invis")
        mp.edge("T_Capteurs", "T_IHM", style="invis")
        mp.edge("T_IHM", "T_MAJ_RTC", style="invis")
        mp.edge("T_MAJ_RTC", "T_BLE_Notif", style="invis")
        mp.edge("T_BLE_Notif", "T_Log", style="invis")

    # ==================================================================
    # COLONNE — Périphériques internes côté sortie
    # ==================================================================
    dot.node("SPI",        "SPI\n(arduino_spi)", **periph_out)
    dot.node("BLE_Radio",  "Radio\nBLE",          **periph_out)
    dot.node("UART",       "UART",                **periph_out)
    dot.node("I2C2_out",   "I2C2",                **periph_out)

    with dot.subgraph() as s:
        s.attr(rank="same")
        for n in ("SPI", "BLE_Radio", "UART", "I2C2_out"):
            s.node(n)

    # ==================================================================
    # COLONNE — Sorties physiques externes
    # ==================================================================
    dot.node("ILI9341",    "Écran TFT\nILI9341",        **ext_out)
    dot.node("Smartphone", "Smartphone\n(Central BLE)",
             shape="box", style="rounded,filled",
             fillcolor="#d5e8d4", color="#82b366",
             width="1.7", height="0.65")
    dot.node("Console",    "Terminal\nConsole",           **ext_out)
    dot.node("RTC_out",    "RV-8263-C8\n(RTC)",           **ext_out)

    with dot.subgraph() as s:
        s.attr(rank="same")
        for n in ("ILI9341", "Smartphone", "Console", "RTC_out"):
            s.node(n)

    # ==================================================================
    # LÉGENDE
    # ==================================================================
    legend_html = """<
    <TABLE BORDER="1" CELLBORDER="0" CELLSPACING="3" CELLPADDING="5"
           BGCOLOR="white" COLOR="#999999">
      <TR><TD COLSPAN="2" ALIGN="CENTER"><B><FONT POINT-SIZE="12">Légende</FONT></B></TD></TR>
      <TR>
        <TD><FONT POINT-SIZE="14"><B>&#x21D2;</B></FONT></TD>
        <TD ALIGN="LEFT"><FONT POINT-SIZE="10">Sg de n Bits</FONT></TD>
      </TR>
      <TR>
        <TD><FONT POINT-SIZE="14" COLOR="#cc0000">&#x21E2;</FONT></TD>
        <TD ALIGN="LEFT"><FONT POINT-SIZE="10">Interruption / événement</FONT></TD>
      </TR>
      <TR>
        <TD><FONT POINT-SIZE="14" COLOR="#274e13">&#x21E2;</FONT></TD>
        <TD ALIGN="LEFT"><FONT POINT-SIZE="10">Start&amp;Go séquentiel (appel)</FONT></TD>
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
        dot.edge(sensor, "I2C1", weight="6", **sg_n)

    dot.edge("EcranTact", "I2C1", weight="6", **sg_n)
    dot.edge("Boutons",   "GPIO_EXTI", weight="6", **sg_1)
    dot.edge("RV8263",    "I2C2", weight="6", **sg_n)

    # ==================================================================
    # FLUX — Interruptions & événements → raw_input_q / event_bus / ME
    # ==================================================================
    dot.edge(
        "GPIO_EXTI",
        "RAW_Q",
        label="  IRQ_Button\n  (RAW_BTNx)  ",
        weight="4",
        **irq,
    )

    dot.edge(
        "Timer250",
        "RAW_Q",
        label="  tick 250ms\n  (k_timer)  ",
        weight="4",
        **irq,
    )

    dot.edge(
        "Timer10",
        "RAW_Q",
        label="  tick 1s\n  (k_timer)  ",
        weight="4",
        **irq,
    )

    dot.edge(
        "RAW_Q",
        "INPUT_MGR",
        label="  raw_input\n  (btn/timer)  ",
        weight="3",
        **irq,
    )

    dot.edge(
        "INPUT_MGR",
        "EVENT_BUS",
        label="  struct event_msg\n  EVT_BTN_*, EVT_TICK_*  ",
        weight="3",
        **irq,
    )

    dot.edge(
        "BLE_Radio",
        "EVENT_BUS",
        label="  EVT_BLE_TIME_SYNC\n  (callback GATT)  ",
        constraint="false",
        **irq,
    )

    dot.edge(
        "EVENT_BUS",
        "ME",
        label="  struct event_msg\n  k_msgq_get(K_FOREVER)  ",
        weight="4",
        **irq,
    )

    # ==================================================================
    # FLUX — CONTROLLER → Threads (Start&Go via sémaphore / flags)
    # ==================================================================
    dot.edge(
        "ME",
        "T_Capteurs",
        label="  k_sem_give\n  (sem_sensor_start)  ",
        **startgo,
    )

    dot.edge(
        "ME",
        "T_IHM",
        label="  atomic_or\n  (ui_action_flags)  ",
        **startgo,
    )

    # ==================================================================
    # FLUX — Données internes (tâches → blocs de données → tâches)
    # ==================================================================

    # I2C1 ↔ thread de lecture capteurs (écriture adresse + lecture données)
    dot.edge(
        "I2C1",
        "T_Capteurs",
        label="  Données brutes\n  (registres I2C)  ",
        dir="both",
        weight="6",
        **sg_n,
    )

    # I2C2 ↔ traitement MAJ RTC (lecture heure + écriture heure)
    dot.edge(
        "I2C2",
        "T_MAJ_RTC",
        label="  Date/Heure  ",
        dir="both",
        weight="6",
        **sg_n,
    )

    # Lecture capteurs → shared_sensors (snapshot protégé par mutex)
    dot.edge(
        "T_Capteurs",
        "D_Capteurs",
        label="  sensor_snapshot\n  (k_mutex_lock/unlock)  ",
        **sg_n,
    )

    # Fin de lecture capteurs → event_bus (EVT_SENSOR_DATA_READY)
    dot.edge(
        "T_Capteurs",
        "EVENT_BUS",
        label="  EVT_SENSOR_DATA_READY\n  (lecture terminée)  ",
        **irq,
    )

    # shared_sensors → Rotation + IHM + BLE Notif
    dot.edge(
        "D_Capteurs",
        "T_Rotation",
        label="  Accel X/Y  ",
        constraint="false",
        **sg_n,
    )

    dot.edge(
        "D_Capteurs",
        "T_IHM",
        label="  Toutes mesures  ",
        **sg_n,
    )

    dot.edge(
        "D_Capteurs",
        "T_BLE_Notif",
        label="  T/H, P, Motion  ",
        **sg_n,
    )

    # MAJ RTC → Bloc Heure
    dot.edge(
        "T_MAJ_RTC",
        "D_Heure",
        label="  timestamp  ",
        **sg_n,
    )

    # Bloc Heure → IHM
    dot.edge(
        "D_Heure",
        "T_IHM",
        label="  Date/Heure  ",
        constraint="false",
        **sg_n,
    )

    # Flags d'action UI : CONTROLLER → ui_action_flags → DISPLAY
    dot.edge(
        "ME",
        "UI_FLAGS",
        label="  UI_ACT_*  ",
        **sg_1,
    )

    dot.edge(
        "UI_FLAGS",
        "T_IHM",
        label="  atomic_get/clear\n  UI_ACT_*  ",
        **sg_1,
    )

    # ==================================================================
    # FLUX — Tâches → Périphériques de sortie → Sorties physiques
    # ==================================================================

    # IHM → SPI → Écran
    dot.edge("T_IHM", "SPI",
             label="  Affichage  ",
             weight="6", **sg_n)
    dot.edge("SPI", "ILI9341",
             label="  Affichage  ",
             weight="6", **sg_n)

    # Rotation → SPI (changement orientation)
    dot.edge("T_Rotation", "SPI",
             label="  Orientation  ",
             constraint="false", **sg_1)

    # BLE Notif → Radio BLE → Smartphone
    dot.edge("T_BLE_Notif", "BLE_Radio",
             label="  Notifications\n  GATT  ",
             weight="6", **sg_n)
    dot.edge("BLE_Radio", "Smartphone",
             dir="both", weight="6", **sg_n)

    # Log → UART → Console
    dot.edge("T_Log", "UART",
             label="  Log  ",
             weight="6", **sg_1)
    dot.edge("UART", "Console",
             label="  Affichage  ",
             weight="6", **sg_1)

    # MAJ RTC → I2C2 → RTC hardware
    dot.edge("T_MAJ_RTC", "I2C2_out",
             label="  rtc_set_time  ",
             weight="6", **sg_n)
    dot.edge("I2C2_out", "RTC_out",
             label="  MAJ Heure  ",
             weight="6", **sg_n)

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
    output_path = diagram.render("diagramme_flot_evenements", cleanup=True)
    print(f"Diagramme de flot d'événements généré : {output_path}")
    fit_to_16_9(output_path)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""
Diagramme de Machine d'État (ME) — Montre Connectée nRF5340
============================================================
Méthodologie : « Les Techniques de l'Ingénieur » (N. Richard, M1 Communicants)
  Machine d'État = le chef unique (commande centralisée hiérarchique)
    - Centralise la gestion des ordres et la réception des signaux de contrôle
    - Structure en 2 parties :
        1. Calcul de l'état futur  : état(t+1) = f(état(t), entrées/evt)
        2. Calcul des sorties      : Moore Out(t) = g(état(t))
    - Implémentée dans le Thread CONTROLLER (Priorité 5)
    - Consomme l'event_bus (k_msgq_get K_FOREVER → CPU dort si rien à faire)

Style reproduit d'après les schémas du cours N. Richard (M1 Communicants)
  → Figure 6 - Diagramme de la machine d'état

Dépendances :
  pip install graphviz Pillow
  brew install graphviz   (macOS)
  sudo apt install graphviz  (Linux)

Exécution :
  python3 diagramme_machine_etat.py
  → génère diagramme_machine_etat.png (16:9)
"""

from graphviz import Digraph
from PIL import Image
Image.MAX_IMAGE_PIXELS = None          # suppress DecompressionBombWarning

FONT = "Helvetica"

# Résolution cible 16:9
TARGET_W = 3840
TARGET_H = 2160


def build_diagram() -> Digraph:
    """Construit et renvoie le diagramme de machine d'état du CONTROLLER."""

    dot = Digraph("ME_Montre", format="png")

    # ---- Paramètres globaux ----
    dot.attr(
        rankdir="LR",
        bgcolor="white",
        dpi="300",
        pad="0.5",
        nodesep="1.2",
        ranksep="2.0",
        splines="spline",
        fontname=FONT,
        fontsize="22",
        label=(
            "\nFIGURE 6 – Machine d'État du CONTROLLER (Thread P5)\n"
            "Montre Connectée (nRF5340) — Architecture Multitâche Préemptive\n"
        ),
        labelloc="b",
        labeljust="c",
    )
    dot.attr("node", fontname=FONT, fontsize="13", margin="0.25,0.2")
    dot.attr("edge", fontname=FONT, fontsize="11")

    # ==================================================================
    # Styles réutilisables
    # ==================================================================

    # États de la FSM (ellipses bleues — convention ME, Techniques de l'Ingénieur)
    state_style = dict(
        shape="ellipse", style="filled",
        fillcolor="#cfe2f3", color="#3c78d8",
        fontname=f"{FONT} Bold",
        fontsize="15",
        width="2.8", height="1.3",
    )

    # Point initial (cercle noir plein)
    init_dot = dict(
        shape="circle", style="filled",
        fillcolor="#333333", color="#333333",
        width="0.3", height="0.3",
        fixedsize="true", label="",
    )

    # Actions dans les états (boîtes vertes — blocs de traitement)
    action_style = dict(
        shape="box", style="rounded,filled",
        fillcolor="#d9ead3", color="#6aa84f",
        fontsize="11",
        width="2.6", height="0.5",
    )

    # Flèches de transition
    trans_evt = dict(
        penwidth="2.0", arrowsize="1.0", color="#333333",
    )
    trans_self = dict(
        penwidth="1.5", arrowsize="0.9", color="#6c8ebf",
        style="dashed",
    )
    trans_init = dict(
        penwidth="2.5", arrowsize="1.2", color="#333333",
    )

    # ==================================================================
    # POINT DE DÉPART
    # ==================================================================
    dot.node("start", **init_dot)

    # ==================================================================
    # ÉTATS DE LA FSM
    # ==================================================================
    dot.node(
        "INIT",
        "STATE_INIT\n─────────────\nDémarrage système\nAttente 1er événement",
        **state_style,
    )

    idle_style = dict(state_style)
    idle_style["width"] = "3.2"
    idle_style["height"] = "1.6"
    dot.node(
        "IDLE",
        "STATE_IDLE\n─────────────\nFonctionnement normal\nÉcoute event_bus\n(k_msgq_get K_FOREVER)",
        **idle_style,
    )

    dot.node(
        "ACTIVE",
        "STATE_ACTIVE\n─────────────\nLecture capteurs I2C\nen cours (SENSORS P7)",
        **state_style,
    )

    dot.node(
        "SLEEP",
        "STATE_SLEEP\n─────────────\nÉcran éteint\nCapteurs en pause\nVeille basse conso",
        **state_style,
    )

    # ==================================================================
    # BLOCS D'ACTIONS (associés aux transitions)
    # ==================================================================
    dot.node(
        "ACT_init_idle",
        "last_activity = k_uptime_get()\nLog: INIT → IDLE",
        **action_style,
    )

    dot.node(
        "ACT_idle_active",
        "k_sem_give(&sem_sensor_start)\n→ Réveiller SENSORS Thread (P7)",
        **action_style,
    )

    dot.node(
        "ACT_active_idle",
        "process_sensor_data()\n→ BLE notify + auto-rotation\n→ atomic_or(ui_action_flags)",
        **action_style,
    )

    dot.node(
        "ACT_idle_sleep",
        "display_blanking_on()\nk_timer_stop(&timer_sensors)\nLog: IDLE → SLEEP",
        **action_style,
    )

    dot.node(
        "ACT_sleep_idle",
        "display_blanking_off()\nk_timer_start(&timer_sensors)\nLog: SLEEP → IDLE",
        **action_style,
    )

    # ==================================================================
    # TRANSITIONS PRINCIPALES (changements d'état)
    # ==================================================================

    # start → INIT
    dot.edge("start", "INIT", label="  Power-On\n  (main() init)  ", **trans_init)

    # INIT → IDLE (via action)
    dot.edge(
        "INIT", "ACT_init_idle",
        label="  1er événement\n  reçu (any)  ",
        **trans_evt,
    )
    dot.edge("ACT_init_idle", "IDLE", **trans_evt)

    # IDLE → ACTIVE (via action)
    dot.edge(
        "IDLE", "ACT_idle_active",
        label="  EVT_TICK_SENSORS\n  (k_timer 250ms)  ",
        **trans_evt,
    )
    dot.edge("ACT_idle_active", "ACTIVE", **trans_evt)

    # ACTIVE → IDLE (via action)
    dot.edge(
        "ACTIVE", "ACT_active_idle",
        label="  EVT_SENSOR_DATA_READY\n  (SENSORS terminé)  ",
        **trans_evt,
    )
    dot.edge("ACT_active_idle", "IDLE", **trans_evt)

    # IDLE → SLEEP (via action)
    dot.edge(
        "IDLE", "ACT_idle_sleep",
        label="  EVT_TICK_TIME\n  + inactivité > 30s  ",
        **trans_evt,
    )
    dot.edge("ACT_idle_sleep", "SLEEP", **trans_evt)

    # SLEEP → IDLE (via action)
    dot.edge(
        "SLEEP", "ACT_sleep_idle",
        label="  EVT_BTN_*\n  (appui bouton)  ",
        **trans_evt,
    )
    dot.edge("ACT_sleep_idle", "IDLE", **trans_evt)

    # ==================================================================
    # TRANSITIONS INTERNES (boucles sur même état)
    # ==================================================================

    # IDLE : boutons → reste en IDLE
    dot.edge(
        "IDLE", "IDLE",
        label=(
            "  EVT_BTN_SWITCH_SCREEN → UI_ACT_SWITCH_SCREEN\n"
            "  EVT_BTN_CYCLE_THEME → UI_ACT_CYCLE_THEME\n"
            "  EVT_BTN_BRIGHTNESS → UI_ACT_BRIGHTNESS\n"
            "  EVT_BTN_STATUS_POPUP → UI_ACT_STATUS_POPUP\n"
            "  (atomic_or + reset last_activity)  "
        ),
        **trans_self,
    )

    # IDLE : BLE time sync → reste en IDLE
    dot.edge(
        "IDLE", "IDLE",
        label=(
            "  EVT_BLE_TIME_SYNC\n"
            "  → handle_ble_time_sync()\n"
            "  → rtc_set_time()  "
        ),
        headport="s", tailport="s",
        **trans_self,
    )

    # ACTIVE : boutons → reste en ACTIVE
    dot.edge(
        "ACTIVE", "ACTIVE",
        label=(
            "  EVT_BTN_*\n"
            "  → atomic_or(ui_action_flags)\n"
            "  + reset last_activity  "
        ),
        **trans_self,
    )

    # ACTIVE : BLE time sync → reste en ACTIVE
    dot.edge(
        "ACTIVE", "ACTIVE",
        label=(
            "  EVT_BLE_TIME_SYNC\n"
            "  → handle_ble_time_sync()  "
        ),
        headport="s", tailport="s",
        **trans_self,
    )

    # SLEEP : tout sauf boutons → ignoré
    dot.edge(
        "SLEEP", "SLEEP",
        label=(
            "  EVT_TICK_* / EVT_BLE_*\n"
            "  → ignoré (veille)  "
        ),
        **trans_self,
    )

    # ==================================================================
    # RANG pour aligner horizontalement les états principaux
    # ==================================================================
    with dot.subgraph() as s:
        s.attr(rank="same")
        s.node("start")
        s.node("INIT")

    with dot.subgraph() as s:
        s.attr(rank="same")
        s.node("IDLE")
        s.node("SLEEP")

    # ==================================================================
    # LÉGENDE
    # ==================================================================
    legend_html = """<
    <TABLE BORDER="1" CELLBORDER="0" CELLSPACING="3" CELLPADDING="5"
           BGCOLOR="white" COLOR="#999999">
      <TR><TD COLSPAN="2" ALIGN="CENTER"><B><FONT POINT-SIZE="13">Légende ME</FONT></B></TD></TR>
      <TR>
        <TD BGCOLOR="#cfe2f3" BORDER="1" COLOR="#3c78d8">&nbsp;&#x25CB;&nbsp;</TD>
        <TD ALIGN="LEFT"><FONT POINT-SIZE="10">État de la FSM</FONT></TD>
      </TR>
      <TR>
        <TD BGCOLOR="#d9ead3" BORDER="1" COLOR="#6aa84f">&nbsp;&#x25A1;&nbsp;</TD>
        <TD ALIGN="LEFT"><FONT POINT-SIZE="10">Actions de sortie (Moore)</FONT></TD>
      </TR>
      <TR>
        <TD BGCOLOR="#333333" BORDER="1" COLOR="#333333">&nbsp;&nbsp;&nbsp;</TD>
        <TD ALIGN="LEFT"><FONT POINT-SIZE="10">Point initial</FONT></TD>
      </TR>
      <TR>
        <TD><FONT POINT-SIZE="14"><B>&#x21D2;</B></FONT></TD>
        <TD ALIGN="LEFT"><FONT POINT-SIZE="10">Transition (événement)</FONT></TD>
      </TR>
      <TR>
        <TD><FONT POINT-SIZE="14" COLOR="#6c8ebf">&#x21E2;</FONT></TD>
        <TD ALIGN="LEFT"><FONT POINT-SIZE="10">Transition interne (boucle)</FONT></TD>
      </TR>
    </TABLE>
    >"""
    dot.node("legend", legend_html, shape="none")

    with dot.subgraph() as s:
        s.attr(rank="same")
        s.node("SLEEP")
        s.node("legend")

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
    output_path = diagram.render("diagramme_machine_etat", cleanup=True)
    print(f"Diagramme de machine d'état généré : {output_path}")
    fit_to_16_9(output_path)


if __name__ == "__main__":
    main()

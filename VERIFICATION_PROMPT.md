# Prompt de Vérification Complète — Firmware Montre Connectée (Zephyr / nRF5340)

> Ce document est un guide d'audit technique exhaustif du projet. Il synthétise toute la connaissance nécessaire issue des sources, de la documentation Zephyr, des datasheets composants et des bonnes pratiques embarquées. Chaque section indique quoi vérifier, pourquoi, et quelle référence consulter.

---

## Contexte Projet

Firmware **event-driven** multi-thread sur **Zephyr RTOS** pour une montre connectée à base de **Nordic nRF5340 DK**. Le projet combine :
- 4 capteurs I2C (X-NUCLEO-IKS01A3 shield)
- Affichage TFT 320×240 ILI9340 via SPI (Adafruit 2.8" TFT Touch Shield v2)
- Tactile résistif TSC2007 via I2C
- RTC Micro Crystal RV-8263-C8 via I2C2
- BLE en rôle périphérique (GATT custom, 7 caractéristiques)
- NVS/Settings pour persistance (thème, pas, horodatage)

---

## 1. Architecture Multi-Thread et IPC

### Contexte documentaire
- **Zephyr Kernel API** : [https://docs.zephyrproject.org/latest/kernel/services/](https://docs.zephyrproject.org/latest/kernel/services/)
- **Message Queues** : `k_msgq_put` est ISR-safe avec `K_NO_WAIT`. `k_msgq_get` bloque avec `K_FOREVER` ou timeout.
- **Sémaphores** : `k_sem_give` est ISR-safe. `k_sem_take` est bloquant pour les threads.
- **Mutex** : `k_mutex_lock`/`unlock` sont exclusivement thread-context (pas ISR).
- **Atomiques** : `atomic_or`, `atomic_cas`, `atomic_clear`, `atomic_set`, `atomic_get` sont ISR-safe, utilisant `__ATOMIC_SEQ_CST`.

### Points à vérifier

#### 1.1 Tailles des files
```
K_MSGQ_DEFINE(event_bus,    sizeof(struct event_msg), 16, 4);
K_MSGQ_DEFINE(raw_input_q,  sizeof(enum raw_input),   16, 4);
```
- `event_bus` : 16 éléments. Pendant une rafale (4 boutons + tick capteurs 250 ms + tick temps 1 s simultanés) : saturation possible si le contrôleur est bloqué. Vérifier que les appels `event_post_log()` loggent correctement les drops.
- `raw_input_q` : 16 éléments partagés entre 4 ISR boutons + 2 timers. Suffisant normalement mais à surveiller.
- **Action** : Confirmer que `-EBUSY` est géré sans crash et qu'aucun appel n'utilise `K_FOREVER` depuis une ISR.

#### 1.2 Priorités et risque d'inversion
| Thread | Priorité | Stack |
|---|---|---|
| `input_mgr_tid` | 3 | 1 Ko |
| `controller_tid` | 5 | 4 Ko |
| `sensors_tid` | 7 | 4 Ko |
| `display_tid` | 9 | 10 Ko |

- Vérifier qu'aucun thread de priorité haute n'attend un mutex tenu par un thread de priorité basse sans protocole d'inversion (Zephyr implémente le Priority Inheritance sur les mutexes).
- `mtx_sensors` : tenu par `sensors_tid` (P7) lors de l'écriture, acquis par `controller_tid` (P5) et `display_tid` (P9) en lecture. Le contrôleur (P5) peut bloquer brièvement sur le capteur (P7) → vérifier que `K_FOREVER` est justifié ici.

#### 1.3 Race condition sur le flag de veille
```c
// Dans display_thread.c
if (entering_sleep) {
    ui_show_sleep_overlay();
    lv_refr_now(NULL);
    atomic_set(&display_sleeping, 1);   // (A)
    continue;
}
```
```c
// Dans controller.c — enter_sleep_mode()
k_sem_reset(&sem_display_wake);         // (B)
atomic_or(&ui_action_flags, UI_ACT_ENTER_SLEEP);
```
- Scénario de race : entre (B) et (A), un bouton de réveil peut arriver. Le contrôleur appellera `k_sem_give(&sem_display_wake)` avant que le display soit effectivement endormi. Le display consommera ce `k_sem_take` immédiatement et ne s'endormira jamais, mais `display_sleeping` reste à 0.
- **Action** : Vérifier si ce scénario est géré (probablement acceptable pour un démonstrateur, mais à documenter).

#### 1.4 Pattern atomic_clear + traitement (display_thread.c)
```c
atomic_val_t actions = atomic_clear(&ui_action_flags);
```
- Correct : lecture atomique + remise à zéro en une opération. Les flags arrivés après ce clear seront traités au prochain cycle. Pas de perte.

---

## 2. Capteurs I2C (X-NUCLEO-IKS01A3)

### Contexte documentaire
- **HTS221** (temp/humidité) : I2C addr 0x5F, ODR 1/7/12.5 Hz, précision ±0.5°C / ±3.5% RH. [Datasheet ST](https://www.st.com/resource/en/datasheet/hts221.pdf)
- **LPS22HH** (pression) : I2C addr 0x5C/0x5D, ODR 0..200 Hz, plage 260-1260 hPa, précision 0.5 hPa. [Datasheet ST](https://www.mouser.de/datasheet/2/389/lps22hh-1395924.pdf)
  - Zephyr binding ODR values : 0=Power Down, **1=1Hz, 2=10Hz**, 3=25Hz, ...
- **LSM6DSO** (accél/gyro) : I2C addr 0x6A/0x6B. [Datasheet ST](https://www.st.com/resource/en/datasheet/lsm6dso.pdf)
  - Zephyr binding `accel-odr` : **2 = 26 Hz**. `gyro-odr` : **2 = 26 Hz**.
  - `accel-range = <0>` = **±2g** (0.061 mg/LSB). `gyro-range = <0>` = **±250 dps**.
- **LIS2MDL** (magnéto) : I2C addr 0x1E, plage ±50 gauss, sensibilité 1.5 mgauss/LSB. [Datasheet ST](https://www.st.com/resource/en/datasheet/lis2mdl.pdf)

### Points à vérifier

#### 2.1 ODR LPS22HH dans app.overlay
```dts
&lps22hh_x_nucleo_iks01a3 { odr = <2>; };
```
- `odr = <2>` = 10 Hz. Le code lit les capteurs toutes les 250 ms (4 Hz en actif, 2 s en veille). L'ODR doit être ≥ fréquence de polling → **10 Hz > 4 Hz : correct**.

#### 2.2 ODR LSM6DSO dans app.overlay
```dts
&lsm6dso_6b_x_nucleo_iks01a3 {
    accel-odr = <2>;   /* 26 Hz */
    gyro-odr = <2>;    /* 26 Hz */
    accel-range = <0>; /* ±2g */
    gyro-range = <0>;  /* ±250 dps */
};
```
- **Vérifier** : `accel-odr = <2>` dans le binding Zephyr LSM6DSO correspond bien à 26 Hz (référence : [st,lsm6dso-i2c binding](https://docs.zephyrproject.org/latest/build/dts/api/bindings/sensor/st,lsm6dso-i2c.html)).
- La plage ±2g donne 0.061 mg/LSB ≈ 0.000598 m/s²/LSB. En mode de polling, les données sont lues via `sensor_sample_fetch` + `sensor_channel_get`.

#### 2.3 Mode polling vs triggers
```
CONFIG_LIS2MDL_TRIGGER_NONE=y
CONFIG_LPS22HH_TRIGGER_NONE=y
CONFIG_LSM6DSO_TRIGGER_NONE=y
```
- Correct pour ce projet : le thread sensors fait du polling via sémaphore. Les interruptions DATA_READY ne sont pas utilisées.
- **Conséquence** : un `sensor_sample_fetch` peut retourner des données déjà présentes (pas nécessairement les plus récentes). Acceptable pour 250 ms de période.

#### 2.4 Vérification du nœud DTS sensor_init
```c
hts221_dev  = DEVICE_DT_GET(DT_NODELABEL(hts221_x_nucleo_iks01a3));
lps22hh_dev = DEVICE_DT_GET(DT_NODELABEL(lps22hh_x_nucleo_iks01a3));
lis2mdl_dev = DEVICE_DT_GET(DT_NODELABEL(lis2mdl_1e_x_nucleo_iks01a3));
lsm6dso_dev = DEVICE_DT_GET(DT_NODELABEL(lsm6dso_6b_x_nucleo_iks01a3));
```
- Ces labels correspondent aux nœuds définis dans le shield `x_nucleo_iks01a3`. Vérifier que le shield est bien inclus dans le build (`-DSHIELD=x_nucleo_iks01a3`).

---

## 3. BLE — Service GATT Custom

### Contexte documentaire
- **Zephyr BLE API** : [https://docs.zephyrproject.org/latest/connectivity/bluetooth/](https://docs.zephyrproject.org/latest/connectivity/bluetooth/)
- **Problème connu** : `bt_gatt_notify()` appelé depuis un thread préemptif peut déclencher des assertions dans la couche L2CAP (issue Zephyr #89705). Recommandation : utiliser un workqueue ou le thread système BLE.
- **Deadlock workqueue** : Appeler `bt_gatt_notify()` depuis le system workqueue peut causer un deadlock (issue #53455).
- **Callbacks de connexion** : `connected()` et `disconnected()` s'exécutent sur le thread BLE RX, pas en ISR.

### Points à vérifier

#### 3.1 Appel bt_gatt_notify() depuis controller_tid (préemptif P5)
```c
// controller.c → process_sensor_data()
ble_update_env_data(...);   // appelle bt_gatt_notify
ble_update_pressure(...);
...
```
- `controller_tid` est un thread préemptif de priorité 5. Appeler `bt_gatt_notify()` directement est **potentiellement problématique** selon l'issue Zephyr #89705.
- **Action recommandée** : Utiliser un `k_work_submit` vers un workqueue dédié pour les notifications BLE, ou appeler les `ble_update_*` depuis un contexte compatible.

#### 3.2 Accès non protégé à current_conn
```c
static struct bt_conn *current_conn;  // modifié par BLE RX thread
...
bool ble_is_connected(void) {
    return (current_conn != NULL);    // lu par display_tid
}
```
- `current_conn` est écrit dans `connected()`/`disconnected()` (BLE RX thread) et lu dans `ble_is_connected()` (display_tid). Sur ARM Cortex-M33, une lecture de pointeur 32 bits est atomique au niveau matériel mais ce n'est pas garanti par le standard C.
- **Action** : Protéger avec un mutex ou utiliser `atomic_ptr_t` (ou au minimum `volatile`).

#### 3.3 Re-advertising dans disconnected()
```c
bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
```
- Correct : redémarrage automatique de l'advertising après déconnexion. Vérifier que `BT_LE_ADV_CONN_FAST_1` est défini dans la version Zephyr utilisée.

#### 3.4 UUID custom
- UUID service : `12345678-1234-5678-1234-56789abcdef0`. Conflit potentiel si d'autres appareils utilisent le même UUID. Pour un produit, utiliser un UUID généré par UUID v4.

#### 3.5 Encodage des données BLE
| Caractéristique | Format | Encodage |
|---|---|---|
| env_data | temp(2B signed) + hum(2B unsigned) | temp = °C × 100, hum = % × 100 |
| press_data | 4B unsigned | Pression en Pa × 1000 (soit mPa) |
| motion_data | 6 × int16_t | accél en m/s² × 100, gyro en dps × 100 |
| mag_data | 3 × int16_t | magnéto en mgauss (gauss × 1000) |
| compass_data | int16_t | heading en centidegrés |
| steps_data | uint32_t LE | compteur brut |

- **Vérifier** : cohérence entre le header (`ble_service.h`) et l'implémentation. La pression est documentée "Pa, integer" dans le header mais encodée `pressure * 1000.0f` dans `process_sensor_data()` → si la valeur Zephyr est en kPa, alors `pressure * 1000.0f` donne Pa × 1 = correct.
- **Vérifier** : le capteur Zephyr retourne la pression en **kPa**. `snap.pressure * 1000.0f` = Pa. Mais `press_data` documente "Pressure (Pa, integer)" → `uint32_t(kPa * 1000)` = valeur en Pa. **Cohérent**.

---

## 4. RTC et Gestion du Temps

### Contexte documentaire
- **RV-8263-C8** : RTC I2C, addr 0x51. Driver Zephyr mergé en juillet 2024 (v4.0). [Zephyr binding](https://docs.zephyrproject.org/latest/build/dts/api/bindings/rtc/microcrystal%2Crv-8263-c8.html). Registres temps : 0x04–0x0A.
- **mktime() vs gmtime_r()** : `mktime()` interprète `struct tm` comme **heure locale** et applique le fuseau horaire. Sur la plupart des systèmes embarqués, TZ=UTC mais ce n'est pas garanti. `timeutil_timegm()` (Zephyr) garantit la conversion UTC sans dépendance au fuseau.

### Points à vérifier

#### 4.1 Incohérence mktime() / gmtime_r()
```c
// rtc_time.c — rtc_time_to_unix() : lecture RTC → Unix
t.tm_isdst = -1;
return mktime(&t);      // ← utilise le fuseau LOCAL

// rtc_time.c — unix_to_rtc_time() : écriture dans la RTC après sync BLE
gmtime_r(&timestamp, &t);   // ← UTC strictement
```
- **Bug potentiel** : Si la RTC stocke de l'UTC (ce qui est le cas puisqu'on y écrit via `gmtime_r()`), alors la relecture doit aussi être interprétée comme UTC. `mktime()` pourrait ajouter un offset TZ.
- **Action** : Remplacer `mktime(&t)` par `timeutil_timegm(&t)` (Zephyr) ou `timegm(&t)` (POSIX) pour garantir une conversion UTC→Unix cohérente.

#### 4.2 Plage de validité des timestamps
```c
#define MIN_VALID_TIMESTAMP 1704067200   // 01/01/2024 00:00:00 UTC
#define MAX_VALID_TIMESTAMP 4102444800U  // 01/01/2100 00:00:00 UTC
```
- `time_t` sur ARM Cortex-M33 avec Zephyr : 32 bits signé → overflow en 2038. `MAX_VALID_TIMESTAMP = 4102444800` > 2^31-1 = 2147483647. Ce sera un problème à partir de 2038.
- **Action** : Vérifier la taille de `time_t` dans la toolchain Zephyr utilisée. Envisager `timeutil_timegm64()` et `int64_t` timestamps.

#### 4.3 Retry sur lecture RTC
```c
for (int attempt = 0; attempt < RTC_READ_ATTEMPTS; attempt++) {
    err = rtc_get_time(rtc_dev, &rt);
    if (err == 0 || err == -ENODATA) { break; }
    k_msleep(RTC_READ_RETRY_DELAY_MS);
}
```
- Logique correcte. `RTC_READ_ATTEMPTS=5`, `RTC_READ_RETRY_DELAY_MS=50` → jusqu'à 250 ms de délai total. Acceptable.

#### 4.4 Alimentation GPIO de la RTC (P0.20)
```dts
rtc_supply: rtc-power {
    compatible = "regulator-fixed";
    enable-gpios = <&gpio0 20 GPIO_ACTIVE_HIGH>;
    regulator-boot-on;
    startup-delay-us = <1000>;
};
```
- P0.20 alimente la RTC. Le délai de démarrage de 1 ms doit être suffisant selon la datasheet RV-8263-C8 (temps de démarrage typique < 1 ms à 3.3V).
- **Note** : Le commentaire dans le DTS mentionne "vin-supply removed to avoid init priority conflict". Vérifier que la RTC est bien initialisée après que le régulateur soit stable.

---

## 5. Affichage — LVGL v8 et ILI9340

### Contexte documentaire
- **LVGL v8 thread safety** : LVGL n'est PAS thread-safe par défaut. Un seul thread doit appeler toutes les fonctions LVGL. [OS and interrupts docs](https://docs.lvgl.io/8.0/porting/os.html)
- **lv_task_handler()** : doit être appelé ~toutes les 5-10 ms. Appelé ici toutes les 10 ms (100 Hz). Correct.
- **Bug Zephyr ILI9XXX** : `display_set_orientation()` ne met pas à jour les dimensions logiques (issue #33269). Le display reste 240×320 dans le driver après rotation.

### Points à vérifier

#### 5.1 Thread safety LVGL
```c
// display_thread.c — seul thread appelant LVGL
lv_task_handler();
ui_switch_screen();
ui_update_temp_humidity(...);
...
```
- **Correct** : un seul thread (`display_tid`, P9) appelle toutes les fonctions LVGL.
- **Vérifier** : aucune fonction LVGL n'est appelée depuis `controller_tid`, `sensors_tid`, ou `main()` après `ui_init()`.

#### 5.2 Tick LVGL
- Zephyr avec `CONFIG_LV_Z_AUTO_INIT=y` gère automatiquement le tick LVGL via un timer noyau. Vérifier que `lv_tick_inc()` n'est pas aussi appelé manuellement (double-tick).

#### 5.3 Auto-rotation et bug ILI9XXX
```c
int err = display_set_orientation(display_dev, rot_target);
if (err == 0) {
    rot_current = rot_target;
    atomic_or(&ui_action_flags, UI_ACT_ROTATION_CHANGE);
}
```
- **Bug connu** : Après `display_set_orientation(ROTATED_270)`, le driver Zephyr ILI9XXX ne met pas à jour `hor_res`/`ver_res`. Les widgets LVGL continuent à penser que l'écran fait 320×240.
- En pratique : si la rotation est entre 90° et 270° (les deux modes landscape), les dimensions restent identiques (320×240). Le bug n'affecte que le passage portrait↔paysage. Ici l'appareil ne passe jamais en portrait → **peu d'impact dans ce projet**.
- **Vérifier** : que `rot_current` est initialisé à `DISPLAY_ORIENTATION_ROTATED_90` cohérent avec `rotation = <90>` dans l'overlay.

#### 5.4 Encodage couleurs (fix_color)
```c
static inline uint32_t fix_color(uint32_t color)
{
    return ((color & 0xFF0000) >> 16) |
           (color & 0x00FF00) |
           ((color & 0x0000FF) << 16);
}
```
- Ce swap R↔B corrige le fait que l'ILI9340 dans la configuration Zephyr utilise BGR au lieu de RGB. **Correct** pour ce shield.
- **Vérifier** : que `CONFIG_LV_COLOR_DEPTH_16=y` correspond bien au format RGB565 du driver ILI9340.

#### 5.5 Pool mémoire LVGL
```
CONFIG_LV_Z_MEM_POOL_SIZE=32768  /* 32 Ko */
CONFIG_LV_Z_VDB_SIZE=20          /* 20% du framebuffer */
```
- Framebuffer complet : 320×240×2 = 153 600 octets. 20% VDB = ~30 720 octets.
- **Vérifier** : que la mémoire totale LVGL (pool 32 Ko + VDB ~30 Ko) rentre dans la RAM disponible du nRF5340 (512 Ko RAM pour le cœur applicatif en mode non-sécurisé).

---

## 6. Tactile — TSC2007 et FT6206

### Contexte documentaire
- **TSC2007** : contrôleur tactile résistif 4 fils, I2C addr 0x48-0x4B (configurable), ADC 12 bits. Communication par commandes simples (pas de registres continus). [Datasheet TI](https://www.ti.com/document-viewer/TSC2007/datasheet)
  - Commande = `(FUNC << 4) | (POWER << 2) | (RESOLUTION << 1)`
  - FUNC : 0x0C=X, 0x0D=Y, 0x0E=Z1, 0x0F=Z2, 0x00=TEMP0
  - Lecture en 2 octets : valeur 12 bits dans les 12 MSB (`(buf[0]<<4)|(buf[1]>>4)`)
- **FT6206** : contrôleur capacitif (non utilisé avec TSC2007 résistif).

### Points à vérifier

#### 6.1 touch.c est du code mort
- `src/touch.c` implémente un driver FT6206 (capacitif, addr 0x38).
- `src/touch.c` n'est **PAS** dans `CMakeLists.txt`.
- `init_touch()` n'est **JAMAIS** appelé depuis `main.c`.
- Le vrai driver tactile est `src/input_tsc2007.c`.
- `&ft5336_adafruit_2_8_tft_touch_v2 { status = "disabled"; }` dans l'overlay confirme que le capacitif est désactivé.
- **Action** : Supprimer `touch.c` et `touch.h` ou les archiver pour éviter toute confusion.

#### 6.2 Calibration TSC2007
```dts
tsc2007@4a {
    screen-width  = <240>;
    screen-height = <320>;
    raw-x-min = <150>;   raw-x-max = <3800>;
    raw-y-min = <130>;   raw-y-max = <4000>;
    pressure-min = <200>;
};
```
- Les dimensions déclarées (`240×320`) sont en **mode portrait**. Le display fonctionne en **paysage 320×240** (rotation=90).
- Le driver `input_tsc2007.c` utilise `input_touchscreen_common_config` qui gère le swap/inversion via les propriétés DTS `swapped-x-y`, `inverted-x`, `inverted-y` de `touchscreen-common.yaml`.
- **Action critique** : Vérifier que ces propriétés sont définies ou héritées correctement pour aligner les coordonnées tactiles avec l'affichage paysage. Un toucher en haut à gauche de l'écran paysage doit retourner (0,0).

#### 6.3 Double lecture X/Y pour stabilité
```c
tsc2007_read_sample(config, TSC2007_FUNC_X, ..., &raw_x1);
tsc2007_read_sample(config, TSC2007_FUNC_Y, ..., &raw_y1);
tsc2007_read_sample(config, TSC2007_FUNC_X, ..., &raw_x2);
tsc2007_read_sample(config, TSC2007_FUNC_Y, ..., &raw_y2);
if (abs(x1-x2) > 100 || abs(y1-y2) > 100) → release
```
- Filtre de stabilité correct. `TSC2007_STABILITY_DELTA=100` (sur 4096 max) = 2.4% de la plage → à ajuster selon le panneau tactile physique.

#### 6.4 Délai de conversion ADC
```c
#define TSC2007_CONVERSION_DELAY_US 500U
```
- La datasheet TSC2007 indique que la conversion prend au maximum ~50 µs @ 3.4 MHz I2C. 500 µs est conservateur mais sûr.

---

## 7. Boutons GPIO

### Contexte documentaire
- **Zephyr GPIO API** : `gpio_pin_interrupt_configure_dt` avec `GPIO_INT_EDGE_TO_ACTIVE` déclenche sur flanc montant (si la broche est active-high) ou descendant (active-low).

### Points à vérifier

#### 7.1 Debounce dans input_mgr
```c
#define DEBOUNCE_MS 200
if ((now - last_btn_ms[raw]) < DEBOUNCE_MS) { continue; }
```
- 200 ms de debounce est conservateur (typiquement 20-50 ms suffisent). Cela empêche une répétition rapide intentionnelle.
- **Vérifier** : que l'ISR utilise `GPIO_INT_EDGE_TO_ACTIVE` et non `GPIO_INT_EDGE_BOTH` (rebond sur les deux fronts non souhaité).

#### 7.2 ISR ne fait que poster dans raw_input_q
```c
static void button0_isr(...) { raw_input_post(RAW_BTN0); }
```
- **Correct** : ISR minimale, appel ISR-safe `k_msgq_put` avec `K_NO_WAIT`. Pas de mutex, pas de log depuis l'ISR.

---

## 8. Podomètre — Algorithme de Détection de Pas

### Contexte documentaire
- Accéléromètre LSM6DSO : plage ±2g. Au repos, le vecteur gravité donne |accel| ≈ 9.81 m/s².
- Marche normale : amplitude de variation ~0.5-1g (~5-10 m/s²) autour de la valeur de repos.
- **Algorithme professionnel** : autocorrélation + filtrage passe-bas. [NXP AN4248](https://nxp.com/docs/en/application-note/AN4248.pdf)

### Points à vérifier

#### 8.1 Seuils de détection
```c
#define STEP_THRESHOLD_HIGH  12.0f   /* m/s² */
#define STEP_THRESHOLD_LOW    8.0f   /* m/s² */
```
- Au repos : |accel| ≈ 9.81 m/s² (1g).
- Pendant la marche : pics à ~12-15 m/s² (1.2-1.5g), creux à ~7-8 m/s² (0.7-0.8g).
- `STEP_THRESHOLD_HIGH = 12.0` est proche du niveau de repos → **risque de faux positifs** si l'appareil est légèrement secoué ou incliné.
- `STEP_THRESHOLD_LOW = 8.0` est raisonnable pour détecter la fin d'un pas.
- **Action** : Augmenter `STEP_THRESHOLD_HIGH` à 14-15 m/s² pour réduire les faux positifs. Ajouter un filtre passe-bas sur la magnitude avant la détection.

#### 8.2 Fréquence d'échantillonnage
- Les capteurs sont lus à 250 ms (4 Hz) en mode actif. LSM6DSO est à 26 Hz.
- Le firmware ne lit qu'un échantillon par cycle de 250 ms (pas de FIFO) → sous-échantillonnage massif.
- Un pas dure ~0.5-1 s → 2-4 échantillons par pas au mieux. Détection de crête possible mais bruyante.
- **Action** : Activer le FIFO LSM6DSO ou augmenter la fréquence de lecture pour le podomètre.

#### 8.3 Pas non persistés
```c
// controller.c
step_count++;
ble_update_steps(step_count);
atomic_set(&shared_step_count, (atomic_val_t)step_count);
// MANQUE : storage_save_steps(step_count);
```
- `storage_save_steps()` n'est **jamais appelé** depuis `controller.c`. Les pas ne sont pas sauvegardés en flash.
- Au démarrage, `step_count` est initialisé à 0 dans `controller_set_display()`, ignorant `storage_load_steps()`.
- **Bug** : compteur de pas remis à zéro à chaque redémarrage.
- **Action** : Appeler `storage_load_steps()` au démarrage et `storage_save_steps()` périodiquement (par exemple toutes les 100 pas pour limiter l'usure flash).

---

## 9. Boussole — Calcul du Cap Magnétique

### Contexte documentaire
- **Heading simple** : `atan2(My, Mx)` — valide **uniquement si le capteur est horizontal**.
- **Tilt compensation** : nécessite accéléromètre pour calculer pitch/roll et corriger le vecteur magnétique. [NXP AN4248](https://nxp.com/docs/en/application-note/AN4248.pdf)
- **Calibration** : hard iron (offset fixe) + soft iron (distorsion ellipsoïdale) doivent être compensés. [NXP AN4246](https://nxp.com/docs/en/application-note/AN4246.pdf)

### Points à vérifier

#### 9.1 Heading sans compensation d'inclinaison
```c
float heading = atan2f(snap.magn[1], snap.magn[0]) * RAD2DEG;
```
- **Limitation majeure** : Pour une montre au poignet, l'inclinaison varie constamment. L'erreur de heading peut atteindre 30-60° sans compensation d'inclinaison.
- **Action pour améliorer** : Implémenter le calcul :
  ```
  pitch = asin(-ax/g)
  roll  = atan2(ay, az)
  Bx_comp = Mx*cos(pitch) + Mz*sin(pitch)
  By_comp = Mx*sin(roll)*sin(pitch) + My*cos(roll) - Mz*sin(roll)*cos(pitch)
  heading = atan2(-By_comp, Bx_comp)
  ```

#### 9.2 Duplication du calcul heading
```c
// controller.c — process_sensor_data()
float heading = atan2f(snap.magn[1], snap.magn[0]) * RAD2DEG;
ble_update_compass(...);

// display_thread.c
float heading = atan2f(snap.magn[1], snap.magn[0]) * rad2deg;
ui_update_compass(heading);
```
- Le calcul est effectué **deux fois** dans deux threads différents (controller + display). Risque de divergence si la logique évolue.
- **Action** : Centraliser le calcul dans `process_sensor_data()` et stocker le heading dans `shared_sensors` ou une variable atomique dédiée.

---

## 10. Gestion de l'Énergie et Mode Veille

### Points à vérifier

#### 10.1 Pas de display_blanking_on() en mode veille
```c
static void enter_sleep_mode(void)
{
    k_timer_stop(&timer_sensors);
    k_timer_stop(&timer_time);
    atomic_or(&ui_action_flags, UI_ACT_ENTER_SLEEP);
    // MANQUE : display_blanking_on(display_dev)
}
```
- L'overlay de veille est un rectangle noir LVGL. Le rétroéclairage de l'ILI9340 reste **allumé** (SPI actif, pixels noirs).
- Pour une vraie économie d'énergie, il faut appeler `display_blanking_on(display_dev)` qui envoie la commande de sleep au contrôleur ILI9340.
- **Action** : Ajouter `display_blanking_on(display_dev)` dans `enter_sleep_mode()` et `display_blanking_off(display_dev)` dans `wake_from_sleep()`. Le pointeur `display_dev` est accessible via `controller_set_display()`.

#### 10.2 Watchdog désactivé
```
# CONFIG_WATCHDOG=y
# CONFIG_WDT_DISABLE_AT_BOOT=n
```
- Le watchdog est commenté avec la note "until feed logic is implemented in controller".
- Sans watchdog, un thread bloqué sur un bus I2C défaillant peut freezer le système indéfiniment.
- **Action** : Implémenter le `task_wdt` Zephyr (watchdog par thread) avec feed dans chaque thread critique.

#### 10.3 FPU désactivé — Impact sur les performances
```
# CONFIG_FPU=y  ← commenté à cause d'un conflit TF-M
```
- La note explique : "FPU disabled to avoid TF-M CP10/CP11 config issue".
- Toutes les opérations flottantes utilisent les routines soft-float (`__aeabi_fsqrt`, `__aeabi_fmul`, etc.).
- Impact mesuré : une division flottante peut prendre ~100 µs au lieu de ~1 ns avec le FPU matériel.
- Fonctions affectées : `sqrtf()` (podomètre), `atan2f()` (boussole), `sinf()`/`cosf()` (aiguille compas), `fmodf()`.
- **Action** : Activer `CONFIG_FPU=y` et `CONFIG_FPU_SHARING=y` après résolution du conflit TF-M (utiliser le mode IPC TF-M ou NCS 2.x qui supporte le FPU hard ABI). Référence : [PR nrfconnect/sdk-zephyr#874](https://github.com/nrfconnect/sdk-zephyr/pull/874).

---

## 11. Persistance NVS/Settings

### Contexte documentaire
- **Zephyr Settings** : [https://docs.zephyrproject.org/latest/services/storage/settings/](https://docs.zephyrproject.org/latest/services/storage/settings/)
- Wear leveling : NVS utilise un buffer circulaire FIFO. Usure typique : millions de cycles d'écriture. `settings_save_one` ne réécrit pas si la valeur est inchangée.

### Points à vérifier

#### 11.1 Thème non restauré au démarrage
```c
// main.c — appelé dans l'ordre :
storage_init();        // charge les settings
ui_init();             // theme_index = 0 (valeur par défaut)
// MANQUE : ui_set_theme(storage_load_theme())
```
- `storage_load_theme()` est disponible mais jamais appelé pour initialiser `theme_index` dans l'UI.
- **Bug** : Le thème sélectionné est perdu à chaque redémarrage.
- **Action** : Après `ui_init()`, appeler une fonction d'initialisation du thème avec la valeur chargée.

#### 11.2 Thème non sauvegardé lors du changement
```c
// display_thread.c
if (actions & UI_ACT_CYCLE_THEME) ui_cycle_theme_color();
// MANQUE : storage_save_theme(new_theme_index)
```
- `storage_save_theme()` n'est jamais appelé lors d'un changement de thème.
- **Action** : Exposer `ui_get_theme_index()` depuis `ui.c` et appeler `storage_save_theme()` après chaque cycle.

#### 11.3 Fréquence d'écriture NVS — Usure flash
- `storage_save_last_sync()` est appelé à **chaque synchronisation BLE**. Si le smartphone envoie l'heure toutes les secondes, cela représente 86400 écritures/jour.
- Flash nRF5340 : ~10 000 cycles d'effacement par page. Avec 2 secteurs NVS de 4 Ko, durée de vie ≈ `2*4096*10000 / (86400*12) ≈ 0.1 jour` → **usure critique**.
- **Action** : Limiter `storage_save_last_sync()` à maximum une fois par heure, ou ne sauvegarder que lors d'un changement significatif (> 1 min d'écart).

---

## 12. Hardware nRF5340 DK — Vérification des Broches

### Contexte documentaire
- **nRF5340 DK pinout** : [Nordic Infocenter](https://infocenter.nordicsemi.com/topic/ps_nrf5340/chapters/pin.html)
- P1.02/P1.03 : broches TWI haute vitesse dédiées (20 mA open-drain).
- Les autres GPIO peuvent aussi être utilisés pour I2C avec configuration manuelle.

### Points à vérifier

#### 12.1 Bus I2C1 (capteurs shield) : P1.02 / P1.03
```dts
i2c1_default: i2c1_default {
    psels = <NRF_PSEL(TWIM_SDA, 1, 2)>,
            <NRF_PSEL(TWIM_SCL, 1, 3)>;
};
```
- **Correct** : ce sont les broches TWIM dédiées du nRF5340 pour la haute vitesse. Standard I2C 100 kHz configuré dans l'overlay.

#### 12.2 Bus I2C2 (RTC) : P0.24 / P0.29
```dts
i2c2_default: i2c2_default {
    psels = <NRF_PSEL(TWIM_SDA, 0, 24)>,
            <NRF_PSEL(TWIM_SCL, 0, 29)>;
};
```
- P0.24 et P0.29 sont des GPIO standard du port 0. TWIM peut être routé sur n'importe quel GPIO via le crossbar nRF5340.
- **Vérifier** : que ces broches ne sont pas en conflit avec d'autres fonctions du nRF5340 DK (LED, boutons, UART debug). Consulter le schéma du nRF5340 DK (PCA10095).

#### 12.3 P0.20 — Alimentation RTC
- P0.20 est utilisé comme GPIO de puissance pour la RTC.
- **Vérifier** : que P0.20 n'est pas une broche spéciale (NFC, JTAG, TRACE) sur le nRF5340 DK.

---

## 13. Diagrammes d'Architecture (diagrams/)

### Points à vérifier

#### 13.1 Cohérence DFE (Diagramme de Flot d'Événements)
- Vérifier que tous les événements de `enum event_type` sont représentés dans le DFE.
- Vérifier que le flux ISR → raw_input_q → input_mgr → event_bus → controller est correct.

#### 13.2 Cohérence DFD (Diagramme de Flot de Données)
- Vérifier que `sensor_snapshot` est bien identifié comme le flux de données entre sensors et controller/display.
- Vérifier que les notifications BLE sont représentées comme sorties de données.

#### 13.3 Machine à États (FSM)
- Vérifier la cohérence avec `enum app_state` dans le code :
  - `STATE_INIT → STATE_IDLE` (sur premier événement quelconque)
  - `STATE_IDLE → STATE_ACTIVE` (sur `EVT_TICK_SENSORS`)
  - `STATE_ACTIVE → STATE_IDLE` (sur `EVT_SENSOR_DATA_READY`)
  - `STATE_IDLE → STATE_SLEEP` (inactivité 120 s)
  - `STATE_SLEEP → STATE_IDLE` (bouton quelconque)
- **Vérifier** : `STATE_INIT` utilise `__attribute__((fallthrough))` pour enchaîner immédiatement sur `STATE_IDLE`. C'est correct mais l'événement reçu est traité depuis IDLE, pas ignoré.

---

## 14. Checklist de Vérification Finale

### 14.1 Critiques (bugs fonctionnels)
- [ ] `rtc_time_to_unix()` : remplacer `mktime()` par `timeutil_timegm()` pour garantir UTC
- [ ] Pas non persistés : appeler `storage_load_steps()` au démarrage et `storage_save_steps()` périodiquement
- [ ] Thème non restauré : appeler `storage_load_theme()` après `ui_init()`
- [ ] Thème non sauvegardé : appeler `storage_save_theme()` lors du cycle de thème
- [ ] Code mort : supprimer `touch.c` / `touch.h` (FT6206 non utilisé)
- [ ] Usure flash NVS excessive : limiter `storage_save_last_sync()` à max 1×/heure

### 14.2 Importants (problèmes de robustesse)
- [ ] `ble_is_connected()` : protéger `current_conn` avec un mutex ou `atomic_ptr_t`
- [ ] `bt_gatt_notify()` depuis thread préemptif P5 : vérifier si problème avec la version Zephyr utilisée (issue #89705)
- [ ] Watchdog : implémenter `task_wdt` pour les threads critique (controller, sensors, display)
- [ ] Veille réelle : appeler `display_blanking_on/off()` pour couper réellement le rétroéclairage
- [ ] `time_t` 32 bits : anticiper le problème 2038 si le projet doit durer longtemps

### 14.3 Améliorations (qualité / précision)
- [ ] Podomètre : augmenter `STEP_THRESHOLD_HIGH` à 14-15 m/s², ajouter filtrage passe-bas
- [ ] Podomètre : activer FIFO LSM6DSO pour augmenter la résolution temporelle
- [ ] Boussole : implémenter la compensation d'inclinaison (pitch/roll via accéléromètre)
- [ ] Boussole : centraliser le calcul heading dans `process_sensor_data()` (éviter la duplication)
- [ ] FPU : activer `CONFIG_FPU=y` après résolution du conflit TF-M pour les performances float
- [ ] Race condition veille/réveil : documenter le comportement attendu ou ajouter une fenêtre de garde

### 14.4 Tests à effectuer sur hardware
- [ ] Vérifier que les 4 capteurs répondent correctement sur I2C1 (addr 0x5F, 0x5C, 0x6A/0x6B, 0x1E)
- [ ] Vérifier que la RTC répond sur I2C2 addr 0x51
- [ ] Vérifier que le TSC2007 répond sur I2C1 addr 0x4A
- [ ] Tester la navigation tactile et vérifier l'alignement des coordonnées (paysage 320×240)
- [ ] Tester la synchronisation BLE (écriture sur la caractéristique TIME)
- [ ] Vérifier que la valeur de pression est affichée en hPa (kPa × 10 dans `ui_update_pressure()`)
- [ ] Tester le mode veille/réveil par bouton
- [ ] Mesurer la consommation en mode veille (rétroéclairage off vs overlay noir)

---

## 15. Références Documentation

| Composant | Référence |
|---|---|
| Zephyr Kernel API | https://docs.zephyrproject.org/latest/kernel/services/ |
| Zephyr BLE GATT | https://docs.zephyrproject.org/latest/connectivity/bluetooth/ |
| Zephyr Settings/NVS | https://docs.zephyrproject.org/latest/services/storage/settings/ |
| Zephyr Atomic | https://docs.zephyrproject.org/latest/kernel/services/other/atomic.html |
| Zephyr RTC RV-8263-C8 | https://docs.zephyrproject.org/latest/build/dts/api/bindings/rtc/microcrystal%2Crv-8263-c8.html |
| Zephyr LSM6DSO binding | https://docs.zephyrproject.org/latest/build/dts/api/bindings/sensor/st,lsm6dso-i2c.html |
| Zephyr LPS22HH binding | https://docs.zephyrproject.org/latest/build/dts/api/bindings/sensor/st%2Clps22hh-i2c.html |
| LVGL v8 Thread Safety | https://docs.lvgl.io/8.0/porting/os.html |
| HTS221 Datasheet | https://www.st.com/resource/en/datasheet/hts221.pdf |
| LPS22HH Datasheet | https://www.mouser.de/datasheet/2/389/lps22hh-1395924.pdf |
| LSM6DSO Datasheet | https://www.st.com/resource/en/datasheet/lsm6dso.pdf |
| LIS2MDL Datasheet | https://www.st.com/resource/en/datasheet/lis2mdl.pdf |
| TSC2007 Datasheet | https://www.ti.com/document-viewer/TSC2007/datasheet |
| NXP Tilt-Compensated Compass | https://nxp.com/docs/en/application-note/AN4248.pdf |
| NXP Hard/Soft Iron Calibration | https://nxp.com/docs/en/application-note/AN4246.pdf |
| Nordic nRF5340 DK Pinout | https://infocenter.nordicsemi.com/topic/ps_nrf5340/chapters/pin.html |
| Zephyr ILI9XXX rotation bug | https://github.com/zephyrproject-rtos/zephyr/issues/33269 |
| Zephyr bt_gatt_notify issue | https://github.com/zephyrproject-rtos/zephyr/issues/89705 |
| Zephyr FPU + TF-M PR | https://github.com/nrfconnect/sdk-zephyr/pull/874 |

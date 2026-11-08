# Kiosque 64

**Le Minitel sur votre Commodore 64.** Kiosque 64 transforme un C64 Ultimate en Minitel couleur, branché sur les services Vidéotex d'aujourd'hui (MiniPAVI, 3615 co.de, 3611…) par Internet, et sert aussi de terminal pour les BBS en PETSCII ou en ASCII.

*English version below.*

| Menu | MiniPAVI | MiniMeteo |
|---|---|---|
| ![Menu de Kiosque 64](docs/images/menu-fr.png) | ![MiniPAVI](docs/images/minipavi.png) | ![MiniMeteo](docs/images/meteo.png) |

*Captures faites sur un vrai C64 Ultimate.*

## Fonctions

- **Émulation Minitel (Vidéotex)** : texte, semi-graphiques joints et disjoints, 8 couleurs (ou niveaux de gris comme un Minitel 1), double hauteur, double largeur, double taille, inversion, soulignement, clignotement, masquage, rangée de service, mode rouleau, accents français.
- **Connexion directe par le réseau du C64 Ultimate**, sans modem ni passerelle : TCP brut, telnet ou **WebSocket** (`ws://`).
- **Carnet d'adresses** : MiniPAVI et la passerelle LABBEJ27 (telnet), 3615 co.de, l'annuaire 3611 et joher.com (WebSocket), Retrocampus BBS, le BBS LABBEJ27, et n'importe quelle autre adresse.
- **Terminal BBS** en PETSCII ou en ASCII.
- **Clavier Minitel** sur les touches du C64, accents avec `C=` comme dans [Plume](https://github.com/labbej27/plume64).
- Éditions **française** et **anglaise**, en cartouche `.crt` ou en programme `.prg`.

## Utilisation

1. Activez le **Command Interface** du C64 Ultimate, une fois pour toutes (voir ci-dessous).
2. Lancez `dist/kiosque-fr.crt` (ou `kiosque-en.crt`, ou les `.prg`).
3. Choisissez un service avec les touches `1` à `9`.

| Touche C64 | Touche Minitel |
|---|---|
| `RETURN` | Envoi |
| `DEL` | Correction |
| `F1` / curseur gauche | Sommaire |
| `F2` | Guide |
| `F3` / curseur haut | Retour |
| `F4` / curseur droite | Répétition |
| `F5` / curseur bas | Suite |
| `F6` | Annulation |
| `F7` | Connexion/Fin |
| `F8` | Retour au menu de Kiosque 64 |
| `C=` + `E` / `R` / `T` | é / è / ê |
| `C=` + `A` / `Q` | à / â |
| `C=` + `C` | ç |
| `C=` + `U` / `Y` | ù / û |
| `C=` + `O` / `I` / `K` / `L` | ô / î / ï / ë |
| `C=` + `P` | œ |

Comme sur un Minitel, le clavier est en majuscules par défaut ; `Shift` donne les minuscules, sauf si le service passe le clavier en minuscules. Ce que vous tapez s'affiche localement (écho local), sauf si le service le coupe pour renvoyer lui-même les caractères.

### Activer le Command Interface

Kiosque 64 passe par le « Command Interface » du C64 Ultimate pour accéder au réseau. Il est désactivé d'origine ; sans lui, Kiosque affiche « Activez le Command Interface ».

1. **Ouvrez le menu du C64U** : poussez l'interrupteur multifonction (le bouton marche/arrêt) vers le haut et relâchez-le.
2. Avec les touches du curseur, choisissez **Memory & ROMs** (« Memory Configuration » dans les firmwares plus récents) et validez avec `RETURN`.
3. Descendez jusqu'à **Command Interface**, juste sous les réglages de la REU, appuyez sur `RETURN` et choisissez **Enabled**.
4. Quittez le menu avec `RUN/STOP`.
5. Quand le C64U propose d'enregistrer les réglages en mémoire flash, appuyez sur **`Y`**. Sans cette étape, le réglage est perdu à l'extinction.

Dans le menu avancé (`F2` depuis le menu principal), le même réglage se trouve dans **C64 and Cartridge Settings → Command Interface**.

Avec l'API REST de l'Ultimate (réseau local), on peut aussi faire :

```sh
curl -X PUT "http://IP-DU-C64/v1/configs/C64%20and%20Cartridge%20Settings/Command%20Interface?value=Enabled"
curl -X PUT "http://IP-DU-C64/v1/configs:save_to_flash"
```

### Édition modem (C64 d'origine)

Sans C64 Ultimate, Kiosque 64 passe par un **modem WiFi** branché sur le port utilisateur, comme ceux qu'on utilise avec CCGMS. Prenez alors `dist/kiosque-modem-fr.crt` (ou `kiosque-modem-en.crt`, ou les `.prg`). La cartouche est au format EasyFlash et fonctionne avec une Kung Fu Flash.

![Réglages du modem](docs/images/modem-fr.png)

- **Branchement** : RS-232 standard du port utilisateur, en niveaux TTL non inversés : réception sur les broches B et C, émission sur la broche M. C'est le réglage « User Port » de CCGMS.
- **Vitesse** : 300, 1200 ou 2400 bauds, au choix dans **M Modem** depuis le menu. 2400 bauds, c'est deux fois plus vite qu'un vrai Minitel.
- **Commandes AT** : Kiosque n'utilise que celles qui sont communes aux modems WiFi pour C64 (firmwares ESP8266 de jsalin, Zimodem et dérivés) : `AT`, `ATDT hôte:port`, `CONNECT`, `NO CARRIER`, puis `+++` et `ATH` pour raccrocher. Le telnet et le WebSocket sont gérés par Kiosque lui-même, que le modem s'en occupe ou non.
- **Réglages** : vitesse, chaîne d'initialisation (envoyée avant chaque appel, par exemple `ATE0`) et préfixe d'appel (`ATDT` par défaut). La touche **4** teste le modem, **E** enregistre les réglages dans `KIOSQUE.CFG` sur le lecteur 8 ; ils sont relus au démarrage.
- **Sans contrôle de flux** : la plupart des modems WiFi n'en ont pas. Kiosque reçoit donc dans une mémoire tampon de 8 Ko, assez pour une page Minitel entière, même quand le dessin à 1 MHz prend du retard.

Le pilote RS-232 ([`src/rs232.s`](src/rs232.s)) est écrit pour Kiosque, sur interruptions NMI. La couche modem ([`src/modem.c`](src/modem.c)) remplace le Command Interface sans toucher au reste : l'édition C64 Ultimate reste identique octet pour octet. `make modem` fabrique les deux cartouches.

### WebSocket et chiffrement

Le C64 ne sait pas faire de TLS : seules les adresses `ws://` (non chiffrées) sont joignables, pas les `wss://`. Pour les passerelles MiniPAVI, le plus simple est leur accès telnet (`go.minipavi.fr:516`, `minitel.labbej.fr:8180`).

### Autre adresse

- `minitel.labbej.fr:8180` : telnet ;
- `ws://hôte:port/chemin` : WebSocket ;
- pour un BBS, `hôte:port` (telnet, port 23 par défaut).

## Matériel testé

| Configuration | État |
|---|---|
| Commodore 64 Ultimate (firmware 1.1.0), WiFi, à 1 MHz et à 64 MHz | Testé : MiniPAVI et LABBEJ27 (telnet), 3615 co.de, 3611 et joher.com (WebSocket), Retrocampus BBS, BBS LABBEJ27 ; cartouche et .prg |
| VICE 3.10 (affichage seulement, pas de réseau) | Testé |
| Édition modem dans VICE 3.10 (RS-232 du port utilisateur émulé, faux modem [`tests/modem/fakemodem.py`](tests/modem/fakemodem.py) relié à Internet) | Testé à 1200 et 2400 bauds : MiniPAVI, LABBEJ27, 3615 co.de (WebSocket), Retrocampus BBS, BBS LABBEJ27, saisie, raccrochage, réglages enregistrés et relus |
| C64 d'origine avec Kung Fu Flash 2 et modem WiFi ESP8266 (User Port, 2400 bauds) | Testé : édition modem |

## Compiler

Outils : `make`, `python3`, [cc65](https://github.com/cc65/cc65). Sur macOS : `brew install cc65`, puis `make`. Les cartouches sont dans `build/`.

## Comment c'est fait

- **Réseau** : le Command Interface du C64 Ultimate (registres `$DF1C-$DF1F`) ouvre les connexions TCP ; Kiosque y ajoute telnet et WebSocket ([`src/net.c`](src/net.c)).
- **Vidéotex** : un décodeur écrit d'après la norme Télétel ([`src/vdt.c`](src/vdt.c)), avec le modèle d'attributs « série » du Minitel (fond et soulignement appliqués par délimiteur, zone par zone).
- **Affichage** : écran bitmap 320×200, chaque case Minitel a sa couleur de texte et de fond, comme sur le C64 ([`src/gfx.s`](src/gfx.s)).
- **Cartouche** : format Magic Desk, qui laisse libre la zone `$DF00` utilisée par le Command Interface.
- **Inspiration** : [EMinEx](https://github.com/ludosevilla/EMinEx) (Minitel sur MSX) et [CCGMS](https://github.com/mist64/ccgmsterm) (terminal BBS). Aucun code n'en est repris : Kiosque 64 est écrit de zéro.

## Licence

[MIT](LICENSE). La police unscii est dans le domaine public ; la bibliothèque d'exécution de cc65 est sous licence zlib (voir [NOTICE](NOTICE)). « Minitel » est une marque d'Orange ; Kiosque 64 n'est pas lié à Orange.

---

# Kiosque 64 (English)

**Minitel on your Commodore 64.** Kiosque 64 turns a C64 Ultimate into a colour Minitel connected to today's French Videotex services (MiniPAVI, 3615 co.de, 3611…) over the Internet. It is also a PETSCII or ASCII terminal for BBSes.

## Features

- **Minitel (Videotex) emulation**: text, joined and separated mosaics, 8 colours (or Minitel 1 style greys), double height/width/size, inverse, underline, blink, conceal, status row, scroll mode, French accents.
- **Direct connection through the C64 Ultimate's network**, no modem or bridge: raw TCP, telnet or **WebSocket** (`ws://`).
- **Phone book** with MiniPAVI and the LABBEJ27 gateway (telnet), 3615 co.de, the 3611 directory and joher.com (WebSocket), Retrocampus BBS, the LABBEJ27 BBS, and any other address.
- **BBS terminal** in PETSCII or ASCII.
- French and English editions, as `.crt` cartridges or `.prg` programs.

## Usage

First enable the **Command Interface**, once and for all (it is off by default):

1. **Open the C64U Menu**: push the Multi Function Switch (the power switch) upward and release it.
2. With the cursor keys, select **Memory & ROMs** ("Memory Configuration" in newer firmware) and press `RETURN`.
3. Go down to **Command Interface**, just below the REU settings, press `RETURN` and choose **Enabled**.
4. Leave the menu with `RUN/STOP`.
5. When the C64U offers to save the settings to flash memory, press **`Y`**. Otherwise the setting is lost at power-off.

In the advanced menu (`F2` from the main menu), the same setting is under **C64 and Cartridge Settings → Command Interface**.

Then start `dist/kiosque-en.crt` and pick a service with `1`-`9`. Keys: `RETURN` Envoi, `DEL` Correction, `F1` Sommaire, `F2` Guide, `F3` Retour, `F4` Répétition, `F5` Suite, `F6` Annulation, `F7` Connexion/Fin, `F8` back to the menu. Accented letters use `C=` plus a letter (see the table above).

The C64 cannot do TLS, so only `ws://` addresses work, not `wss://`.

### Modem edition (original C64)

Without a C64 Ultimate, use `dist/kiosque-modem-en.crt` (or the `.prg`) with a **WiFi modem on the user port**, the kind people use with CCGMS. The cartridge is in EasyFlash format and works with a Kung Fu Flash.

- **Wiring**: the C64's standard user port RS-232, TTL levels, not inverted: receive on pins B and C, transmit on pin M (CCGMS "User Port").
- **Speed**: 300, 1200 or 2400 baud, set with **M Modem** in the menu.
- **AT commands**: only the ones all C64 WiFi modems share (jsalin's ESP8266 firmware, Zimodem and their offspring): `AT`, `ATDT host:port`, `CONNECT`, `NO CARRIER`, then `+++` and `ATH` to hang up. Kiosque handles telnet and WebSocket itself, whether or not the modem does.
- **Settings**: speed, an init string sent before each call (e.g. `ATE0`) and the dial prefix (`ATDT` by default). Key **4** tests the modem, **E** saves `KIOSQUE.CFG` on drive 8, read back at startup.
- **No flow control** on most WiFi modems, so Kiosque receives into an 8 KB buffer, enough for a whole Minitel page while drawing at 1 MHz catches up.

Tested on an original C64 with a Kung Fu Flash 2 and an ESP8266 WiFi modem at 2400 baud, and in VICE 3.10 at 1200 and 2400 baud with an emulated user port modem ([`tests/modem/fakemodem.py`](tests/modem/fakemodem.py)). `make modem` builds both cartridges.

## Building

`make`, `python3` and [cc65](https://github.com/cc65/cc65), then `make`.

## License

[MIT](LICENSE). Inspired by EMinEx and CCGMS, but written from scratch. "Minitel" is a trademark of Orange; Kiosque 64 is not affiliated with Orange.

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

1. Dans le menu du C64 Ultimate, **C64 and Cartridge Settings → Command Interface → Enabled**.
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
| C64 d'origine avec Kung Fu Flash 2 et modem WiFi | **Pas encore** : prévu plus tard |

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

Enable **C64 and Cartridge Settings → Command Interface** in the Ultimate menu, start `dist/kiosque-en.crt`, pick a service with `1`-`9`. Keys: `RETURN` Envoi, `DEL` Correction, `F1` Sommaire, `F2` Guide, `F3` Retour, `F4` Répétition, `F5` Suite, `F6` Annulation, `F7` Connexion/Fin, `F8` back to the menu. Accented letters use `C=` plus a letter (see the table above).

The C64 cannot do TLS, so only `ws://` addresses work, not `wss://`.

## Building

`make`, `python3` and [cc65](https://github.com/cc65/cc65), then `make`.

## License

[MIT](LICENSE). Inspired by EMinEx and CCGMS, but written from scratch. "Minitel" is a trademark of Orange; Kiosque 64 is not affiliated with Orange.

#!/usr/bin/env python3
"""Écrans de Kiosque 64 (menu, messages) composés en Vidéotex, en français
et en anglais. mkscreens.py sortie.h"""
import sys

ESC, SO, SI, US, REP, SS2 = "\x1b", "\x0e", "\x0f", "\x1f", "\x12", "\x19"
CLS, CUR_ON, CUR_OFF, CAN = "\x0c", "\x11", "\x14", "\x18"
BLACK, RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE = range(8)

ACCENTS = {"é": "Be", "è": "Ae", "ê": "Ce", "ë": "He", "à": "Aa", "â": "Ca", "ç": "Kc",
           "ù": "Au", "û": "Cu", "ô": "Co", "î": "Ci", "ï": "Hi", "É": "BE", "À": "AA",
           "°": "0", "£": "#", "œ": "z", "←": ",", "→": "."}


def text(s):
    out = ""
    for ch in s:
        out += SS2 + ACCENTS[ch] if ch in ACCENTS else ch
        if ord(out[-1]) > 127:
            sys.exit(f"caractère non prévu : {ch!r}")
    return out


def pos(r, c):
    return US + chr(0x40 + r) + chr(0x40 + c)


def fg(n): return ESC + chr(0x40 + n)
def bg(n): return ESC + chr(0x50 + n)
def rep(n): return REP + chr(0x40 + n)


INV, NOINV, DBLH, NORMAL = ESC + "]", ESC + "\\", ESC + "M", ESC + "L"


def band(r, color):
    """Rangée pleine d'une couleur (pavés semi-graphiques vides)."""
    return pos(r, 1) + SO + bg(color) + " " + rep(39)


def item(r, key, label, kind):
    return (pos(r, 3) + fg(YELLOW) + INV + key + NOINV + " " + fg(WHITE) + text(label)
            + pos(r, 40 - len(kind)) + fg(CYAN) + text(kind))


def menu(L, modem=False):
    s = CUR_OFF + CLS + pos(0, 1) + CAN
    s += band(1, BLUE) + band(2, BLUE)
    s += pos(2, 15) + bg(BLUE) + fg(YELLOW) + DBLH + " KIOSQUE 64"
    s += pos(3, 1) + SO + bg(BLUE) + fg(CYAN) + "\x23" + rep(39)      # liseré
    s += pos(4, (41 - len(L["tagline"])) // 2) + fg(CYAN) + text(L["tagline"])
    s += pos(6, 2) + fg(GREEN) + text(L["minitel"])
    s += item(7, "1", "MiniPAVI", "Telnet")
    s += item(8, "2", "LABBEJ27", "Telnet")
    s += item(9, "3", "3615 co.de", "WebSocket")
    s += item(10, "4", L["annuaire"], "WebSocket")
    s += item(11, "5", "Hacker (joher.com)", "WebSocket")
    s += item(12, "6", L["other"], "")
    s += pos(14, 2) + fg(GREEN) + text(L["bbs"])
    s += item(15, "7", "Retrocampus BBS", "PETSCII")
    s += item(16, "8", "BBS LABBEJ27", "ASCII")
    s += item(17, "9", L["other"], "")
    s += item(18, "G", L["grey"], "")
    if modem:
        s += pos(18, 24) + fg(YELLOW) + INV + "M" + NOINV + " " + fg(WHITE) + text(L["modem"])
    s += pos(19, 2) + fg(GREEN) + text(L["keys"])
    for i, line in enumerate(L["keylines"]):
        assert len(line) <= 38, line
        s += pos(20 + i, 3) + fg(WHITE) + text(line)
    s += pos(24, 2) + fg(BLUE) + text(L["footer_modem"] if modem else L["footer"])
    return s


FR = {
    "tagline": "Le Minitel sur votre Commodore 64",
    "minitel": "Services Minitel",
    "annuaire": "Annuaire 3611",
    "other": "Autre adresse...",
    "bbs": "BBS (terminal texte)",
    "grey": "Couleurs / gris",
    "keys": "Touches pendant la connexion",
    "keylines": ["RETURN Envoi       DEL Correction",
                 "F1 Sommaire  F2 Guide  F3 Retour",
                 "F4 Répétition  F5 Suite  F6 Annulation",
                 "F7 Connexion/Fin    F8 Menu"],
    "footer": "v0.2.2  2026 LABBEJ27  licence MIT",
    "footer_modem": "v0.3.1 modem  2026 LABBEJ27  MIT",
    "modem": "Modem",
    "m_title": "Réglages du modem",
    "m_iface": "Interface",
    "m_speed": "Vitesse",
    "m_init": "Initialisation",
    "m_dial": "Appel",
    "m_test": "Tester le modem",
    "m_term": "Terminal (commandes AT)",
    "m_save": "Enregistrer (lecteur 8)",
    "m_help": ["Initialisation : envoyée avant chaque",
               "appel (ex. ATE0). SwiftLink : version",
               ".prg seulement (port cartouche)."],
    "m_back": "F8 Retour au menu",
    "m_empty": "(aucune)",
    "m_ok": "Le modem répond",
    "m_no": "Pas de réponse : vitesse, câble ?",
    "m_saved": "Réglages enregistrés",
    "m_nosave": "Échec : disque sur le lecteur 8 ?",
    "e_nomodem": "Le modem ne répond pas (touche M)",
    "connecting": "Connexion à ",
    "e_nouci": "Activez le Command Interface (menu C64)",
    "e_connect": "Connexion impossible",
    "e_ws": "WebSocket refusé par le serveur",
    "closed": "Connexion terminée - une touche",
    "ask": "Adresse : ",
    "ask_mt": "Ex. minitel.labbej.fr:8180, ws://hôte/",
    "ask_keys": "RETURN valider     F8 annuler",
    "ask_bbs": "Ex. bbs.retrocampus.com:6510",
}
EN = {
    "tagline": "Minitel on your Commodore 64",
    "minitel": "Minitel services",
    "annuaire": "3611 directory",
    "other": "Other address...",
    "bbs": "BBS (text terminal)",
    "grey": "Colour / grey",
    "keys": "Keys while connected",
    "keylines": ["RETURN Envoi       DEL Correction",
                 "F1 Sommaire  F2 Guide  F3 Retour",
                 "F4 Répétition  F5 Suite  F6 Annulation",
                 "F7 Connexion/Fin    F8 Menu"],
    "footer": "v0.2.2  2026 LABBEJ27  MIT licence",
    "footer_modem": "v0.3.1 modem  2026 LABBEJ27  MIT",
    "modem": "Modem",
    "m_title": "Modem settings",
    "m_iface": "Interface",
    "m_speed": "Speed",
    "m_init": "Init string",
    "m_dial": "Dial prefix",
    "m_test": "Test the modem",
    "m_term": "Terminal (AT commands)",
    "m_save": "Save (drive 8)",
    "m_help": ["Init string: sent before each call",
               "(e.g. ATE0). SwiftLink: .prg version",
               "only (it uses the cartridge port)."],
    "m_back": "F8 Back to the menu",
    "m_empty": "(none)",
    "m_ok": "The modem answers",
    "m_no": "No answer: speed, cable?",
    "m_saved": "Settings saved",
    "m_nosave": "Failed: disk in drive 8?",
    "e_nomodem": "The modem does not answer (key M)",
    "connecting": "Connecting to ",
    "e_nouci": "Enable the Command Interface (C64 menu)",
    "e_connect": "Cannot connect",
    "e_ws": "The server refused the WebSocket",
    "closed": "Connection closed - press a key",
    "ask": "Address: ",
    "ask_mt": "E.g. minitel.labbej.fr:8180, ws://host/",
    "ask_keys": "RETURN to confirm    F8 to cancel",
    "ask_bbs": "E.g. bbs.retrocampus.com:6510",
}


def modem_screen(L):
    """Page des réglages ; les valeurs sont écrites par main.c."""
    s = CUR_OFF + CLS + pos(0, 1) + CAN
    s += band(1, BLUE) + band(2, BLUE)
    s += pos(2, (41 - len(L["m_title"])) // 2) + bg(BLUE) + fg(YELLOW) + DBLH + text(L["m_title"])
    s += pos(3, 1) + SO + bg(BLUE) + fg(CYAN) + "\x23" + rep(39)
    for r, k, label in ((5, "1", L["m_iface"]), (7, "2", L["m_speed"]), (9, "3", L["m_init"]),
                        (12, "4", L["m_dial"]), (14, "5", L["m_test"]), (16, "6", L["m_term"]),
                        (17, "E", L["m_save"])):
        s += pos(r, 3) + fg(YELLOW) + INV + k + NOINV + " " + fg(WHITE) + text(label)
    for i, line in enumerate(L["m_help"]):
        assert len(line) <= 38, line
        s += pos(20 + i, 3) + fg(GREEN) + text(line)
    s += pos(24, 3) + fg(CYAN) + text(L["m_back"])
    return s


def c_string(name, s):
    body = "".join(f"\\x{ord(c):02x}" for c in s)
    return f'static const char {name}[] = "{body}";\n'


def status(msg, color):
    """Message en rangée 0 (la rangée de service du Minitel)."""
    return pos(0, 1) + fg(color) + text(msg) + CAN


with open(sys.argv[1], "w") as f:
    f.write("/* Généré par tools/mkscreens.py : écrans en Vidéotex. */\n")
    for lang, L in (("LANG_EN", EN), (None, FR)):
        f.write(f"#ifdef {lang}\n" if lang else "#else\n")
        # même ordre que l'édition C64 Ultimate, pour qu'elle reste identique
        f.write("#ifdef MODEM\n")
        f.write(c_string("SCR_MENU", menu(L, True)))
        f.write("#else\n")
        f.write(c_string("SCR_MENU", menu(L)))
        f.write("#endif\n")
        f.write(c_string("MSG_CONNECTING", pos(0, 1) + fg(YELLOW) + text(L["connecting"])))
        f.write("#ifdef MODEM\n")
        f.write(c_string("MSG_NOUCI", status(L["e_nomodem"], RED)))
        f.write("#else\n")
        f.write(c_string("MSG_NOUCI", status(L["e_nouci"], RED)))
        f.write("#endif\n")
        f.write(c_string("MSG_CONNECT", status(L["e_connect"], RED)))
        f.write(c_string("MSG_WS", status(L["e_ws"], RED)))
        f.write(c_string("MSG_CLOSED", status(L["closed"], YELLOW)))
        # saisie d'une adresse : le bas du menu (aide des touches) est effacé
        clear = "".join(pos(r, 1) + CAN for r in range(19, 24))
        f.write(c_string("MSG_ASK", pos(23, 2) + fg(GREEN) + text(L["ask_keys"])
                         + pos(22, 2) + fg(YELLOW) + text(L["ask"]) + fg(WHITE) + CUR_ON))
        f.write(c_string("MSG_ASK_MT", clear + pos(20, 2) + fg(CYAN) + text(L["ask_mt"])))
        f.write(c_string("MSG_ASK_BBS", clear + pos(20, 2) + fg(CYAN) + text(L["ask_bbs"])))
        f.write("#ifdef MODEM\n")
        f.write(c_string("SCR_MODEM", modem_screen(L)))
        for key in ("m_empty", "m_ok", "m_no", "m_saved", "m_nosave"):
            f.write(c_string("MSG_" + key.upper(), text(L[key])))
        f.write("#endif\n")
    f.write("#endif\n")

TokelaCoop - co-op mod (antes KenshiCoop / formerly KenshiCoop)
===============================================================
  La versión está en PROVENANCE.json y en el juego: arriba a la izquierda y en
  el título del panel F2 sale "TokelaCoop vX.YY". Los dos, la misma.
  The version is in PROVENANCE.json and in game: "TokelaCoop vX.YY" in the
  top-left corner and on the F2 panel title. Both players: the same one.

INSTALAR (los dos jugadores) - en español
-----------------------------------------
  1. Descomprime el zip entero en cualquier carpeta.
  2. Doble clic en "Instalar TokelaCoop.cmd". Si Windows pregunta, pulsa
     "Ejecutar" / "Más información > Ejecutar de todas formas".
  3. El instalador busca Kenshi, instala RE_Kenshi si falta (abre su instalador
     oficial: pulsa Install), copia el mod y lo activa. Al acabar dice "Listo".
     Si tenías KenshiCoop (el nombre antiguo, hasta la v0.53), lo cambia por
     TokelaCoop y conserva tu coop_config.json.
  Para actualizar, descarga el zip nuevo y repite. Para jugar: mira PLAY abajo
  (el anfitrión pulsa F2 > "Invitar a un amigo de Steam"). Las siguientes
  veces, los dos pulsáis F2 > "Volver a jugar con <amigo>".
  Sin invitación: el anfitrión pulsa F2 > "Abrir mi partida a mis amigos de
  Steam" y el amigo, con Kenshi ya abierto, hace clic derecho en su nombre en
  la lista de amigos de Steam y pulsa "Unirse a la partida".

  VERSIONES NUEVAS: al abrir Kenshi, TokelaCoop pregunta a GitHub cuál es la
  última versión (solo eso) y, si hay una más nueva, F2 lo dice y trae un botón
  para descargarla. Instaladla los dos. Para que no pregunte nunca, pon
  "checkUpdates": false en mods\TokelaCoop\coop_config.json.

  ¿ALGO VA MAL? Doble clic en "Recoger diagnostico.cmd": deja en el Escritorio
  un zip con los logs, la lista de mods y las versiones (no tus partidas; los
  logs llevan tu Steam ID y el de tu amigo). Mándaselo a quien te ayude. Los
  logs de las dos veces anteriores que abriste Kenshi también se guardan
  (TokelaCoop_*.1.log y .2.log), así que no se pierden al reabrirlo.
  Cuando la partida que llega del anfitrión sustituye una que ya tenías con el
  mismo nombre, la anterior se guarda en %LOCALAPPDATA%\kenshi\TokelaCoop_backups
  (las 3 últimas de cada partida, 9 como mucho): si era tuya, sácala de ahí
  cuanto antes. Al guardar, el amigo ve en el banner de arriba "guardando..." y
  después "partida guardada" (la guarda el anfitrión y llega a los dos).

INSTALL (both players)
----------------------
  1. Extract the whole zip anywhere.
  2. Double-click "Instalar TokelaCoop.cmd" (if Windows asks, choose "Run" /
     "More info > Run anyway").
  3. It finds Kenshi (any Steam library or GOG; it asks if it can't), installs
     RE_Kenshi if missing (it downloads the pinned release from GitHub, checks
     its SHA-256 and opens RE_Kenshi's official installer - click Install),
     copies the mod into <Kenshi>\mods\TokelaCoop and enables it in the mod
     list. It ends with "Done". Run it again to update. If you had KenshiCoop
     (the old name, up to v0.53) it switches it to TokelaCoop and keeps your
     coop_config.json.

  Manual install instead: install RE_Kenshi 0.3.5
  (https://github.com/BFrizzleFoShizzle/RE_Kenshi/releases), copy the
  "TokelaCoop" folder into <Kenshi>\mods\ and enable "TokelaCoop" in the
  launcher's Mods list. Upgrading from KenshiCoop by hand: move
  mods\KenshiCoop\coop_config.json into mods\TokelaCoop\, delete
  mods\KenshiCoop and untick KenshiCoop - with both loaded, TokelaCoop stays
  off and says why.

PREREQUISITES (both players)
----------------------------
  1. Kenshi 1.0.65 or 1.0.68 (Steam or GOG).
  2. RE_Kenshi 0.3.4 or 0.3.5 - the installer takes care of it.
  3. For the Steam transport (recommended): Steam RUNNING and ONLINE on both
     machines. No port forwarding, no IPs, no config editing.
  4. The SAME TokelaCoop version on both machines - download the same zip. The
     connection only refuses a different network protocol, and two releases can
     share one, so compare the "TokelaCoop vX.YY" you both see in game.

PLAY (Steam - recommended)
--------------------------
  EASIEST - invite from the panel (new, not yet tested in-game):
  Your friend just needs Kenshi running with the mod enabled (main menu is
  fine). The HOST loads a save, presses F2 and clicks "Invite a Steam
  friend", then clicks "Invite <name>" next to the friend. The friend
  accepts the Steam notification and both sides connect on their own.
  If that doesn't work, use the manual steps below.

  NEXT TIME / LA PRÓXIMA VEZ: F2 remembers your friend. Both of you press
  "Volver a jugar con <amigo>" ("Play again with <friend>"): the host waits for
  that friend and the friend connects - no invite, no Steam IDs.

  NO INVITE / SIN INVITACIÓN: the HOST presses F2 > "Abrir mi partida a mis
  amigos de Steam" ("Open my game to Steam friends"). The friend, with Kenshi
  ALREADY running (main menu is fine), right-clicks the host in the Steam
  friends list and picks "Unirse a la partida" ("Join Game"). "Cerrar la
  partida a mis amigos" closes it again.

  1. Press F2 to open the Co-op panel. It works at the MAIN MENU (before loading
     a game) as well as in-game.
  2. Swap Steam IDs: each player clicks "Copy my Steam ID" and sends it to the
     other (Steam chat, Discord, etc.). When you receive your friend's ID, copy
     it, then click "Paste friend's Steam ID" in your panel. The panel shows the
     ID it captured. (This is per-session - re-paste it if you relaunch Kenshi.)
  3. HOST: load the save you want to play (or start a new game), set Role: HOST,
     leave Transport on STEAM, and toggle Connection to ONLINE.
  4. JOIN: straight from the MAIN MENU - no save needed - set Role: JOIN, leave
     Transport on STEAM, and toggle Connection to ONLINE. The host sends its
     world to you on connect and you load right into it. (You do NOT need the
     host's save beforehand. If you already have an identical copy on disk it is
     used as-is instead of transferring.)
  5. The white status line shows live state, and a status banner in the TOP-LEFT
     corner shows it too - at the main menu as well as in-game, so a joining
     player can watch the transfer before the world loads. Toggle Connection to
     OFFLINE to leave.

  SQUADS: each player controls only their own squad(s): you cannot select,
  order or move your friend's characters (the game tells you "ese personaje es
  de tu amigo"). The "TokelaCoop (Wanderer x2)" starts already give each player
  a squad. With a one-squad save, the HOST moves some characters into a second
  squad BEFORE the friend connects: it becomes the friend's when they join.
  Squads either of you creates or hires into later stay with whoever made them,
  also after saving and loading. Dismissing one of your characters (squad
  screen) dismisses it in both games, also after saving and loading; you cannot
  dismiss your friend's characters.

PLAY (LAN / direct UDP - advanced)
----------------------------------
  Skip the Steam ID swap. Open <Kenshi>\mods\TokelaCoop\coop_config.json in
  Notepad, set "transport": "udp", and put the host's address in "ip" (and
  "port" if you changed it). In the panel set Transport: UDP, pick Host/Join,
  and go ONLINE. ip/port are re-read whenever you go ONLINE, so no restart is
  needed after an edit.

UNINSTALL
---------
  Delete <Kenshi>\mods\TokelaCoop and remove the "TokelaCoop.mod" line from
  <Kenshi>\data\mods.cfg (or untick it in the launcher). To remove RE_Kenshi,
  run its installer and choose Uninstall.

TROUBLESHOOTING
---------------
  * Anything odd: double-click "Recoger diagnostico.cmd" and send the zip it
    leaves on the Desktop (logs of this run and the two before - with both
    Steam IDs -, mod list, versions; not your saves). When a received save
    replaces one you had with the same name, the old one goes to
    %LOCALAPPDATA%\kenshi\TokelaCoop_backups (the newest 3 of each save, 9 in
    all): move yours out of there soon.
  * "The co-op plugin has not started": RE_Kenshi didn't load it. Check
    <Kenshi>\RE_Kenshi_log.txt for 'TokelaCoop'; reinstalling RE_Kenshi
    usually fixes it.
  * No connection (Steam): both Steams must be RUNNING and ONLINE, and each side
    must have Pasted the OTHER player's ID (the panel shows the captured ID -
    confirm it matches). If "Paste friend's Steam ID" says the clipboard wasn't
    a Steam ID, have your friend re-copy theirs with "Copy my Steam ID". Look for
    '[steam] session ... active=1' in <Kenshi>\TokelaCoop_*.log.
  * "Mods DIFFERENT: ..." (Spanish: "Mods DISTINTOS: ...") on the F2 panel: you and your friend don't have the same
    mods, versions or load order. <Kenshi>\TokelaCoop_mods_diff.txt lists every
    difference; "Copy friend's mod list" copies their load order. The check
    only warns, it never blocks the connection. With very many mods it may say
    the lists are too long to compare them all: check them by hand.
  * "your friend has another version" on the F2 panel: one player has an
    older/newer build with a different network protocol; both should use the
    same release. If it says "probably KenshiCoop", your friend still has the
    old KenshiCoop (v0.53 or older): they should run this installer.
  * A warning box at start, or "untick KenshiCoop" on the banner: the old
    KenshiCoop is still installed next to TokelaCoop. Run the installer again,
    or untick KenshiCoop in the launcher's Mods tab.

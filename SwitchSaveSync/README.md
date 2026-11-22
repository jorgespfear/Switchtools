# Save Sync

🇬🇧 English · 🇪🇸 [Español](#español)

## English

Back up your game saves to **your own Telegram bot** and restore them on any other Switch.

### One-time setup (5 minutes, on your phone/PC)
1. In Telegram, open **@BotFather** → `/newbot` → follow the steps → copy the **token** it gives you.
2. Open your new bot and send it `/start`. (Use a private chat with your bot. Don't share the token with anyone.)
3. Put `SwitchSaveSync.nro` in `sd:/switch/` and launch it once. It creates `sd:/switch/SwitchSaveSync/config.ini`.
4. Put the token in `config.ini` (`token=123456:ABC...`) and launch the app again. The app finds your chat by itself.
5. On every other Switch: same `.nro` and the **same token**. Give each console a different `console=` name.

### Controls
| Screen | Button | Action |
|---|---|---|
| Saves list | ↑/↓, L/R | Move / jump 8 |
| | **A** | Back up and upload the selected save |
| | **Y** | Upload every save marked "SIN SUBIR" |
| | **X** | Open the cloud list (restore) |
| | **−** | Refresh the cloud list |
| | **+** | Exit |
| | **ZL** | Switch language (ES/EN) |
| | **ZR** | Check for app updates |
| Cloud list | **A** | Restore the selected copy |
| | **B** | Back |
| Restore confirmation | ←/→ | Choose the target profile |
| | **A / B** | Confirm / cancel |

### Status of each save
- **AL DIA**: nothing to do. **SIN SUBIR**: changed since the last sync. **NUBE MAS NUEVA**: another console uploaded a newer copy. **CONFLICTO**: both changed — you decide which one to keep.

### Good to know
- While uploading or downloading it shows the **live speed** (MB/s and Mbps), a speed graph, bytes transferred and the estimated time left.
- The language (Spanish/English) follows your console language the first time; press **ZL** to change it. The choice is shared by all Switch Tools apps (`sd:/config/SwitchTools/lang.txt`).
- Only **user saves** are handled (not system or device saves).
- To restore, the game needs an existing save on the target console: launch the game once, create a new save, then restore.
- Before overwriting, the app stores a copy of the current save in `sd:/switch/SwitchSaveSync/backups/`.
- Files are split into 18 MB parts (Telegram bots can only download 20 MB per file). The newest copies are listed through a pinned file `ssv-index.txt` in your bot chat — don't unpin or delete it.
- Saves are **not encrypted**: anyone with your token or access to the chat can read them. Keep the chat private.
- Up to 5 versions per game/profile/console are kept in the list; older uploads stay in the chat history.

## Español

Copia tus partidas guardadas a **tu propio bot de Telegram** y restáuralas en cualquier otra Switch.

### Configuración (una sola vez, 5 minutos)
1. En Telegram abre **@BotFather** → `/newbot` → sigue los pasos → copia el **token**.
2. Abre tu bot nuevo y envíale `/start`. (Chat privado con tu bot; no compartas el token con nadie.)
3. Copia `SwitchSaveSync.nro` a `sd:/switch/` y ábrelo una vez. Crea `sd:/switch/SwitchSaveSync/config.ini`.
4. Pon el token en `config.ini` (`token=123456:ABC...`) y vuelve a abrir la app. Ella encuentra tu chat sola.
5. En las demás Switch: el mismo `.nro` y el **mismo token**. Pon un nombre distinto en `console=` en cada consola.

### Controles
| Pantalla | Botón | Acción |
|---|---|---|
| Lista de partidas | ↑/↓, L/R | Mover / saltar 8 |
| | **A** | Copiar y subir la partida seleccionada |
| | **Y** | Subir todas las marcadas "SIN SUBIR" |
| | **X** | Abrir la nube (restaurar) |
| | **−** | Actualizar la nube |
| | **+** | Salir |
| | **ZL** | Cambiar idioma (ES/EN) |
| | **ZR** | Buscar actualizaciones de la app |
| Nube | **A** | Restaurar la copia seleccionada |
| | **B** | Volver |
| Confirmar restauración | ←/→ | Elegir perfil de destino |
| | **A / B** | Confirmar / cancelar |

### Estado de cada partida
- **AL DIA**: nada que hacer. **SIN SUBIR**: cambió desde la última sincronización. **NUBE MAS NUEVA**: otra consola subió una copia más reciente. **CONFLICTO**: cambiaron las dos; tú decides cuál conservar.

### Conviene saber
- Al subir o descargar muestra la **velocidad en tiempo real** (MB/s y Mbps), una gráfica, los bytes transferidos y el tiempo estimado.
- El idioma (español/inglés) sigue el de tu consola la primera vez; pulsa **ZL** para cambiarlo. La elección es común a todas las apps de Switch Tools (`sd:/config/SwitchTools/lang.txt`).
- Solo gestiona **partidas de usuario** (no las de sistema ni de consola).
- Para restaurar, el juego debe tener ya una partida en la consola de destino: ábrelo una vez, crea una partida nueva y luego restaura.
- Antes de sobrescribir, guarda una copia de la partida actual en `sd:/switch/SwitchSaveSync/backups/`.
- Los archivos se parten en trozos de 18 MB (los bots solo pueden descargar 20 MB por archivo). La lista de copias va en un archivo anclado `ssv-index.txt` en el chat de tu bot: no lo desancles ni lo borres.
- Las partidas **no van cifradas**: quien tenga tu token o acceso al chat puede leerlas. Mantén el chat privado.
- La lista conserva hasta 5 versiones por juego/perfil/consola; las subidas antiguas siguen en el historial del chat.

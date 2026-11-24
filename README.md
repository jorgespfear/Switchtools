# Switch Tools

🇬🇧 [English](#english) · 🇪🇸 [Español](#español)

---

## English

**Switch Tools** is a small set of homebrew utilities (`.nro`) for Nintendo Switch consoles running custom firmware (CFW, e.g. Atmosphère). You can see what is taking up space on your microSD card, measure how fast it is, check your real internet speed and back up your game saves to the cloud. Everything runs on the console itself, with an interface designed for Joy-Con or Pro Controller input.

All four apps are built automatically with GitHub Actions from this repository.

### The apps

| App | What it does |
|---|---|
| 📊 **Switch Storage Analyzer** | Scans the microSD and shows which folders and files use the most space, with a treemap or donut chart and a folder browser. Read-only. |
| 🚀 **Switch Speed Test** | Measures ping, jitter, download and upload speed with a live graph. Lets you choose the test server. |
| 💾 **SD Benchmark** | Measures microSD read and write speed: sequential (MB/s) and random 4 KB (IOPS). |
| ☁️ **Switch Save Sync** *(new)* | Backs up your game saves to **your own Telegram bot** and restores them on another Switch. Detects which saves changed and shows live transfer speed. Setup guide: [SwitchSaveSync/README.md](SwitchSaveSync/README.md). |

### Common to all apps (new)

- **Language (ZL):** switch between Spanish and English. The first time, the app uses the console language. The choice is shared by all apps (`sd:/config/SwitchTools/lang.txt`).
- **Online update (ZR):** checks GitHub for a newer version, downloads it and replaces the `.nro` automatically. Restart the app to use the new version.

### Installation

1. Go to the **[Releases](../../releases)** section and download the `.nro` files you want.
2. Copy them to the `switch` folder on your microSD card (`sd:/switch/`).
3. Open the **Homebrew Menu** on your console and launch the app.

From version 0.3.0 on, you can update from inside each app with **ZR**. Versions before 0.3.0 have to be replaced manually once.

**Requirements:** a Nintendo Switch with CFW (Atmosphère or another compatible one) and the Homebrew Menu. Speed Test, Save Sync and online updates need an internet connection.

### Quick controls

| Button | Storage Analyzer | Speed Test | SD Benchmark | Save Sync |
|---|---|---|---|---|
| **A** | Open folder / show file info | Start test | Start test | Upload selected save |
| **B** | Go up one level | Cancel | Cancel | Back |
| **X** | Switch chart | – | – | Cloud list / restore |
| **Y** | Change sort order | – | – | Upload all pending saves |
| **← / →** | – | Choose server (0 = auto) | Choose test size | Choose profile (when restoring) |
| **↑ / ↓** | Move through the list | – | – | Move through the list |
| **L / R** | Jump 8 items | – | – | Jump 8 items |
| **+** | Rescan | Exit | Exit | Exit |
| **−** | Exit | – | – | Refresh cloud list |
| **ZL** | Language ES/EN | Language ES/EN | Language ES/EN | Language ES/EN |
| **ZR** | Update app | Update app | Update app | Update app |

Details for each app are in the [release notes](../../releases).

### Building from source

The project uses [devkitPro](https://devkitpro.org/) (devkitA64 + libnx), SDL2 (`switch-sdl2`) and libcurl (`switch-curl`).

- **Without installing anything:** push the repository to GitHub and let the **Build NRO** workflow (*Actions* tab) build all four apps. Download the `SwitchTools-nro` artifact.
- **Locally:** with devkitPro installed, run `make` inside each app's folder (`SwitchStorageAnalyzer`, `SwitchSpeedTest`, `SDBenchmark`, `SwitchSaveSync`).
- **Publishing a version:** create a release on GitHub with a tag like `v0.3.0`. The workflow builds the apps with that version number and attaches the four `.nro` files to the release, with the text from `RELEASE_NOTES.md`.

### Disclaimer

Software provided "as is", without warranty of any kind. Use at your own risk. Storage Analyzer and Speed Test do not modify your card. SD Benchmark creates and deletes a temporary file in `sd:/switch/` and performs write operations, which causes minimal wear on the microSD card. Save Sync overwrites a game's save when you restore one, but it first stores a copy of the current save in `sd:/switch/SwitchSaveSync/backups/`.

---

## Español

**Switch Tools** es un pequeño conjunto de utilidades *homebrew* (`.nro`) para Nintendo Switch con firmware personalizado (CFW, p. ej. Atmosphère). Sirven para saber qué ocupa espacio en tu microSD, medir lo rápida que es, comprobar la velocidad real de tu conexión a internet y guardar copias de tus partidas en la nube. Todo funciona desde la propia consola, con una interfaz pensada para usarse con los Joy-Con o el mando Pro.

Las cuatro apps se compilan automáticamente con GitHub Actions a partir de este repositorio.

### Las aplicaciones

| App | Para qué sirve |
|---|---|
| 📊 **Switch Storage Analyzer** | Escanea la microSD y muestra qué carpetas y archivos ocupan más espacio, con gráfico *treemap* o circular y explorador de carpetas. Solo lectura. |
| 🚀 **Switch Speed Test** | Mide ping, jitter, velocidad de descarga y de subida con gráfica en directo. Permite elegir el servidor de pruebas. |
| 💾 **SD Benchmark** | Mide la velocidad de lectura y escritura de la microSD: secuencial (MB/s) y aleatoria de 4 KB (IOPS). |
| ☁️ **Switch Save Sync** *(nueva)* | Sube tus partidas guardadas a **tu propio bot de Telegram** y las restaura en otra Switch. Detecta qué partidas han cambiado y muestra la velocidad de transferencia en directo. Guía de configuración: [SwitchSaveSync/README.md](SwitchSaveSync/README.md). |

### Común a todas las apps (nuevo)

- **Idioma (ZL):** cambia entre español e inglés. La primera vez se usa el idioma de la consola. La elección se comparte entre todas las apps (`sd:/config/SwitchTools/lang.txt`).
- **Actualización online (ZR):** busca en GitHub una versión más nueva, la descarga y sustituye el `.nro` automáticamente. Reinicia la app para usar la nueva versión.

### Instalación

1. Ve a la sección **[Releases](../../releases)** y descarga los archivos `.nro` que quieras.
2. Cópialos a la carpeta `switch` de tu microSD (`sd:/switch/`).
3. Abre el **Homebrew Menu** desde la consola y lanza la aplicación.

A partir de la versión 0.3.0 puedes actualizar desde dentro de cada app con **ZR**. Las versiones anteriores a la 0.3.0 hay que sustituirlas a mano una vez.

**Requisitos:** Nintendo Switch con CFW (Atmosphère u otro compatible) y el Homebrew Menu. Speed Test, Save Sync y la actualización online necesitan conexión a internet.

### Controles rápidos

| Botón | Storage Analyzer | Speed Test | SD Benchmark | Save Sync |
|---|---|---|---|---|
| **A** | Abrir carpeta / ver archivo | Empezar test | Empezar test | Subir la partida elegida |
| **B** | Subir un nivel | Cancelar | Cancelar | Volver |
| **X** | Cambiar gráfico | – | – | Lista de la nube / restaurar |
| **Y** | Cambiar orden | – | – | Subir todas las pendientes |
| **← / →** | – | Elegir servidor (0 = auto) | Elegir tamaño de prueba | Elegir perfil (al restaurar) |
| **↑ / ↓** | Moverse por la lista | – | – | Moverse por la lista |
| **L / R** | Saltar 8 elementos | – | – | Saltar 8 elementos |
| **+** | Volver a escanear | Salir | Salir | Salir |
| **−** | Salir | – | – | Refrescar la lista de la nube |
| **ZL** | Idioma ES/EN | Idioma ES/EN | Idioma ES/EN | Idioma ES/EN |
| **ZR** | Actualizar app | Actualizar app | Actualizar app | Actualizar app |

Los detalles de cada app están en las [notas de la versión](../../releases).

### Compilar desde el código

El proyecto usa [devkitPro](https://devkitpro.org/) (devkitA64 + libnx), SDL2 (`switch-sdl2`) y libcurl (`switch-curl`).

- **Sin instalar nada:** sube el repositorio a GitHub y deja que el workflow **Build NRO** (pestaña *Actions*) compile las cuatro apps. Descarga el artefacto `SwitchTools-nro`.
- **En local:** con devkitPro instalado, ejecuta `make` dentro de la carpeta de cada app (`SwitchStorageAnalyzer`, `SwitchSpeedTest`, `SDBenchmark`, `SwitchSaveSync`).
- **Publicar una versión:** crea una release en GitHub con una etiqueta tipo `v0.3.0`. El workflow compila las apps con ese número de versión y adjunta los cuatro `.nro` a la release, con el texto de `RELEASE_NOTES.md`.

### Aviso

Software proporcionado «tal cual», sin garantías. Úsalo bajo tu responsabilidad. Storage Analyzer y Speed Test no modifican tu tarjeta. SD Benchmark crea y borra un archivo temporal en `sd:/switch/` y realiza escrituras, lo que supone un desgaste mínimo de la microSD. Save Sync sobrescribe la partida de un juego al restaurar, pero antes guarda una copia de la partida actual en `sd:/switch/SwitchSaveSync/backups/`.

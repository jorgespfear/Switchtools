# Switch Tools

🇬🇧 [English](#english) · 🇪🇸 [Español](#español)

---

## English

**Switch Tools** is a small set of homebrew utilities (`.nro`) for Nintendo Switch consoles running custom firmware (CFW, e.g. Atmosphère). They let you see what is taking up space on your microSD card, measure how fast it is, and check your real internet speed, all from the console itself, with an interface designed for Joy-Con or Pro Controller input.

All three apps are built automatically with GitHub Actions from this repository.

### The apps

| App | What it does |
|---|---|
| 📊 **Switch Storage Analyzer** | Scans the microSD and shows which folders and files use the most space, with a treemap or donut chart and a folder browser. Read-only. |
| 🚀 **Switch Speed Test** | Measures ping, jitter, download and upload speed with a live graph. Lets you choose the test server. |
| 💾 **SD Benchmark** | Measures microSD read and write speed: sequential (MB/s) and random 4 KB (IOPS). |

### Installation

1. Go to the **[Releases](../../releases)** section and download the `.nro` files you want.
2. Copy them to the `switch` folder on your microSD card (`sd:/switch/`).
3. Open the **Homebrew Menu** on your console and launch the app.

**Requirements:** a Nintendo Switch with CFW (Atmosphère or another compatible one) and the Homebrew Menu.

### Quick controls

| Button | Storage Analyzer | Speed Test | SD Benchmark |
|---|---|---|---|
| **A** | Open folder / show file info | Start test | Start test |
| **B** | Go up one level | Cancel | Cancel |
| **X** | Switch chart | – | – |
| **Y** | Change sort order | – | – |
| **← / →** | – | Choose server | Choose test size |
| **↑ / ↓** | Move through the list | – | – |
| **L / R** | Jump 8 items | – | – |
| **+** | Rescan | Exit | Exit |
| **−** | Exit | – | – |

Details for each app are in the [release notes](../../releases).

### Building from source

The project uses [devkitPro](https://devkitpro.org/) (devkitA64 + libnx), SDL2 (`switch-sdl2`) and libcurl (`switch-curl`).

- **Without installing anything:** push the repository to GitHub and let the **Build NRO** workflow (*Actions* tab) build all three apps. Download the `SwitchTools-nro` artifact.
- **Locally:** with devkitPro installed, run `make` inside each app's folder (`SwitchStorageAnalyzer`, `SwitchSpeedTest`, `SDBenchmark`).

### Disclaimer

Software provided "as is", without warranty of any kind. Use at your own risk. Storage Analyzer and Speed Test do not modify your card; SD Benchmark creates and deletes a temporary file in `sd:/switch/` and performs write operations, which causes minimal wear on the microSD card.

---

## Español

**Switch Tools** es un pequeño conjunto de utilidades *homebrew* (`.nro`) para Nintendo Switch con firmware personalizado (CFW, p. ej. Atmosphère). Sirven para saber qué ocupa espacio en tu microSD, medir lo rápida que es y comprobar la velocidad real de tu conexión a internet, todo desde la propia consola y con una interfaz pensada para usarse con los Joy-Con o el mando Pro.

Las tres apps se compilan automáticamente con GitHub Actions a partir de este repositorio.

### Las aplicaciones

| App | Para qué sirve |
|---|---|
| 📊 **Switch Storage Analyzer** | Escanea la microSD y muestra qué carpetas y archivos ocupan más espacio, con gráfico *treemap* o circular y explorador de carpetas. Solo lectura. |
| 🚀 **Switch Speed Test** | Mide ping, jitter, velocidad de descarga y de subida con gráfica en directo. Permite elegir el servidor de pruebas. |
| 💾 **SD Benchmark** | Mide la velocidad de lectura y escritura de la microSD: secuencial (MB/s) y aleatoria de 4 KB (IOPS). |

### Instalación

1. Ve a la sección **[Releases](../../releases)** y descarga los archivos `.nro` que quieras.
2. Cópialos a la carpeta `switch` de tu microSD (`sd:/switch/`).
3. Abre el **Homebrew Menu** desde la consola y lanza la aplicación.

**Requisitos:** Nintendo Switch con CFW (Atmosphère u otro compatible) y el Homebrew Menu.

### Controles rápidos

| Botón | Storage Analyzer | Speed Test | SD Benchmark |
|---|---|---|---|
| **A** | Abrir carpeta / ver archivo | Empezar test | Empezar test |
| **B** | Subir un nivel | Cancelar | Cancelar |
| **X** | Cambiar gráfico | – | – |
| **Y** | Cambiar orden | – | – |
| **← / →** | – | Elegir servidor | Elegir tamaño de prueba |
| **↑ / ↓** | Moverse por la lista | – | – |
| **L / R** | Saltar 8 elementos | – | – |
| **+** | Volver a escanear | Salir | Salir |
| **−** | Salir | – | – |

Los detalles de cada app están en las [notas de la versión](../../releases).

### Compilar desde el código

El proyecto usa [devkitPro](https://devkitpro.org/) (devkitA64 + libnx), SDL2 (`switch-sdl2`) y libcurl (`switch-curl`).

- **Sin instalar nada:** sube el repositorio a GitHub y deja que el workflow **Build NRO** (pestaña *Actions*) compile las tres apps. Descarga el artefacto `SwitchTools-nro`.
- **En local:** con devkitPro instalado, ejecuta `make` dentro de la carpeta de cada app (`SwitchStorageAnalyzer`, `SwitchSpeedTest`, `SDBenchmark`).

### Aviso

Software proporcionado «tal cual», sin garantías. Úsalo bajo tu responsabilidad. Storage Analyzer y Speed Test no modifican tu tarjeta; SD Benchmark crea y borra un archivo temporal en `sd:/switch/` y realiza escrituras, lo que supone un desgaste mínimo de la microSD.

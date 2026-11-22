# Actualizaciones desde la Switch / Updating from the Switch

**ES** — En cada app, pulsa **ZR**. La app consulta la última *release* de este repositorio, compara versiones y, si hay una nueva, la descarga (con barra de progreso y velocidad) y sustituye su propio `.nro` en la SD. Después, vuelve a abrir la app desde el Homebrew Menu. Hace falta WiFi.

**Cómo publicar una versión nueva:** sube los cambios → *Releases → Draft a new release* → crea una etiqueta nueva (por ejemplo `v0.3.0`, siempre mayor que la anterior) → *Publish release*. GitHub Actions compila las 4 apps con ese número de versión y adjunta los `.nro` a la release (tarda unos minutos).

**EN** — In any app, press **ZR**. The app checks this repository's latest release, compares versions and, if a newer one exists, downloads it (with progress bar and speed) and replaces its own `.nro` on the SD card. Then reopen the app from the Homebrew Menu. WiFi required.

**Publishing a new version:** push your changes → *Releases → Draft a new release* → create a new tag (e.g. `v0.3.0`, always higher than the previous one) → *Publish release*. GitHub Actions builds the 4 apps with that version number and attaches the `.nro` files to the release (takes a few minutes).

# UltimateTracker

App de escritorio para gamificar el estudio: pomodoros como cuadraditos en una grilla, horas por actividad camino a las 10.000 h, recreos, recreos indebidos y estadísticas. C++20 + Qt 6, nativa, sin navegador embebido.

La especificación completa está en [`SPEC.md`](SPEC.md).

## Compilar en Windows 11

### 1. Instalar Qt (una sola vez)

1. Bajá el **Qt Online Installer** desde qt.io (pide crear una cuenta gratuita).
2. Elegí **Custom installation** y marcá:
   - **Qt 6.8.x** (LTS) →
     - **MinGW 13.1.0 64-bit**, que es el compilador más simple porque no necesita Visual Studio.
     - **Additional Libraries → Qt Multimedia**, que se usa para los sonidos.
   - **Developer and Designer Tools** → **Qt Creator**, **MinGW 13.1.0 64-bit**, **CMake** y **Ninja** (suelen venir marcados).
3. Instalá.

Si ya tenés Visual Studio 2022 con "Desarrollo para el escritorio con C++", podés usar el kit **MSVC 2022 64-bit** en lugar de MinGW.

### 2. Abrir y correr

1. Abrí **Qt Creator** → *File → Open File or Project…* → elegí `CMakeLists.txt`.
2. Elegí el kit **Desktop Qt 6.8.x MinGW 64-bit** → *Configure Project*.
3. Abajo a la izquierda poné **Release** y tocá ▶ (Ctrl+R).

Para recompilar después de un cambio alcanza con `recompilar.bat` (en la carpeta de arriba).

### 3. Dejarla instalada como app

Qt Creator compila en una carpeta tipo `build/Desktop_Qt_6_8_..._Release/`. Para tener un ejecutable que funcione solo:

```bat
mkdir C:\Apps\UltimateTracker
copy build\...\UltimateTracker.exe C:\Apps\UltimateTracker\
C:\Qt\6.8.x\mingw_64\bin\windeployqt.exe --release C:\Apps\UltimateTracker\UltimateTracker.exe
```

`windeployqt` copia al lado del `.exe` las DLLs de Qt, el driver de SQLite y el backend de audio. Después:

- **Acceso directo:** clic derecho en `UltimateTracker.exe` → *Mostrar más opciones → Crear acceso directo*, y anclalo a Inicio o a la barra de tareas.
- **Que arranque con Windows:** `Win+R` → `shell:startup` → pegá ahí el acceso directo.

## Uso

| Acción | Cómo |
|---|---|
| Arrancar / pausar / seguir | Botón grande del reproductor o **Espacio** |
| Elegir actividad y duración | Arriba y en el − / + del reproductor (la actividad se puede cambiar con el pomodoro corriendo) |
| Vista día → semana → mes | Botones de la barra superior, **Ctrl + rueda** o **Ctrl −** / **Ctrl +** |
| Ir a un día | Click sobre el día en la vista semana o mes |
| Cuadraditos proporcionales al tiempo | Botón de los dos cuadrados en la barra superior |
| Esconder en un borde | Botón ⇥, o arrastrar la ventana y soltar con el mouse contra cualquier borde. Se vuelve a abrir con un click en la pestañita. |
| Siempre visible | Botón del pin |
| Actividades y ajustes | Botón ⋯ |
| Borrar o mover un pomodoro | Click derecho sobre el cuadradito |
| Stats en vivo: actividad / todo el día | "ver todo el día ⇄" abajo de la grilla |
| Volver al tamaño original | Click derecho en la barra de arriba |
| Estadísticas | Botón de barras |

- Cerrar la ventana con un pomodoro, pausa o recreo en marcha la manda a la bandeja del sistema y el tiempo sigue contando. Si no, cierra la app.
- Cuando termina un pomodoro o un recreo, la ventana aparece sola, aunque esté escondida en el borde o en la bandeja.
- La alarma del recreo se repite y sube de volumen hasta que la cortás.

**Colores de los cuadraditos:** gris llenándose = en curso · negro = terminado · rojo = último del día.

## Datos

Todo queda en `%APPDATA%\UltimateTracker\ultimatetracker.db` (SQLite). La primera vez se copian solos los datos de cuando la app se llamaba Constancia (`%APPDATA%\Constancia`, que queda como respaldo). Para hacer backup alcanza con copiar ese archivo con la app cerrada.

Si la app se cierra de golpe (corte de luz, cuelgue), el pomodoro en curso se guarda hasta donde llegó, porque se persiste cada 5 segundos, y queda marcado como final.

## Estructura

```
src/core/                 lógica pura, sin interfaz (testeable)
  Database                SQLite: actividades, pomodoros, recreos, hitos, ajustes
  SessionController       máquina de estados del timer (ver diagrama en el .h)
  Stats                   totales, horas inactivas, racha con comodines, hitos
  FolderTree              árbol de actividades y suma de horas hacia arriba
src/ui/
  MainWindow              ventana gadget, borde, bandeja, tamaños
  TimerDeck               el reproductor (cronómetro con todos los controles)
  LiveStats               stats del día en vivo
  GridWidget              la grilla pintada a mano (día / semana / mes)
  StatsWidget             estadísticas y mapa de calor
  ActivitiesWidget        gestor de actividades
  EdgeTab, FolderMenu, SettingsDialog, Sounds, Icons, Theme
resources/                style.qss (tema gris) y sonidos
tools/make_sounds.py      genera los .wav (sonidos propios)
tests/test_core.cpp       tests del motor con reloj simulado
```

## Tests

```bat
cmake -S . -B build-tests -DCONSTANCIA_TESTS=ON
cmake --build build-tests
build-tests\core_tests.exe
```

En Qt Creator: activá `CONSTANCIA_TESTS` en *Projects → Build → CMake* y corré el target `core_tests`.

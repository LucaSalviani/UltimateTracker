# Constancia

App de escritorio para gamificar el estudio: pomodoros como cuadraditos en una grilla, horas por carpeta camino a las 10.000 h, recreos, recreos indebidos y estadísticas. C++20 + Qt 6, nativa, sin navegador embebido.

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

### 3. Dejarla instalada como app

Qt Creator compila en una carpeta tipo `build/Desktop_Qt_6_8_..._Release/`. Para tener un ejecutable que funcione solo:

```bat
mkdir C:\Apps\Constancia
copy build\...\Constancia.exe C:\Apps\Constancia\
C:\Qt\6.8.x\mingw_64\bin\windeployqt.exe --release C:\Apps\Constancia\Constancia.exe
```

`windeployqt` copia al lado del `.exe` las DLLs de Qt, el driver de SQLite y el backend de audio. Después:

- **Acceso directo:** clic derecho en `Constancia.exe` → *Mostrar más opciones → Crear acceso directo*, y anclalo a Inicio o a la barra de tareas.
- **Que arranque con Windows:** `Win+R` → `shell:startup` → pegá ahí el acceso directo.

## Uso

| Acción | Cómo |
|---|---|
| Arrancar / pausar / seguir | Botón redondo o **Espacio** |
| Elegir carpeta y duración | Abajo a la izquierda, antes de darle play |
| Vista día → semana → mes | Botones de la barra superior, **Ctrl + rueda** o **Ctrl −** / **Ctrl +** |
| Ir a un día | Click sobre el día en la vista semana o mes |
| Cuadraditos proporcionales al tiempo | Botón de los dos cuadrados en la barra superior |
| Esconder en un borde | Botón ⇥, o arrastrar la ventana y soltar con el mouse contra cualquier borde. Se vuelve a abrir con un click en la pestañita. |
| Siempre visible | Botón del pin |
| Carpetas y ajustes | Botón ⋯ |
| Estadísticas | Botón de barras |

- Cerrar la ventana con una sesión abierta la manda a la bandeja del sistema y el tiempo sigue contando. Sin sesión abierta, cierra la app.
- Cuando termina un pomodoro o un recreo, la ventana aparece sola, aunque esté escondida en el borde o en la bandeja.
- La alarma del recreo se repite y sube de volumen hasta que la cortás.

**Colores de los cuadraditos:** gris llenándose = en curso · negro = completo · rojo = último de la sesión (terminada o abandonada).

## Datos

Todo queda en `%APPDATA%\Constancia\constancia.db` (SQLite). Para hacer backup alcanza con copiar ese archivo con la app cerrada.

Si la app se cierra de golpe (corte de luz, cuelgue), el pomodoro en curso se guarda hasta donde llegó, porque se persiste cada 5 segundos, y queda marcado como final.

## Estructura

```
src/core/                 lógica pura, sin interfaz (testeable)
  Database                SQLite: carpetas, pomodoros, recreos, hitos, ajustes
  SessionController       máquina de estados del timer (ver diagrama en el .h)
  Stats                   totales, horas inactivas, racha con comodines, hitos
  FolderTree              árbol de carpetas y suma de horas hacia arriba
src/ui/
  MainWindow              ventana gadget, controles por estado, borde, bandeja
  GridWidget              la grilla pintada a mano (día / semana / mes)
  StatsWidget             estadísticas y mapa de calor
  EdgeTab, FolderMenu, FoldersDialog, SettingsDialog, Sounds, Icons, Theme
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

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
- **Que arranque con Windows:** *⋯ → Ajustes → Sistema → Iniciar con Windows* (si antes pusiste un acceso directo en `shell:startup`, borralo para que no abra dos veces).

## Uso

| Acción | Cómo |
|---|---|
| Arrancar / pausar / seguir | Botón grande del reproductor o **Espacio** |
| Elegir actividad y duración | Arriba del reproductor; la duración con los botones −1 −5 +5 +1, la rueda del mouse sobre el tiempo o click en el tiempo para escribirla |
| Objetivo del día | Click en la barra de arriba, botón de la diana o **G**; para otro día, click derecho sobre el día en la semana o el mes |
| Mini reproductor flotante | **M** o ⋯ → Mini reproductor (doble click en él abre la app) |
| Tamaño de los cuadraditos | Botón de la barra: cicla entre iguales, solo ancho y proporcional completo |
| Pantalla completa | ⋯ → Pantalla completa o **F11** (Esc sale) |
| Pausar / seguir desde cualquier lado | **Ctrl + Alt + P** |
| Vista día → semana → mes | Botones de la barra superior, **Ctrl + rueda** o **Ctrl −** / **Ctrl +** |
| Ir a un día | Click sobre el día en la vista semana o mes |
| Esconder en un borde | ⋯ → Esconder en el borde, o arrastrar la ventana y soltarla contra cualquier borde. Se vuelve a abrir con un click en la pestañita. |
| Siempre visible | ⋯ → Siempre visible |
| Actividades y ajustes | Botón ⋯ |
| Mover, anotar, corregir o borrar un pomodoro | Click derecho sobre el cuadradito (borrar se puede deshacer) |
| Horas que ya tenías | Actividades → click derecho → Horas previas… |
| Color de una actividad | Actividades → click derecho → Color (o Automático) |
| Stats en vivo: actividad / todo el día | "ver todo el día ⇄" abajo de la grilla |
| Volver al tamaño original | Click derecho en la barra de arriba |
| Estadísticas | Botón de barras |

- Cerrar la ventana con un pomodoro, pausa o recreo en marcha la manda a la bandeja del sistema y el tiempo sigue contando. Si no, cierra la app.
- Cuando termina un pomodoro o un recreo, la ventana aparece sola, aunque esté escondida en el borde o en la bandeja.
- La alarma del recreo se repite y sube de volumen hasta que la cortás.

**Colores de los cuadraditos:** el que corre se llena con el color del pomodoro · los terminados, del color de "terminado" (negro por defecto) · punteados = lo que falta del objetivo. Todo se cambia en *Ajustes › Cuadraditos*.

## Datos

Todo queda en `%APPDATA%\UltimateTracker\ultimatetracker.db` (SQLite) y se guarda al instante. La primera vez se copian solos los datos de cuando la app se llamaba Constancia (`%APPDATA%\Constancia`, que queda como respaldo).

Además, una vez por día se hace una copia de seguridad en `%APPDATA%\UltimateTracker\backups` (se guardan las últimas 7; la carpeta se puede cambiar en Ajustes, por ejemplo a OneDrive). Para volver a una copia: cerrá la app y reemplazá `ultimatetracker.db` por la copia (renombrándola).

Si la app se cierra de golpe (corte de luz, cuelgue), el pomodoro en curso se guarda hasta donde llegó, porque se persiste cada 5 segundos, y queda marcado como final.

## Estructura

```
src/core/                 lógica pura, sin interfaz (testeable)
  Database                SQLite: actividades, pomodoros, recreos, hitos, ajustes
  SessionController       máquina de estados del timer (ver diagrama en el .h)
  Stats                   totales, horas inactivas, racha con comodines, hitos
  FolderTree              árbol de actividades y suma de horas hacia arriba
  DayClock                el día de la app (empieza a las 4 AM)
  Goals                   metas diarias: specs, reglas que se repiten
src/ui/
  MainWindow              ventana gadget, borde, bandeja, tamaños
  TimerDeck               el reproductor (cronómetro con todos los controles)
  LiveStats               stats del día en vivo
  GridWidget              la grilla pintada a mano (día / semana / mes)
  StatsWidget             estadísticas y mapa de calor
  ActivitiesWidget        gestor de actividades
  GoalPanel               meta del día (panel deslizable)
  Celebration             animación de logro
  Greeting                saludo al iniciar con Windows
  SystemIntegration       backup, inicio con Windows, atajo global, pantalla completa
  EdgeTab, FolderMenu, SettingsDialog, Sounds, Icons, Theme
resources/                style.qss (plantilla de temas) y sonidos
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

## Módulos (para sacar uno si no te gusta)

Cada cambio grande está en su propio commit de git. Para sacar uno: `git revert <commit>` y recompilar. Las columnas y tablas que agregó un módulo quedan en la base sin usar (no molestan).

| Commit | Módulo |
|---|---|
| `6d76b94` | M1 · borrar y mover el pomodoro en pausa |
| `120a2f3` | M2 · el día empieza a las 4 AM |
| `e34d4a5` | M3 · temas de color y ajustes por secciones |
| `e150601` | M4 · ajuste del cronómetro (rueda, escribir, ±1/±5) |
| `4a3a1e9` | M5 · tamaños de ventana y pantalla completa |
| `0ddb560` | M6 · nota por pomodoro, corregir duración, deshacer |
| `0d6088c` | M7 · meta diaria |
| `3e66f79` | M8 · cuadraditos nuevos, agrupado, animaciones y festejos |
| `bfc5ff2` | M9 · backup, inicio con Windows, atajo global, no molestar |
| `842eff6` | M10 · saludo al iniciar |
| `96d1f8f` | M11 · horas previas |
| `1927114` | M12 · arreglos: editor del cronómetro y pantalla completa |
| `921e05e` | M13 · sin "¿En qué trabajaste?" |
| `a2f5a94` | M14 · "Objetivo" y la barra de arriba |
| `dcada70` | M15 · modo claro para cada color |
| `f90ba49` | M16 · tamaños de cuadraditos cíclicos |
| `1a28944` | M17 · objetivo con botones y precarga del próximo pomodoro |
| `388d9e4` | M18 · estilo de cuadraditos (formas, marcas, llenado, completado) |
| `1a8b3ee` | M19 · mini reproductor flotante |
| `1795836` | M20 · modo desarrollador |
| `2b5a6d9` | M21 · objetivo por tiempo acumulado, filas, barra con tu texto |
| `5ec3223` | M22 · acentos fuertes |
| `dee394e` | M23 · cuadraditos v2 (tamaño base, invertidos, paletas, borde, marcas nuevas…) |
| `39b30e8` | M24 · agrupado en línea recuadrado y tetris en proporcional completo |
| `43cc66c` | M25 · colores por actividad elegibles |
| `621d381` | M26 · festejos minimalistas (Línea, Borde, Tilde, Cifra) |
| `dd61ec3` | M27 · sonidos alternativos y volumen |
| `5e8157c` | M28 · reinicio del modo desarrollador, Enter en Ajustes, visor claro |
| `f0cc8cf` | M29 · ajustes de cuadraditos, número afuera, resaltar el que corre |
| `056cca6` | M30 · proporcional completo como rompecabezas |
| `8771c40` | M31 · texto negro sobre el resaltado, visor del acento, festejo por cosa, arreglo del tema |
| `e0a88e6` | M32 · segmentos "contorno", formato de tiempo único, Ajustes reordenado, barra liviana |

## Modo desarrollador

*Ajustes → Sistema → Modo desarrollador* y reiniciar. La app abre una **copia** de tu base (`ultimatetracker-pruebas.db`, se rehace en cada arranque) y aparece el botón 🧪: completar ya, acelerar el tiempo (×10, ×60, ×300), ver los festejos, la animación de completado, los sonidos y el saludo. Tus datos reales no se tocan. Para salir: 🧪 → Salir del modo desarrollador.

# UltimateTracker — Especificación

App de escritorio minimalista para gamificar el estudio: cuenta horas por tema, reemplaza la hoja de pomodoros en la pared y el cronómetro online.

## Stack
- C++20 + Qt 6 (Widgets, Sql, Multimedia) + SQLite + CMake.
- Objetivo principal: Windows 11. Portable a Linux/macOS (y Android a futuro).
- Sin navegador embebido. Estética gris oscura, minimalista (tipo app de Claude / Spotify).

## Actividades
- Árbol de actividades de profundidad libre, creadas y nombradas por el usuario (ej. Facultad › Sistemas Operativos › Unidad 3).
- Cada actividad acumula sus horas + las de todas sus subactividades.
- Cada pomodoro pertenece a una actividad (cualquier nivel).
- El contador de 10.000 h existe para cada actividad.
- Gestor: doble click para renombrar, arrastrar para ordenar o anidar, click derecho para agregar subactividad o eliminar. Eliminar no borra horas: pasan a "Sin actividad".

## Ciclo de un pomodoro
1. Se configura actividad + duración (default 25 min) y se le da **play** manualmente.
2. Se puede **pausar** (cartel grande de PAUSA, sin contador de indebidos) o **terminar el pomodoro** antes de tiempo: se guarda lo que llegó a correr. Menos de 1 minuto no se guarda.
3. Al terminar: suena el sonido de progreso. Pantalla con:
   - Botón principal: **Iniciar recreo** (duración editable, default 10 min).
   - **Saltear recreo**: arranca el siguiente pomodoro al instante, "pegado" al anterior (súper pomodoro).
   - No tomar recreo y no volver es un posible final del día.
4. El recreo tiene timer. Al terminar suena una **alarma fuerte que se repite hasta que el usuario la corta** (para despertar).
5. Después del recreo (o al saltearlo) se muestra el siguiente pomodoro **pre-cargado con la configuración del anterior**; se edita ahí mismo si se quiere y se le da play.

## Reproductor (cronómetro)
- Panel a la derecha de la grilla, ~80 % del alto de la vista día. Inspirado en caseteras/DVD: visor con contador grande, LED, carretes que giran, barra de progreso con transcurrido/total como Spotify.
- Rojo = pomodoro; verde agua = recreo.
- Adentro está todo lo que avanza, cambia o mueve el tiempo: actividad, duración (− / +), play/pausa, terminar pomodoro, iniciar/saltear recreo, terminar recreo, silenciar.
- La actividad se puede cambiar en cualquier momento:
  - En el primer minuto del pomodoro: se corrige la actividad (fue un error).
  - Después: avisa y el pomodoro queda **en pausa para su actividad** (⏸ en el menú, borde punteado en la grilla). Al volver a esa actividad aparece pausado para seguirlo. Si no se retoma en el día, se cierra solo con el tiempo que llevaba. Sobrevive a cerrar la app.
- Los pomodoros se registran solo hoy: viendo otro día, el reproductor muestra "SOLO HOY" e "Ir a hoy".

## Stats en vivo
- Abajo de la grilla: pomodoros, minutos // horas y un tercer dato.
- Por defecto muestra la actividad actual (tercer dato: total histórico de la actividad); se puede cambiar a todo el día (tercer dato: cuántas actividades distintas).
- El mismo toggle filtra los cuadraditos de la grilla (y su numeración) en todas las vistas.

## Tiempos
| Contador | Qué suma |
|---|---|
| Estudio | Tiempo efectivo de pomodoros (por actividad). |
| Recreos | Tiempo de recreo dentro de la duración pactada. |
| Recreos indebidos | Tiempo muerto entre medio de la jornada: pausas, esperar para arrancar el recreo o el próximo pomodoro, recreo excedido mientras suena la alarma. Solo cuenta si después se vuelve a estudiar ese día. No se muestra mientras pasa: se ve en el resumen del día y en estadísticas. |
| Horas inactivas | Todo lo demás del día (24 h − lo anterior), incluido el tiempo muerto después del último pomodoro. |

## Jornada (sesión)
- La sesión es el día: no hay botón para terminarla.
- Se cierra sola pasada la medianoche (cuando no hay nada en marcha) o al salir de la app.
- El tiempo muerto queda "pendiente": si volvés a estudiar, era recreo indebido; si no, era el final del día (inactivo).
- Al día siguiente, la primera vez que mirás la ventana, aparece el resumen del día anterior con los recreos indebidos.
- Si la app se cierra o se cae con un pomodoro corriendo, al volver se guarda lo que llegó a correr.
- Si se cruza la medianoche, el pomodoro cuenta para el día en que empezó.

## La grilla
- Un cuadradito por pomodoro, se agregan a medida que se arrancan (no hay cantidad fija).
- Dentro de cada cuadradito: su número del día (1, 2, 3…) y el tiempo que representa (ej. `25m`, `1h05`).
- Los pomodoros arrancados salteando el recreo van pegados al anterior con un puente.
- Estados:
  - **En curso:** se va llenando de gris.
  - **Terminado:** negro (como tachar la hoja), con el tiempo que duró.
  - **Último del día:** **rojo**. Para hoy es provisorio: se pinta rojo mientras no haya nada en marcha.
- Click derecho sobre un cuadradito → **Mover a actividad** o **Borrar pomodoro** (con confirmación; se descuenta de las estadísticas).
- En semana y mes, el día bajo el mouse se ilumina con "ver día ›" y cursor de mano.
- Toggle **proporcional**: el área de cada cuadradito pasa a ser proporcional a sus minutos.
- Vistas tipo Google Calendar:
  - **Día** (al abrir siempre muestra hoy): ventana horizontal chica. Banda gris con la tarea del día (opcional) a la izquierda y la fecha a la derecha (por defecto `26/10/26`; en Ajustes se puede poner escrita).
  - **Semana** (lunes a domingo): la ventana se agranda.
  - **Mes**: la ventana se agranda más. Siempre se ven los cuadraditos.

## Estadísticas
- Por día, semana, mes y año, filtrables por actividad, con los cuatro contadores.
- **Mapa de calor** anual (una celda por día, más intensa cuanto más se estudió).
- **Racha**: día estudiado = al menos 1 pomodoro iniciado. 1 o 2 comodines por semana (configurable) que no la rompen.

## Hitos
- Aviso al llegar a 1, 2, …, 10 h y luego cada 25 h, en cada actividad.
- Los stages hacia las 10.000 h quedan para una versión futura.

## Ventana
- Visible mientras el usuario quiera; opción "siempre visible"; botón de minimizar.
- Redimensionable desde los bordes (se puede desactivar en Ajustes). Cada vista recuerda para siempre el tamaño que le dejaste.
- Click derecho en la barra de arriba → restablecer el tamaño original (de esta vista o de todas).
- Al agrandar la vista (semana, mes, estadísticas) y volver a achicarla, la ventana vuelve al lugar donde estaba.
- Tooltips en todos los botones de la barra.
- **Modo edge**: se esconde contra cualquier borde (incluido el superior) con una animación, dejando solo una pestañita fina, sin timer visible. Con el mouse encima la pestañita sale un poco más.
- Cuando termina un timer, la ventana sale sola y suena.

## Fuera de la v1
- Importar el historial de las hojas en papel.
- Sincronizar con el celular.
- Stages hacia las 10.000 h.

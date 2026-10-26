# Constancia — Especificación v1

App de escritorio minimalista para gamificar el estudio: cuenta horas por tema, reemplaza la hoja de pomodoros en la pared y el cronómetro online.

## Stack
- C++20 + Qt 6 (Widgets, Sql, Multimedia) + SQLite + CMake.
- Objetivo principal: Windows 11. Portable a Linux/macOS (y Android a futuro).
- Sin navegador embebido. Estética gris oscura, minimalista (tipo app de Claude / Spotify).

## Carpetas (temas)
- Árbol de carpetas de profundidad libre, creadas y nombradas por el usuario (ej. Facultad › Sistemas Operativos › Unidad 3).
- Cada carpeta acumula sus horas + las de todas sus subcarpetas.
- Cada pomodoro pertenece a una carpeta (cualquier nivel).
- El contador de 10.000 h existe para cada carpeta.

## Ciclo de un pomodoro
1. Se configura carpeta + duración (default 25 min) y se le da **play** manualmente.
2. Se puede **pausar**. El tiempo en pausa suma a **Recreos indebidos**.
3. Al terminar: suena el sonido de progreso. Pantalla con:
   - Botón principal: **Iniciar recreo** (duración editable, default 10 min).
   - Botones chicos: **Saltear recreo** y **Terminar sesión**.
4. El recreo tiene timer. Al terminar suena una **alarma fuerte que se repite hasta que el usuario la corta** (para despertar).
5. Después del recreo (o al saltearlo) se muestra el siguiente pomodoro **pre-cargado con la configuración del anterior**; se edita ahí mismo si se quiere y se le da play.

## Tiempos
| Contador | Qué suma |
|---|---|
| Estudio | Tiempo efectivo de pomodoros (por carpeta). |
| Recreos | Tiempo de recreo dentro de la duración pactada. |
| Recreos indebidos | Pausas, y todo tiempo muerto con la sesión abierta: esperar para arrancar el recreo o el próximo pomodoro, recreo excedido mientras suena la alarma. |
| Horas inactivas | Todo lo demás del día (24 h − lo anterior), incluido el tiempo con la sesión terminada. |

## Sesión del día
- Se abre al dar play al primer pomodoro.
- Se cierra **solo** con el botón **Terminar sesión** (se puede terminar también estando en pausa o en medio de un pomodoro).
- Al cerrar: se muestra el resumen con los recreos indebidos del día. Desde ahí el tiempo cuenta como inactivo.
- Se puede **retomar**: siempre arranca con un pomodoro nuevo.
- Si la app se cierra o se cae con un pomodoro corriendo, al volver se guarda lo que llegó a correr y se marca como final.
- Si se cruza la medianoche, el pomodoro cuenta para el día en que empezó.

## La grilla
- Un cuadradito por pomodoro, se agregan a medida que se arrancan (no hay cantidad fija).
- Dentro de cada cuadradito: el tiempo que representa (ej. `25'`).
- Estados:
  - **En curso:** se va llenando de gris.
  - **Completo:** negro (como tachar la hoja).
  - **Final** (último del bloque, por terminar sesión o abandono): **rojo**, con el tiempo al que llegó.
- Toggle **proporcional**: el área de cada cuadradito pasa a ser proporcional a sus minutos.
- Vistas tipo Google Calendar:
  - **Día** (al abrir siempre muestra hoy): ventana horizontal del tamaño aprox. de la calculadora. Fecha sobre fondo gris + título opcional de la tarea del día.
  - **Semana** (lunes a domingo): la ventana se agranda.
  - **Mes**: la ventana se agranda más. Siempre se ven los cuadraditos.

## Estadísticas
- Por día, semana, mes y año, filtrables por carpeta, con los cuatro contadores.
- **Mapa de calor** anual (una celda por día, más intensa cuanto más se estudió).
- **Racha**: día estudiado = al menos 1 pomodoro iniciado. 1 o 2 comodines por semana (configurable) que no la rompen.

## Hitos
- Aviso al llegar a 1, 2, …, 10 h y luego cada 25 h, en cada carpeta.
- Los stages hacia las 10.000 h quedan para una versión futura.

## Ventana
- Visible mientras el usuario quiera; opción "siempre visible".
- **Modo edge**: se esconde contra cualquier borde (incluido el superior) dejando solo una pestañita fina, sin timer visible.
- Cuando termina un timer, la ventana sale sola y suena.

## Fuera de la v1
- Importar el historial de las hojas en papel.
- Sincronizar con el celular.
- Stages hacia las 10.000 h.

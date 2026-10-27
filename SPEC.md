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
- Rojo = pomodoro; verde agua = recreo (cada tema de color trae sus propios tonos).
- Ajustar la duración antes de arrancar (cada forma se puede apagar en *Ajustes › Cronómetro*):
  - **Rueda del mouse** sobre el tiempo (sensibilidad configurable).
  - **Click en el tiempo** para escribir los minutos (`45`, `90` o `1:30`; Enter confirma, Esc cancela).
  - Botones **−1 −5 · +5 +1** (los de afuera y los de adentro con paso configurable). Sin ellos queda el − / + clásico (de a 5 arriba de 10, de a 1 abajo).
- La nota de un pomodoro se agrega con click derecho y se ve en el tooltip.
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
- **El día de la app empieza a las 4 AM** (configurable de 0 a 6 en *Ajustes › General*): lo que estudies a las 2 AM cuenta para el día anterior.
- Se cierra sola pasado el inicio del día (cuando no hay nada en marcha) o al salir de la app.
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
- Click derecho sobre un cuadradito → **Mover a actividad**, **Nota**, **Corregir duración** (solo terminados) o **Borrar pomodoro**. Uno terminado se borra al toque con **Deshacer** en el aviso; el que está en marcha o en pausa pide confirmación.
- **Agrupar** (*Ajustes › Apariencia*): sin agrupar; por actividad con títulos (cada una en su renglón); o en línea, cada actividad recuadrada con una línea fina de su color. En proporcional completo siempre va en un bloque tipo tetris con el contorno de cada actividad.
- **Colores por actividad**: automáticos, elegidos a mano (Actividades → Color) o apagados.
- Al cambiar de vista los cuadraditos entran en cascada con un rebote; uno nuevo "salta" solo.
- **Fantasmas de la meta**: los pomodoros que faltan para la meta del día se ven punteados después de los reales (también en semana y mes). Click en uno abre la meta.
- En semana y mes, el día bajo el mouse se ilumina con "ver día ›" y cursor de mano.
- **Tamaño**: el botón de la barra cicla entre todos iguales, proporcional solo a lo ancho (alto fijo, largo ∝ minutos) y proporcional completo (área ∝ minutos). En Ajustes se pueden desactivar variantes o fijar una y esconder el botón.
- **Resaltar** (*Ajustes › Cuadraditos*): por defecto, el que está corriendo se llena con el color del pomodoro (texto negro encima; con la paleta roja, el resaltado es negro); el rojo del último del día queda como opción.
- **Número**: adentro arriba al centro, afuera arriba (como en los bocetos) u oculto; con "Nº" opcional. Los números de los cuadraditos usan la fuente del cronómetro.
- **Proporcional completo**: todas las piezas se encastran como un rompecabezas lo más cuadrado posible, cada actividad junta y con su contorno.
- **Estilo** (*Ajustes › Cuadraditos*, con muestra en vivo):
  - Forma: Clásico, Inclinado (romboide apoyado sobre su base, esquinas suaves), Tarjeta (vertical), Biselado (dos esquinas cortadas suaves). El número va arriba al centro.
  - Marca de hecho: ninguna, diagonal (finita, del color del número, sin tapar el tiempo), cruz delicada, tilde redondo, tilde cuadrado.
  - Llenado del que corre: barra, líquido (avanza de izquierda a derecha con el frente ondulado), segmentos (vacíos con borde fino, llenos sólidos, el actual se carga de abajo hacia arriba; inclinados en la forma inclinada), reloj.
  - Invertidos (por defecto): fondo gris que se va llenando del color de terminado.
  - Colores propios de los cuadraditos (del tema, naranja, rojo, azul, verde, papel), grosor de borde, tamaño base (60–170 %), rayita de la actividad y nombre de la actividad opcionales.
  - Al completarse: rebote, sellado, destello, volteo (después del festejo grande).
- Vistas tipo Google Calendar:
  - **Día** (al abrir siempre muestra hoy): ventana horizontal chica. Banda gris con la tarea del día (opcional) a la izquierda y la fecha a la derecha (por defecto `26/10/26`; en Ajustes se puede poner escrita).
  - **Semana** (lunes a domingo): la ventana se agranda.
  - **Mes**: la ventana se agranda más. Siempre se ven los cuadraditos.

## Objetivo del día
- La barra de arriba muestra solo lo que escribiste en "¿Qué querés lograr hoy?" (vacía si no escribiste nada); click ahí, el botón de la diana o **G** abren el panel que sale desde la derecha. Para otros días dice "¿Qué querés lograr este día?".
- **El progreso es por tiempo acumulado**: los minutos estudiados ese día van llenando los bloques en orden; el bloque que va a medias se ve lleno en parte (dos pomodoros de 3m no completan dos bloques de 40m). Click derecho en un día de la semana o el mes → "Poner meta…" para planearla.
- La tarea del día (texto) también se edita ahí.
- **Por tiempo** (ej. 4 h → se pasa a pomodoros con la duración que estés usando, redondeando para arriba) o **por pomodoros**: filas "[−] 3 [+] × [−] 30m [+]" con "+ Agregar" (scrollea si hay muchas), o escrito como `3x30 + 2x25` (se elige en Ajustes › General).
- Por pomodoros, el reproductor viene precargado con la duración del próximo bloque del objetivo.
- **Repetir** como en Google Calendar: solo ese día, todos los días, de lunes a viernes o días elegidos. Cambiar una meta que se repite corta la anterior el día antes (el pasado no cambia). Quitar: "solo este día" o "desde este día en adelante".
- Progreso en el panel; al cumplirla, festejo con sonido (una vez por día).

## Festejos
- Al terminar un pomodoro, cumplir el objetivo o llegar a un hito, cada uno con su estilo (*Ajustes › Festejos y sonidos*):
  - Visor: anillo tipo visor de cámara, emblema con rebote (dorado para hitos). Click lo cierra.
  - Minimalistas, que no tapan ni bloquean: Línea (trazo fino arriba + texto chico), Borde (el contorno de la ventana se dibuja una vez), Tilde (círculo chico con tilde), Cifra ("+25m" grande y finito que sube y se va).
- Se pueden apagar del todo.

## Sonidos
- *Ajustes › Festejos y sonidos*, con ▶ para escuchar: fin de pomodoro (Arpegio, Suave, Campana, Madera, Digital, ninguno), logro (Obturador y arpegio, Acorde suave, ninguno), alarma del recreo (Despertador, Marimba, Campanitas) y volumen general. Los genera `tools/make_sounds.py`.

## Estadísticas
- Por día, semana, mes y año, filtrables por actividad, con los cuatro contadores.
- **Mapa de calor** anual (una celda por día, más intensa cuanto más se estudió).
- **Racha**: día estudiado = al menos 1 pomodoro iniciado. 1 o 2 comodines por semana (configurable) que no la rompen.
- **Colores**: barras y mapa de calor en el acento del tema (monocromo en Gris); cada actividad con su color en las filas, la línea del día y el menú de actividades.
- **Exportar a CSV** (botón ⤓): lo que estás viendo (período + actividad) o todo el historial. Separador `;`, coma decimal, UTF-8: abre directo en Excel en español.
- Cambiar de período o actividad hace un fundido corto; los períodos vacíos lo dicen ("Sin horas esta semana").

## Hitos
- Aviso al llegar a 1, 2, …, 10 h y luego cada 25 h, en cada actividad.
- **Horas previas**: en Actividades, click derecho → "Horas previas…" para sumar lo que ya tenías (la hoja de papel). Cuentan en el total y los hitos, no en los días.
- Los stages hacia las 10.000 h quedan para una versión futura.

## Ventana
- Visible mientras el usuario quiera; opción "siempre visible"; botón de minimizar.
- Redimensionable desde los bordes (se puede desactivar en Ajustes). Cada vista recuerda para siempre el tamaño que le dejaste.
- Click derecho en la barra de arriba → restablecer el tamaño original (de esta vista o de todas).
- Al agrandar la vista (semana, mes, estadísticas) y volver a achicarla, la ventana vuelve al lugar donde estaba.
- Tooltips en todos los botones de la barra.
- **Modo edge**: se esconde contra cualquier borde (incluido el superior) con una animación, dejando solo una pestañita fina, sin timer visible. Con el mouse encima la pestañita sale un poco más.
- Cuando termina un timer, la ventana sale sola y suena. Si estás en pantalla completa (juego, presentación, video) no se mete adelante: avisa en la bandeja (se puede apagar).
- **Tamaño**: chica / mediana (por defecto, +15 %) / grande (+35 %) en *Ajustes › Apariencia*.
- **Pantalla completa**: menú ⋯ o **F11**; **Esc** sale.
- **Barra de arriba** liviana: tamaño de cuadraditos, zoom, objetivo, estadísticas y ⋯ (actividades, ajustes, mini reproductor, siempre visible, esconder en el borde, pantalla completa, salir).
- **Visor del cronómetro**: del color del acento (por defecto, con tinta negra), oscuro siempre o siguiendo al tema.
- **Formato de tiempo** único en toda la app: "25m", "1h40".
- **Temas de color**: color (Gris por defecto, Ultra, Rosa, Rojo, Azul, Mostaza) + modo Oscuro / Claro / Seguir a Windows, con vista previa en vivo. Los claros, estilo MKBHD. Opción de acentos fuertes.
- **Atajos**: Espacio play/pausa · ← → día o período · T hoy · Ctrl −/+ zoom · G objetivo · N nota del último pomodoro · S estadísticas · A actividades · M mini · F11 pantalla completa · Esc volver · Ctrl+, ajustes · Ctrl+Z deshacer · ? (o F1) panel con todos.
- **Deshacer**: borrar o mover pomodoros, borrar actividades (sin pregunta: vuelven con subactividades, colores y pomodoros), guardar o quitar objetivos, corregir duración. El aviso de abajo ofrece "Deshacer" (o Ctrl+Z).
- **Estados vacíos**: un cuadradito punteado y dos líneas que dicen qué pasa y qué hacer (primer uso, hoy en blanco, día que no llegó, día sin pomodoros).
- **Ajustes**: "Restablecer esta sección" vuelve la sección visible a los valores por defecto (se guarda con Guardar).
- **Mini reproductor flotante** (M): ventanita siempre visible con pomodoro, actividad, tiempo, play/pausa, terminar y saltear recreo.

## Sistema
- **Iniciar con Windows** (entrada en `HKCU\…\Run` con `--autostart`).
- **Saludo**: al iniciar con Windows, una vez por día: "Buen día, ¿estás listo para ser productivo?" con una frase, la racha, lo de ayer y la meta. "A trabajar" se expande hasta la ventana y abre la meta del día; "Por ahora no" cierra el programa. Para verlo a mano: `UltimateTracker.exe --greeting`.
- **Atajo global Ctrl + Alt + P**: pausar / seguir desde cualquier lado.
- **Modo desarrollador**: trabaja sobre una copia de la base (al prenderlo o apagarlo en Ajustes la app se reinicia sola); botón 🧪 para completar ya, acelerar el tiempo y ver festejos, sonidos y saludo.
- **Copia de seguridad diaria**: `%APPDATA%\UltimateTracker\backups` (o la carpeta que elijas), se guardan las últimas 7.

## Fuera de la v1
- Sincronizar con el celular.
- Stages hacia las 10.000 h.

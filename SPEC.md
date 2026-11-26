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
- Por defecto muestra la actividad actual: lo de **hoy** en esa actividad (tercer dato: qué parte del día fue; el total histórico va chiquito en la cabecera, "en total: 124h"); se puede cambiar a todo el día (tercer dato: cuántas actividades distintas).
- El mismo toggle filtra los cuadraditos de la grilla (y su numeración) en todas las vistas.

## Tiempos
| Contador | Qué suma |
|---|---|
| Estudio | Tiempo efectivo de pomodoros (por actividad). |
| Recreos | Tiempo de recreo dentro de la duración pactada. |
| Recreos indebidos | Tiempo muerto entre medio de la jornada: pausas, esperar para arrancar el recreo o el próximo pomodoro, recreo excedido mientras suena la alarma. Solo cuenta si después se vuelve a estudiar ese día. No se muestra mientras pasa: se ve en el resumen del día y en estadísticas. |
| Horas inactivas | Todo lo demás del día (24 h − lo anterior), incluido el tiempo muerto después del último pomodoro. |

## Jornada (sesión)
- La sesión es el día. **«Me voy»** (debajo del reproductor cuando no corre un pomodoro, en la bandeja, Ctrl+Shift+Q; en el celu, debajo de los botones) la termina antes: guarda lo que llevás y el tiempo hasta que vuelvas no cuenta como recreo indebido.
- **El día de la app empieza a las 4 AM** (configurable de 0 a 6 en *Ajustes › General*): lo que estudies a las 2 AM cuenta para el día anterior.
- Se cierra sola pasado el inicio del día (cuando no hay nada en marcha) o al salir de la app.
- El tiempo muerto queda "pendiente": si volvés a estudiar, era recreo indebido; si no, era el final del día (inactivo).
- **Tarjeta del día** (sobre la misma ventana, una vez por día, la primera vez que la mirás): con el saludo activado, "Buen día, ¿listo para ser productivo?" y una frase; si no, "Así te fue ayer". Trae el día anterior (horas, pomodoros, recreos, indebidos), la racha y los comodines que quedan (avisa si hoy se usa un comodín o se corta), el objetivo de hoy y "Hoy arrancás con <actividad> ▾" para cambiarla ahí. En el celu, una tarjeta igual arriba de Hoy.
- Hasta el primer pomodoro del día, la actividad del reproductor titila suave unos segundos y queda marcada (¿es esto lo que querés hacer hoy?).
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
- **Agrupar** (*Ajustes › Apariencia*): sin agrupar; por actividad con títulos (cada una en su renglón); o en línea, cada actividad recuadrada con una línea fina de su color.
- **Colores por actividad**: automáticos, elegidos a mano (Actividades → Color) o apagados.
- Al cambiar de vista los cuadraditos entran en cascada con un rebote; uno nuevo "salta" solo.
- **Fantasmas de la meta**: los pomodoros que faltan para la meta del día se ven punteados después de los reales (también en semana y mes). Click en uno abre la meta.
- En semana y mes, el día bajo el mouse se ilumina con "ver día ›" y cursor de mano.
- **Tamaño**: el botón de la barra cicla entre todos iguales, proporcional solo a lo ancho (alto fijo, largo ∝ minutos) y proporcional completo (área ∝ minutos). En Ajustes se pueden desactivar variantes o fijar una y esconder el botón.
- **Resaltar** (*Ajustes › Cuadraditos*): por defecto, el que está corriendo se llena con el color del pomodoro (texto negro encima; con la paleta roja, el resaltado es negro); el rojo del último del día queda como opción.
- **Número**: adentro arriba al centro, afuera arriba (como en los bocetos) u oculto; con "Nº" opcional. Los números de los cuadraditos usan la fuente del cronómetro.
- **Proporcional completo**: lado ∝ √minutos (área ∝ tiempo), en orden y alineados abajo como un gráfico, con el agrupado elegido. El de 25' conserva su tamaño salvo que el más grande no entre.
- **Proporcional a lo ancho**: solo cambia el largo; el tiempo, el número y la marca tienen el mismo tamaño en todos y no se estiran. Los segmentos: 5 por cada 25'.
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
- **Por tiempo** (se escribe directo en el número: `3h30`, `3:30`, `4` o `210`; con el foco, la rueda va de a 15 min; se pasa a pomodoros con la duración que estés usando, redondeando para arriba) o **por pomodoros**: filas "[−] 3 [+] × [−] 30m [+]" con "+ Agregar" (scrollea si hay muchas), o escrito como `3x30 + 2x25` (se elige en Ajustes › General).
- Por pomodoros, el reproductor viene precargado con la duración del próximo bloque del objetivo.
- **Con actividad** (B2): cada fila elige su actividad ("1 de 25 para AM2 + 3 de 30 para SO") o "cualquiera". Se guarda como `1x25@<id>+3x30@<id>`. Los fantasmas toman el color de su actividad y van en su grupo; los bloques con actividad se llenan con lo estudiado en ella (o en sus subactividades) y lo que sobra llena los "cualquiera". El próximo pomodoro viene con la duración y la actividad del próximo bloque.
- **El pomodoro en curso** (*Ajustes › General*): *reemplaza al fantasma* (por defecto: al dar play, el fantasma que le toca se oculta; si lo cortás antes, vuelve con lo que faltó) o *se funde con el fantasma al terminar* (el fantasma se va llenando y al terminar vuela hasta el cuadradito hecho).
- **Repetir** como en Google Calendar: solo ese día, todos los días, de lunes a viernes o días elegidos. Cambiar una meta que se repite corta la anterior el día antes (el pasado no cambia). Quitar: "solo este día" o "desde este día en adelante".
- Progreso en el panel; al cumplirla, festejo con sonido (una vez por día).
- **Con recreos**: debajo del total, "3h20 de estudio · ≈ 4h05 con recreos" (un recreo entre bloque y bloque). *Ajustes › General › Recreos del objetivo*: tu duración de recreo (por defecto) o tu promedio real de las últimas 4 semanas. También en el celu.

## Festejos
- Al terminar un pomodoro, cumplir el objetivo o llegar a un hito, cada uno con su estilo (*Ajustes › Festejos y sonidos*):
  - Visor: anillo tipo visor de cámara, emblema con rebote (dorado para hitos). Click lo cierra.
  - Minimalistas, que no tapan ni bloquean: Línea (trazo fino arriba + texto chico), Borde (el contorno de la ventana se dibuja una vez), Tilde (círculo chico con tilde), Cifra ("+25m" grande y finito que sube y se va).
- Se pueden apagar del todo.

## Sonidos
- *Ajustes › Festejos y sonidos*, con ▶ para escuchar: fin de pomodoro (Arpegio, Suave, Campana, Madera, Digital, ninguno), logro (Obturador y arpegio, Acorde suave, ninguno), alarma del recreo (Despertador, Marimba, Campanitas) y volumen general. Los genera `tools/make_sounds.py`.
- El reproductor sigue a la salida de audio actual de Windows (se rearma si cambia, si falló o si pasó mucho sin sonar, y reintenta si no sonó). Cada sonido o falla queda en `sonidos.log` junto a la base. Al probar uno, Ajustes dice por qué salida suena. Además, una notificación de Windows al terminar el pomodoro o el recreo (se puede apagar).

## Estadísticas
- Por día, semana, mes y año, filtrables por actividad, con los cuatro contadores.
- **Mapa de calor** anual (una celda por día, más intensa cuanto más se estudió).
- **Objetivos** (B2): tarjeta "4 de 6 cumplidos · faltaron 2h30", rayita del objetivo en cada barra y barras verdes los días cumplidos, borde en el mapa de calor, y en el día "objetivo 4h: faltaron 50m".
- **Racha**: cuentan solo los días con algún pomodoro guardado. 1 o 2 comodines por semana (configurable) la sostienen pero no suman días.
- **Colores**: barras y mapa de calor en el color del visor del cronómetro (el rojo suave en Gris); cada actividad con su color en las filas, la línea del día y el menú de actividades.
- **Exportar a CSV** (botón ⤓): lo que estás viendo (período + actividad) o todo el historial. Separador `;`, coma decimal, UTF-8: abre directo en Excel en español.
- Cambiar de período o actividad hace un fundido corto; los períodos vacíos lo dicen ("Sin horas esta semana").

## Hitos
Tres plazos; cada uno se prende, se apaga y se ajusta en *Ajustes › Hitos*.
- **Corto plazo**
  - *Marca del día*: bronce, plata y oro (2 h, 4 h y 6 h por defecto). Tres puntitos al lado de "estudiado" (vista de todo el día).
  - *Rachas*: aviso a los 3, 7, 14, 30, 60, 100, 180 y 365 días. *Récords*: mejor día, mejor semana y racha más larga.
  - *Meta semanal* (20 h por defecto): una línea finita abajo de la grilla que se llena; al cumplirla, aviso.
  - Festejo: el estilo elegido para "objetivo" (minimalista por defecto).
- **Mediano plazo**
  - *Horas por actividad*: 1, 5, 10, 25, 50, 100, 150, 200, 300, 400, 500, 750, 1.000 y después cada 500 h.
  - *Constancia*: 2, 4, 8, 12, 26 y 52 semanas seguidas cumpliendo la meta.
  - *Resumen del mes*: al abrir la app en un mes nuevo, horas, días, comparación con el anterior, lo más estudiado y el mejor día.
- **Largo plazo: etapas hacia las 10.000 h**, por actividad y un nivel global con la suma de todo:
  Novato (0–20 h) · Aprendiz (20–100) · Practicante (100–500) · Competente (500–1.000) · Avanzado (1.000–2.500) · Especialista (2.500–5.000) · Experto (5.000–10.000) · Maestro. Cada etapa con subniveles I, II y III. En Estadísticas, cada actividad muestra su etapa y la barra hasta el próximo subnivel.
- Los que saltan con el mismo pomodoro salen juntos en un solo aviso; el festejo es el del más importante.
- **Horas previas**: en Actividades, click derecho → "Horas previas…" para sumar lo que ya tenías. Cuentan en el total, los hitos y las etapas, no en los días.

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
- **Iniciar con Windows** (entrada en `HKCU\…\Run` con `--autostart`). Apagado por defecto para instalaciones nuevas.
- **Saludo** (apagado por defecto): la primera vez que usás la PC en el día —al iniciar con Windows, al despertarla o al desbloquearla—, una vez por día. Con la app abierta lo detecta sola; cerrada, una tarea programada de Windows ("UltimateTracker - saludo") la abre con `--wake` y, si hoy ya saludó, no abre nada: "Buen día, ¿estás listo para ser productivo?" con una frase, la racha, lo de ayer y la meta. "A trabajar" se expande hasta la ventana y abre la meta del día; "Por ahora no" cierra el programa. Para verlo a mano: `UltimateTracker.exe --greeting`.
- **Atajo global Ctrl + Alt + P**: pausar / seguir desde cualquier lado.
- **Modo desarrollador**: trabaja sobre una copia de la base (al prenderlo o apagarlo en Ajustes la app se reinicia sola); botón 🧪 para completar ya, acelerar el tiempo y ver festejos, sonidos y saludo.
- **Copia de seguridad diaria**: `%APPDATA%\UltimateTracker\backups` (o la carpeta que elijas), se guardan las últimas 7.

## Celular (Android, beta)
- App en QML sobre el mismo motor de la PC (`mobile/`), con cuatro pestañas abajo:
  - **Hoy**: el reloj (actividad, duración del pomodoro y del recreo, play/pausa, terminar, recreo o seguir sin recreo), el objetivo del día con su barra y los cuadraditos de hoy, con los fantasmas de lo que falta. Tocar un cuadradito: nota, cambiar la actividad o borrarlo (con deshacer).
  - **Objetivo del día** (tocando la tarjeta): "¿Qué querés lograr hoy?", por pomodoros (grupos "4 × 25m", cada uno de una actividad o de cualquiera) o por tiempo, y repetir (solo hoy, lun a vie, todos los días o días sueltos). Igual que en la PC: el próximo pomodoro viene con la duración y la actividad del bloque que sigue.
  - **Estadísticas**: día, semana, mes y año, con flechas para ir hacia atrás y filtro por actividad (tocar una actividad entra en sus subactividades). Total, pomodoros, promedio, días con estudio, objetivos cumplidos (barras verdes), por actividad, y recreos / recreo indebido / inactivo. En "Día", los cuadraditos con sus notas.
  - **Hitos**: etapas hacia las 10.000 h (global y por actividad), marca del día, racha y comodines, récords, meta semanal y cómo va el mes contra el anterior.
  - **Ajustes**: cuenta, duraciones, "el pomodoro en curso ocupa un bloque del objetivo", inicio del día, comodines, hitos (prender/apagar, meta semanal, bronce/plata/oro) y actividades (nombre, color, horas previas, subactividades, borrar).
- Festejos (objetivo cumplido, marcas, rachas, récords, etapas y horas) como en la PC.
- Con la pantalla apagada: notificación con la cuenta regresiva (servicio en primer plano, así Android no corta la app) y alarma exacta que suena y vibra al terminar el pomodoro o el recreo.
- Se compila sola en GitHub Actions y se publica en Releases como `android-beta` (APK firmado siempre con la misma llave: cada versión se instala arriba de la anterior).

## Ayudas
- **Rueda del mouse**: los números, listas y deslizadores no cambian con la rueda mientras recorrés una página; solo si antes les hiciste click y el cursor está encima.
- **Mover varios cuadraditos**: Ctrl + click elige uno por uno, Shift + click los del medio; click derecho › "Mover los N a actividad" (con deshacer). En el menú de un cuadradito: "Mover los N de <actividad> de ese día a…". En el celu, mantener apretado.
- **Mini tutorial**: la primera vez que se abre y desde ⋯ › *Ver tutorial* (en el celu, Ajustes › *Ver tutorial*).

## Sincronización (PC ↔ celu)
- Supabase (gratis): una tabla `records` con una fila por actividad, pomodoro, recreo, objetivo o título de día, protegida para que cada cuenta vea solo lo suyo (`tools/supabase.sql`).
- Cada aparato guarda todo localmente y funciona sin internet. Unos triggers anotan lo que cambia; al sincronizar se baja lo nuevo (gana el cambio más reciente) y se sube lo anotado. Cada minuto con la app abierta en la PC, cada 30 s en el celu, al terminar un pomodoro y al volver a la app.
- La primera vez, las actividades con el mismo nombre y lugar se unen (no queda la "General" duplicada). Un pomodoro que llega corriendo de otro aparato no se cierra solo.
- Cuenta: ⋯ › *Sincronizar con el celu* en la PC; la nube arriba a la derecha en el celu. Crear cuenta → confirmar el mail → Entrar.

## Mejoras 3.0 (beta 4)
- **Estadísticas**: la marca de hoy se ve entera (chips en filas, letra que se achica, medallas arriba a la derecha). Pasar el mouse sobre cualquier dato muestra el detalle; hacer click abre una ventanita adentro de la app (tiempo, recreos, recreo indebido, inactivo, racha, objetivos, marca, semana, récords, nivel). Los récords muestran el dato, la fecha y la comparación con hoy.
- **Racha**: cambiar los comodines por semana rige desde hoy y no toca el pasado (historial en `jokers_history`). "Reiniciar racha…" en el detalle de la racha (con confirmación y "Deshacer el reinicio"); el récord de la racha más larga se conserva. En el celu: Hitos › *Reiniciar racha*.
- **Seguir sin recreo** extiende el mismo pomodoro (tramos seguidos, `segments`; cuenta por unidades de la duración base) y el recreo salteado queda guardado (hasta 60 min, se pierde al cerrar el día) para tomarlo a mitad del pomodoro.
- **Tipo de actividad** (Estudio, Trabajo, Lectura, Práctica o propio): menú de la actividad › *Tipo*. Las subactividades heredan; cuando hay mezcla se usa "foco". Cambia los verbos y textos en toda la app (PC y celu).
- **Saludo tras suspender**: una vez por día; detecta el despertar por salto de reloj (latido de 10 s), muestra la ventana aunque esté minimizada o anclada y espera si la PC está bloqueada. Registro en `arranque.log` (junto a `sonidos.log`).
- **Distracciones** (Ajustes › Cronómetro › Distracciones; apagado por defecto): con un pomodoro corriendo, si la ventana activa coincide con la lista de palabras (por defecto Instagram y WhatsApp) pasado el tiempo de gracia, pantalla roja "RECREO INDEBIDO" y, si se eligió, pausa. Todo local: solo se mira el título/programa de la ventana activa, nada se guarda ni se envía.

## Mejoras 4.0 (beta 5)
- **Marcar a mano**: click derecho en un cuadradito del día › *Marcar como hecho* (duración a elegir). Cuenta igual que uno cronometrado y lleva la marca "manual" (triangulito en la esquina; `pomodoros.manual`, sincronizado).
- **Pausar el recreo**: botón del reproductor (Espacio en PC, botón izquierdo en el celu). El tiempo en pausa cuenta como recreo indebido; se refleja en el vínculo PC↔celu (`break_paused`).
- **Objetivo en bolsa de minutos**: los fantasmas se reparten por los minutos que faltan; si un pomodoro dura más que su bloque, el siguiente se achica (8×25: el primero de 30 → el segundo pasa a 20).
- **Recreo guardado**: tomar cualquier recreo lo pone en cero (el botón sigue sirviendo por si te arrepentís a mitad de un pomodoro doble).
- **Récord roto**: festejo corto con aviso claro (`Celebration::Kind::Record`).
- **Mapa de calor**: selector de métrica (horas, pomodoros, objetivo, indebido, inactivo), escala por cuartiles, puntito = objetivo cumplido, leyenda, hover y click por día.
- **Gráfico del mes**: letra del día, línea de promedio, hover y click con el detalle del día.
- **Evolución** (nueva pestaña en Estadísticas): horas con media móvil de 7 días + regresión + últimas 4 semanas vs 4 anteriores, recreo indebido, horas inactivas, metas semanales y marcas (historial desde que existe la versión, tabla `weekly_goals`), ánimo del día. Lógica en `core/Insights`.
- **Diario del día**: en el panel Objetivo, nota libre + "¿Cómo te va hoy?" (1–5); también en la tarjeta del día y en el celu (hoja del objetivo). `days.note` / `days.mood`, sincronizados. Esquema v9.
- **Primera apertura del día**: abrir la app a mano ahora muestra la tarjeta enseguida (antes dependía del reloj de 30 s). `arranque.log` registra la línea "inicio (a mano/automático)" con los ms hasta tener la ventana.

## Mejoras 4.1 (beta 6, PC)
- **Anotador**: dentro del panel Objetivo, el botón *Anotador ▸* despliega una aleta pegada al borde derecho de la ventana (crece hacia afuera; si la pantalla no deja lugar, corre la ventana a la izquierda) con una hoja libre por día (lo que querés hacer, cómo salió, lo que sea) con el "¿Cómo te va hoy?" (1–5) abajo. Usa `days.note` / `days.mood` (sincronizados, el celu lo ve). *Ajustes › General › Anotador desplegable* apagado devuelve el diario dentro del panel, como en la beta 5.
- **Ánimo al terminar la sesión**: «Me voy» pregunta cómo te fue si todavía no lo cargaste (se apaga en *Ajustes › General*).
- **Detalle solo con click**: pasar el mouse por Estadísticas ya no muestra textos (queda el resaltado y la manito); el click abre el detalle. En Evolución, el click en una barra o punto muestra su dato. El botón ⓘ (interruptor, se recuerda) vuelve a activar la info al pasar el mouse. Los cuadraditos de la grilla conservan su hover.
- **Vista simple / avanzada** (botón junto a la actividad, arranca en Simple y se recuerda, `stats_advanced`): la simple muestra estudio, recreos, racha, objetivos, meta semanal, marca de hoy y récords, el gráfico (el promedio solo como dato) y las actividades; la avanzada suma la línea de promedio, recreos indebidos, horas inactivas, nivel global, el selector del mapa de calor y la pestaña Evolución.
- **Ajustes ordenados**: grupos con títulos (Tiempos y racha, Objetivo y diario, Inicio, Atajos y avisos, Copia de seguridad…), lo menos usado en grupos plegables (*Avanzado*, *Pasos y sensibilidad*, *Tamaño*) y textos más cortos.

## Mejoras 5.0 (beta 7, PC) — tanda 5.0
- **Avisos**: la info extra del hover apagada ya no dice "(queda el click)"; los avisos simples duran 4 s (los de "Deshacer", 8 s) y se hacen **cola** (máx. 3, sin repetidos): no se pisan entre sí ni tapan un "Deshacer".
- **Foco**: los avisos automáticos (fin de pomodoro/recreo, hito) ya no roban el foco ni dejan pasar teclas sueltas (Espacio/N/M) durante 4 s: corrige el recreo que arrancaba solo, el pedido de nota y el mini reproductor inesperados.
- **Próximo pomodoro** = minutos restantes del próximo cuadradito fantasma (16 → el reloj arranca en 16, no en 25).
- **Marcar a mano** (click derecho en un fantasma): usa los minutos y la actividad de ese cuadradito.
- **Estadísticas**: racha grande arriba; "Foco %" reemplaza al recreo indebido en la cabecera (el detalle del indebido sigue en el día y en Avanzada); bloque de actividades más bajo, scrolleable, con selector "Mostrar ▾" de cuáles se ven (`stats_hidden_acts`).
- **Mes**: en la pestaña Mes se alterna *Calendario | Últimos 30 días*; el valor por defecto se elige en Ajustes (`month_mode`).
- **Barra de arriba**: un solo botón de vista (día/semana/mes); el tamaño de los cuadraditos pasa al menú ⋯ (`size_button` apagado por defecto).
- **Evolución**: cuatro fichas con número grande y variación "↑ 12 % vs 28 días antes" (estudio, pomodoros, indebido, inactivo), tarjetas más bajas y sin bordes, titular en lenguaje natural más grande.
- Pendiente de la tanda: color con significado y menos líneas en el resto de la app. Siguen 5.1 (cantidad de sesiones, rutina) y 5.2 (personalizar paneles).
- **Menú ⋯ más corto**: se sacaron *Hitos* (ahora solo en Ajustes › Hitos) y *Actividades* (tecla A, menú de actividad › administrar, o *Ajustes › General › Abrir actividades…*). *Ajustes* queda al final, debajo de *Ver tutorial* y arriba de *Salir*.
- **Menos líneas**: los campos del panel Objetivo, el anotador, el ánimo y las pastillas de tema pierden el borde (se distinguen por el fondo); el borde aparece solo al editar. Colores: rojo = pomodoro, verde agua = recreo, dorado = hitos (sin cambios de paleta).

## Mejoras 5.1 (beta 8, PC)
- **Cantidad de sesiones**: una sesión va del primer pomodoro hasta «Me voy» o una pausa larga (*Ajustes › General › Una pausa más larga corta la sesión*, 60 min por defecto, `session_gap_min`). Cada «Me voy» queda guardado (`session_ends`, esquema v11, solo local); las sesiones se calculan desde los pomodoros (`insights::sessions`), así que también valen para el historial: cuentan en el día en que empezaron y respetan el filtro por actividad. Se ve en la vista avanzada (tarjeta *Sesiones*, con duración promedio y detalle con las últimas) y como renglón en el detalle de cada día.
- **Rutina** (*Ajustes › Rutina*, tabla `routine_events`, `core/Routine`): actividades por fuera de la app con nombre, categoría (Facultad, Trabajo, Gimnasio, Médico, Traslado, Comida, Otro o la que escribas), hora desde/hasta (cruza la medianoche si «hasta» < «desde»), repetición por días de la semana o un solo día. No avisa nada. Se guarda al instante. Sirve para explicar lo inactivo: el detalle de *Horas inactivas* y el de cada día reparten el tiempo en «Facultad 4 h · Gimnasio 1 h · sin motivo 4 h», sin contar dos veces lo que coincide con un pomodoro o recreo. Solo en la PC por ahora (no se sincroniza con el celu).
- Pendiente: mostrar la rutina como bandas en la vista semana, y pasarla al celu.

## Mejoras 5.2 (beta 9, PC)
- **Personalizar estadísticas**: botón *Personalizar* en la vista avanzada (en la simple no existe). Cuatro paneles: *Resumen*, *Hitos y objetivos*, *Gráfico* y *Actividades*. Cada uno se puede **ocultar/mostrar**, **subir/bajar** y tiene tres tamaños **S / M / L**: Resumen (alto de las tarjetas), Hitos (ancho mínimo → cuántos por fila), Gráfico (alto de las barras), Actividades (3 / 6 / 12 filas antes del scroll). **Restablecer** vuelve al orden y tamaños originales; **Listo** sale del modo edición. Se guarda en `stats_layout` (`id:visible:tamaño`, p. ej. `hero:1:1,chips:1:1,chart:1:1,folders:1:1`). Vale también para la vista simple (lo que ocultes o reordenes sigue igual), pero solo se edita desde la avanzada.
- Los paneles ocultos solo aparecen (apagados, con «Mostrar») mientras editás.

## Mejoras 6.0 (beta 10, PC)
- **Volver a la vista principal**: desde Estadísticas o Actividades, el botón *acercar* de la barra vuelve a la vista principal (tooltip «Volver a la vista principal»).
- **Tres vistas de actividad** (el selector de abajo de la grilla, `live_scope`): **Esta actividad** (solo sus pomodoros) → **Todo el día · por actividad** (todo, separado por actividad aunque en Ajustes esté sin agrupar) → **Todo el día · junto** (todo, sin separar: menos ruido). Rota con un click y se recuerda.
- **Personalizar estadísticas, por tarjeta**: se saca el botón *Personalizar*; ahora está en el menú ⋯ (*Personalizar estadísticas*, solo dentro de Estadísticas). Cada tarjeta suelta (Racha, Estudio, Foco %, Recreos, Recreo indebido, Horas inactivas, Sesiones, Objetivos, Marca de hoy, Meta semanal, Récords, Nivel global) tiene una ✕ para sacarla y *＋ Tarjetas* para agregarlas; las franjas siguen con ocultar / ▲▼ / S-M-L. **La vista simple y la avanzada tienen cada una su propio diseño** (`stats_cards_simple|adv`, `stats_layout_simple|adv`); *Restablecer* vuelve la vista actual a sus valores. Simple arranca con Racha, Estudio, Foco, Objetivos, Marca, Meta semanal y Récords; Avanzada, con todas.
- **Rutina en semana y mes**: botón *Rutina* en la barra de fecha (solo semana y mes, `routine_layer`) que muestra los eventos como barritas de color por categoría (hora + nombre en semana; «+N más» si no entran). Click derecho en un día › *Agregar a la rutina…*; click en una barrita la edita; click derecho en ella, editar o agregar otro. Ventanita estilo Google Calendar (título grande, categoría con color, horario, repetir: no se repite / todos los días / lunes a viernes / cada semana / personalizar, «termina el…», borrar). Al guardar, la capa se prende sola. Lo mismo se usa desde *Ajustes › Rutina*.
- **Libreta** (anotador permanente): la aleta de la derecha ahora tiene dos solapas, **Diario** (la hoja del día, como antes) y **Libreta** (`L` o ⋯ › Libreta): páginas con título y texto que quedan hasta que las borres, con buscador, fijar páginas arriba, casillas (`- [ ]` se tilda con un click) y exportar todo a .txt. Se guarda sola (tabla `notebook_pages`, esquema v12), entra en la copia de seguridad y no depende del día. Solo PC por ahora.

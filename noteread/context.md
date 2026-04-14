# NoteRead — Contexto del proyecto

## Petición original
Crear una app de entrenamiento de lectura musical dentro de lukeda.me.
El usuario ve una nota en el pentagrama y debe identificarla (en español: Do, Re, Mi, Fa, Sol, La, Si) lo más rápido posible.
Objetivo: automatizar el reconocimiento de notas hasta que sea instantáneo.

## Ampliaciones pedidas
- **Cambio de clave**: Sol (treble) y Fa (bass)
- **Alteraciones**: Sostenidos (♯), Bemoles (♭), o ambos
- Más ajustes a discreción del equipo de desarrollo

---

## Features a implementar (spec completa)

### 1. Claves
- **Clave de Sol** (treble): notas Do C4 – Sol G5 (con línea adicional inferior)
- **Clave de Fa** (bass): notas Sol G2 – Do C4 (con línea adicional superior)
- Selector en panel de configuración: [Sol] [Fa]
- Ambas claves usan el mismo SVG con staff en y=40,54,68,82,96

### 2. Alteraciones
- Modos: Ninguna / Sostenidos ♯ / Bemoles ♭ / Ambas
- Cuando está activo: 40% de probabilidad de que la nota mostrada tenga alteración
- La alteración se dibuja en SVG a la izquierda de la nota
- La respuesta sigue siendo el nombre base (Do, Re...) — la alteración es visual training
- Símbolos: ♯ (U+266F) y ♭ (U+266D)

### 3. Rango / Dificultad
- **Fácil**: Solo notas sobre las líneas del pentagrama (sin líneas adicionales), sin alteraciones
- **Medio**: Rango completo (incluye líneas adicionales), sin alteraciones
- **Difícil**: Rango completo + alteraciones disponibles

### 4. Modo de juego
- **Libre**: Sin límite de tiempo. Practica indefinidamente. (default)
- **Contrarreloj 60s**: Cuenta atrás de 60 segundos. Al terminar, overlay con puntuación final.

### 5. Sonido
- Web Audio API: toca la frecuencia real de la nota al aparecer
- Onda seno + envelope: attack 10ms, decay 800ms
- Toggle ON/OFF en settings
- Accidentales modifican la frecuencia: × 2^(1/12) para ♯, × 2^(-1/12) para ♭
- No requiere ninguna librería externa

### 6. Estadísticas
- Correctas / Errores / Racha actual / Precisión % / Tiempo promedio
- Mejor racha (best streak) persistida en localStorage (clave: `nr_best`)
- Configuración persistida en localStorage (clave: `nr_settings`)
- Reset limpia sesión pero NO borra el best streak

### 7. Modo Aprendizaje
- Toggle en settings
- Cuando está ON: antes de aceptar respuesta, el nombre de la nota aparece brevemente en el SVG (0.6s), luego desaparece y empieza a contar el tiempo
- Útil para principiantes que aún no memorizaron las posiciones

### 8. Focus en notas débiles
- Trackea aciertos/fallos por nota individualmente en la sesión
- En modo Libre, si hay notas con < 50% de acierto y >= 3 intentos, aparecen con el doble de frecuencia (weighted random)

---

## Arquitectura SVG

### Staff (ambas claves)
```
viewBox="0 0 500 148"
Líneas del pentagrama: y = 40, 54, 68, 82, 96
Staff x: 92 → 470
Treble clef: <text x="4" y="108" font-size="88">𝄞</text>
Bass clef:   <text x="90" y="82" font-size="44">𝄢</text>
Note X position: 290
```

### Notas Clave de Sol (TREBLE)
```
Do C4  y=110  ledger below (línea adicional inferior)
Re D4  y=103
Mi E4  y=96   (línea 1)
Fa F4  y=89
Sol G4 y=82   (línea 2)
La A4  y=75
Si B4  y=68   (línea 3)
Do C5  y=61
Re D5  y=54   (línea 4)
Mi E5  y=47
Fa F5  y=40   (línea 5)
Sol G5 y=33   (espacio sobre línea 5 — sin línea adicional)
```

### Notas Clave de Fa (BASS)
```
Sol G2 y=96   (línea 1)
La A2  y=89
Si B2  y=82   (línea 2)
Do C3  y=75
Re D3  y=68   (línea 3 — middle line)
Mi E3  y=61
Fa F3  y=54   (línea 4 — characteristic F line)
Sol G3 y=47
La A3  y=40   (línea 5)
Si B3  y=33   (espacio sobre línea 5)
Do C4  y=26   (línea adicional superior — Middle C)
```

### Frecuencias
```
G2=98.00 A2=110.00 B2=123.47 C3=130.81 D3=146.83
E3=164.81 F3=174.61 G3=196.00 A3=220.00 B3=246.94
C4=261.63 D4=293.66 E4=329.63 F4=349.23 G4=392.00
A4=440.00 B4=493.88 C5=523.25 D5=587.33 E5=659.25
F5=698.46 G5=783.99
```

---

## UI Structure

```
sysbar
header-panel
navbar

[settings panel — collapsible]
  ├── Clave:        [Sol] [Fa]
  ├── Alteraciones: [Ninguna] [♯] [♭] [♯♭]
  ├── Dificultad:   [Fácil] [Medio] [Difícil]
  ├── Modo:         [Libre] [Contrarreloj]
  ├── Sonido:       [ON/OFF]
  └── Aprendizaje:  [ON/OFF]

[game panel]
  ├── stat-bar (Correctas / Errores / Racha / Precisión / T.prom)
  ├── timer-bar (solo en modo contrarreloj — se vacía en 60s)
  ├── staff-panel (SVG)
  ├── feedback-bar (3px strip)
  ├── note-buttons (7 botones)
  └── hint-area

[score overlay — solo al terminar contrarreloj]

ticker-bar
footbar
```

---

## Aesthetic
- Fondo: `#08070f` → `#050408`
- Acento primario: `#818cf8` (indigo-400)
- Acento secundario: `#c4b5fd` (violet-300)
- Correcto: `#4ade80`
- Error: `#f87171`
- Panel borders: `rgba(129,140,248,0.14-0.18)`
- Panel bg: `#0a091a`
- Fuente monoespaciada: Courier New
- Fuente principal: Verdana, Tahoma

## Keyboard shortcuts
```
1–7   → Do Re Mi Fa Sol La Si
R     → Reset / restart
Space → (en modo contrarreloj) iniciar sesión
S     → toggle sonido
```

---

## localStorage keys
- `nr_settings` — JSON con { clef, accidentals, difficulty, mode, sound, learning }
- `nr_best` — número entero (mejor racha global)

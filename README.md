# Chachalaca

**Sonidos y cambio de voz en tu micrófono, gratis.** Para Discord y para los juegos con chat de voz.
Sin publicidad, sin cuentas y sin recoger tus datos. Hecho por [Brizna Soft](https://brizna-soft.github.io).

La chachalaca es el pájaro más escandaloso del monte: este programa hace que tus amigos te escuchen igual.

## Qué hace

- **Tablero de sonidos:** trae 16 sonidos hechos por Brizna Soft (trombón triste, bocina, wiii, boing, ba dum tss,
  grillos, redoble, sirena, láser, explosión y más). Agrega los tuyos (WAV, MP3 o FLAC) y tócalos con un clic o con
  un atajo de teclado, incluso dentro de un juego. La tecla **Pausa** detiene todos.
- **Cambio de voz en vivo:** Grave, Aguda, Ardilla, Monstruo, Duende, Ratón, Caricatura, Robot, Autotune,
  Robot autotune, Eco, Radio y Cueva.
- **Micrófono limpio:** quita el ruido de fondo y, si quieres, calla tu micrófono cuando no hablas.
- **Tú decides qué escuchas:** puedes oír los sonidos y tu voz cambiada en tus audífonos, o no.
- Mezcla tu voz con los sonidos, así que tus amigos oyen las dos cosas a la vez.

## Lo que necesitas

- Windows 10 u 11 de 64 bits. Nada más: el micrófono virtual viene adentro.

## El micrófono virtual (VB-CABLE)

Para que tus amigos te escuchen, Chachalaca usa **VB-CABLE**, el micrófono virtual gratis de
[VB-Audio](https://vb-cable.com). Viene dentro del .zip, tal cual lo publica VB-Audio, y se instala con un botón.
VB-CABLE es de VB-Audio, no de Brizna Soft, y es **donationware**: si te sirve, puedes
[apoyar a su autor](https://vb-audio.com/Cable/). VB-Audio permite incluirlo así en programas gratis
([sus condiciones](https://vb-audio.com/Services/licensing.htm)).

## Cómo se usa

1. Baja Chachalaca de [Releases](https://github.com/Brizna-Soft/chachalaca/releases/latest), descomprime el .zip y
   abre `Chachalaca.exe`. No hay que instalarlo.
2. Si Windows dice «Windows protegió su PC», pulsa «Más información» y luego «Ejecutar de todas formas».
   Sale porque el programa es nuevo y todavía no tiene firma digital.
3. Si falta el micrófono virtual, pulsa **«Instalar el micrófono virtual»** y acepta los avisos de Windows.
   Al instalarse, Windows a veces pone el cable como su salida de sonido y dejas de oír tus juegos: Chachalaca te
   avisa y te abre la página de Sonido para que elijas tus audífonos otra vez.
4. En Chachalaca, revisa que «Micrófono virtual» diga **CABLE Input**.
5. En **Discord**: Ajustes de usuario → Voz y video → Dispositivo de entrada → **CABLE Output**.
   Pon «Supresión de ruido» en **Ninguna**; si no, Discord borra los sonidos y la música.
6. En **tu juego**: en las opciones de chat de voz, elige **CABLE Output** como micrófono.

## Preguntas

- **No me escuchan:** revisa que en Discord o en el juego esté elegido «CABLE Output» y que en Chachalaca esté
  marcado «Mandar mi voz».
- **Me oigo con eco:** desmarca «Escucharme» o usa audífonos.
- **Los sonidos se cortan en Discord:** apaga la supresión de ruido de Discord.
- **¿Se conecta a internet?** Solo si pulsas «Ayuda → Buscar actualización». No tiene publicidad ni telemetría.

## Para quien programa

C++20, wxWidgets, [miniaudio](https://github.com/mackron/miniaudio) para el audio (WASAPI) y
[Signalsmith Stretch](https://github.com/Signalsmith-Audio/signalsmith-stretch) para cambiar el tono.
Se compila con `compilar.ps1` (Visual Studio 2022 y vcpkg). Los avisos de licencia de las bibliotecas están en
`AVISOS-DE-TERCEROS.txt`.

## Licencia

MIT, © 2026 Brizna Soft. Hecho con ayuda de IA.
¿Algo no funciona o tienes una idea? [Cuéntanos aquí](https://github.com/Brizna-Soft/chachalaca/issues/new/choose).

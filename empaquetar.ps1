# Arma publicar\Chachalaca-<versión>-x64.zip (programa y licencias) y su .sha256.
# Antes hay que compilar con compilar.ps1.
$ErrorActionPreference = 'Stop'
$raiz = $PSScriptRoot
$version = (Get-Content (Join-Path $raiz 'version.txt') -TotalCount 1).Trim()
$programa = Join-Path $raiz 'programa'
$publicar = Join-Path $raiz 'publicar'
$nombre = "Chachalaca-$version-x64"
$temporal = Join-Path $publicar $nombre

if (-not (Test-Path (Join-Path $programa 'Chachalaca.exe'))) { throw "Falta Chachalaca.exe en ${programa}. Compila primero." }
& (Join-Path $raiz 'generar-avisos.ps1')

if (Test-Path $temporal) { Remove-Item -Recurse -Force $temporal }
New-Item -ItemType Directory -Force $temporal | Out-Null
Copy-Item (Join-Path $programa 'Chachalaca.exe') $temporal
if (-not (Test-Path (Join-Path $programa 'sonidos\trombon-triste.wav'))) { throw 'Faltan los sonidos en programa\sonidos. Compila primero.' }
Copy-Item (Join-Path $programa 'sonidos') (Join-Path $temporal 'sonidos') -Recurse
Copy-Item (Join-Path $raiz 'LICENSE') (Join-Path $temporal 'LICENCIA.txt')
Copy-Item (Join-Path $raiz 'AVISOS-DE-TERCEROS.txt') $temporal

# VB-CABLE, el micrófono virtual de VB-Audio, tal cual lo publican. VB-Audio permite incluirlo en programas gratis si se
# dice que es suyo y que es donationware (https://vb-audio.com/Services/licensing.htm). La huella se fija aquí: si
# VB-Audio publica otro paquete, la compilación se detiene hasta revisarlo.
$vbUrl = 'https://download.vb-audio.com/Download_CABLE/VBCABLE_Driver_Pack45.zip'
$vbHuella = 'B950E39F01AF1D04EA623C8F6D8EB9B6EA5C477C637295FABF20631C85116BFB'
$vbZip = Join-Path $raiz 'vendor\VBCABLE_Driver_Pack45.zip'
if (-not (Test-Path $vbZip)) {
    New-Item -ItemType Directory -Force (Split-Path $vbZip) | Out-Null
    Invoke-WebRequest -Uri $vbUrl -OutFile $vbZip -UseBasicParsing -Headers @{ 'User-Agent' = 'Mozilla/5.0' }
}
if ((Get-FileHash $vbZip -Algorithm SHA256).Hash -ne $vbHuella) { throw 'El paquete de VB-CABLE no es el esperado (cambió su huella). Revísalo antes de incluirlo.' }
Expand-Archive -Path $vbZip -DestinationPath (Join-Path $temporal 'VB-CABLE')
$notaVB = @"
VB-CABLE, el micrófono virtual
==============================

VB-CABLE es un programa de VB-Audio Software (Vincent Burel), no de Brizna Soft: https://vb-cable.com
Es donationware: se usa gratis y, si te sirve, puedes apoyar a su autor en https://vb-audio.com/Cable/

Va en la carpeta VB-CABLE tal cual lo publica VB-Audio, sin cambios. Chachalaca lo instala con el botón
"Instalar el micrófono virtual". Para quitarlo: abre VB-CABLE\VBCABLE_Setup_x64.exe como administrador,
pulsa "Remove Driver" y reinicia la PC.
"@
[System.IO.File]::WriteAllText((Join-Path $temporal 'VB-CABLE-es-de-VB-Audio.txt'), $notaVB, (New-Object System.Text.UTF8Encoding($true)))

$leeme = @"
Chachalaca $version - sonidos y cambio de voz en tu micrófono, gratis, de Brizna Soft
==================================================================================

1. Abre Chachalaca.exe (no hace falta instalarlo).
2. Si Windows dice "Windows protegió su PC": pulsa "Más información" y luego "Ejecutar de todas formas".
   Sale porque el programa es nuevo y todavía no tiene firma digital.
3. Si falta el micrófono virtual, pulsa "Instalar el micrófono virtual". Viene adentro: es VB-CABLE, de VB-Audio.
   Windows te pide permiso de administrador. Si después no oyes tus juegos, elige tus audífonos en
   Configuración > Sonido > Salida (Chachalaca te avisa).
4. En Discord o en tu juego, elige "CABLE Output" como micrófono.
   En Discord, apaga la supresión de ruido para que se escuchen bien los sonidos (Chachalaca trae la suya).
5. Trae 16 sonidos hechos por Brizna Soft (carpeta "sonidos"). Para agregar los tuyos, arrástralos a la ventana.

Ayuda, novedades y reportes de problemas: https://github.com/Brizna-Soft/chachalaca
Sin publicidad, sin virus y sin recoger tus datos. Licencia MIT.
"@
[System.IO.File]::WriteAllText((Join-Path $temporal 'LEEME.txt'), $leeme, (New-Object System.Text.UTF8Encoding($true)))

$zip = Join-Path $publicar "$nombre.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path (Join-Path $temporal '*') -DestinationPath $zip
Remove-Item -Recurse -Force $temporal
$huella = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLower()
[System.IO.File]::WriteAllText("$zip.sha256", "$huella  $nombre.zip`n", (New-Object System.Text.UTF8Encoding($false)))
Write-Host "Listo: $zip"
Write-Host "SHA256: $huella"

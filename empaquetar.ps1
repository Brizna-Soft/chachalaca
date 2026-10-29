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
Copy-Item (Join-Path $raiz 'LICENSE') (Join-Path $temporal 'LICENCIA.txt')
Copy-Item (Join-Path $raiz 'AVISOS-DE-TERCEROS.txt') $temporal
$leeme = @"
Chachalaca $version - sonidos y cambio de voz en tu micrófono, gratis, de Brizna Soft
==================================================================================

1. Instala el micrófono virtual gratis VB-CABLE: https://vb-audio.com/Cable/
   (lo hace la empresa VB-Audio; Chachalaca lo usa para que tus amigos te escuchen).
2. Abre Chachalaca.exe (no hace falta instalarlo).
3. Si Windows dice "Windows protegió su PC": pulsa "Más información" y luego "Ejecutar de todas formas".
   Sale porque el programa es nuevo y todavía no tiene firma digital.
4. En Discord o en tu juego, elige "CABLE Output" como micrófono.
   En Discord, apaga la supresión de ruido para que se escuchen bien los sonidos.

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

# Dibuja el ícono de Chachalaca: cuadrado redondeado café rojizo (el color de la chachalaca) con un pico
# abierto blanco que suelta tres ondas de sonido.
# Genera res\app.ico (varios tamaños, PNG dentro).
#
# Uso: powershell -ExecutionPolicy Bypass -File res\crear-icono.ps1
param(
    [string]$Raiz = (Split-Path $PSScriptRoot -Parent)
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$fondo = [System.Drawing.Color]::FromArgb(255, 0xA8, 0x4A, 0x23)
$blanco = [System.Drawing.Color]::White

function Redondeado([single]$x, [single]$y, [single]$w, [single]$h, [single]$r) {
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = 2 * $r
    $p.AddArc($x, $y, $d, $d, 180, 90)
    $p.AddArc($x + $w - $d, $y, $d, $d, 270, 90)
    $p.AddArc($x + $w - $d, $y + $h - $d, $d, $d, 0, 90)
    $p.AddArc($x, $y + $h - $d, $d, $d, 90, 90)
    $p.CloseFigure()
    return $p
}

function Dibujar([int]$lado) {
    $bmp = New-Object System.Drawing.Bitmap($lado, $lado, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.Clear([System.Drawing.Color]::Transparent)
    $k = $lado / 100.0

    $margen = if ($lado -le 20) { 0 } else { 4 * $k }
    $caja = Redondeado $margen $margen ($lado - 2 * $margen) ($lado - 2 * $margen) (20 * $k)
    $g.FillPath((New-Object System.Drawing.SolidBrush($fondo)), $caja)

    $pincel = New-Object System.Drawing.SolidBrush($blanco)
    # Cabeza redonda con el ojo
    $g.FillEllipse($pincel, [single](14 * $k), [single](34 * $k), [single](32 * $k), [single](32 * $k))
    if ($lado -gt 20) {
        $g.FillEllipse((New-Object System.Drawing.SolidBrush($fondo)), [single](30 * $k), [single](42 * $k), [single](7 * $k), [single](7 * $k))
    }
    # Pico abierto hacia la derecha
    $arriba = [System.Drawing.PointF[]]@(
        (New-Object System.Drawing.PointF([single](40 * $k), [single](40 * $k))),
        (New-Object System.Drawing.PointF([single](64 * $k), [single](38 * $k))),
        (New-Object System.Drawing.PointF([single](43 * $k), [single](49 * $k))))
    $abajo = [System.Drawing.PointF[]]@(
        (New-Object System.Drawing.PointF([single](43 * $k), [single](52 * $k))),
        (New-Object System.Drawing.PointF([single](61 * $k), [single](63 * $k))),
        (New-Object System.Drawing.PointF([single](40 * $k), [single](60 * $k))))
    $g.FillPolygon($pincel, $arriba)
    $g.FillPolygon($pincel, $abajo)

    # Ondas de sonido que salen del pico
    $grosor = [single][Math]::Max(1.5, 5.5 * $k)
    $lapiz = New-Object System.Drawing.Pen($blanco, $grosor)
    $lapiz.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $lapiz.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    foreach ($r in 14, 24) {
        $g.DrawArc($lapiz, [single](62 * $k - $r * $k), [single](50 * $k - $r * $k), [single](2 * $r * $k), [single](2 * $r * $k), -45, 90)
    }

    $g.Dispose()
    return $bmp
}

function Png($bmp) {
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    return , $ms.ToArray()
}

$tamanos = 16, 20, 24, 32, 40, 48, 64, 96, 128, 256
$imagenes = New-Object 'System.Collections.Generic.List[byte[]]'
foreach ($t in $tamanos) { $imagenes.Add((Png (Dibujar $t))) }
$salida = New-Object System.IO.MemoryStream
$w = New-Object System.IO.BinaryWriter($salida)
$w.Write([uint16]0); $w.Write([uint16]1); $w.Write([uint16]$tamanos.Count)
$desplazamiento = 6 + 16 * $tamanos.Count
for ($i = 0; $i -lt $tamanos.Count; $i++) {
    $t = $tamanos[$i]
    $lado = if ($t -ge 256) { 0 } else { $t }
    $w.Write([byte]$lado); $w.Write([byte]$lado); $w.Write([byte]0); $w.Write([byte]0)
    $w.Write([uint16]1); $w.Write([uint16]32)
    $w.Write([uint32]$imagenes[$i].Length); $w.Write([uint32]$desplazamiento)
    $desplazamiento += $imagenes[$i].Length
}
foreach ($img in $imagenes) { $w.Write($img) }
$w.Flush()
[System.IO.File]::WriteAllBytes((Join-Path $Raiz 'res\app.ico'), $salida.ToArray())
[System.IO.File]::WriteAllBytes((Join-Path $Raiz 'res\icono-256.png'), (Png (Dibujar 256)))
Write-Host 'Ícono listo.'

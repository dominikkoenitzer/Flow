# Icon Generator for FLOW (Windows)
# Regenerates the app icon (Flow.ico) and the hero brand image (Flow.png) from
# code, so the mark is reproducible/editable rather than an opaque binary blob.
#
# The mark: a white fast-forward double-chevron on the brand-blue squircle tile
# (#3B82F6 -> #2563EB, matching ACCENT_HOVER -> ACCENT_PRIMARY in src/main.cpp).
# Pure GDI+ via System.Drawing -- no external tools or image editors needed.
#
# Usage:
#   .\scripts\make-icon.ps1                 # writes Flow.ico + Flow.png to repo root
#   .\scripts\make-icon.ps1 -HeroSize 1024  # larger hero PNG
#
# After regenerating, recompile the resource object so the exe picks it up:
#   windres resource.rc -O coff -o build/resource.o   (or just run scripts\build.ps1)
param(
    [int]$HeroSize = 512,
    [string]$OutIco  = '',
    [string]$OutHero = ''
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$ProjectRoot = Split-Path -Parent $PSScriptRoot
if ($OutIco  -eq '') { $OutIco  = Join-Path $ProjectRoot 'Flow.ico' }
if ($OutHero -eq '') { $OutHero = Join-Path $ProjectRoot 'Flow.png' }

# --- Brand colors -----------------------------------------------------------
$ColTop    = [System.Drawing.Color]::FromArgb(255, 59, 130, 246)   # #3B82F6 blue-500
$ColBottom = [System.Drawing.Color]::FromArgb(255, 37,  99, 235)   # #2563EB blue-600
$ColWhite  = [System.Drawing.Color]::FromArgb(255,255,255,255)

# --- Geometry helpers (coords are fractions of the icon size S) -------------
function New-RoundedRectPath([single]$x,[single]$y,[single]$w,[single]$h,[single]$r) {
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = $r * 2
    $p.AddArc($x,       $y,       $d, $d, 180, 90)
    $p.AddArc($x+$w-$d, $y,       $d, $d, 270, 90)
    $p.AddArc($x+$w-$d, $y+$h-$d, $d, $d,   0, 90)
    $p.AddArc($x,       $y+$h-$d, $d, $d,  90, 90)
    $p.CloseFigure()
    $p
}

function Draw-Tile($g, [int]$S) {
    $rect = New-Object System.Drawing.RectangleF(0, 0, $S, $S)
    $tile = New-RoundedRectPath 0 0 $S $S ([single]($S * 0.225))   # iOS-ish squircle
    $grad = New-Object System.Drawing.Drawing2D.LinearGradientBrush($rect, $ColTop, $ColBottom, [System.Drawing.Drawing2D.LinearGradientMode]::ForwardDiagonal)
    $g.FillPath($grad, $tile)
    # subtle top sheen
    $g.SetClip($tile)
    $sheenRect = New-Object System.Drawing.RectangleF(0, 0, $S, [single]($S * 0.6))
    $sg = New-Object System.Drawing.Drawing2D.LinearGradientBrush($sheenRect, [System.Drawing.Color]::FromArgb(40,255,255,255), [System.Drawing.Color]::FromArgb(0,255,255,255), [System.Drawing.Drawing2D.LinearGradientMode]::Vertical)
    $g.FillRectangle($sg, $sheenRect)
    $g.ResetClip()
    $grad.Dispose(); $sg.Dispose(); $tile.Dispose()
}

# Fast-forward double chevron: bold stroke, clear ~1-stroke channel between the
# two chevrons (holds at 16px), ~90 deg apex, rounded caps, pair nudged 1% left.
function Draw-Chevron($g, [int]$S) {
    $stroke = 0.110; $w = 0.195; $h = 0.205; $spacing = 0.215; $startX = 0.285; $ym = 0.5
    $pen = New-Object System.Drawing.Pen($ColWhite, [single]($S * $stroke))
    $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.EndCap   = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    foreach ($c in 0,1) {
        $xl = $startX + $c * $spacing
        $pts = [System.Drawing.PointF[]]@(
            (New-Object System.Drawing.PointF([single](($xl)*$S),      [single](($ym-$h)*$S))),
            (New-Object System.Drawing.PointF([single](($xl+$w)*$S),   [single](($ym)*$S))),
            (New-Object System.Drawing.PointF([single](($xl)*$S),      [single](($ym+$h)*$S)))
        )
        $g.DrawLines($pen, $pts)
    }
    $pen.Dispose()
}

function New-IconBitmap([int]$S) {
    $bmp = New-Object System.Drawing.Bitmap($S, $S, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode     = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.InterpolationMode  = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.PixelOffsetMode    = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.Clear([System.Drawing.Color]::Transparent)
    Draw-Tile $g $S
    Draw-Chevron $g $S
    $g.Dispose()
    $bmp
}

# --- Assemble multi-resolution .ico (PNG-compressed entries) ----------------
$sizes = @(16, 24, 32, 48, 64, 128, 256)
$entries = foreach ($s in $sizes) {
    $bmp = New-IconBitmap $s
    $ms  = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    [pscustomobject]@{ Size = $s; Bytes = $ms.ToArray() }
    $ms.Dispose(); $bmp.Dispose()
}

$out = New-Object System.IO.MemoryStream
$bw  = New-Object System.IO.BinaryWriter($out)
$bw.Write([uint16]0); $bw.Write([uint16]1); $bw.Write([uint16]$entries.Count)   # ICONDIR
$offset = 6 + 16 * $entries.Count
foreach ($e in $entries) {
    $dim = if ($e.Size -ge 256) { 0 } else { $e.Size }   # 0 == 256 in the dir entry
    $bw.Write([byte]$dim); $bw.Write([byte]$dim); $bw.Write([byte]0); $bw.Write([byte]0)
    $bw.Write([uint16]1); $bw.Write([uint16]32)
    $bw.Write([uint32]$e.Bytes.Length); $bw.Write([uint32]$offset)
    $offset += $e.Bytes.Length
}
foreach ($e in $entries) { $bw.Write($e.Bytes) }
$bw.Flush()
[System.IO.File]::WriteAllBytes($OutIco, $out.ToArray())
$bw.Dispose(); $out.Dispose()
Write-Host "Wrote $OutIco ($($entries.Count) sizes: $($sizes -join ', '))" -ForegroundColor Green

# --- Hero PNG ---------------------------------------------------------------
$hero = New-IconBitmap $HeroSize
$hero.Save($OutHero, [System.Drawing.Imaging.ImageFormat]::Png)
$hero.Dispose()
Write-Host "Wrote $OutHero (${HeroSize}px)" -ForegroundColor Green

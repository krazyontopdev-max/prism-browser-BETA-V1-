$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$images = @()
foreach ($size in 16,24,32,48,64,128,256) {
    $bitmap = New-Object Drawing.Bitmap ($size * 4),($size * 4)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.Clear([Drawing.Color]::FromArgb(245,245,245))
    $graphics.ScaleTransform(($size * 4.0 / 72),($size * 4.0 / 72))
    $graphics.TranslateTransform(4,4)
    $pen = New-Object Drawing.Pen ([Drawing.Color]::Black),3
    $pen.LineJoin = [Drawing.Drawing2D.LineJoin]::Round
    $pen.StartCap = $pen.EndCap = [Drawing.Drawing2D.LineCap]::Round
    $path = New-Object Drawing.Drawing2D.GraphicsPath
    $path.AddLine(32,5,59,54); $path.AddLine(59,54,5,54); $path.AddLine(5,54,32,5)
    $path.StartFigure(); $path.AddLine(32,5,25,40); $path.AddLine(25,40,5,54)
    $path.StartFigure(); $path.AddLine(25,40,59,54)
    $graphics.DrawPath($pen,$path)
    $small = New-Object Drawing.Bitmap $size,$size
    $scaled = [Drawing.Graphics]::FromImage($small)
    $scaled.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $scaled.DrawImage($bitmap,0,0,$size,$size)
    $stream = New-Object IO.MemoryStream
    $small.Save($stream,[Drawing.Imaging.ImageFormat]::Png)
    $images += @{Size=$size;Bytes=$stream.ToArray()}
    $stream.Dispose(); $scaled.Dispose(); $small.Dispose(); $path.Dispose(); $pen.Dispose(); $graphics.Dispose(); $bitmap.Dispose()
}
$output = New-Object IO.MemoryStream
$writer = New-Object IO.BinaryWriter $output
$writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$images.Count)
$offset = 6 + 16 * $images.Count
foreach ($image in $images) {
    $dimension = if ($image.Size -eq 256) { 0 } else { $image.Size }
    $writer.Write([byte]$dimension); $writer.Write([byte]$dimension)
    $writer.Write([byte]0); $writer.Write([byte]0); $writer.Write([uint16]1); $writer.Write([uint16]32)
    $writer.Write([uint32]$image.Bytes.Length); $writer.Write([uint32]$offset)
    $offset += $image.Bytes.Length
}
foreach ($image in $images) { $writer.Write([byte[]]$image.Bytes) }
$file = Join-Path $PSScriptRoot '..\resources\prism.ico'
[IO.File]::WriteAllBytes($file,$output.ToArray())
$writer.Dispose(); $output.Dispose()
$check = New-Object Drawing.Icon $file
if ($check.Width -lt 16) { throw 'Invalid generated icon.' }
$check.Dispose()
Write-Output 'Generated the existing Prism mark as a multi-resolution Windows icon.'

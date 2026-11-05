Add-Type -AssemblyName System.Drawing
$sizes = @(256, 64)
foreach ($size in $sizes) {
    $bitmap = [Drawing.Bitmap]::new($size, $size)
    $g = [Drawing.Graphics]::FromImage($bitmap)
    $g.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.ScaleTransform($size / 128.0, $size / 128.0)
    $g.Clear([Drawing.Color]::Transparent)
    $path = [Drawing.Drawing2D.GraphicsPath]::new()
    $path.AddArc(4, 4, 56, 56, 180, 90); $path.AddArc(68, 4, 56, 56, 270, 90)
    $path.AddArc(68, 68, 56, 56, 0, 90); $path.AddArc(4, 68, 56, 56, 90, 90)
    $path.CloseFigure()
    $brush = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#0f171c'))
    $g.FillPath($brush, $path)
    $pen = [Drawing.Pen]::new([Drawing.ColorTranslator]::FromHtml('#3fe0c5'), 9)
    $pen.StartCap = $pen.EndCap = [Drawing.Drawing2D.LineCap]::Round
    $pen.LineJoin = [Drawing.Drawing2D.LineJoin]::Round
    $points = @([Drawing.PointF]::new(25,92),[Drawing.PointF]::new(25,37),[Drawing.PointF]::new(44,62),[Drawing.PointF]::new(64,37),[Drawing.PointF]::new(84,62),[Drawing.PointF]::new(103,37),[Drawing.PointF]::new(103,92))
    $g.DrawLines($pen, $points)
    $pen.Color = [Drawing.ColorTranslator]::FromHtml('#8b9ba5'); $pen.Width = 6
    $g.DrawLine($pen, 49, 92, 79, 92)
    $name = if ($size -eq 256) { 'MintVPN.png' } else { 'MintVPN-Tray.png' }
    $bitmap.Save((Join-Path $PSScriptRoot "public/$name"), [Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bitmap.Dispose(); $path.Dispose(); $brush.Dispose(); $pen.Dispose()
}
# Windows ICO may embed a PNG image; 0 indicates the 256-pixel dimensions.
$png = [IO.File]::ReadAllBytes((Join-Path $PSScriptRoot 'public/MintVPN.png'))
$stream = [IO.File]::Create((Join-Path $PSScriptRoot 'MintVPN.ico'))
$writer = [IO.BinaryWriter]::new($stream)
$writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]1)
$writer.Write([byte]0); $writer.Write([byte]0); $writer.Write([byte]0); $writer.Write([byte]0)
$writer.Write([uint16]1); $writer.Write([uint16]32); $writer.Write([uint32]$png.Length); $writer.Write([uint32]22)
$writer.Write($png); $writer.Dispose()

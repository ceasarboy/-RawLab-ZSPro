param([Parameter(Mandatory=$true)][string]$Source,[Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$sourceImage=[Drawing.Image]::FromFile($Source)
try {
    $sizes=@(16,32,48,256)
    $frames=@()
    foreach($size in $sizes) {
        $bitmap=[Drawing.Bitmap]::new($size,$size)
        $graphics=[Drawing.Graphics]::FromImage($bitmap)
        $stream=[IO.MemoryStream]::new()
        try {
            $graphics.InterpolationMode=[Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $graphics.DrawImage($sourceImage,0,0,$size,$size)
            $bitmap.Save($stream,[Drawing.Imaging.ImageFormat]::Png)
            $frames+= ,$stream.ToArray()
        } finally { $stream.Dispose();$graphics.Dispose();$bitmap.Dispose() }
    }
    $writer=[IO.BinaryWriter]::new([IO.File]::Create($Output))
    try {
        $writer.Write([uint16]0);$writer.Write([uint16]1);$writer.Write([uint16]$sizes.Count)
        $offset=6+16*$sizes.Count
        for($i=0;$i -lt $sizes.Count;$i++) {
            $dimension=if($sizes[$i] -eq 256){0}else{$sizes[$i]}
            $writer.Write([byte]$dimension);$writer.Write([byte]$dimension)
            $writer.Write([uint16]0);$writer.Write([uint16]1);$writer.Write([uint16]32)
            $writer.Write([uint32]$frames[$i].Length);$writer.Write([uint32]$offset)
            $offset+=$frames[$i].Length
        }
        foreach($frame in $frames){$writer.Write([byte[]]$frame)}
    } finally {$writer.Dispose()}
} finally {$sourceImage.Dispose()}

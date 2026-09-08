$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$taskRoot = Split-Path -Parent $PSScriptRoot
$taskImage = [Drawing.Image]::FromFile((Join-Path $taskRoot 'assets/ReadyForLaunch.png'))
$taskSizes = @(16,24,32,48,64,128,256)
$taskStreams = [Collections.Generic.List[byte[]]]::new()
try {
  foreach ($taskSize in $taskSizes) {
    $taskBitmap = [Drawing.Bitmap]::new($taskSize,$taskSize,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $taskGraphics = [Drawing.Graphics]::FromImage($taskBitmap)
    $taskMemory = [IO.MemoryStream]::new()
    try {
      $taskGraphics.CompositingMode = [Drawing.Drawing2D.CompositingMode]::SourceCopy
      $taskGraphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
      $taskGraphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
      $taskGraphics.DrawImage($taskImage,0,0,$taskSize,$taskSize)
      $taskBitmap.Save($taskMemory,[Drawing.Imaging.ImageFormat]::Png)
      $taskStreams.Add($taskMemory.ToArray())
    } finally { $taskGraphics.Dispose(); $taskBitmap.Dispose(); $taskMemory.Dispose() }
  }
  $taskFile = [IO.File]::Create((Join-Path $taskRoot 'assets/ReadyForLaunch.ico'))
  $taskWriter = [IO.BinaryWriter]::new($taskFile)
  try {
    $taskWriter.Write([uint16]0); $taskWriter.Write([uint16]1); $taskWriter.Write([uint16]$taskSizes.Count)
    $taskOffset = 6 + 16 * $taskSizes.Count
    for ($taskIndex=0;$taskIndex -lt $taskSizes.Count;$taskIndex++) {
      $taskByteSize = if ($taskSizes[$taskIndex] -eq 256) { 0 } else { $taskSizes[$taskIndex] }
      $taskWriter.Write([byte]$taskByteSize); $taskWriter.Write([byte]$taskByteSize)
      $taskWriter.Write([byte]0); $taskWriter.Write([byte]0)
      $taskWriter.Write([uint16]1); $taskWriter.Write([uint16]32)
      $taskWriter.Write([uint32]$taskStreams[$taskIndex].Length); $taskWriter.Write([uint32]$taskOffset)
      $taskOffset += $taskStreams[$taskIndex].Length
    }
    foreach ($taskBytes in $taskStreams) { $taskWriter.Write($taskBytes) }
  } finally { $taskWriter.Dispose(); $taskFile.Dispose() }
} finally { $taskImage.Dispose() }

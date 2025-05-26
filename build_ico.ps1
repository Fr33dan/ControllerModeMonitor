$inkscape = "C:\Program Files\Inkscape\bin\inkscape.com"
$resolutionList = 256, 128, 64, 48, 32, 24, 16
Write-Host "Building ICO"
New-Item -Name "ico" -Type "Directory"
$imageFile = ""
foreach($resolution in $resolutionList){
	$filePath = "ico\$resolution.png"
	Write-Host "Creating $filePath"
	&$inkscape --export-background-opacity=0 --export-height=$resolution --export-type=png --export-filename=$filePath "vector_logo.svg"
	$imageFile += "$filePath "
}
magick convert $imageFile ControllerMonitorMode.ico
Write-Host "Creating ControllerMonitorMode.ico"
Remove-Item -Path "ico" -Recurse -Force
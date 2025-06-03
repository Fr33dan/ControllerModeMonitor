
if(Test-Path "ControllerModeMonitor.ico"){
	return
}
$inkscape = "C:\Program Files\Inkscape\bin\inkscape.com"
$resolutionList = 256, 128, 64, 48, 32, 24, 16
Write-Host "Building ICO"
New-Item -Name "ico" -Type "Directory"
$magickArgs = @()
foreach($resolution in $resolutionList){
	$filePath = "ico\$resolution.png"
	Write-Host "Creating $filePath"
	&$inkscape --export-background-opacity=0 --export-height=$resolution --export-type=png --export-filename=$filePath "vector_logo.svg"
	$magickArgs += "$filePath"
}
$magickArgs += "ControllerModeMonitor.ico"
Start-Process magick -ArgumentList $magickArgs -Wait
Write-Host "Creating ControllerModeMonitor.ico"
Remove-Item -Path "ico" -Recurse -Force
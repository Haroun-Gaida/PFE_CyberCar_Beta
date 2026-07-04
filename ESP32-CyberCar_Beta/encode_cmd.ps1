$cmd = 'Add-Type -AssemblyName PresentationFramework;$v=New-Object -ComObject SAPI.SpVoice;$v.Speak("Hydra has entered the system. Your files are being processed.");[System.Windows.MessageBox]::Show("CRITICAL ERROR: System compromised. Hydra is running.","Project Hydra","OK","Error")|Out-Null'
$bytes = [System.Text.Encoding]::Unicode.GetBytes($cmd)
[Convert]::ToBase64String($bytes)

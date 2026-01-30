# Start CRE Exchange Server in a new terminal window
# Usage: .\start_server.ps1

$serverCmd = @"
cd /mnt/c/workshop/repo/LOB/central_exchange/build
export LD_LIBRARY_PATH=/mnt/c/workshop/repo/LOB/central_exchange/deps/fxcm-linux/ForexConnectAPI-1.6.5-Linux-x86_64/lib
export FXCM_USER=8000065022
export FXCM_PASS=Meniscus_321957
export FXCM_CONNECTION=Real
export ADMIN_API_KEY=cre2026admin
./central_exchange
"@

# Start in a new Windows Terminal tab
Start-Process wt -ArgumentList "-w 0 new-tab --title `"CRE Exchange`" wsl -e bash -c `"$serverCmd`""

Write-Host "Exchange server starting in new terminal tab..."

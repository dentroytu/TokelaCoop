<#
  TokelaCoop - recoge todo lo necesario para diagnosticar un problema en un zip en
  el Escritorio (logs, lista de mods, configuración y versiones). No cambia nada.
  Lo lanza "Recoger diagnostico.cmd".
#>
param([string]$KenshiPath = '', [switch]$NoPause)
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
Import-Module (Join-Path $here 'TokelaCoopInstaller.psm1') -Force -DisableNameChecking
$kit = Split-Path -Parent $here
Write-Host ''
Write-Host (T 'TokelaCoop - recoger diagnóstico' 'TokelaCoop - collect diagnostics')
Write-Host (T 'Buscando Kenshi...' 'Looking for Kenshi...')
try {
    $dirs = @()
    if ($KenshiPath) { $dirs = @($KenshiPath) } else { $dirs = @(Find-KenshiInstalls) }
    if ($dirs.Count -eq 0 -and -not $NoPause) {
        $p = Read-Host (T 'No encuentro Kenshi. Pega la ruta de su carpeta (la que tiene kenshi_x64.exe)' `
                          "Kenshi not found. Paste its folder's path (the one with kenshi_x64.exe)")
        $p = $p.Trim().Trim('"')
        if ($p -and (Test-KenshiDir $p)) { $dirs = @($p) }
    }
    foreach ($d in $dirs) { Write-Host "  $d" }
    $zip = New-TokelaCoopDiagnostics -KenshiDirs $dirs -KitDir $kit
    Write-Host ''
    Write-Host (T "Listo: $zip" "Done: $zip")
    Write-Host (T 'Envía ese archivo a quien te ayuda. Lleva tus logs (con tu Steam ID, el de tu amigo y su nombre de Steam, y tu usuario de Windows en las rutas), tu lista de mods y tu configuración del mod. No lleva tus partidas.' `
                  "Send that file to whoever is helping you. It holds your logs (with your Steam ID, your friend's Steam ID and name, and your Windows user name in paths), your mod list and the mod's config. It does not hold your saves.")
    try { Start-Process explorer.exe "/select,`"$zip`"" } catch {}
} catch {
    Write-Host ''
    Write-Host (T "No se pudo crear el diagnóstico: $($_.Exception.Message)" "Could not create the diagnostics: $($_.Exception.Message)")
}
Write-Host ''
if (-not $NoPause) { Read-Host (T 'Pulsa Enter para cerrar' 'Press Enter to close') | Out-Null }
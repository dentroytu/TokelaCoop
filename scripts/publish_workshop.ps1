<#
  publish_workshop.ps1 - sube o actualiza TokelaCoop en el Steam Workshop de Kenshi.

  Usa la sesión de Steam abierta en este PC (no pide contraseña) y la DLL de la release
  publicada en GitHub para la versión de src/netproto/Version.h, así que el Workshop lleva
  exactamente lo mismo que el kit. Pasos:
    1. dist\workshop_upload.exe check: comprueba, sin escribir nada, que la interfaz del
       Workshop de la steam_api64.dll de Kenshi es la esperada (lee tus suscripciones y las
       compara con las carpetas descargadas; hace falta al menos una suscripción).
    2. Descarga el kit de la release vX.YY, comprueba su SHA-256 y lo descomprime.
    3. Si no hay tools\WorkshopUpload\item_id.txt, crea el item y guarda su número.
    4. Arma dist\workshop\TokelaCoop (el .mod, la DLL, RE_Kenshi.json, coop_config.json,
       el README del kit, la licencia y los _TokelaCoop.img/.info que usa el launcher).
    5. Sube contenido, título, descripción, imagen y visibilidad.

  powershell -ExecutionPolicy Bypass -File scripts\publish_workshop.ps1 [-Visibility 3] [-Note "..."] [-CheckOnly]
  Visibilidad: 0 pública, 1 solo amigos, 2 privada, 3 no listada (la ve quien tenga el enlace).
#>
param(
    [int]$Visibility = 3,
    [string]$Note = "",
    [switch]$CheckOnly,
    [string]$Kenshi = "C:\Program Files (x86)\Steam\steamapps\common\Kenshi"
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$tool = Join-Path $root 'tools\WorkshopUpload'
$exe  = Join-Path $root 'dist\workshop_upload.exe'
$utf8 = New-Object System.Text.UTF8Encoding($false)

# Run the uploader. steam_api64.dll writes notes ("Setting breakpad minidump AppID")
# to stderr, which PowerShell 5.1 turns into a terminating error under 'Stop'.
function Invoke-Uploader([string[]]$UploaderArgs) {
    $old = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $lines = & $exe @UploaderArgs 2>&1 | ForEach-Object { "$_" } |
             Where-Object { $_ -notmatch 'breakpad|Steam_SetMinidumpSteamID' }
    $script:UploaderExit = $LASTEXITCODE
    $ErrorActionPreference = $old
    $lines | ForEach-Object { Write-Host $_ }
    return $lines
}

if (-not (Test-Path $exe)) {
    & cmd.exe /c "`"$PSScriptRoot\build_workshop_upload.cmd`""
    if ($LASTEXITCODE -ne 0) { throw "No se pudo compilar workshop_upload.exe" }
}

# 1. Read-only interface check.
$workshopDir = Join-Path (Split-Path (Split-Path $Kenshi)) 'workshop\content\233860'
$null = Invoke-Uploader @('check', '--kenshi', $Kenshi, '--workshop-dir', $workshopDir)
if ($script:UploaderExit -ne 0) { throw "La comprobación del Workshop falló: no se sube nada" }
if ($CheckOnly) { return }

# 2. The released kit for this version.
$verLine = Select-String -Path (Join-Path $root 'src\netproto\Version.h') -Pattern '#define TOKELACOOP_VERSION "([0-9.]+)"'
$version = $verLine.Matches[0].Groups[1].Value
$tag = "v$version"
$src = Join-Path $root 'dist\workshop-src'
if (Test-Path $src) { Remove-Item -Recurse -Force $src }
New-Item -ItemType Directory -Force $src | Out-Null
$ErrorActionPreference = 'Continue'
& gh release download $tag --repo dentroytu/TokelaCoop -D $src 2>&1 | ForEach-Object { Write-Host "$_" }
$ghExit = $LASTEXITCODE
$ErrorActionPreference = 'Stop'
if ($ghExit -ne 0) { throw "No se pudo descargar la release $tag (¿está publicada?)" }
$zip = Join-Path $src 'TokelaCoop-kit.zip'
$want = ((Get-Content (Join-Path $src 'TokelaCoop-kit.zip.sha256') -Raw).Trim() -split '\s+')[0]
$have = (Get-FileHash $zip -Algorithm SHA256).Hash
if ($want -ne $have) { throw "El SHA-256 del kit $tag no coincide" }
Expand-Archive $zip (Join-Path $src 'kit')
$kitMod = Join-Path $src 'kit\TokelaCoop'
$prov = Get-Content (Join-Path $src 'kit\PROVENANCE.json') -Raw | ConvertFrom-Json
if ($prov.version -ne $version) { throw "El kit dice versión $($prov.version), Version.h $version" }

# 3. The item (created once; its number is kept in the repo).
$idFile = Join-Path $tool 'item_id.txt'
if (Test-Path $idFile) {
    $item = (Get-Content $idFile -Raw).Trim()
} else {
    $out = Invoke-Uploader @('create', '--kenshi', $Kenshi)
    $m = $out | Select-String '^ITEM (\d+)'
    if (-not $m) { throw "Steam no creó el item" }
    $item = $m.Matches[0].Groups[1].Value
    [IO.File]::WriteAllText($idFile, "$item`r`n", $utf8)
}
Write-Host "Item del Workshop: $item"

# 4. The item folder, laid out like the Workshop mods Kenshi's own uploader makes.
$content = Join-Path $root 'dist\workshop\TokelaCoop'
if (Test-Path $content) { Remove-Item -Recurse -Force $content }
New-Item -ItemType Directory -Force $content | Out-Null
foreach ($f in 'TokelaCoop.mod', 'TokelaCoop.dll', 'RE_Kenshi.json', 'coop_config.json') {
    Copy-Item (Join-Path $kitMod $f) (Join-Path $content $f)
}
Copy-Item (Join-Path $src 'kit\README.txt') (Join-Path $content 'README.txt')
Copy-Item (Join-Path $src 'kit\PROVENANCE.json') (Join-Path $content 'PROVENANCE.json')
# The kit's installer and diagnostics, so a Workshop player can install RE_Kenshi
# (which the Workshop cannot) with one double-click: "Instalar RE_Kenshi.cmd" runs
# the installer with -Workshop (kits from v0.63 on; older ones lack it).
Copy-Item (Join-Path $src 'kit\installer') (Join-Path $content 'installer') -Recurse
Copy-Item (Join-Path $src 'kit\Recoger diagnostico.cmd') (Join-Path $content 'Recoger diagnostico.cmd')
if (Select-String -Path (Join-Path $content 'installer\TokelaCoopInstaller.psm1') -Pattern 'Get-WorkshopKenshiDir' -Quiet) {
    Copy-Item (Join-Path $tool 'Instalar RE_Kenshi.cmd') (Join-Path $content 'Instalar RE_Kenshi.cmd')
} else {
    Write-Host "El kit $tag no trae el modo -Workshop del instalador: subo sin 'Instalar RE_Kenshi.cmd'."
}
Copy-Item (Join-Path $root 'LICENSE') (Join-Path $content 'LICENSE.txt')
Copy-Item (Join-Path $tool 'preview.jpg') (Join-Path $content '_TokelaCoop.img')
$title = (Get-Content (Join-Path $tool 'title.txt') -Raw -Encoding UTF8).Trim()
$info = @"
<?xml version="1.0"?>
<ModData xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:xsd="http://www.w3.org/2001/XMLSchema">
  <id>$item</id>
  <mod>TokelaCoop</mod>
  <title>$([Security.SecurityElement]::Escape($title))</title>
  <tags>
    <string>Gameplay</string>
  </tags>
  <visibility>$Visibility</visibility>
  <lastUpdate>$((Get-Date).ToString('o'))</lastUpdate>
</ModData>
"@
[IO.File]::WriteAllText((Join-Path $content '_TokelaCoop.info'), $info, $utf8)

# 5. Upload.
if (-not $Note) { $Note = "TokelaCoop $tag" }
$noteFile = Join-Path $root 'dist\workshop\note.txt'
[IO.File]::WriteAllText($noteFile, $Note, $utf8)
$null = Invoke-Uploader @('update', '--kenshi', $Kenshi, '--item', $item, '--content', $content,
    '--preview', (Join-Path $tool 'preview.jpg'), '--title-file', (Join-Path $tool 'title.txt'),
    '--desc-file', (Join-Path $tool 'description.txt'), '--note-file', $noteFile,
    '--visibility', "$Visibility", '--tag', 'Gameplay')
if ($script:UploaderExit -ne 0) { throw "La subida falló (código $($script:UploaderExit))" }
Write-Host "Listo: https://steamcommunity.com/sharedfiles/filedetails/?id=$item"

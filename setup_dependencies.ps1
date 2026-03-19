$ErrorActionPreference = "Stop"

New-Item -ItemType Directory -Force -Path External | Out-Null

$ASSIMP_VERSION = "v6.0.4"
$ENET_VERSION   = "v1.3.18"
$JOLT_VERSION   = "v5.5.0"
$JSON_VERSION   = "v3.12.0"
$IMGUI_VERSION  = "v1.92.6-docking"

function Clone-If-Missing {
    param(
        [string]$Version,
        [string]$Url,
        [string]$Path
    )

    if (Test-Path "$Path/.git") {
        Write-Host "[skip] $Path already exists"
    }
    else {
        Write-Host "[clone] $Path -> $Version"
        git clone --depth 1 --branch $Version $Url $Path
    }
}

Clone-If-Missing $ASSIMP_VERSION "https://github.com/assimp/assimp.git" "External/assimp"
Clone-If-Missing $ENET_VERSION   "https://github.com/lsalzman/enet.git" "External/enet"
Clone-If-Missing $JOLT_VERSION   "https://github.com/jrouwe/JoltPhysics.git" "External/JoltPhysics"
Clone-If-Missing $JSON_VERSION   "https://github.com/nlohmann/json.git" "External/nlohmann_json"
Clone-If-Missing $IMGUI_VERSION  "https://github.com/ocornut/imgui.git" "External/imgui"

# FMOD Core API (cannot be cloned — requires manual install from fmod.com)
$FMOD_PATH = "External/fmod"
$FMOD_INSTALL = "C:\Program Files (x86)\FMOD SoundSystem\FMOD Studio API Windows\api\core"

if (Test-Path "$FMOD_PATH/inc/fmod.h") {
    Write-Host "[skip] $FMOD_PATH already exists"
}
elseif (Test-Path "$FMOD_INSTALL/inc/fmod.h") {
    Write-Host "[copy] FMOD Core API from system install -> $FMOD_PATH"
    New-Item -ItemType Directory -Force -Path "$FMOD_PATH/inc"     | Out-Null
    New-Item -ItemType Directory -Force -Path "$FMOD_PATH/lib/x64" | Out-Null
    Copy-Item "$FMOD_INSTALL/inc/*.h"       "$FMOD_PATH/inc/"
    Copy-Item "$FMOD_INSTALL/lib/x64/*.lib" "$FMOD_PATH/lib/x64/"
    Copy-Item "$FMOD_INSTALL/lib/x64/*.dll" "$FMOD_PATH/lib/x64/"
}
else {
    Write-Host "[WARN] FMOD not found. Install FMOD Engine from https://www.fmod.com/download#fmodengine then re-run this script."
    Write-Host "       Expected install path: $FMOD_INSTALL"
}

Write-Host "[done] all dependencies are ready"
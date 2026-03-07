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

Write-Host "[done] all dependencies are ready"
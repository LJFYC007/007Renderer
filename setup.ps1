# Define download URLs
$slangUrl = "https://github.com/shader-slang/slang/releases/download/v2025.10.4/slang-2025.10.4-windows-x86_64.zip"
$dxcUrl = "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.8.2505/dxc_2025_05_24.zip"

# Define target directories
$externalDir = "external"
$slangDir = "$externalDir/slang"
$dxcDir = "$externalDir/dxc"

# Define temporary zip names
$slangZip = "slang.zip"
$dxcZip = "dxc.zip"

# Create external/ directory if it doesn't exist
if (-not (Test-Path $externalDir)) {
    New-Item -ItemType Directory -Path $externalDir | Out-Null
    Write-Host "[INFO] Created directory '$externalDir'"
}

# Helper function to download and extract
function DownloadAndExtract($url, $zipFile, $targetDir, $checkFile) {
    if (Test-Path $targetDir) {
        Write-Host "[INFO] '$targetDir' already exists. Skipping download."
        return 
    }

    try {
        Write-Host "[INFO] Downloading from $url"
        Invoke-WebRequest -Uri $url -OutFile $zipFile
    } catch {
        Write-Error "[ERROR] Failed to download '$url'"
        exit 1
    }

    Expand-Archive -Path $zipFile -DestinationPath $targetDir

    if (-not (Test-Path $checkFile)) {
        Write-Error "[ERROR] Extraction failed. '$checkFile' not found."
        exit 1
    }

    Remove-Item $zipFile
    Write-Host "[SUCCESS] Installed to '$targetDir'"
}

# Install Slang
DownloadAndExtract $slangUrl $slangZip $slangDir "$slangDir/bin/slangc.exe"

# Install DXC
DownloadAndExtract $dxcUrl $dxcZip $dxcDir "$dxcDir/bin/x64/dxc.exe"

Write-Host "[DONE] Slang and DXC setup completed."

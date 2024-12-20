function Windows-Deps {
    $ProjectRoot = Resolve-Path -Path "$PSScriptRoot/../../.."
    $DepsDirectory = "$ProjectRoot/.deps"
    New-Item -ItemType Directory -Force -Path $DepsDirectory
    
    $BoostDirectory = "$DepsDirectory/boost"
    if(-Not (Test-Path -Path $BoostDirectory)) {
        Write-Output "Building Boost"
        $BoostUrl = "https://archives.boost.io/release/1.87.0/source/boost_1_87_0.7z"
        $BoostZip = "$DepsDirectory/boost.7z"
        Invoke-WebRequest -Uri $BoostUrl -OutFile $BoostZip
        Expand-ArchiveExt -Path $BoostZip -DestinationPath $DepsDirectory
        Move-Item -Path "$DepsDirectory/boost_1_87_0" -Destination $BoostDirectory
        Remove-Item -Path $BoostZip
        Set-Location -Path $BoostDirectory
        & ./bootstrap | Out-Default
        & ./b2 link=static address-model=64 --with-system --with-url --with-json | Out-Default
    } else {
        Write-Output "Boost directory exists, skipping build"
    }

    $FmtDirectory = "$DepsDirectory/fmt"
    if(-Not (Test-Path -Path $FmtDirectory)) {
        Write-Output "Downloading fmt"
        $FmtUrl = "https://github.com/fmtlib/fmt/archive/refs/tags/11.0.2.zip"
        $FmtZip = "$DepsDirectory/fmt.zip"
        Invoke-WebRequest -Uri $FmtUrl -OutFile $FmtZip
        Expand-ArchiveExt -Path $FmtZip -DestinationPath $DepsDirectory
        Move-Item -Path "$DepsDirectory/fmt-11.0.2" -Destination $FmtDirectory
        Remove-Item -Path $FmtZip
    } else {
        Write-Output "Fmt directory exists, skipping download"
    }
}
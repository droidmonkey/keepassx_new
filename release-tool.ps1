<#
.SYNOPSIS
KeePassXC Release Tool

.DESCRIPTION
Commands:
  merge      Merge release branch into main branch and create release tags
  build      Build and package binary release from sources
  sign       Sign previously compiled release packages

.NOTES
The following are descriptions of certain parameters:
  -Vcpkg           Specify VCPKG toolchain location (example: C:\vcpkg)
  -Tag             Release tag to check out (defaults to version number)
  -Snapshot        Build current HEAD without checkout out Tag
  -CMakeGenerator  Override the default CMake generator
  -CMakeOptions    Additional CMake options for compiling the sources
  -CPackGenerators Set CPack generators (default: WIX;ZIP)
  -Compiler        Compiler to use (example: g++, clang, msbuild)
  -MakeOptions     Options to pass to the make program
  -SignBuild       Perform platform specific App Signing before packaging
  -SignCert        Specify the App Signing Certificate
  -TimeStamp       Explicitly set the timestamp server to use for appsign
  -SourceBranch    Source branch to merge from (default: 'release/$Version')
  -TargetBranch    Target branch to merge to (default: master)
  -VSToolChain     Specify Visual Studio Toolchain by name if more than one is available
#>

param(
    [Parameter(ParameterSetName = "merge", Mandatory, Position = 0)]
    [switch] $Merge,
    [Parameter(ParameterSetName = "build", Mandatory, Position = 0)]
    [switch] $Build,
    [Parameter(ParameterSetName = "sign", Mandatory, Position = 0)]
    [switch] $Sign,

    [Parameter(ParameterSetName = "merge", Mandatory, Position = 1)]
    [Parameter(ParameterSetName = "build", Mandatory, Position = 1)]
    [Parameter(ParameterSetName = "sign", Mandatory, Position = 1)]
    [string] $Version,

    [Parameter(ParameterSetName = "build", Mandatory)]
    [string] $Vcpkg,

    [Parameter(ParameterSetName = "sign", Mandatory)]
    [SupportsWildcards()]
    [string[]] $SignFiles,

    # [Parameter(ParameterSetName = "build")]
    # [switch] $DryRun,
    [Parameter(ParameterSetName = "build")]
    [switch] $Snapshot,
    [Parameter(ParameterSetName = "build")]
    [switch] $SignBuild,
    
    [Parameter(ParameterSetName = "build")]
    [string] $CMakeGenerator = "Ninja",
    [Parameter(ParameterSetName = "build")]
    [string] $CMakeOptions,
    [Parameter(ParameterSetName = "build")]
    [string] $CPackGenerators = "WIX;ZIP",
    [Parameter(ParameterSetName = "build")]
    [string] $Compiler,
    [Parameter(ParameterSetName = "build")]
    [string] $MakeOptions,
    [Parameter(ParameterSetName = "build")]
    [Parameter(ParameterSetName = "sign")]
    [X509Certificate] $SignCert,
    [Parameter(ParameterSetName = "build")]
    [Parameter(ParameterSetName = "sign")]
    [string] $Timestamp = "http://timestamp.sectigo.com",
    [Parameter(ParameterSetName = "merge")]
    [Parameter(ParameterSetName = "build")]
    [Parameter(ParameterSetName = "sign")]
    [string] $GpgKey = "CFB4C2166397D0D2",
    [Parameter(ParameterSetName = "merge")]
    [Parameter(ParameterSetName = "build")]
    [string] $SourceDir = ".",
    [Parameter(ParameterSetName = "build")]
    [string] $OutDir = ".\release",
    [Parameter(ParameterSetName = "merge")]
    [Parameter(ParameterSetName = "build")]
    [string] $Tag,
    [Parameter(ParameterSetName = "merge")]
    [string] $SourceBranch,
    [Parameter(ParameterSetName = "build")]
    [string] $VSToolChain,
    [Parameter(ParameterSetName = "merge")]
    [Parameter(ParameterSetName = "build")]
    [Parameter(ParameterSetName = "sign")]
    [string] $ExtraPath
)

# Helper function definitions
function Test-RequiredPrograms {
    # If any of these fail they will throw an exception terminating the script
    if ($Build) {
        Get-Command git | Out-Null
        Get-Command cmake | Out-Null
    }
    if ($Merge) {
        Get-Command git | Out-Null
        Get-Command tx | Out-Null
        Get-Command lupdate | Out-Null
    }
    if ($Sign) {
        Get-Command gpg | Out-Null
    }
}

function Test-VersionInFiles {
    # Check CMakeLists.txt
    $Major, $Minor, $Patch = $Version.split(".", 3)
    if (!(Select-String "$SourceDir\CMakeLists.txt" -pattern "KEEPASSXC_VERSION_MAJOR `"$Major`"" -Quiet) `
            -or !(Select-String "$SourceDir\CMakeLists.txt" -pattern "KEEPASSXC_VERSION_MINOR `"$Minor`"" -Quiet) `
            -or !(Select-String "$SourceDir\CMakeLists.txt" -pattern "KEEPASSXC_VERSION_PATCH `"$Patch`"" -Quiet)) {
        throw "CMakeLists.txt has not been updated to $Version."
    }

    # Check Changelog
    if (!(Select-String "$SourceDir\CHANGELOG.md" -pattern "^## $Version \(\d{4}-\d{2}-\d{2}\)$" -Quiet)) {
        throw "CHANGELOG.md does not contain a section for $Version."
    }

    # Check AppStreamInfo
    if (!(Select-String "$SourceDir\share\linux\org.keepassxc.KeePassXC.appdata.xml" `
                -pattern "<release version=`"$Version`" date=`"\d{4}-\d{2}-\d{2}`">" -Quiet)) {
        throw "share/linux/org.keepassxc.KeePassXC.appdata.xml does not contain a section for $Version."
    }
}

function Test-WorkingTreeClean {
    & git diff-index --quiet HEAD --
    if ($LASTEXITCODE) {
        throw "Current working tree is not clean! Please commit or unstage any changes."
    }
}

function Invoke-VSToolchain([String] $Toolchain, [String] $Path, [String] $Arch) {
    # Find Visual Studio installations
    $vs = Get-CimInstance MSFT_VSInstance -Namespace root/cimv2/vs
  
    if ($vs.count -eq 0) {
        $err = "No Visual Studio installations found, download one from https://visualstudio.com/downloads."
        $err = "$err`nIf Visual Studio is installed, you may need to repair the install then restart."
        throw $err
    }

    $VSBaseDir = $vs[0].InstallLocation
    if ($Toolchain) {
        # Try to find the specified toolchain by name
        foreach ($_ in $vs) {
            if ($_.Name -eq $Toolchain) {
                $VSBaseDir = $_.InstallLocation
                break
            }
        }
    } elseif ($vs.count -gt 1) {
        # Ask the user which install to use
        $i = 0
        foreach ($_ in $vs) {
            $i = $i + 1
            $i.ToString() + ") " + $_.Name | Write-Host
        }
        $i = Read-Host -Prompt "Which Visual Studio installation do you want to use?"
        $i = [Convert]::ToInt32($i, 10) - 1
        if ($i -lt 0 -or $i -ge $vs.count) {
            throw "Invalid selection made"
        }
        $VSBaseDir = $vs[$i].InstallLocation
    }
    
    # Bootstrap the specified VS Toolchain
    Import-Module "$VSBaseDir\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
    Enter-VsDevShell -VsInstallPath $VSBaseDir -Arch $Arch -StartInPath $Path | Write-Host
    Write-Host # Newline after command output
}

function Invoke-Cmd([string] $command, [string[]] $options = @(), [switch] $maskargs, [switch] $quiet) {
    $call = ('{0} {1}' -f $command, ($options -Join ' '))
    if ($maskargs) {
        Write-Host "$command <masked>" -ForegroundColor DarkGray
    }
    else {
        Write-Host $call -ForegroundColor DarkGray
    }
    if ($quiet) {
        Invoke-Expression $call > $null
    } else {
        Invoke-Expression $call
    }
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to run command: {0}" -f $command
    }
    Write-Host #insert newline after command output
}

function Find-SignCert() {
    $certs = Get-ChildItem Cert:\CurrentUser\My -codesign
    if ($certs.Count -eq 0) {
        throw "No code signing certificate found in User certificate store"
    } elseif ($certs.Count -gt 1) {
        # Ask the user which to use
        $i = 0
        foreach ($_ in $certs) {
            $i = $i + 1
            $i.ToString() + ") $($_.Thumbprint) - $($_.NotAfter)" | Write-Host
        }
        $i = Read-Host -Prompt "Which certificate do you want to use?"
        $i = [Convert]::ToInt32($i, 10) - 1
        if ($i -lt 0 -or $i -ge $certs.count) {
            throw "Invalid selection made"
        }
        return $certs[$i]
    } else {
        Write-Host "Found signing certificate: $($certs[0].Subject) ($($certs[0].Thumbprint))" -ForegroundColor Cyan
        Write-Host
        return $certs[0]
    }
}

function Invoke-SignFiles([string[]] $files, [X509Certificate] $cert, [string] $time) {
    if ($files.Length -eq 0) {
        return
    }

    Write-Host "Signing files using $($cert.Subject) ($($cert.Thumbprint))" -ForegroundColor Cyan
    
    foreach ($_ in $files) {
        $sig = Get-AuthenticodeSignature -FilePath "$_" -ErrorAction SilentlyContinue
        if ($sig.Status -ne "Valid") {
            Write-Host "Signing file '$_'"
            $tmp = Set-AuthenticodeSignature -Certificate $cert -FilePath "$_" -TimestampServer "$Timestamp" -HashAlgorithm "SHA256"
        }
    }
}

function Invoke-GpgSignFiles([string[]] $files, [string] $key) {
    if ($files.Length -eq 0) {
        return
    }

    Write-Host "Signing files using GPG key $key" -ForegroundColor Cyan

    foreach ($_ in $files) {
        Write-Host "Signing file '$_' and creating DIGEST..."
        if (Test-Path "$_.sig") {
            Remove-Item "$_.sig"
        }
        Invoke-Cmd "gpg" "--output `"$_.sig`" --armor --local-user `"$key`" --detach-sig `"$_`""
        $FileName = (Get-Item $_).Name
        (Get-FileHash "$_" SHA256).Hash + " *$FileName" | Out-File "$_.DIGEST" -NoNewline
    }
}

# Handle errors and restore state
$OrigDir = (Get-Location).Path
$OrigBranch = & git rev-parse --abbrev-ref HEAD
$ErrorActionPreference = 'Stop'
trap {
    Write-Host "Restoring state..." -ForegroundColor Yellow
    & git checkout $OrigBranch
    Set-Location "$OrigDir"
}

Write-Host "KeePassXC Release Preparation Helper" -ForegroundColor Green
Write-Host "Copyright (C) 2022 KeePassXC Team <https://keepassxc.org/>`n" -ForegroundColor Green

# Prepend extra PATH locations as specified
if ($ExtraPath) {
    $env:Path = "$ExtraPath;$env:Path"
}

# Resolve absolute directory for paths
$SourceDir = (Resolve-Path $SourceDir).Path

# Check format of -Version
if ($Version -notmatch "^\d+\.\d+\.\d+(-Beta\d*)?$") {
    throw "Invalid format for -Version input"
}

# Check platform
if (!$IsWindows) {
    throw "The PowerShell release tool is not available for Linux or macOS at this time."
}

if ($Merge) {
    Test-RequiredPrograms

    # Change to SourceDir
    Set-Location "$SourceDir"

    Test-VersionInFiles
    Test-WorkingTreeClean

    if (!$SourceBranch.Length) {
        $SourceBranch = & git branch --show-current
    }

    if ($SourceBranch -notmatch "^release/.*$") {
        throw "Must be on a release/* branch to continue."
    }

    # Update translation files
    Write-Host "Updating source translation file..."
    Invoke-Cmd "lupdate" "-no-ui-lines -disable-heuristic similartext -locations none", `
        "-extensions c,cpp,h,js,mm,qrc,ui -no-obsolete ./src -ts share/translations/keepassxc_en.ts"

    Write-Host "Pulling updated translations from Transifex..."
    Invoke-Cmd "tx" "pull -af --minimum-perc=60 -r keepassxc.share-translations-keepassxc-en-ts--develop"

    # Only commit if there are changes
    $changes = & git status --porcelain
    if ($changes.Length -gt 0) {
        Write-Host "Committing translation updates..."
        Invoke-Cmd "git" "add -A ./share/translations/" -quiet
        Invoke-Cmd "git" "commit -m `"Update translations`"" -quiet
    }

    # Read the version release notes from CHANGELOG
    $Changelog = ""
    $ReadLine = $false
    Get-Content "CHANGELOG.md" | ForEach-Object {
        if ($ReadLine) {
            if ($_ -match "^## ") {
                $ReadLine = $false
            } else {
                $Changelog += $_ + "`n"
            }
        } elseif ($_ -match "$Version \(\d{4}-\d{2}-\d{2}\)") {
            $ReadLine = $true
        }
    }

    Write-Host "Creating tag for '$Version'..."
    $tmp = New-TemporaryFile
    "Release $Version`n$Changelog" | Out-File $tmp.FullName
    Invoke-Cmd "git" "tag -a `"$Version`" -F `"$tmp`" -s" -quiet
    Remove-Item $tmp.FullName -Force

    Write-Host "Moving latest tag..."
    Invoke-Cmd "git" "tag -f -a `"latest`" -m `"Latest stable release`" -s" -quiet

    Write-Host "All done!"
    Write-Host "Please merge the release branch back into the develop branch now and then push your changes."
    Write-Host "Don't forget to also push the tags using 'git push --tags'."

} elseif ($Build) {
    $Vcpkg = (Resolve-Path "$Vcpkg/scripts/buildsystems/vcpkg.cmake").Path

    # Find Visual Studio and establish build environment
    Invoke-VSToolchain $VSToolChain $SourceDir -Arch "amd64"

    if ($SignBuild -and !$SignCert) {
        $SignCert = Find-SignCert
    }

    Test-RequiredPrograms

    # Create directories
    New-Item "$OutDir" -ItemType Directory -Force | Out-Null
    $OutDir = (Resolve-Path $OutDir).Path

    $BuildDir = "$OutDir\build-release"
    New-Item "$BuildDir" -ItemType Directory -Force | Out-Null

    if ($Snapshot) {
        $Tag = "HEAD"
        $SourceBranch = & git rev-parse --abbrev-ref HEAD
        $ReleaseName = "$Version-snapshot"
        $CMakeOptions = "-DKEEPASSXC_BUILD_TYPE=Snapshot -DOVERRIDE_VERSION=`"$ReleaseName`" $CMakeOptions"
        Write-Host "Using current branch '$SourceBranch' to build." -ForegroundColor Cyan
    } else {
        Test-WorkingTreeClean

        # Clear build directory except for installed vcpkg files to prevent having to re-sign everything
        if (Test-Path $BuildDir) {
            Get-ChildItem $BuildDir -Recurse | Where-Object {$_.FullName -notlike "$BuildDir\vcpkg_installed*"} | Remove-Item -Recurse -Force
        }
        
        $CMakeOptions = "-DKEEPASSXC_BUILD_TYPE=Release $CMakeOptions"

        # Setup Tag if not defined then checkout tag
        if ($Tag -eq "" -or $null -eq $Tag) {
            $Tag = $Version
        }
        Write-Host "Checking out tag 'tags/$Tag' to build." -ForegroundColor Cyan
        Invoke-Cmd "git" "checkout `"tags/$Tag`""
    }

    # Enter build directory
    Set-Location "$BuildDir"

    # Setup CMake options
    $CMakeOptions = "-DWITH_TESTS=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE:FILEPATH=`"$Vcpkg`" -DX_VCPKG_APPLOCAL_DEPS_INSTALL=ON"

    Write-Host "Configuring build..." -ForegroundColor Cyan
    Invoke-Cmd "cmake" "-G `"$CMakeGenerator`" $CMakeOptions `"$SourceDir`""

    Write-Host "Compiling sources..." -ForegroundColor Cyan
    Invoke-Cmd "cmake" "--build . --config Release -- $MakeOptions"
    
    if ($SignBuild) {
        $VcpkgDir = $BuildDir + "\vcpkg_installed\"
        if (Test-Path $VcpkgDir) {
            $files = Get-ChildItem $VcpkgDir -Filter "*.dll" -Recurse -File | 
                Where-Object {$_.FullName -notlike "$VcpkgDir*debug\*" -and $_.FullName -notlike "$VcpkgDir*tools\*"} | 
                ForEach-Object {$_.FullName}
        }
        $files += Get-ChildItem "$BuildDir\src" -Include "*keepassxc*.exe", "*keepassxc*.dll" -Recurse -File | ForEach-Object { $_.FullName }
        Invoke-SignFiles $files $SignCert $Timestamp
    }

    Write-Host "Create deployment packages..." -ForegroundColor Cyan
    Invoke-Cmd "cpack" "-G `"$CPackGenerators`""
    Move-Item "$BuildDir\keepassxc-*" -Destination "$OutDir" -Force

    if ($SignBuild) {
        # Enter output directory
        Set-Location -Path "$OutDir"

        # Sign MSI files using AppSign key
        $files = Get-ChildItem $OutDir -Include "*.msi" -Name
        Invoke-SignFiles $files $SignCert $Timestamp

        # Sign all output files using the GPG key then hash them
        $files = Get-ChildItem $OutDir -Include "*.msi", "*.zip" -Name
        Invoke-GpgSignFiles $files $GpgKey
    }

    # Restore state
    Invoke-Command {git checkout $OrigBranch}
    Set-Location "$OrigDir"

} elseif ($Sign) {
    Test-RequiredPrograms

    if (!$SignCert) {
        $SignCert = Find-SignCert
    }

    # Resolve wildcard paths
    $ResolvedFiles = @()
    foreach ($_ in $SignFiles) {
        $ResolvedFiles += (Get-ChildItem $_ -File | ForEach-Object { $_.FullName })
    }

    $AppSignFiles = $ResolvedFiles.Where({ $_ -match "\.(msi|exe|dll)$" })
    Invoke-SignFiles $AppSignFiles $SignCert $Timestamp

    $GpgSignFiles = $ResolvedFiles.Where({ $_ -match "\.(msi|zip|gz|xz|dmg|appimage)$" })
    Invoke-GpgSignFiles $GpgSignFiles $GpgKey
}

# SIG # Begin signature block
# MIImVAYJKoZIhvcNAQcCoIImRTCCJkECAQExDzANBglghkgBZQMEAgEFADB5Bgor
# BgEEAYI3AgEEoGswaTA0BgorBgEEAYI3AgEeMCYCAwEAAAQQH8w7YFlLCE63JNLG
# KX7zUQIBAAIBAAIBAAIBAAIBADAxMA0GCWCGSAFlAwQCAQUABCBnhvt4NN16Hfle
# eDfbtgcr43dOfERdBCPSlLlozTRPoaCCH2owggYUMIID/KADAgECAhB6I67aU2mW
# D5HIPlz0x+M/MA0GCSqGSIb3DQEBDAUAMFcxCzAJBgNVBAYTAkdCMRgwFgYDVQQK
# Ew9TZWN0aWdvIExpbWl0ZWQxLjAsBgNVBAMTJVNlY3RpZ28gUHVibGljIFRpbWUg
# U3RhbXBpbmcgUm9vdCBSNDYwHhcNMjEwMzIyMDAwMDAwWhcNMzYwMzIxMjM1OTU5
# WjBVMQswCQYDVQQGEwJHQjEYMBYGA1UEChMPU2VjdGlnbyBMaW1pdGVkMSwwKgYD
# VQQDEyNTZWN0aWdvIFB1YmxpYyBUaW1lIFN0YW1waW5nIENBIFIzNjCCAaIwDQYJ
# KoZIhvcNAQEBBQADggGPADCCAYoCggGBAM2Y2ENBq26CK+z2M34mNOSJjNPvIhKA
# VD7vJq+MDoGD46IiM+b83+3ecLvBhStSVjeYXIjfa3ajoW3cS3ElcJzkyZlBnwDE
# JuHlzpbN4kMH2qRBVrjrGJgSlzzUqcGQBaCxpectRGhhnOSwcjPMI3G0hedv2eNm
# GiUbD12OeORN0ADzdpsQ4dDi6M4YhoGE9cbY11XxM2AVZn0GiOUC9+XE0wI7CQKf
# OUfigLDn7i/WeyxZ43XLj5GVo7LDBExSLnh+va8WxTlA+uBvq1KO8RSHUQLgzb1g
# bL9Ihgzxmkdp2ZWNuLc+XyEmJNbD2OIIq/fWlwBp6KNL19zpHsODLIsgZ+WZ1AzC
# s1HEK6VWrxmnKyJJg2Lv23DlEdZlQSGdF+z+Gyn9/CRezKe7WNyxRf4e4bwUtrYE
# 2F5Q+05yDD68clwnweckKtxRaF0VzN/w76kOLIaFVhf5sMM/caEZLtOYqYadtn03
# 4ykSFaZuIBU9uCSrKRKTPJhWvXk4CllgrwIDAQABo4IBXDCCAVgwHwYDVR0jBBgw
# FoAU9ndq3T/9ARP/FqFsggIv0Ao9FCUwHQYDVR0OBBYEFF9Y7UwxeqJhQo1SgLqz
# YZcZojKbMA4GA1UdDwEB/wQEAwIBhjASBgNVHRMBAf8ECDAGAQH/AgEAMBMGA1Ud
# JQQMMAoGCCsGAQUFBwMIMBEGA1UdIAQKMAgwBgYEVR0gADBMBgNVHR8ERTBDMEGg
# P6A9hjtodHRwOi8vY3JsLnNlY3RpZ28uY29tL1NlY3RpZ29QdWJsaWNUaW1lU3Rh
# bXBpbmdSb290UjQ2LmNybDB8BggrBgEFBQcBAQRwMG4wRwYIKwYBBQUHMAKGO2h0
# dHA6Ly9jcnQuc2VjdGlnby5jb20vU2VjdGlnb1B1YmxpY1RpbWVTdGFtcGluZ1Jv
# b3RSNDYucDdjMCMGCCsGAQUFBzABhhdodHRwOi8vb2NzcC5zZWN0aWdvLmNvbTAN
# BgkqhkiG9w0BAQwFAAOCAgEAEtd7IK0ONVgMnoEdJVj9TC1ndK/HYiYh9lVUacah
# RoZ2W2hfiEOyQExnHk1jkvpIJzAMxmEc6ZvIyHI5UkPCbXKspioYMdbOnBWQUn73
# 3qMooBfIghpR/klUqNxx6/fDXqY0hSU1OSkkSivt51UlmJElUICZYBodzD3M/SFj
# eCP59anwxs6hwj1mfvzG+b1coYGnqsSz2wSKr+nDO+Db8qNcTbJZRAiSazr7KyUJ
# Go1c+MScGfG5QHV+bps8BX5Oyv9Ct36Y4Il6ajTqV2ifikkVtB3RNBUgwu/mSiSU
# ice/Jp/q8BMk/gN8+0rNIE+QqU63JoVMCMPY2752LmESsRVVoypJVt8/N3qQ1c6F
# ibbcRabo3azZkcIdWGVSAdoLgAIxEKBeNh9AQO1gQrnh1TA8ldXuJzPSuALOz1Uj
# b0PCyNVkWk7hkhVHfcvBfI8NtgWQupiaAeNHe0pWSGH2opXZYKYG4Lbukg7HpNi/
# KqJhue2Keak6qH9A8CeEOB7Eob0Zf+fU+CCQaL0cJqlmnx9HCDxF+3BLbUufrV64
# EbTI40zqegPZdA+sXCmbcZy6okx/SjwsusWRItFA3DE8MORZeFb6BmzBtqKJ7l93
# 9bbKBy2jvxcJI98Va95Q5JnlKor3m0E7xpMeYRriWklUPsetMSf2NvUQa/E5vVye
# fQIwggYaMIIEAqADAgECAhBiHW0MUgGeO5B5FSCJIRwKMA0GCSqGSIb3DQEBDAUA
# MFYxCzAJBgNVBAYTAkdCMRgwFgYDVQQKEw9TZWN0aWdvIExpbWl0ZWQxLTArBgNV
# BAMTJFNlY3RpZ28gUHVibGljIENvZGUgU2lnbmluZyBSb290IFI0NjAeFw0yMTAz
# MjIwMDAwMDBaFw0zNjAzMjEyMzU5NTlaMFQxCzAJBgNVBAYTAkdCMRgwFgYDVQQK
# Ew9TZWN0aWdvIExpbWl0ZWQxKzApBgNVBAMTIlNlY3RpZ28gUHVibGljIENvZGUg
# U2lnbmluZyBDQSBSMzYwggGiMA0GCSqGSIb3DQEBAQUAA4IBjwAwggGKAoIBgQCb
# K51T+jU/jmAGQ2rAz/V/9shTUxjIztNsfvxYB5UXeWUzCxEeAEZGbEN4QMgCsJLZ
# UKhWThj/yPqy0iSZhXkZ6Pg2A2NVDgFigOMYzB2OKhdqfWGVoYW3haT29PSTahYk
# wmMv0b/83nbeECbiMXhSOtbam+/36F09fy1tsB8je/RV0mIk8XL/tfCK6cPuYHE2
# 15wzrK0h1SWHTxPbPuYkRdkP05ZwmRmTnAO5/arnY83jeNzhP06ShdnRqtZlV59+
# 8yv+KIhE5ILMqgOZYAENHNX9SJDm+qxp4VqpB3MV/h53yl41aHU5pledi9lCBbH9
# JeIkNFICiVHNkRmq4TpxtwfvjsUedyz8rNyfQJy/aOs5b4s+ac7IH60B+Ja7TVM+
# EKv1WuTGwcLmoU3FpOFMbmPj8pz44MPZ1f9+YEQIQty/NQd/2yGgW+ufflcZ/ZE9
# o1M7a5Jnqf2i2/uMSWymR8r2oQBMdlyh2n5HirY4jKnFH/9gRvd+QOfdRrJZb1sC
# AwEAAaOCAWQwggFgMB8GA1UdIwQYMBaAFDLrkpr/NZZILyhAQnAgNpFcF4XmMB0G
# A1UdDgQWBBQPKssghyi47G9IritUpimqF6TNDDAOBgNVHQ8BAf8EBAMCAYYwEgYD
# VR0TAQH/BAgwBgEB/wIBADATBgNVHSUEDDAKBggrBgEFBQcDAzAbBgNVHSAEFDAS
# MAYGBFUdIAAwCAYGZ4EMAQQBMEsGA1UdHwREMEIwQKA+oDyGOmh0dHA6Ly9jcmwu
# c2VjdGlnby5jb20vU2VjdGlnb1B1YmxpY0NvZGVTaWduaW5nUm9vdFI0Ni5jcmww
# ewYIKwYBBQUHAQEEbzBtMEYGCCsGAQUFBzAChjpodHRwOi8vY3J0LnNlY3RpZ28u
# Y29tL1NlY3RpZ29QdWJsaWNDb2RlU2lnbmluZ1Jvb3RSNDYucDdjMCMGCCsGAQUF
# BzABhhdodHRwOi8vb2NzcC5zZWN0aWdvLmNvbTANBgkqhkiG9w0BAQwFAAOCAgEA
# Bv+C4XdjNm57oRUgmxP/BP6YdURhw1aVcdGRP4Wh60BAscjW4HL9hcpkOTz5jUug
# 2oeunbYAowbFC2AKK+cMcXIBD0ZdOaWTsyNyBBsMLHqafvIhrCymlaS98+QpoBCy
# KppP0OcxYEdU0hpsaqBBIZOtBajjcw5+w/KeFvPYfLF/ldYpmlG+vd0xqlqd099i
# ChnyIMvY5HexjO2AmtsbpVn0OhNcWbWDRF/3sBp6fWXhz7DcML4iTAWS+MVXeNLj
# 1lJziVKEoroGs9Mlizg0bUMbOalOhOfCipnx8CaLZeVme5yELg09Jlo8BMe80jO3
# 7PU8ejfkP9/uPak7VLwELKxAMcJszkyeiaerlphwoKx1uHRzNyE6bxuSKcutisqm
# KL5OTunAvtONEoteSiabkPVSZ2z76mKnzAfZxCl/3dq3dUNw4rg3sTCggkHSRqTq
# lLMS7gjrhTqBmzu1L90Y1KWN/Y5JKdGvspbOrTfOXyXvmPL6E52z1NZJ6ctuMFBQ
# ZH3pwWvqURR8AgQdULUvrxjUYbHHj95Ejza63zdrEcxWLDX6xWls/GDnVNueKjWU
# H3fTv1Y8Wdho698YADR7TNx8X8z2Bev6SivBBOHY+uqiirZtg0y9ShQoPzmCcn63
# Syatatvx157YK9hlcPmVoa1oDE5/L9Uo2bC5a4CH2RwwggZJMIIEsaADAgECAhAG
# Qz/MzOQzqJLMF7dGpYxlMA0GCSqGSIb3DQEBDAUAMFQxCzAJBgNVBAYTAkdCMRgw
# FgYDVQQKEw9TZWN0aWdvIExpbWl0ZWQxKzApBgNVBAMTIlNlY3RpZ28gUHVibGlj
# IENvZGUgU2lnbmluZyBDQSBSMzYwHhcNMjQwMjIzMDAwMDAwWhcNMjcwMjIyMjM1
# OTU5WjBgMQswCQYDVQQGEwJVUzERMA8GA1UECAwIVmlyZ2luaWExHjAcBgNVBAoM
# FURyb2lkTW9ua2V5IEFwcHMsIExMQzEeMBwGA1UEAwwVRHJvaWRNb25rZXkgQXBw
# cywgTExDMIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEAuJtEjRyetghx
# 6Abi1cpMT88uT6nIcTe3AyUvdSkjCtUM8Gat0YJfqTxokb9dBzJa7j8YWOUU1Yc4
# EDXoYYtVRE+1UkdPAcXNMf2hNXGI45iZVwhBPQZBU4QfKltzYqrjAZgDvxeYd68q
# ImjzUfrCY3uZHwEIuCewmNMPpEgbdjuSXDyBAKKBtaO2iqyaJpqcC39QnDKlXMic
# DPqqH5fI7wK7Lg9f4BwOsaO4P68I3pOv7L/6E5GR9+hTj6txhxFz/yCbDxN1PUvD
# sGaXjMmVeP2M95fkwOFwut5yBESDIwAGEWUFsTJ32hSmE74+xG6rVqtueayV7U9c
# GURznSk9ZlTUqQOW9Z4K+pu29gTZ9zVWlONIsQR7QXfGKZWF+Xik6rTujSRTTsK7
# QNMYzBI6b9v0nD2pEWuGZDXIO5o5N2HzXEFlwxCFY483yWSObHNBp9PFtiDueqv+
# 8vrN+lsirZlDFCxI6hW+F8oYp3XxHdSqxsMRTqbO6dUjH2Tyd0G5fbyT8Rid7DbP
# 6p/apzIrdFOM0kdcKLmppYBp7BInTdjbWJYhtuORIUZQbUOSM71vYCUHj7xkckiY
# YmkUf0XH8xx8jqgVWseBW63gCEowhCEYxaWt0QGyXJ6UrlV4WTUCWzxm45I5OQoo
# fymUvdutKgr9bR3nJ5yS/c+E3KnqJhkCAwEAAaOCAYkwggGFMB8GA1UdIwQYMBaA
# FA8qyyCHKLjsb0iuK1SmKaoXpM0MMB0GA1UdDgQWBBQta729krTac3CUndU0S0Dd
# DscjHTAOBgNVHQ8BAf8EBAMCB4AwDAYDVR0TAQH/BAIwADATBgNVHSUEDDAKBggr
# BgEFBQcDAzBKBgNVHSAEQzBBMDUGDCsGAQQBsjEBAgEDAjAlMCMGCCsGAQUFBwIB
# FhdodHRwczovL3NlY3RpZ28uY29tL0NQUzAIBgZngQwBBAEwSQYDVR0fBEIwQDA+
# oDygOoY4aHR0cDovL2NybC5zZWN0aWdvLmNvbS9TZWN0aWdvUHVibGljQ29kZVNp
# Z25pbmdDQVIzNi5jcmwweQYIKwYBBQUHAQEEbTBrMEQGCCsGAQUFBzAChjhodHRw
# Oi8vY3J0LnNlY3RpZ28uY29tL1NlY3RpZ29QdWJsaWNDb2RlU2lnbmluZ0NBUjM2
# LmNydDAjBggrBgEFBQcwAYYXaHR0cDovL29jc3Auc2VjdGlnby5jb20wDQYJKoZI
# hvcNAQEMBQADggGBAJSy5YPCbh9ZsuDCKgDuzOWZzNza4/FrA+kT7EitDezYN3S/
# P0EVc0tPbgYAKfNqY+ihAMyjZHdgybfBWhGzUTDo+HEipcnZ2pgwPadsw23jJ8MN
# 1tdms9iKDakIQ2MVsB7cGFRU8QjLovkPdZkyLcjuYbkiZRoNoKlhmrOOf6n1oCwX
# VJ9ONJijc+Lr3+4EIqZ39ET2+uI9Wg9Bfd9XrDZfYFEcRJjNzRpCtHb26aIzV/Xi
# MWasHRPaII34SzD0BmaPbsLeGW1UGvW3tQcgVNdT/uajegmShVb+c5J5ktRSJ0cq
# yxmTAYaeMuA6IxG1f6kui1SAFQs2lzlGyEgxgiNGo7cHHN2KidhrBL3U2bGr9Tkd
# p3gmV+Gj3esCdQzJE4aqmUZvIvHpkrair4qbLFZRNozAZJn2SIeQa5u2U0ZmvcAr
# 1C7S3JVLP3t9LKE0mlFkV9pbIU97ND3iH3tO0Zb3SvCK/XjO1PZVb8EXsi67wbfM
# SWAwi2CETDonb7+gBjCCBl0wggTFoAMCAQICEDpSaiyEzlXmHWX8zBLY6YkwDQYJ
# KoZIhvcNAQEMBQAwVTELMAkGA1UEBhMCR0IxGDAWBgNVBAoTD1NlY3RpZ28gTGlt
# aXRlZDEsMCoGA1UEAxMjU2VjdGlnbyBQdWJsaWMgVGltZSBTdGFtcGluZyBDQSBS
# MzYwHhcNMjQwMTE1MDAwMDAwWhcNMzUwNDE0MjM1OTU5WjBuMQswCQYDVQQGEwJH
# QjETMBEGA1UECBMKTWFuY2hlc3RlcjEYMBYGA1UEChMPU2VjdGlnbyBMaW1pdGVk
# MTAwLgYDVQQDEydTZWN0aWdvIFB1YmxpYyBUaW1lIFN0YW1waW5nIFNpZ25lciBS
# MzUwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIKAoICAQCN0Wf0wUibvf04STpN
# YYGbw9jcRaVhBDaNBp7jmJaA9dQZW5ighrXGNMYjK7Dey5RIHMqLIbT9z9if753m
# YbojJrKWO4ZP0N5dBT2TwZZaPb8E+hqaDZ8Vy2c+x1NiEwbEzTrPX4W3QFq/zJvD
# DbWKL99qLL42GJQzX3n5wWo60KklfFn+Wb22mOZWYSqkCVGl8aYuE12SqIS4MVO4
# PUaxXeO+4+48YpQlNqbc/ndTgszRQLF4MjxDPjRDD1M9qvpLTZcTGVzxfViyIToR
# NxPP6DUiZDU6oXARrGwyP9aglPXwYbkqI2dLuf9fiIzBugCDciOly8TPDgBkJmjA
# fILNiGcVEzg+40xUdhxNcaC+6r0juPiR7bzXHh7v/3RnlZuT3ZGstxLfmE7fRMAF
# wbHdDz5gtHLqjSTXDiNF58IxPtvmZPG2rlc+Yq+2B8+5pY+QZn+1vEifI0MDtiA6
# BxxQuOnj4PnqDaK7NEKwtD1pzoA3jJFuoJiwbatwhDkg1PIjYnMDbDW+wAc9FtRN
# 6pUsO405jaBgigoFZCw9hWjLNqgFVTo7lMb5rVjJ9aSBVVL2dcqzyFW2LdWk5Xdp
# 65oeeOALod7YIIMv1pbqC15R7QCYLxcK1bCl4/HpBbdE5mjy9JR70BHuYx27n4XN
# OZbwrXcG3wZf9gEUk7stbPAoBQIDAQABo4IBjjCCAYowHwYDVR0jBBgwFoAUX1jt
# TDF6omFCjVKAurNhlxmiMpswHQYDVR0OBBYEFGjvpDJJabZSOB3qQzks9BRqngyF
# MA4GA1UdDwEB/wQEAwIGwDAMBgNVHRMBAf8EAjAAMBYGA1UdJQEB/wQMMAoGCCsG
# AQUFBwMIMEoGA1UdIARDMEEwNQYMKwYBBAGyMQECAQMIMCUwIwYIKwYBBQUHAgEW
# F2h0dHBzOi8vc2VjdGlnby5jb20vQ1BTMAgGBmeBDAEEAjBKBgNVHR8EQzBBMD+g
# PaA7hjlodHRwOi8vY3JsLnNlY3RpZ28uY29tL1NlY3RpZ29QdWJsaWNUaW1lU3Rh
# bXBpbmdDQVIzNi5jcmwwegYIKwYBBQUHAQEEbjBsMEUGCCsGAQUFBzAChjlodHRw
# Oi8vY3J0LnNlY3RpZ28uY29tL1NlY3RpZ29QdWJsaWNUaW1lU3RhbXBpbmdDQVIz
# Ni5jcnQwIwYIKwYBBQUHMAGGF2h0dHA6Ly9vY3NwLnNlY3RpZ28uY29tMA0GCSqG
# SIb3DQEBDAUAA4IBgQCw3C7J+k82TIov9slP1e8YTx+fDsa//hJ62Y6SMr2E89rv
# 82y/n8we5W6z5pfBEWozlW7nWp+sdPCdUTFw/YQcqvshH6b9Rvs9qZp5Z+V7nHwP
# TH8yzKwgKzTTG1I1XEXLAK9fHnmXpaDeVeI8K6Lw3iznWZdLQe3zl+Rejdq5l2jU
# 7iUfMkthfhFmi+VVYPkR/BXpV7Ub1QyyWebqkjSHJHRmv3lBYbQyk08/S7TlIeOr
# 9iQ+UN57fJg4QI0yqdn6PyiehS1nSgLwKRs46T8A6hXiSn/pCXaASnds0LsM5OVo
# KYfbgOOlWCvKfwUySWoSgrhncihSBXxH2pAuDV2vr8GOCEaePZc0Dy6O1rYnKjGm
# qm/IRNkJghSMizr1iIOPN+23futBXAhmx8Ji/4NTmyH9K0UvXHiuA2Pa3wZxxR9r
# 9XeIUVb2V8glZay+2ULlc445CzCvVSZV01ZB6bgvCuUuBx079gCcepjnZDCcEuIC
# 5Se4F6yFaZ8RvmiJ4hgwggaCMIIEaqADAgECAhA2wrC9fBs656Oz3TbLyXVoMA0G
# CSqGSIb3DQEBDAUAMIGIMQswCQYDVQQGEwJVUzETMBEGA1UECBMKTmV3IEplcnNl
# eTEUMBIGA1UEBxMLSmVyc2V5IENpdHkxHjAcBgNVBAoTFVRoZSBVU0VSVFJVU1Qg
# TmV0d29yazEuMCwGA1UEAxMlVVNFUlRydXN0IFJTQSBDZXJ0aWZpY2F0aW9uIEF1
# dGhvcml0eTAeFw0yMTAzMjIwMDAwMDBaFw0zODAxMTgyMzU5NTlaMFcxCzAJBgNV
# BAYTAkdCMRgwFgYDVQQKEw9TZWN0aWdvIExpbWl0ZWQxLjAsBgNVBAMTJVNlY3Rp
# Z28gUHVibGljIFRpbWUgU3RhbXBpbmcgUm9vdCBSNDYwggIiMA0GCSqGSIb3DQEB
# AQUAA4ICDwAwggIKAoICAQCIndi5RWedHd3ouSaBmlRUwHxJBZvMWhUP2ZQQRLRB
# QIF3FJmp1OR2LMgIU14g0JIlL6VXWKmdbmKGRDILRxEtZdQnOh2qmcxGzjqemIk8
# et8sE6J+N+Gl1cnZocew8eCAawKLu4TRrCoqCAT8uRjDeypoGJrruH/drCio28aq
# IVEn45NZiZQI7YYBex48eL78lQ0BrHeSmqy1uXe9xN04aG0pKG9ki+PC6VEfzutu
# 6Q3IcZZfm00r9YAEp/4aeiLhyaKxLuhKKaAdQjRaf/h6U13jQEV1JnUTCm511n5a
# vv4N+jSVwd+Wb8UMOs4netapq5Q/yGyiQOgjsP/JRUj0MAT9YrcmXcLgsrAimfWY
# 3MzKm1HCxcquinTqbs1Q0d2VMMQyi9cAgMYC9jKc+3mW62/yVl4jnDcw6ULJsBkO
# krcPLUwqj7poS0T2+2JMzPP+jZ1h90/QpZnBkhdtixMiWDVgh60KmLmzXiqJc6lG
# wqoUqpq/1HVHm+Pc2B6+wCy/GwCcjw5rmzajLbmqGygEgaj/OLoanEWP6Y52Hfle
# f3XLvYnhEY4kSirMQhtberRvaI+5YsD3XVxHGBjlIli5u+NrLedIxsE88WzKXqZj
# j9Zi5ybJL2WjeXuOTbswB7XjkZbErg7ebeAQUQiS/uRGZ58NHs57ZPUfECcgJC+v
# 2wIDAQABo4IBFjCCARIwHwYDVR0jBBgwFoAUU3m/WqorSs9UgOHYm8Cd8rIDZssw
# HQYDVR0OBBYEFPZ3at0//QET/xahbIICL9AKPRQlMA4GA1UdDwEB/wQEAwIBhjAP
# BgNVHRMBAf8EBTADAQH/MBMGA1UdJQQMMAoGCCsGAQUFBwMIMBEGA1UdIAQKMAgw
# BgYEVR0gADBQBgNVHR8ESTBHMEWgQ6BBhj9odHRwOi8vY3JsLnVzZXJ0cnVzdC5j
# b20vVVNFUlRydXN0UlNBQ2VydGlmaWNhdGlvbkF1dGhvcml0eS5jcmwwNQYIKwYB
# BQUHAQEEKTAnMCUGCCsGAQUFBzABhhlodHRwOi8vb2NzcC51c2VydHJ1c3QuY29t
# MA0GCSqGSIb3DQEBDAUAA4ICAQAOvmVB7WhEuOWhxdQRh+S3OyWM637ayBeR7djx
# Q8SihTnLf2sABFoB0DFR6JfWS0snf6WDG2gtCGflwVvcYXZJJlFfym1Doi+4PfDP
# 8s0cqlDmdfyGOwMtGGzJ4iImyaz3IBae91g50QyrVbrUoT0mUGQHbRcF57olpfHh
# QEStz5i6hJvVLFV/ueQ21SM99zG4W2tB1ExGL98idX8ChsTwbD/zIExAopoe3l6J
# rzJtPxj8V9rocAnLP2C8Q5wXVVZcbw4x4ztXLsGzqZIiRh5i111TW7HV1AtsQa6v
# Xy633vCAbAOIaKcLAo/IU7sClyZUk62XD0VUnHD+YvVNvIGezjM6CRpcWed/ODip
# tK+evDKPU2K6synimYBaNH49v9Ih24+eYXNtI38byt5kIvh+8aW88WThRpv8lUJK
# aPn37+YHYafob9Rg7LyTrSYpyZoBmwRWSE4W6iPjB7wJjJpH29308ZkpKKdpkiS9
# WNsf/eeUtvRrtIEiSJHN899L1P4l6zKVsdrUu1FX1T/ubSrsxrYJD+3f3aKg6yxd
# bugot06YwGXXiy5UUGZvOu3lXlxA+fC13dQ5OlL2gIb5lmF6Ii8+CQOYDwXM+yd9
# dbmocQsHjcRPsccUd5E9FiswEqORvz8g3s+jR3SFCgXhN4wz7NgAnOgpCdUo4uDy
# llU9PzGCBkAwggY8AgEBMGgwVDELMAkGA1UEBhMCR0IxGDAWBgNVBAoTD1NlY3Rp
# Z28gTGltaXRlZDErMCkGA1UEAxMiU2VjdGlnbyBQdWJsaWMgQ29kZSBTaWduaW5n
# IENBIFIzNgIQBkM/zMzkM6iSzBe3RqWMZTANBglghkgBZQMEAgEFAKCBhDAYBgor
# BgEEAYI3AgEMMQowCKACgAChAoAAMBkGCSqGSIb3DQEJAzEMBgorBgEEAYI3AgEE
# MBwGCisGAQQBgjcCAQsxDjAMBgorBgEEAYI3AgEVMC8GCSqGSIb3DQEJBDEiBCB3
# VDtldlpqUJGX7ENprygJeJxr+V2oUTlguFLaSOkkcTANBgkqhkiG9w0BAQEFAASC
# AgAIu1lvIg5zEwMJ87vlGg8H5BvvXdCiQllHuZir0ULpGs/IG03o3uvL9bI8IEt0
# Elq5ePQ1qlg2z3BT3a+7+SyMChGVVoko3U60aQyjjlXPtzh7HUEKdQWDyrabHmEk
# YACxpRF/h6FoJAkQqVr5cF5d+Ey8SdfE9h9MyqcaxeWtJPEFI/qjp4sfQuJZuXpN
# saallK9xM+xKrigwTx8s7C/clWZX8OtAIIegpf8mnU5ETYS6XnvefcAMsXKBmxjI
# 01GadXwlpJ/I7d/1LrZ5OjOmqVrAWnJEOkdV5+5xJbnJInPvgnBOrLeQyR7QEU6V
# XtiD6k+LqgfU7bP8AQCouz33M2DdxmfKZPPReMuZaNI3RvNX9M49wAXey/iuy+pm
# G9+DwGT3IHCIhOrL5GimdTO8vcVkdzIFdPgAb/jOR4n8P3VRaPH1sH0rO5vHuwfG
# nO7Mrb4AMYo4sP0RXkIGWXhj0Z7VrajAYHtcSnwCedUbHxk9faDbF5Vrh4kTCt+T
# utMXrepDzK6kWtr8BX68aA/meoSzN0ZhOchWbWXxC4kPSTtNTnBHAWVKaCjxBVq4
# J+CTlKGLdj78SVMTeE8guyifYNiRkrqRCuXghKQZEIN0ry1SbpQA27wZQWVJrL8E
# XE0F54gWMGrwqCik1GSfAgPYZd3IWdLc6jPR9ec/uS/QaaGCAyIwggMeBgkqhkiG
# 9w0BCQYxggMPMIIDCwIBATBpMFUxCzAJBgNVBAYTAkdCMRgwFgYDVQQKEw9TZWN0
# aWdvIExpbWl0ZWQxLDAqBgNVBAMTI1NlY3RpZ28gUHVibGljIFRpbWUgU3RhbXBp
# bmcgQ0EgUjM2AhA6UmoshM5V5h1l/MwS2OmJMA0GCWCGSAFlAwQCAgUAoHkwGAYJ
# KoZIhvcNAQkDMQsGCSqGSIb3DQEHATAcBgkqhkiG9w0BCQUxDxcNMjUwMTAzMDQx
# MzI4WjA/BgkqhkiG9w0BCQQxMgQwKlrwmwJXs6mQ/rkiyCJkh8uDUj2DERGpO4aA
# iq1TY56EYG/QvCktwMVZYRTI8Cu1MA0GCSqGSIb3DQEBAQUABIICAGyQ98hsyLlN
# dOnWHLSncZ5ovUQBXEAeeMxuRhB3G7bpdcpi4a/t7q+tLzLH2CKrpCZ5dS0tbRd9
# ng7kH4X8WbllElxzvnnwAmPrYDfX/VO8bVS/JJ8qmfrtgHhs+4dtMFkV11VXNRYs
# IC2xbYMbwuH7cWA/L+muDADM0VLONKB3HECPhbIhfHCSKJr8i2cYyW3AixC77SPG
# mimDoh3umEJLjMvCsGJ32BrVbXKR4hBCOxmDN470nxKDuEurW6JvDh6GjevcdzeT
# ungxb0pnFIfj5IMhjjrZhccCQK1ekvFLmb/ybzfZGbXGb8QHJP4JvFm8sWNAccIn
# dQ3SixhyKW+gOC2r0URTPrIDpbdHuGnm3LVBPhBijrJrv7w1/neEQFFLFXfrjgJb
# K8zc2IMYPW+Vn/hclNGfK8EOBqLGvsn7xB0K1f13kVt7QT36nIYojIE5MwWhvPDi
# 1oIDdJEvZS/P8tmX6lwAXxPRQagPVhNefUUHk11/kUaBUGGi+bRpKOgcS1fPqsP1
# 1RQMSEzjsnGEzLxxqa+AtxLDre2vBJsllCD4JJwrpkQW+xu014bZleJ3i1IFDUaR
# E16d6Qi4KEC0RfZ1NwahKQFqBj3wBCMLGEY6fNndE4jVGO0BkP3vO23pt7n4xmM+
# swJJsR48RDIDkSiV+2vPvlk/9KXVW9ji
# SIG # End signature block

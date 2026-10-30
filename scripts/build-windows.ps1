param([int]$Jobs = 4)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$Root = Split-Path $PSScriptRoot -Parent
Set-Location $Root
function Run([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}
if (!(Get-Command cmake -ErrorAction SilentlyContinue)) { throw 'Install CMake 3.28+ and add it to PATH.' }
if (!(Get-Command git -ErrorAction SilentlyContinue)) { throw 'Install Git for Windows and add it to PATH.' }
$Deps = Join-Path $Root '.deps'
New-Item -ItemType Directory -Force $Deps | Out-Null
$OpenCVSource = Join-Path $Deps 'opencv-source'
$OpenCVBuild = Join-Path $Deps 'opencv-build'
$OpenCVInstall = Join-Path $Deps 'opencv-install'
if (!(Test-Path "$OpenCVSource/CMakeLists.txt")) {
    Run 'git' @('clone','--depth','1','--branch','4.12.0','https://github.com/opencv/opencv.git',$OpenCVSource)
}
$Commit = (& git -C $OpenCVSource rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $Commit -ne '49486f61fb25722cbcf586b7f4320921d46fb38e') { throw 'Unexpected OpenCV source revision.' }
Run 'cmake' @('-S',$OpenCVSource,'-B',$OpenCVBuild,'-G','Visual Studio 17 2022','-A','x64',
    "-DCMAKE_INSTALL_PREFIX=$OpenCVInstall",'-DBUILD_LIST=core,imgproc,dnn,objdetect',
    '-DBUILD_SHARED_LIBS=OFF','-DBUILD_WITH_STATIC_CRT=OFF','-DBUILD_TESTS=OFF','-DBUILD_PERF_TESTS=OFF',
    '-DBUILD_EXAMPLES=OFF','-DBUILD_opencv_apps=OFF','-DWITH_IPP=OFF','-DWITH_ITT=OFF',
    '-DWITH_OPENCL=OFF','-DWITH_FFMPEG=OFF','-DWITH_MSMF=OFF','-DWITH_DSHOW=OFF',
    '-DWITH_JASPER=OFF','-DWITH_JPEG=OFF','-DWITH_TIFF=OFF','-DWITH_WEBP=OFF','-DWITH_OPENJPEG=OFF','-DWITH_PNG=OFF','-DWITH_OPENEXR=OFF',
    '-DWITH_QT=OFF','-DWITH_GTK=OFF','-DWITH_ADE=OFF','-DCPU_DISPATCH=',
    '-DCMAKE_POLICY_VERSION_MINIMUM=3.5')
Run 'cmake' @('--build',$OpenCVBuild,'--config','Release','--parallel',"$Jobs")
Run 'cmake' @('--install',$OpenCVBuild,'--config','Release')
Run 'cmake' @('--preset','windows-x64',"-DOpenCV_DIR=$OpenCVInstall",'-DOpenCV_STATIC=ON','-DBUILD_TESTING=ON')
Run 'cmake' @('--build','build_x64','--config','Release','--parallel',"$Jobs")
Run 'ctest' @('--test-dir','build_x64','-C','Release','--output-on-failure')
$Package = Join-Path $Root 'package'
Run 'cmake' @('--install','build_x64','--config','Release','--prefix',$Package)
Copy-Item 'scripts/Install-SteadyFrame.ps1' $Package -Force
Copy-Item 'scripts/Install.cmd' $Package -Force
Copy-Item 'QUICKSTART.md','LICENSE','THIRD-PARTY.md' $Package -Force
# Include the license of statically linked OpenCV, and template license attribution.
Copy-Item 'LICENSE-OBS-TEMPLATE' $Package -Force
Copy-Item "$OpenCVSource/LICENSE" (Join-Path $Package 'LICENSE-OpenCV') -Force
$Licenses = Join-Path $Package 'third-party-licenses'
New-Item -ItemType Directory -Force $Licenses | Out-Null
foreach ($Dependency in @('protobuf','flatbuffers','zlib')) {
    $Folder = Join-Path $OpenCVSource "3rdparty/$Dependency"
    Get-ChildItem $Folder -File -Recurse | Where-Object { $_.Name -match '^(LICENSE|COPYING|NOTICE)' } | ForEach-Object {
        $Relative = $_.FullName.Substring($Folder.Length).TrimStart([char[]]"/\")
        $Destination = Join-Path $Licenses "$Dependency/$Relative"
        New-Item -ItemType Directory -Force (Split-Path $Destination -Parent) | Out-Null
        Copy-Item $_.FullName $Destination -Force
    }
}
$Zip = Join-Path $Root 'SteadyFrame-0.1.0-Windows-x64.zip'
Compress-Archive -Path "$Package/*" -DestinationPath $Zip -Force
Write-Host "Built $Zip"

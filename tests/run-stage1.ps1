$ErrorActionPreference = 'Stop'
$savedTestEnvironment = @{}
foreach ($name in @('PATH', 'QT_QPA_PLATFORM', 'QT_QPA_FONTDIR')) {
    $savedTestEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}
$qtRoot = 'E:/qt5.15/5.15.2/mingw81_64'
$env:PATH = "$qtRoot/bin;E:/qt5.15/Tools/mingw810_64/bin;" + $env:PATH
Push-Location "$PSScriptRoot/../build-fix"
try {
    qmake ../Weather.pro
    if ($LASTEXITCODE) { throw 'qmake failed' }
    mingw32-make -j4
    if ($LASTEXITCODE) { throw 'build failed' }
    g++ -std=c++11 -fPIC -I../ -I. "-I$qtRoot/include" "-I$qtRoot/include/QtWidgets" "-I$qtRoot/include/QtGui" "-I$qtRoot/include/QtCore" "-I$qtRoot/include/QtNetwork" "-I$qtRoot/include/QtTest" ../tests/stage1-test.cpp release/weather.o release/WeatherData.o release/WeatherCache.o release/moc_weather.o release/qrc_citydata.o release/qrc_icons.o release/qrc_images.o release/qrc_weatypeico_manual.o "-L$qtRoot/lib" -lQt5Widgets -lQt5Gui -lQt5Network -lQt5Core -lQt5Test -o stage1-test.exe
    if ($LASTEXITCODE) { throw 'test compile failed' }
    $env:QT_QPA_PLATFORM = 'offscreen'
    $systemFonts = Join-Path $env:WINDIR 'Fonts'
    if (Test-Path -LiteralPath $systemFonts -PathType Container) {
        $env:QT_QPA_FONTDIR = $systemFonts
    }
    ./stage1-test.exe
    if ($LASTEXITCODE) { throw 'tests failed' }
    Write-Host 'PASS: Stage 1 tests completed (exit code 0).' -ForegroundColor Green
} finally {
    Pop-Location
    foreach ($name in $savedTestEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($name, $savedTestEnvironment[$name], 'Process')
    }
}

# Build and optionally flash one example (Windows, PowerShell).
#
#   .\build.ps1 -Example 01-led-blink
#   .\build.ps1 -Example 02-button-led -Flash
#
# Needs arm-none-eabi-gcc and STM32_Programmer_CLI. STM32CubeIDE ships both;
# point -ToolsRoot at its plugins folder, or put the tools on PATH and pass
# -ToolsRoot ''. On Linux/macOS use the plain gcc command from the README.
param(
    [Parameter(Mandatory = $true)][string]$Example,
    [switch]$Flash,
    [string]$ToolsRoot = 'C:\ST\STM32CubeIDE_2.2.0\STM32CubeIDE\plugins'
)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

if ($ToolsRoot) {
    $gcc  = (Get-ChildItem $ToolsRoot -Recurse -Filter 'arm-none-eabi-gcc.exe' | Select-Object -First 1).FullName
    $prog = (Get-ChildItem $ToolsRoot -Recurse -Filter 'STM32_Programmer_CLI.exe' | Select-Object -First 1).FullName
} else {
    $gcc  = 'arm-none-eabi-gcc'
    $prog = 'STM32_Programmer_CLI'
}

$src = "examples\$Example\main.c"
if (-not (Test-Path $src)) { throw "Unknown example: $Example (looked for $src)" }

New-Item -ItemType Directory -Force build | Out-Null
$elf = "build\$Example.elf"

& $gcc -mcpu=cortex-m4 -mthumb -O1 -g -Wall -Wextra -nostdlib `
    -Icommon -T common\stm32f401re.ld `
    common\startup.c common\systick.c $src -o $elf
if ($LASTEXITCODE) { throw 'Compilation failed' }

if ($Flash) {
    & $prog -c port=SWD -w $elf -v -rst
    if ($LASTEXITCODE) { throw 'Flashing failed (is the board connected?)' }
}
Write-Host "Built $elf"

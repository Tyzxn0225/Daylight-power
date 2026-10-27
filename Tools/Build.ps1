param(
    [Parameter(Mandatory=$true)][string]$EngineRoot,
    [ValidateSet('Editor','Game')][string]$Target='Editor'
)
$ErrorActionPreference='Stop'
$taskProject=Split-Path -Parent $PSScriptRoot
$taskBuild=Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
if(!(Test-Path -LiteralPath $taskBuild)){throw 'EngineRoot must point to a valid Unreal Engine installation.'}
$taskTarget=if($Target -eq 'Editor'){'DaylightRightsEditor'}else{'DaylightRights'}
& $taskBuild $taskTarget Win64 Development "-Project=$taskProject\DaylightRights.uproject" -WaitMutex -NoHotReloadFromIDE -MaxParallelActions=1
exit $LASTEXITCODE

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$lunara = Join-Path $root "build\Debug\lunara.exe"

if (-not (Test-Path $lunara)) {
    throw "lunara.exe not found at $lunara"
}

$patternOutput = (& $lunara (Join-Path $root "examples\pattern_match_defer_demo.lunara")) | Out-String
if ($LASTEXITCODE -ne 0) {
    throw "pattern_match_defer_demo failed"
}
if ($patternOutput -notmatch '"refund": "caught:refund_blocked"') {
    throw "pattern match / throw flow failed"
}
if ($patternOutput -notmatch '"custom": "other:shipment"') {
    throw "binding pattern failed"
}
if ($patternOutput -notmatch '"trail": "try;finally;func;try;finally;func;"') {
    throw "defer/finally ordering failed"
}

$destructuringOutput = (& $lunara (Join-Path $root "examples\match_destructuring_generics_demo.lunara")) | Out-String
if ($LASTEXITCODE -ne 0) {
    throw "match_destructuring_generics_demo failed"
}
if ($destructuringOutput -notmatch '"currency": "TRY"') {
    throw "nested match destructuring failed"
}
if ($destructuringOutput -notmatch '"tag_count": 3') {
    throw "generic list runtime flow failed"
}

$packageOutput = (& $lunara (Join-Path $root "examples\package_demo\src\main.lunara")) | Out-String
if ($LASTEXITCODE -ne 0) {
    throw "package_demo failed"
}
if ($packageOutput -notmatch '"package": "shopkit"') {
    throw "package self import failed"
}
if ($packageOutput -notmatch '"currency": "TRY"') {
    throw "dependency package import failed"
}

$registryOutput = (& $lunara (Join-Path $root "examples\package_registry_demo\src\main.lunara")) | Out-String
if ($LASTEXITCODE -ne 0) {
    throw "package_registry_demo failed"
}
if ($registryOutput -notmatch '"audience": "customer"') {
    throw "lock/registry package import failed"
}

$analysisOutput = (& $lunara analyze (Join-Path $root "examples\analyzer_type_error_demo.lunara") 2>&1 | Out-String)
if ($LASTEXITCODE -ne 0) {
    throw "analyze command failed"
}
if ($analysisOutput -notmatch "Variable 'title' does not match declared type") {
    throw "missing variable type diagnostic"
}
if ($analysisOutput -notmatch "Type mismatch in call to 'total_price' for argument 1") {
    throw "missing function argument diagnostic"
}

$genericAnalysisOutput = (& $lunara analyze (Join-Path $root "examples\analyzer_generic_type_error_demo.lunara") 2>&1 | Out-String)
if ($LASTEXITCODE -ne 0) {
    throw "generic analyze command failed"
}
if ($genericAnalysisOutput -notmatch "Variable 'tags' does not match declared type") {
    throw "missing generic list diagnostic"
}
if ($genericAnalysisOutput -notmatch "Variable 'labels' does not match declared type") {
    throw "missing generic object diagnostic"
}

$previousPreference = $ErrorActionPreference
$ErrorActionPreference = "Continue"
$parserOutput = (& $lunara check (Join-Path $root "examples\parser_recovery_demo.lunara") 2>&1 | Out-String)
$ErrorActionPreference = $previousPreference
if ($LASTEXITCODE -eq 0) {
    throw "parser recovery demo should fail"
}
if ($parserOutput -notmatch "Parser found") {
    throw "missing aggregated parser error output"
}
if ($parserOutput -notmatch "\^\^\^\^") {
    throw "missing parser code frame caret output"
}

Write-Host "Core expansion smoke test passed"

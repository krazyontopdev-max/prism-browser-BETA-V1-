param([int]$RootProcessId)
$processes = Get-CimInstance Win32_Process | Select-Object ProcessId,ParentProcessId,Name,CommandLine
$ids = [System.Collections.Generic.HashSet[int]]::new()
[void]$ids.Add($RootProcessId)
do {
    $changed = $false
    foreach ($item in $processes) {
        if ($ids.Contains([int]$item.ParentProcessId) -and $ids.Add([int]$item.ProcessId)) { $changed = $true }
    }
} while ($changed)
$working = 0.0
$private = 0.0
$cpu = 0.0
$count = 0
$cpuByPid = @{}
$cpuByIdentity = @{}
$details = @()
foreach ($processId in $ids) {
    $item = Get-Process -Id $processId -ErrorAction SilentlyContinue
    if ($item) {
        $working += $item.WorkingSet64 / 1MB
        $private += $item.PrivateMemorySize64 / 1MB
        $cpu += $item.CPU
        $cpuByPid[[string]$processId] = $item.CPU
        $identity='{0}:{1}' -f $processId,$item.StartTime.ToUniversalTime().Ticks
        $cpuByIdentity[$identity]=$item.CPU
        $metadata=$processes | Where-Object ProcessId -eq $processId
        $role=if($processId -eq $RootProcessId){'browser'}elseif($metadata.CommandLine -match '--type=([^ ]+)'){$Matches[1]}else{'other'}
        $details+=@{pid=$processId;identity=$identity;name=$item.ProcessName;role=$role;privateMiB=$item.PrivateMemorySize64/1MB}
        $count++
    }
}
@{workingSetMiB=$working; privateMiB=$private; cpuSeconds=$cpu; cpuByPid=$cpuByPid; cpuByIdentity=$cpuByIdentity; processes=$count; details=$details} | ConvertTo-Json -Depth 5 -Compress

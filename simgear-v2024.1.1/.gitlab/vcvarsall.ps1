# copied from KitWare's VTK CI scripts

echo "Running VCVarsAll wrapper"

cmd /c "`"$env:VCVARSALL`" $env:VCVARSPLATFORM -vcvars_ver=$env:VCVARSVERSION & set" |
      foreach {
          if ($_ -match "=") {
              $v = $_.split("=")
              [Environment]::SetEnvironmentVariable($v[0], $v[1])
          }
      }

echo "Path is now: $env:Path"

ssh WINDOWS_PC "psexec64 -s -i 1 -w C:\\v4d_build\\$1 cmd.exe /Q /C \"C:\\v4d_build\\$1\\$2.exe || pause\""

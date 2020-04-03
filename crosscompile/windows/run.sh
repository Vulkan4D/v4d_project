ssh WINDOWS_PC "START /wait taskkill /f /im demo.exe" &&\
ssh WINDOWS_PC "START /wait taskkill /f /im incubator.exe" &&\
ssh WINDOWS_PC "START /wait taskkill /f /im tests.exe" &&\
ssh WINDOWS_PC "del /q /s C:\\v4d_build\\$1\\*.exe"
scp -rq $1/* WINDOWS_PC:/v4d_build/$1/ &&\
ssh WINDOWS_PC "psexec64 -s -i 1 -w C:\\v4d_build\\$1 cmd.exe /Q /C \"C:\\v4d_build\\$1\\$2.exe || pause\""

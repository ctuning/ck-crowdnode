call ck set env tags=compiler,lang-c --local --bat_file=tmp-ck-env.bat --bat_new --print && call tmp-ck-env.bat && del /Q tmp-ck-env.bat
if %errorlevel% neq 0 exit /b %errorlevel%

call ck set env tags=tool,cmake --local --bat_file=tmp-ck-env.bat --bat_new --print && call tmp-ck-env.bat && del /Q tmp-ck-env.bat
if %errorlevel% neq 0 exit /b %errorlevel%

mkdir build
cd build

cmake ..

msbuild /p:Configuration=Release /p:Platform=Win32 Project.sln

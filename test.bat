del /s /q build\*
rmdir /s /q build\
mkdir build\
python ./build-scripts/gen_tests.py || goto :error
meson setup build -Dtests=true -Dmath_library=false || goto :error
cd build
ninja || goto :error_late
meson test || goto :error_late
cd ..
goto :EOF

:error_late
cd ..
:error
echo Failed with error #%errorlevel%.

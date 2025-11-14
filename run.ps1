Write-Host "=== Building mycc ==="
gcc -std=c99 -O2 -Iinclude src/*.c -o mycc

Write-Host "=== Compiling examples/countdown.my to countdown.asm ==="
.\mycc examples\countdown.my -o countdown

Write-Host "=== Assembling countdown.asm ==="
nasm -f elf64 countdown.asm -o countdown.o

Write-Host "=== Building runtime ==="
gcc -c src/runtime.c -o runtime.o

Write-Host "=== Linking final executable countdown.exe ==="
gcc countdown.o runtime.o -o countdown.exe

Write-Host "=== Running countdown.exe ==="
.\countdown.exe
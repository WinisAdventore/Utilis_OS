./bmp2asm image.bmp image.asm
./bmp2asm твой_изоброжение.bmp код_asm.asm
nasm -f bin bios_example.asm -o bootloader.bin
linux
bmp2asm.exe image.bmp image.asm
bmp2asm.exe твой_изоброжение.bmp код_asm.asm
nasm -f bin bios_example.asm -o bootloader.bin
windows
gcc -o bmp2asm bmp2asm.c
nasm -f bin bios_image.asm -o bootloader.bin
fasm -f bin bios_image.asm -o bootloader.bin


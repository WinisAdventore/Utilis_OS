#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#pragma pack(push, 1)
typedef struct {
    uint16_t signature;
    uint32_t fileSize;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t dataOffset;
} BMPHeader;

typedef struct {
    uint32_t headerSize;
    uint32_t width;
    uint32_t height;
    uint16_t planes;
    uint16_t bitsPerPixel;
    uint32_t compression;
    uint32_t imageSize;
    uint32_t xPixelsPerM;
    uint32_t yPixelsPerM;
    uint32_t colorsUsed;
    uint32_t importantColors;
} DIBHeader;
#pragma pack(pop)

typedef struct {
    uint8_t r, g, b;
} RGBColor;

// Функция для отладки - сохраняет сырые данные
void debug_save_raw(const char* filename, uint8_t* data, int size) {
    FILE* f = fopen(filename, "wb");
    if (f) {
        fwrite(data, 1, size, f);
        fclose(f);
        printf("  Отладочный файл сохранен: %s\n", filename);
    }
}

RGBColor getPixel(FILE* file, int x, int y, int width, int height, 
                  int bitsPerPixel, int rowSize, int dataOffset, 
                  RGBColor* palette, int paletteSize) {
    
    RGBColor color = {0, 0, 0};
    long pos = dataOffset + (height - 1 - y) * rowSize;
    
    switch(bitsPerPixel) {
        case 1: {
            int bytePos = x / 8;
            int bitPos = 7 - (x % 8); // BMP использует MSB first
            fseek(file, pos + bytePos, SEEK_SET);
            uint8_t byte;
            fread(&byte, 1, 1, file);
            int index = (byte >> bitPos) & 1;
            if (index < paletteSize) {
                color = palette[index];
                // Инвертируем для наглядности если нужно
                // if (index == 0) { color.r = 255; color.g = 255; color.b = 255; }
            }
            break;
        }
        case 4: {
            int bytePos = x / 2;
            fseek(file, pos + bytePos, SEEK_SET);
            uint8_t byte;
            fread(&byte, 1, 1, file);
            int index = (x % 2 == 0) ? (byte >> 4) & 0x0F : byte & 0x0F;
            if (index < paletteSize) color = palette[index];
            break;
        }
        case 8: {
            fseek(file, pos + x, SEEK_SET);
            uint8_t index;
            fread(&index, 1, 1, file);
            if (index < paletteSize) color = palette[index];
            break;
        }
        case 24: {
            fseek(file, pos + x * 3, SEEK_SET);
            uint8_t bgr[3];
            fread(bgr, 3, 1, file);
            color.b = bgr[0];
            color.g = bgr[1];
            color.r = bgr[2];
            break;
        }
    }
    return color;
}

void convertBMPtoMultisector(const char* inputFile, const char* baseName) {
    FILE* bmpFile = fopen(inputFile, "rb");
    if (!bmpFile) {
        printf("Ошибка: Не удалось открыть файл %s\n", inputFile);
        return;
    }
    
    BMPHeader bmpHeader;
    DIBHeader dibHeader;
    
    fread(&bmpHeader, sizeof(BMPHeader), 1, bmpFile);
    fread(&dibHeader, sizeof(DIBHeader), 1, bmpFile);
    
    if (bmpHeader.signature != 0x4D42) {
        printf("Ошибка: Неверный формат файла\n");
        fclose(bmpFile);
        return;
    }
    
    int width = dibHeader.width;
    int height = dibHeader.height;
    int bitsPerPixel = dibHeader.bitsPerPixel;
    
    printf("Информация о BMP: %d x %d, %d бит\n", width, height, bitsPerPixel);
    printf("Смещение данных: %d\n", bmpHeader.dataOffset);
    
    // Читаем палитру
    RGBColor* palette = NULL;
    int paletteSize = 0;
    
    if (bitsPerPixel <= 8) {
        paletteSize = (bitsPerPixel == 1) ? 2 : 
                     (bitsPerPixel == 4) ? 16 : 
                     (bitsPerPixel == 8) ? 256 : 0;
        
        printf("Размер палитры: %d цветов\n", paletteSize);
        
        if (paletteSize > 0) {
            palette = (RGBColor*)malloc(paletteSize * sizeof(RGBColor));
            fseek(bmpFile, sizeof(BMPHeader) + dibHeader.headerSize, SEEK_SET);
            
            for (int i = 0; i < paletteSize; i++) {
                uint8_t bgra[4];
                fread(bgra, 4, 1, bmpFile);
                palette[i].b = bgra[0];
                palette[i].g = bgra[1];
                palette[i].r = bgra[2];
                printf("  Цвет %d: R=%d G=%d B=%d\n", i, palette[i].r, palette[i].g, palette[i].b);
            }
        }
    }
    
    int rowSize = ((width * bitsPerPixel + 31) / 32) * 4;
    printf("Размер строки: %d байт\n", rowSize);
    
    int totalPixels = width * height;
    uint8_t* pixels = (uint8_t*)malloc(totalPixels);
    uint8_t* raw_pixels = (uint8_t*)malloc(totalPixels); // для отладки
    
    // Находим уникальные цвета
    RGBColor uniqueColors[256];
    int colorCount = 0;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            RGBColor color = getPixel(bmpFile, x, y, width, height, 
                                     bitsPerPixel, rowSize, 
                                     bmpHeader.dataOffset, 
                                     palette, paletteSize);
            
            // Сохраняем raw значение для отладки
            if (bitsPerPixel == 1) {
                int bytePos = x / 8;
                int bitPos = 7 - (x % 8);
                fseek(bmpFile, bmpHeader.dataOffset + (height - 1 - y) * rowSize + bytePos, SEEK_SET);
                uint8_t byte;
                fread(&byte, 1, 1, bmpFile);
                raw_pixels[y * width + x] = (byte >> bitPos) & 1;
            }
            
            int found = -1;
            for (int i = 0; i < colorCount; i++) {
                if (uniqueColors[i].r == color.r && 
                    uniqueColors[i].g == color.g && 
                    uniqueColors[i].b == color.b) {
                    found = i;
                    break;
                }
            }
            
            if (found == -1 && colorCount < 256) {
                found = colorCount;
                uniqueColors[colorCount].r = color.r;
                uniqueColors[colorCount].g = color.g;
                uniqueColors[colorCount].b = color.b;
                colorCount++;
            }
            
            pixels[y * width + x] = (found == -1) ? 0 : found;
        }
    }
    
    // Сохраняем отладочные файлы
    debug_save_raw("debug_pixels.raw", pixels, totalPixels);
    if (bitsPerPixel == 1) {
        debug_save_raw("debug_raw.raw", raw_pixels, totalPixels);
    }
    
    if (palette) free(palette);
    fclose(bmpFile);
    
    printf("Уникальных цветов: %d\n", colorCount);
    for (int i = 0; i < colorCount; i++) {
        printf("  Цвет %d: R=%d G=%d B=%d\n", i, uniqueColors[i].r, uniqueColors[i].g, uniqueColors[i].b);
    }
    
    // Вычисляем нужное количество секторов
    int dataSize = colorCount * 3 + totalPixels;
    int sectorsNeeded = (dataSize + 511) / 512;
    if (sectorsNeeded < 1) sectorsNeeded = 1;
    
    printf("Данные займут: %d байт\n", dataSize);
    printf("Нужно секторов: %d\n", sectorsNeeded);
    
    // Создаем файл загрузчика
    char bootFileName[256];
    sprintf(bootFileName, "%s_boot.asm", baseName);
    
    FILE* bootFile = fopen(bootFileName, "w");
    if (!bootFile) {
        printf("Ошибка: Не удалось создать файл %s\n", bootFileName);
        free(pixels);
        free(raw_pixels);
        return;
    }
    
    fprintf(bootFile, "; Bootloader for %s\n", inputFile);
    fprintf(bootFile, "org 0x7C00\n");
    fprintf(bootFile, "bits 16\n\n");
    
    fprintf(bootFile, "start:\n");
    fprintf(bootFile, "    ; Save boot drive\n");
    fprintf(bootFile, "    mov [bootdrv], dl\n\n");
    
    fprintf(bootFile, "    ; Set video mode 13h\n");
    fprintf(bootFile, "    mov ax, 0x0013\n");
    fprintf(bootFile, "    int 0x10\n\n");
    
    fprintf(bootFile, "    ; Load image data from disk\n");
    fprintf(bootFile, "    mov ax, 0x1000\n");
    fprintf(bootFile, "    mov es, ax\n");
    fprintf(bootFile, "    xor bx, bx\n");
    fprintf(bootFile, "    mov ah, 2\n");
    fprintf(bootFile, "    mov al, %d\n", sectorsNeeded);
    fprintf(bootFile, "    mov ch, 0\n");
    fprintf(bootFile, "    mov cl, 2\n");
    fprintf(bootFile, "    mov dh, 0\n");
    fprintf(bootFile, "    mov dl, [bootdrv]\n");
    fprintf(bootFile, "    int 0x13\n");
    fprintf(bootFile, "    jc disk_error\n\n");
    
    fprintf(bootFile, "    ; Set palette\n");
    fprintf(bootFile, "    push 0x1000\n");
    fprintf(bootFile, "    pop ds\n");
    fprintf(bootFile, "    xor si, si\n");
    fprintf(bootFile, "    mov cx, %d\n", colorCount);
    fprintf(bootFile, "    xor bx, bx\n");
    fprintf(bootFile, "set_palette:\n");
    fprintf(bootFile, "    mov dx, 0x3C8\n");
    fprintf(bootFile, "    mov al, bl\n");
    fprintf(bootFile, "    out dx, al\n");
    fprintf(bootFile, "    inc dx\n");
    fprintf(bootFile, "    mov al, [si]\n");
    fprintf(bootFile, "    shr al, 2\n");
    fprintf(bootFile, "    out dx, al\n");
    fprintf(bootFile, "    inc si\n");
    fprintf(bootFile, "    mov al, [si]\n");
    fprintf(bootFile, "    shr al, 2\n");
    fprintf(bootFile, "    out dx, al\n");
    fprintf(bootFile, "    inc si\n");
    fprintf(bootFile, "    mov al, [si]\n");
    fprintf(bootFile, "    shr al, 2\n");
    fprintf(bootFile, "    out dx, al\n");
    fprintf(bootFile, "    inc si\n");
    fprintf(bootFile, "    inc bx\n");
    fprintf(bootFile, "    loop set_palette\n\n");
    
    // Центрирование
    int x_offset = (320 - width) / 2;
    int y_offset = (200 - height) / 2;
    int start_offset = y_offset * 320 + x_offset;
    
    fprintf(bootFile, "    ; Copy image to video memory\n");
    fprintf(bootFile, "    mov ax, 0xA000\n");
    fprintf(bootFile, "    mov es, ax\n");
    fprintf(bootFile, "    mov di, %d\n", start_offset);
    fprintf(bootFile, "    mov si, %d\n", colorCount * 3);
    fprintf(bootFile, "    mov cx, %d\n", totalPixels);
    fprintf(bootFile, "    cld\n");
    fprintf(bootFile, "    rep movsb\n\n");
    
    fprintf(bootFile, "    ; Draw border to verify position\n");
    fprintf(bootFile, "    mov ax, 0xA000\n");
    fprintf(bootFile, "    mov es, ax\n");
    fprintf(bootFile, "    mov di, %d\n", start_offset - 321);
    fprintf(bootFile, "    mov al, 15  ; White\n");
    fprintf(bootFile, "    mov cx, %d\n", width + 2);
    fprintf(bootFile, "    rep stosb\n\n");
    
    fprintf(bootFile, "    ; Wait for key\n");
    fprintf(bootFile, "    xor ax, ax\n");
    fprintf(bootFile, "    int 0x16\n\n");
    
    fprintf(bootFile, "    ; Return to text mode\n");
    fprintf(bootFile, "    mov ax, 0x0003\n");
    fprintf(bootFile, "    int 0x10\n\n");
    
    fprintf(bootFile, "    ret\n\n");
    
    fprintf(bootFile, "disk_error:\n");
    fprintf(bootFile, "    mov si, errmsg\n");
    fprintf(bootFile, "print:\n");
    fprintf(bootFile, "    lodsb\n");
    fprintf(bootFile, "    or al, al\n");
    fprintf(bootFile, "    jz hang\n");
    fprintf(bootFile, "    mov ah, 0x0E\n");
    fprintf(bootFile, "    int 0x10\n");
    fprintf(bootFile, "    jmp print\n");
    fprintf(bootFile, "hang:\n");
    fprintf(bootFile, "    jmp hang\n\n");
    
    fprintf(bootFile, "errmsg db 'Disk error!', 0\n");
    fprintf(bootFile, "bootdrv db 0\n\n");
    
    fprintf(bootFile, "times 510-($-$$) db 0\n");
    fprintf(bootFile, "dw 0xAA55\n");
    
    fclose(bootFile);
    
    // Создаем файл данных
    char dataFileName[256];
    sprintf(dataFileName, "%s_data.asm", baseName);
    
    FILE* dataFile = fopen(dataFileName, "w");
    if (!dataFile) {
        printf("Ошибка: Не удалось создать файл %s\n", dataFileName);
        free(pixels);
        free(raw_pixels);
        return;
    }
    
    fprintf(dataFile, "; Image data for %s\n", inputFile);
    fprintf(dataFile, "org 0x1000\n");
    fprintf(dataFile, "bits 16\n\n");
    
    fprintf(dataFile, "section .data\n");
    fprintf(dataFile, "data_start:\n\n");
    
    // Палитра
    fprintf(dataFile, "; Palette (%d colors)\n", colorCount);
    for (int i = 0; i < colorCount; i++) {
        if (i % 4 == 0) fprintf(dataFile, "    db ");
        fprintf(dataFile, "0x%02X, 0x%02X, 0x%02X", 
                uniqueColors[i].r, uniqueColors[i].g, uniqueColors[i].b);
        if (i % 4 == 3 || i == colorCount - 1) 
            fprintf(dataFile, "\n");
        else 
            fprintf(dataFile, ", ");
    }
    fprintf(dataFile, "\n");
    
    // Данные изображения
    fprintf(dataFile, "\n; Image data (%d x %d = %d pixels)\n", width, height, totalPixels);
    fprintf(dataFile, "image_data:\n");
    for (int i = 0; i < totalPixels; i++) {
        if (i % 16 == 0) {
            if (i > 0) fprintf(dataFile, "\n");
            fprintf(dataFile, "    db ");
        } else {
            fprintf(dataFile, ", ");
        }
        fprintf(dataFile, "0x%02X", pixels[i]);
    }
    fprintf(dataFile, "\n\n");
    
    // Заполнение до размера сектора
    int currentPos = colorCount * 3 + totalPixels;
    int padding = 512 - (currentPos % 512);
    if (padding < 512) {
        fprintf(dataFile, "; Padding to sector boundary\n");
        fprintf(dataFile, "times %d db 0\n", padding);
    }
    
    fclose(dataFile);
    
    // Создаем скрипт для сборки
    char buildFileName[256];
    sprintf(buildFileName, "%s_build.bat", baseName);
    
    FILE* buildFile = fopen(buildFileName, "w");
    if (buildFile) {
        fprintf(buildFile, "@echo off\n");
        fprintf(buildFile, "echo Сборка загрузчика...\n");
        fprintf(buildFile, "nasm -f bin %s_boot.asm -o boot.bin\n", baseName);
        fprintf(buildFile, "if errorlevel 1 goto error\n");
        fprintf(buildFile, "nasm -f bin %s_data.asm -o data.bin\n", baseName);
        fprintf(buildFile, "if errorlevel 1 goto error\n");
        fprintf(buildFile, "copy /b boot.bin + data.bin bootloader.bin\n");
        fprintf(buildFile, "echo.\n");
        fprintf(buildFile, "echo Размер bootloader.bin:\n");
        fprintf(buildFile, "dir bootloader.bin\n");
        fprintf(buildFile, "echo.\n");
        fprintf(buildFile, "echo Запуск в QEMU...\n");
        fprintf(buildFile, "qemu-system-x86_64 -drive format=raw,file=bootloader.bin -vga std\n");
        fprintf(buildFile, "goto end\n");
        fprintf(buildFile, ":error\n");
        fprintf(buildFile, "echo Ошибка сборки!\n");
        fprintf(buildFile, "pause\n");
        fprintf(buildFile, ":end\n");
        fclose(buildFile);
    }
    
    printf("\n=== РЕЗУЛЬТАТ ===\n");
    printf("Файлы созданы:\n");
    printf("  %s - загрузчик\n", bootFileName);
    printf("  %s - данные\n", dataFileName);
    printf("  %s - скрипт сборки\n", buildFileName);
    printf("\nДля отладки сохранены:\n");
    printf("  debug_pixels.raw - обработанные пиксели\n");
    if (bitsPerPixel == 1) {
        printf("  debug_raw.raw - сырые биты из BMP\n");
    }
    printf("\nЗапустите:\n");
    printf("  %s\n", buildFileName);
    
    free(pixels);
    free(raw_pixels);
}

int main(int argc, char* argv[]) {
    printf("BMP to Multisector ASM Converter v2.0 (с отладкой)\n");
    printf("================================================\n\n");
    
    if (argc < 3) {
        printf("Использование: %s <input.bmp> <output_base_name>\n", argv[0]);
        printf("Пример: %s image.bmp myimage\n", argv[0]);
        printf("Создаст: myimage_boot.asm, myimage_data.asm, myimage_build.bat\n");
        return 1;
    }
    
    convertBMPtoMultisector(argv[1], argv[2]);
    
    return 0;
}
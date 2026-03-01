#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <png.h>

#pragma pack(push, 1)
typedef struct {
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} BMPFileHeader;

typedef struct {
    uint32_t biSize;
    int32_t biWidth;
    int32_t biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t biXPelsPerMeter;
    int32_t biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} BMPInfoHeader;
#pragma pack(pop)

// Функция для определения является ли цвет белым
int isWhiteColor(uint8_t r, uint8_t g, uint8_t b) {
    return (r > 240 && g > 240 && b > 240);
}

// Конвертация 1-битного BMP в RGBA массив (с учетом переворота)
uint8_t* convert1BitToRGBA(uint8_t* indexedData, uint8_t* palette, int paletteSize,
                          int width, int height, int rowSize) {
    uint8_t* rgbaData = (uint8_t*)malloc(width * height * 4);
    if (!rgbaData) return NULL;
    
    // Если палитры нет, создаем стандартную черно-белую
    uint8_t localPalette[8][4] = {0};
    if (palette == NULL || paletteSize == 0) {
        localPalette[0][0] = 0;    // B - черный
        localPalette[0][1] = 0;    // G
        localPalette[0][2] = 0;    // R
        localPalette[1][0] = 255;  // B - белый
        localPalette[1][1] = 255;  // G
        localPalette[1][2] = 255;  // R
        palette = (uint8_t*)localPalette;
        paletteSize = 2;
    }
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int byteIndex = y * rowSize + (x / 8);
            int bitIndex = 7 - (x % 8);
            
            uint8_t byte = indexedData[byteIndex];
            uint8_t bit = (byte >> bitIndex) & 1;
            
            int colorIndex = bit;
            if (colorIndex >= paletteSize) colorIndex = 0;
            
            uint8_t blue = palette[colorIndex * 4];
            uint8_t green = palette[colorIndex * 4 + 1];
            uint8_t red = palette[colorIndex * 4 + 2];
            
            // Переворачиваем строки (BMP снизу вверх, PNG сверху вниз)
            int pngY = height - 1 - y;
            int idx = (pngY * width + x) * 4;
            
            rgbaData[idx] = red;
            rgbaData[idx + 1] = green;
            rgbaData[idx + 2] = blue;
            
            // Белые пиксели делаем прозрачными
            if (isWhiteColor(red, green, blue)) {
                rgbaData[idx + 3] = 0; // Прозрачный
            } else {
                rgbaData[idx + 3] = 255; // Непрозрачный
            }
        }
    }
    
    return rgbaData;
}

// Конвертация 24-битного BMP в RGBA массив (с учетом переворота)
uint8_t* convert24BitToRGBA(uint8_t* rgbData, int width, int height, int rowSize) {
    uint8_t* rgbaData = (uint8_t*)malloc(width * height * 4);
    if (!rgbaData) return NULL;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint8_t blue = rgbData[y * rowSize + x * 3];
            uint8_t green = rgbData[y * rowSize + x * 3 + 1];
            uint8_t red = rgbData[y * rowSize + x * 3 + 2];
            
            // Переворачиваем строки (BMP снизу вверх, PNG сверху вниз)
            int pngY = height - 1 - y;
            int idx = (pngY * width + x) * 4;
            
            rgbaData[idx] = red;
            rgbaData[idx + 1] = green;
            rgbaData[idx + 2] = blue;
            
            // Белые пиксели делаем прозрачными
            if (isWhiteColor(red, green, blue)) {
                rgbaData[idx + 3] = 0; // Прозрачный
            } else {
                rgbaData[idx + 3] = 255; // Непрозрачный
            }
        }
    }
    
    return rgbaData;
}

// Конвертация 32-битного BMP в RGBA массив (с учетом переворота)
uint8_t* convert32BitToRGBA(uint8_t* bgraData, int width, int height, int rowSize) {
    uint8_t* rgbaData = (uint8_t*)malloc(width * height * 4);
    if (!rgbaData) return NULL;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int srcIdx = y * rowSize + x * 4;
            
            // Переворачиваем строки (BMP снизу вверх, PNG сверху вниз)
            int pngY = height - 1 - y;
            int idx = (pngY * width + x) * 4;
            
            // BGRA -> RGBA
            rgbaData[idx] = bgraData[srcIdx + 2];     // R
            rgbaData[idx + 1] = bgraData[srcIdx + 1]; // G
            rgbaData[idx + 2] = bgraData[srcIdx];     // B
            rgbaData[idx + 3] = bgraData[srcIdx + 3]; // A
        }
    }
    
    return rgbaData;
}

// Преобразование в чересстрочный формат для RGBA массива
void convertToInterlacedRGBA(uint8_t* data, int width, int height) {
    uint8_t* tempBuffer = (uint8_t*)malloc(width * height * 4);
    
    if (!tempBuffer) {
        printf("Memory allocation error\n");
        return;
    }
    
    memcpy(tempBuffer, data, width * height * 4);
    
    for (int y = 0; y < height; y++) {
        int dstY;
        if (y % 2 == 0) {
            dstY = y / 2;
        } else {
            dstY = height / 2 + (y - 1) / 2;
        }
        
        memcpy(data + dstY * width * 4, 
               tempBuffer + y * width * 4, 
               width * 4);
    }
    
    free(tempBuffer);
}

// Функция сохранения в PNG
int saveAsPNG(const char* filename, uint8_t* rgbaData, int width, int height) {
    FILE* fp = fopen(filename, "wb");
    if (!fp) {
        printf("Error opening output file\n");
        return 0;
    }
    
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fclose(fp);
        return 0;
    }
    
    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, NULL);
        fclose(fp);
        return 0;
    }
    
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        fclose(fp);
        return 0;
    }
    
    png_init_io(png, fp);
    
    // Устанавливаем заголовок PNG
    png_set_IHDR(
        png,
        info,
        width, height,
        8, // бит на канал
        PNG_COLOR_TYPE_RGBA, // RGBA с прозрачностью
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT
    );
    
    png_write_info(png, info);
    
    // Создаем массив указателей на строки
    png_bytep* row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
    for (int y = 0; y < height; y++) {
        row_pointers[y] = (png_bytep)(rgbaData + y * width * 4);
    }
    
    png_write_image(png, row_pointers);
    png_write_end(png, NULL);
    
    free(row_pointers);
    png_destroy_write_struct(&png, &info);
    fclose(fp);
    
    return 1;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s <input.bmp> <output.png>\n", argv[0]);
        return 1;
    }
    
    // Проверяем расширение выходного файла
    const char* ext = strrchr(argv[2], '.');
    if (!ext || strcmp(ext, ".png") != 0) {
        printf("Output file must have .png extension\n");
        return 1;
    }
    
    FILE* inputFile = fopen(argv[1], "rb");
    if (!inputFile) {
        printf("Error opening input file\n");
        return 1;
    }
    
    BMPFileHeader fileHeader;
    BMPInfoHeader infoHeader;
    
    fread(&fileHeader, sizeof(BMPFileHeader), 1, inputFile);
    fread(&infoHeader, sizeof(BMPInfoHeader), 1, inputFile);
    
    if (fileHeader.bfType != 0x4D42) {
        printf("Invalid BMP file format\n");
        fclose(inputFile);
        return 1;
    }
    
    printf("BMP Information:\n");
    printf("Width: %d\n", infoHeader.biWidth);
    printf("Height: %d\n", infoHeader.biHeight);
    printf("Bits per pixel: %d\n", infoHeader.biBitCount);
    
    uint8_t* palette = NULL;
    
    // Читаем палитру если есть
    if (infoHeader.biClrUsed > 0 || infoHeader.biBitCount <= 8) {
        int paletteEntries = infoHeader.biClrUsed;
        if (paletteEntries == 0) {
            paletteEntries = 1 << infoHeader.biBitCount;
        }
        
        printf("Reading palette with %d entries\n", paletteEntries);
        palette = (uint8_t*)malloc(paletteEntries * 4);
        
        fseek(inputFile, sizeof(BMPFileHeader) + infoHeader.biSize, SEEK_SET);
        fread(palette, 1, paletteEntries * 4, inputFile);
    }
    
    // Читаем данные изображения
    fseek(inputFile, fileHeader.bfOffBits, SEEK_SET);
    
    int inputRowSize;
    if (infoHeader.biBitCount == 1) {
        inputRowSize = ((infoHeader.biWidth + 31) / 32) * 4;
    } else {
        inputRowSize = ((infoHeader.biWidth * (infoHeader.biBitCount / 8) + 3) & ~3);
    }
    
    uint8_t* inputData = (uint8_t*)malloc(infoHeader.biHeight * inputRowSize);
    size_t bytesRead = fread(inputData, 1, infoHeader.biHeight * inputRowSize, inputFile);
    printf("Read %zu bytes of image data\n", bytesRead);
    fclose(inputFile);
    
    // Конвертируем в RGBA массив
    uint8_t* imageData = NULL;
    
    if (infoHeader.biBitCount == 1) {
        imageData = convert1BitToRGBA(inputData, palette, 
                                     infoHeader.biClrUsed ? infoHeader.biClrUsed : 2,
                                     infoHeader.biWidth, infoHeader.biHeight,
                                     inputRowSize);
    } else if (infoHeader.biBitCount == 24) {
        imageData = convert24BitToRGBA(inputData, infoHeader.biWidth, 
                                      infoHeader.biHeight, inputRowSize);
    } else if (infoHeader.biBitCount == 32) {
        imageData = convert32BitToRGBA(inputData, infoHeader.biWidth,
                                      infoHeader.biHeight, inputRowSize);
    } else {
        printf("Unsupported bit depth: %d\n", infoHeader.biBitCount);
        free(inputData);
        if (palette) free(palette);
        return 1;
    }
    
    free(inputData);
    if (palette) free(palette);
    
    if (!imageData) {
        printf("Conversion failed\n");
        return 1;
    }
    
    // Проверяем первые несколько пикселей
    printf("First few pixels (RGBA) after flip:\n");
    for (int i = 0; i < 10 && i < infoHeader.biWidth; i++) {
        int idx = i * 4;
        printf("[%d]=R=%d,G=%d,B=%d,A=%d ", i, 
               imageData[idx], 
               imageData[idx + 1], 
               imageData[idx + 2],
               imageData[idx + 3]);
    }
    printf("\n");
    
    // Преобразуем в чересстрочный формат
    printf("Converting to interlaced format...\n");
    convertToInterlacedRGBA(imageData, infoHeader.biWidth, infoHeader.biHeight);
    
    // Сохраняем в PNG
    printf("Saving to PNG...\n");
    if (saveAsPNG(argv[2], imageData, infoHeader.biWidth, infoHeader.biHeight)) {
        printf("Successfully saved to %s\n", argv[2]);
        printf("White pixels are now transparent\n");
    } else {
        printf("Error saving PNG\n");
    }
    
    free(imageData);
    
    return 0;
}
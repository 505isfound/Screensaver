// Loads the picture, the star-free background and the star list that are
// packed inside the .scr file.
#pragma once
#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>

struct Pixels
{
    UINT width = 0, height = 0;
    std::vector<uint8_t> bgra;  // 4 bytes per pixel, top row first
};

struct StarRecord
{
    float x, y, radius, brightness;
};

// Size of the packed picture in pixels.
bool PictureSize(UINT* width, UINT* height);

// Decodes part of the picture (in picture pixels), resized to outWidth x outHeight.
bool LoadPicturePart(float left, float top, float width, float height,
                     UINT outWidth, UINT outHeight, Pixels* out);

bool LoadBackground(Pixels* out);
bool LoadStars(std::vector<StarRecord>* out);
std::string LoadShaderSource();

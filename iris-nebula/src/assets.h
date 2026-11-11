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

// Decodes part of the picture (in picture pixels), resized to outWidth x outHeight.
bool LoadPicturePart(float left, float top, float width, float height,
                     UINT outWidth, UINT outHeight, Pixels* out);

// Ready-made copy of the cut and resized picture, kept in
// %LOCALAPPDATA%\Iris Nebula Screensaver so the next start is instant.
struct PictureCacheKey
{
    float left, top, width, height;   // part of the picture (picture pixels)
    UINT outWidth, outHeight;         // size it was resized to
};
bool LoadCachedPicture(const wchar_t* slot, const PictureCacheKey& key, Pixels* out);
void SaveCachedPicture(const wchar_t* slot, const PictureCacheKey& key, const Pixels& px);

// Small files in the same cache folder (used for ready-made graphics card code).
bool ReadCacheFile(const wchar_t* name, std::vector<uint8_t>* out);
void WriteCacheFile(const wchar_t* name, const void* data, size_t size);

bool LoadBackground(Pixels* out);
bool LoadStars(std::vector<StarRecord>* out);
std::string LoadShaderSource();

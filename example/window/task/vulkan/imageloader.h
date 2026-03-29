#pragma once

#include <cstdint>
#include <string>
#include <vector>


struct ImageData
{
    std::vector<uint8_t> pixels;
    int width    = 0;
    int height   = 0;
    int channels = 0;

    bool isValid() const { return width > 0 && height > 0 && !pixels.empty(); }
};

class ImageLoader
{
  public:
    static ImageData loadImage(const std::string &filePath, int desiredChannels = 0);
};

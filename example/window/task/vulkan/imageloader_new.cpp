#define STB_IMAGE_IMPLEMENTATION
#include "imageloader.h"

#include "external/stb/stb_image.h"

#include <iostream>


ImageData ImageLoader::loadImage(const std::string &filePath, int desiredChannels)
{
    ImageData result;

    unsigned char *data = stbi_load(filePath.c_str(), &result.width, &result.height,
                                    &result.channels, desiredChannels);

    if (!data)
    {
        std::cerr << "Failed to load image: " << filePath << std::endl;
        return result;
    }

    // Copy pixel data
    size_t dataSize =
        result.width * result.height * (desiredChannels ? desiredChannels : result.channels);
    result.pixels.resize(dataSize);
    std::memcpy(result.pixels.data(), data, dataSize);

    // Free stb_image data
    stbi_image_free(data);

    return result;
}

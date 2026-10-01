#ifndef IMAGE_H
#define IMAGE_H

#include <string>
#include <vector>

struct Pixel
{
    unsigned char r, g, b;
};

struct Image
{
    int width;
    int height;
    std::vector<Pixel> pixels;
};

Image readBMP(const std::string &path);
Image checkerboard(const Image &a, const Image &b, int tileSize);
void writeBMP(const std::string &path, const Image &image);

#endif
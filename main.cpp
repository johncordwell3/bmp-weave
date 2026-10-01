#include "image.h"

#include <exception>
#include <iostream>
#include <string>

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <imageA> <imageB>\n";
        return 1;
    }

    try
    {
        std::string pathA = "input/" + std::string(argv[1]) + ".bmp";
        std::string pathB = "input/" + std::string(argv[2]) + ".bmp";

        Image imageA = readBMP(pathA);
        Image imageB = readBMP(pathB);

        Image result = checkerboard(imageA, imageB, 10);
        writeBMP("output.bmp", result);

        std::cout << "Saved checkerboard to output.bmp\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
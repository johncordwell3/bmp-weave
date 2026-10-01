#include "image.h"

#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

Image readBMP(const std::string &path)
{
    std::ifstream file(path, std::ios::binary);

    if (!file)
    {
        throw std::runtime_error("Cannot open BMP: " + path);
    }

    // Read the file header and the first 40 bytes of the DIB header.
    unsigned char header[54];

    if (!file.read(reinterpret_cast<char *>(header), sizeof(header)))
    {
        throw std::runtime_error("Incomplete BMP header");
    }

    if (header[0] != 'B' || header[1] != 'M')
    {
        throw std::runtime_error("Not a BMP file");
    }

    // BMP integers are stored least-significant byte first.
    auto read16 = [&](int offset) -> std::uint16_t
    {
        return std::uint16_t(header[offset]) | (std::uint16_t(header[offset + 1]) << 8);
    };

    auto read32 = [&](int offset) -> std::uint32_t
    {
        return std::uint32_t(header[offset]) | (std::uint32_t(header[offset + 1]) << 8) | (std::uint32_t(header[offset + 2]) << 16) | (std::uint32_t(header[offset + 3]) << 24);
    };

    // Interpret a signed 32-bit field without relying on a signed cast.
    auto readSigned32 = [&](int offset) -> std::int64_t
    {
        std::uint32_t value = read32(offset);
        return (value & 0x80000000u)
                   ? std::int64_t(value) - 0x100000000LL
                   : std::int64_t(value);
    };

    std::uint32_t pixelOffset = read32(10);
    std::uint32_t dibSize = read32(14);
    std::int64_t width = readSigned32(18);
    std::int64_t signedHeight = readSigned32(22);
    std::uint16_t planes = read16(26);
    std::uint16_t bitsPerPixel = read16(28);
    std::uint32_t compression = read32(30);

    if (dibSize < 40 || planes != 1 ||
        bitsPerPixel != 24 || compression != 0)
    {
        throw std::runtime_error("Expected an uncompressed 24-bit BMP");
    }

    bool bottomUp = signedHeight > 0;
    std::int64_t height = bottomUp ? signedHeight : -signedHeight;

    if (width <= 0 || height <= 0 ||
        width > std::numeric_limits<int>::max() ||
        height > std::numeric_limits<int>::max())
    {
        throw std::runtime_error("Invalid BMP dimensions");
    }

    if (pixelOffset < 14ULL + dibSize)
    {
        throw std::runtime_error("Invalid BMP pixel offset");
    }

    // Each pixel uses 3 bytes; each stored row is padded to 4 bytes.
    std::uint64_t rowSize = ((std::uint64_t(width) * 3 + 3) / 4) * 4;
    std::uint64_t dataSize = rowSize * height;

    // Check that the file contains all the expected pixel data.
    file.seekg(0, std::ios::end);
    auto fileSize = file.tellg();

    if (fileSize == std::streampos(-1) ||
        std::uint64_t(fileSize) < pixelOffset + dataSize)
    {
        throw std::runtime_error("Incomplete BMP pixel data");
    }

    Image image;
    image.width = static_cast<int>(width);
    image.height = static_cast<int>(height);

    std::uint64_t pixelCount = std::uint64_t(width) * height;

    if (pixelCount > image.pixels.max_size() ||
        rowSize > std::numeric_limits<std::streamsize>::max() ||
        rowSize > std::vector<unsigned char>().max_size())
    {
        throw std::runtime_error("BMP is too large");
    }

    image.pixels.resize(static_cast<std::size_t>(pixelCount));
    std::vector<unsigned char> row(static_cast<std::size_t>(rowSize));

    file.seekg(pixelOffset, std::ios::beg);

    for (int fileY = 0; fileY < image.height; ++fileY)
    {
        if (!file.read(reinterpret_cast<char *>(row.data()),
                       static_cast<std::streamsize>(row.size())))
        {
            throw std::runtime_error("Failed to read BMP row");
        }

        // Convert the file's row position into our top-down row position.
        int y = bottomUp ? image.height - 1 - fileY : fileY;

        for (int x = 0; x < image.width; ++x)
        {
            std::size_t source = std::size_t(x) * 3;
            std::size_t destination =
                std::size_t(y) * image.width + x;

            // BMP stores BGR; our Pixel structure stores RGB.
            image.pixels[destination] = {
                row[source + 2],
                row[source + 1],
                row[source]};
        }
    }

    return image;
}

Image checkerboard(const Image &a, const Image &b, int tileSize)
{
    if (a.width != b.width || a.height != b.height)
    {
        throw std::invalid_argument("Images must have matching dimensions");
    }

    if (tileSize <= 0)
    {
        throw std::invalid_argument("Tile size must be positive");
    }

    Image output;
    output.width = a.width;
    output.height = a.height;
    output.pixels.resize(a.pixels.size());

    for (int y = 0; y < a.height; ++y)
    {
        for (int x = 0; x < a.width; ++x)
        {
            std::size_t index = std::size_t(y) * a.width + x;

            int tileRow = y / tileSize;
            int tileColumn = x / tileSize;

            bool useA = (tileRow % 2) == (tileColumn % 2);

            output.pixels[index] = useA
                                       ? a.pixels[index]
                                       : b.pixels[index];
        }
    }

    return output;
}

void writeBMP(const std::string &path, const Image &image)
{
    if (image.width <= 0 || image.height <= 0)
    {
        throw std::invalid_argument("Image dimensions must be positive");
    }

    std::uint64_t width = image.width;
    std::uint64_t height = image.height;

    if (image.pixels.size() != width * height)
    {
        throw std::invalid_argument("Pixel count does not match dimensions");
    }

    // Each row contains BGR pixels, padded to a multiple of 4 bytes.
    std::uint64_t rowSize = ((width * 3 + 3) / 4) * 4;
    std::uint64_t dataSize = rowSize * height;
    std::uint64_t fileSize = 54 + dataSize;

    if (fileSize > std::numeric_limits<std::uint32_t>::max() ||
        rowSize > std::numeric_limits<std::streamsize>::max() ||
        rowSize > std::vector<unsigned char>().max_size())
    {
        throw std::runtime_error("Image is too large for this BMP writer");
    }

    unsigned char header[54] = {};

    // Store integers in little-endian byte order.
    auto write16 = [&](int offset, std::uint16_t value)
    {
        for (int i = 0; i < 2; ++i)
        {
            header[offset + i] =
                static_cast<unsigned char>(value >> (8 * i));
        }
    };

    auto write32 = [&](int offset, std::uint32_t value)
    {
        for (int i = 0; i < 4; ++i)
        {
            header[offset + i] =
                static_cast<unsigned char>(value >> (8 * i));
        }
    };

    // BMP file header.
    header[0] = 'B';
    header[1] = 'M';
    write32(2, static_cast<std::uint32_t>(fileSize));
    write32(10, 54); // Pixel data starts after the headers.

    // BITMAPINFOHEADER.
    write32(14, 40); // DIB header size.
    write32(18, static_cast<std::uint32_t>(width));
    write32(22, static_cast<std::uint32_t>(height));
    write16(26, 1);  // Number of color planes.
    write16(28, 24); // Bits per pixel.
    write32(30, 0);  // No compression.
    write32(34, static_cast<std::uint32_t>(dataSize));

    std::vector<unsigned char> row(
        static_cast<std::size_t>(rowSize), 0);

    std::ofstream file(path, std::ios::binary);

    if (!file)
    {
        throw std::runtime_error("Cannot create BMP: " + path);
    }

    if (!file.write(reinterpret_cast<const char *>(header),
                    sizeof(header)))
    {
        throw std::runtime_error("Failed to write BMP header");
    }

    // Positive BMP height means rows must be written bottom-up.
    for (int y = image.height - 1; y >= 0; --y)
    {
        for (int x = 0; x < image.width; ++x)
        {
            const Pixel &pixel =
                image.pixels[std::size_t(y) * image.width + x];

            std::size_t offset = std::size_t(x) * 3;

            row[offset] = pixel.b;
            row[offset + 1] = pixel.g;
            row[offset + 2] = pixel.r;
        }

        // Padding bytes remain zero.
        if (!file.write(reinterpret_cast<const char *>(row.data()),
                        static_cast<std::streamsize>(row.size())))
        {
            throw std::runtime_error("Failed to write BMP pixels");
        }
    }

    file.close();

    if (!file)
    {
        throw std::runtime_error("Failed to finish writing BMP");
    }
}
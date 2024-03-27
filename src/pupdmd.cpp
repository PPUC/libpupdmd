#include "pupdmd.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <vector>

#include "komihash/komihash.h"

namespace fs = std::filesystem;

namespace PUPDMD
{

DMD::DMD() {}

DMD::~DMD() {}

bool DMD::Load(const char* const puppath, const char* const romname)
{
  char folderPath[PUPDMD_MAX_PATH_SIZE + PUPDMD_MAX_NAME_SIZE + 12];
  snprintf(folderPath, PUPDMD_MAX_PATH_SIZE + PUPDMD_MAX_NAME_SIZE + 11, "%s/%s/PupCapture", puppath, romname);

  // Regular expression to extract numeric part from file name (case insensitive)
  std::regex pattern(R"((\d+)\.bmp)", std::regex_constants::icase);

  for (const auto& entry : fs::directory_iterator(folderPath))
  {
    std::string filePath = entry.path().string();
    uint16_t triggerID = 0;
    std::smatch matches;
    std::string fileName = fs::path(filePath).filename().string();

    if (!std::regex_search(fileName, matches, pattern))
    {
      continue;  // Skip files that don't match the pattern
    }
    triggerID = std::stoi(matches[1].str());

    std::ifstream file(filePath, std::ios::binary);

    if (!file.is_open())
    {
      // @todo use log callback
      std::cerr << "Error opening file: " << filePath << std::endl;
      continue;
    }

    // Read BMP header
    BMPHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(BMPHeader));

    // Check if file is a BMP file
    if (header.signature[0] != 'B' || header.signature[1] != 'M')
    {
      // @todo use log callback
      std::cerr << "Not a BMP file: " << filePath << std::endl;
      continue;
    }

    if (header.compression != 0)
    {
      // @todo use log callback
      std::cerr << "Compression is not supported: " << filePath << std::endl;
      continue;
    }

    // Move file pointer to the beginning of pixel data
    file.seekg(header.dataOffset, std::ios::beg);

    // Calculate the size of the pixel data
    size_t pixelDataSize = header.imageSize == 0 ? header.fileSize - header.dataOffset : header.imageSize;

    if (pixelDataSize == 128 * 32 * 3)
    {
      PUPDMD::Hash hash;
      std::vector<char> pixelData(pixelDataSize);

      file.read(pixelData.data(), pixelDataSize);

      for (uint16_t y = 0; y < 32; y++)
      {
        for (uint16_t x = 0; x < 128; x++)
        {
          uint8_t r = pixelData.at((y * 128 + x) * 3);
          uint8_t g = pixelData.at((y * 128 + x) * 3 + 1);
          uint8_t b = pixelData.at((y * 128 + x) * 3 + 2);
          if (PUPDMD_MASK_R == r && PUPDMD_MASK_G == g && PUPDMD_MASK_B == b)
          {
            if (hash.x == 255)
            {
              // Found left top corner
              hash.x = x;
              hash.x = y;
            }
            else if (hash.x < 128)
            {
              if (y == hash.y)
                hash.width++;
              else if (x == hash.x)
                hash.height++;
            }
          }
        }
      }

      if (hash.x < 128)
      {
        hash.x++;
        hash.y++;
        hash.width--;
        hash.height--;
      }
      else
      {
        hash.mask = false;
      }

      CalculateHash((uint8_t*)pixelData.data(), &hash);

      m_ExactColorMap[triggerID] = hash;
    }

    file.close();
  }

  return true;
}

void DMD::CalculateHash(uint8_t* frame, Hash* hash)
{
  if (hash->mask)
  {
    hash->hash = komihash(frame, 128 * 32 * 3, 0);
  }
  else
  {
    uint16_t width = (uint16_t)hash->width * 3;
    uint16_t length = width * hash->height;
    uint8_t* buffer = (uint8_t*)malloc(length);
    uint16_t idx = 0;
    for (uint16_t y = hash->y; y < (hash->y + hash->height); y++)
    {
      memcpy(&buffer[idx], &frame[((y * 128) + hash->x) * 3], width);
      idx += width;
    }
    hash->hash = komihash(buffer, length, 0);
    free(buffer);
  }
}

uint16_t DMD::Match(uint8_t* frame, uint16_t width, uint16_t height)
{
  for (const auto& pair : m_ExactColorMap)
  {
    Hash hash;
    hash.mask = pair.second.mask;
    if (!hash.mask)
    {
      hash.x = pair.second.x;
      hash.y = pair.second.y;
      hash.width = pair.second.width;
      hash.height = pair.second.height;
    }

    CalculateHash(frame, &hash);

    if (hash.hash == pair.second.hash) return pair.first;
  }

  return 0;
}

}  // namespace PUPDMD

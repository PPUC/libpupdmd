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

void DMD::SetLogCallback(PUPDMD_LogCallback callback, const void* userData)
{
  m_logCallback = callback;
  m_logUserData = userData;
}

void DMD::Log(const char* format, ...)
{
  if (!m_logCallback)
  {
    return;
  }

  va_list args;
  va_start(args, format);
  (*(m_logCallback))(format, args, m_logUserData);
  va_end(args);
}

bool DMD::Load(const char* const puppath, const char* const romname, uint8_t bitDepth)
{
  char folderPath[PUPDMD_MAX_PATH_SIZE + PUPDMD_MAX_NAME_SIZE + 12];
  snprintf(folderPath, PUPDMD_MAX_PATH_SIZE + PUPDMD_MAX_NAME_SIZE + 11, "%s/%s/PupCapture", puppath, romname);

  if (!fs::is_directory(folderPath))
  {
    Log("Directory does not exist: %s", folderPath);
    return false;
  }

  Log("Scanning directory: %s", folderPath);

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
      Log("Error opening file: %s", filePath.c_str());
      continue;
    }

    // Read BMP header
    BMPHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(BMPHeader));

    // Check if file is a BMP file
    if (header.signature[0] != 'B' || header.signature[1] != 'M')
    {
      Log("Not a BMP file: %s", filePath.c_str());
      continue;
    }

    if (header.compression != 0)
    {
      Log("Compression is not supported: %s", filePath.c_str());
      continue;
    }

    // Move file pointer to the beginning of pixel data
    file.seekg(header.dataOffset, std::ios::beg);

    // Calculate the size of the pixel data
    size_t pixelDataSize = header.imageSize == 0 ? header.fileSize - header.dataOffset : header.imageSize;

    if (pixelDataSize == 128 * 32 * 3)
    {
      PUPDMD::Hash hash;
      std::vector<uint8_t> pixelData(pixelDataSize);
      std::vector<uint8_t> rgb(0);
      std::vector<uint8_t> indexed(0);

      file.read(reinterpret_cast<char*>(pixelData.data()), pixelDataSize);

      for (uint8_t y = 0; y < 32; y++)
      {
        for (uint8_t x = 0; x < 128; x++)
        {
          // Usually the order is BGR in BMP
          uint8_t b = pixelData.at((y * 128 + x) * 3);
          uint8_t g = pixelData.at((y * 128 + x) * 3 + 1);
          uint8_t r = pixelData.at((y * 128 + x) * 3 + 2);
          rgb.push_back(r);
          rgb.push_back(g);
          rgb.push_back(b);

          // Since PupCapture DMDs are orange it is sufficient to look at red
          switch (bitDepth)
          {
            case 2:
              if (r == PUPDMD_MASK_R)
                indexed.push_back(0);
              else if (r < 8)
                indexed.push_back(0);
              else if (r < 48)
                indexed.push_back(1);
              else if (r < 128)
                indexed.push_back(2);
              else
                indexed.push_back(3);
              break;
            case 4:
              if (r == PUPDMD_MASK_R)
                indexed.push_back(0);
              else if (r < 8)
                indexed.push_back(0);
              else if (r < 24)
                indexed.push_back(1);
              else if (r < 48)
                indexed.push_back(2);
              else if (r < 56)
                indexed.push_back(3);
              else if (r < 72)
                indexed.push_back(4);
              else if (r < 96)
                indexed.push_back(5);
              else if (r < 112)
                indexed.push_back(6);
              else if (r < 128)
                indexed.push_back(7);
              else if (r < 144)
                indexed.push_back(8);
              else if (r < 160)
                indexed.push_back(9);
              else if (r < 176)
                indexed.push_back(10);
              else if (r < 192)
                indexed.push_back(11);
              else if (r < 208)
                indexed.push_back(12);
              else if (r < 224)
                indexed.push_back(13);
              else if (r < 240)
                indexed.push_back(14);
              else
                indexed.push_back(15);
              break;
          }

          // if (r > 0)
          //   Log("Found illuminated pixel RGB %03d %03d %03d, converted to index %02d", r, g, b, indexed.back());

          if (PUPDMD_MASK_R == r && PUPDMD_MASK_G == g && PUPDMD_MASK_B == b)
          {
            if (hash.x == 255)
            {
              // Found left top corner
              hash.x = x;
              hash.y = y;
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
        hash.mask = true;
        hash.x++;
        hash.y++;
        hash.width--;
        hash.height--;
      }
      else
      {
        hash.mask = false;
        hash.x = 0;
        hash.y = 0;
        hash.width = 128;
        hash.height = 32;
      }

      CalculateHash(rgb.data(), &hash, true);
      CalculateHash(rgb.data(), &hash, false);
      CalculateHashIndexed(rgb.data(), &hash);

      m_HashMap[triggerID] = hash;
      Log("Added PUP DMD trigger ID: %03d, mask: %d, x: %03d, y: %03d, width: %03d, height: %03d, exactColorHash: "
          "%020" PRIu64 ", booleanHash: %020" PRIu64 ", indexedHash: %020" PRIu64,
          triggerID, hash.mask, hash.x, hash.y, hash.width, hash.height, hash.exactColorHash, hash.booleanHash,
          hash.indexedHash);
    }

    file.close();
  }

  return true;
}

void DMD::CalculateHash(const uint8_t* pFrame, Hash* pHash, bool exactColor)
{
  if (exactColor)
  {
    if (pHash->mask)
    {
      uint16_t width = (uint16_t)pHash->width * 3;
      uint16_t length = width * pHash->height;
      uint8_t* pBuffer = (uint8_t*)malloc(length);
      uint16_t idx = 0;
      for (uint8_t y = pHash->y; y < (pHash->y + pHash->height); y++)
      {
        memcpy(&pBuffer[idx], &pFrame[((y * 128) + pHash->x) * 3], width);
        idx += width;
      }
      pHash->exactColorHash = komihash(pBuffer, length, 0);
      free(pBuffer);
    }
    else
    {
      pHash->exactColorHash = komihash(pFrame, 128 * 32 * 3, 0);
    }
  }
  else
  {
    uint8_t booleanFrame[128 * 32];
    for (uint16_t i = 0; i < 128 * 32; i++)
    {
      booleanFrame[i] = !(pFrame[i * 3] == 0 && pFrame[(i * 3) + 1] == 0 && pFrame[(i * 3) + 2] == 0);
    }

    if (pHash->mask)
    {
      uint16_t length = pHash->width * pHash->height;
      uint8_t* pBuffer = (uint8_t*)malloc(length);
      uint16_t idx = 0;
      for (uint8_t y = pHash->y; y < (pHash->y + pHash->height); y++)
      {
        memcpy(&pBuffer[idx], &booleanFrame[(y * 128) + pHash->x], pHash->width);
        idx += pHash->width;
      }
      pHash->booleanHash = komihash(pBuffer, length, 0);
      free(pBuffer);
    }
    else
    {
      pHash->booleanHash = komihash(booleanFrame, 128 * 32, 0);
    }
  }
}

void DMD::CalculateHashIndexed(const uint8_t* pFrame, Hash* pHash)
{
  if (pHash->mask)
  {
    uint16_t length = pHash->width * pHash->height;
    uint8_t* buffer = (uint8_t*)malloc(length);
    uint16_t idx = 0;

    for (uint8_t y = pHash->y; y < (pHash->y + pHash->height); y++)
    {
      for (uint8_t x = pHash->x; x < (pHash->x + pHash->width); x++)
      {
        buffer[idx++] = pFrame[(y * 128) + x];
      }
    }
    pHash->indexedHash = komihash(buffer, length, 0);
    free(buffer);
  }
  else
  {
    pHash->indexedHash = komihash(pFrame, 128 * 32, 0);
  }
}

uint16_t DMD::Match(const uint8_t* pFrame, bool exactColor)
{
  uint64_t fullHash = 0;

  for (const auto& pair : m_HashMap)
  {
    Hash hash;
    hash.mask = pair.second.mask;
    if (hash.mask)
    {
      hash.x = pair.second.x;
      hash.y = pair.second.y;
      hash.width = pair.second.width;
      hash.height = pair.second.height;
      CalculateHash(pFrame, &hash, exactColor);
    }
    else
    {
      if (fullHash)
      {
        hash.exactColorHash = hash.booleanHash = fullHash;
      }
      else
      {
        CalculateHash(pFrame, &hash, exactColor);
        fullHash = exactColor ? hash.exactColorHash : hash.booleanHash;
      }
    }

    if ((exactColor && hash.exactColorHash == pair.second.exactColorHash) ||
        (!exactColor && hash.booleanHash == pair.second.booleanHash))
    {
      Log("Matched PUP DMD trigger ID: %d", pair.first);
      return pair.first;
    }
  }

  return 0;
}

uint16_t DMD::MatchIndexed(const uint8_t* pFrame)
{
  uint64_t fullHash = 0;

  for (const auto& pair : m_HashMap)
  {
    Hash hash;
    hash.mask = pair.second.mask;
    if (hash.mask)
    {
      hash.x = pair.second.x;
      hash.y = pair.second.y;
      hash.width = pair.second.width;
      hash.height = pair.second.height;
      CalculateHashIndexed(pFrame, &hash);
    }
    else
    {
      if (fullHash)
      {
        hash.indexedHash = fullHash;
      }
      else
      {
        CalculateHashIndexed(pFrame, &hash);
        fullHash = hash.indexedHash;
      }
    }

    if (hash.indexedHash == pair.second.indexedHash)
    {
      Log("Matched PUP DMD trigger ID: %d", pair.first);
      return pair.first;
    }
  }

  return 0;
}

}  // namespace PUPDMD

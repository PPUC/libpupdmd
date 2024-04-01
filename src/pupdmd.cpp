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
  std::string puppathObj(puppath);
  puppathObj.erase(puppathObj.find_last_not_of("/\\") + 1);
  snprintf(folderPath, PUPDMD_MAX_PATH_SIZE + PUPDMD_MAX_NAME_SIZE + 11, "%s/%s/PupCapture", puppathObj.c_str(),
           romname);

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

    if (pixelDataSize == header.width * header.height * 3 &&
        ((header.width == 128 && header.height == 16) || (header.width == 128 && header.height == 32) ||
         (header.width == 192 && header.height == 64)))
    {
      PUPDMD::Hash hash;
      std::vector<uint8_t> pixelData(pixelDataSize);
      std::vector<uint8_t> rgb(0);
      std::vector<uint8_t> indexed(0);

      file.read(reinterpret_cast<char*>(pixelData.data()), pixelDataSize);

      for (uint8_t y = 0; y < header.height; y++)
      {
        for (uint8_t x = 0; x < header.width; x++)
        {
          // Usually the order is BGR in BMP
          // BMP starts at the lower left, pinball frames at upper left
          uint8_t b = pixelData.at(((header.height - 1 - y) * header.width + x) * 3);
          uint8_t g = pixelData.at(((header.height - 1 - y) * header.width + x) * 3 + 1);
          uint8_t r = pixelData.at(((header.height - 1 - y) * header.width + x) * 3 + 2);
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
            if (hash.maskX == 255)
            {
              // Found left top corner of a mask
              hash.maskX = x;
              hash.maskY = y;
            }
            else if (hash.maskX < 192)
            {
              if (y == hash.maskY)
                hash.maskWidth++;
              else if (x == hash.maskX)
                hash.maskHeight++;
            }
          }
        }
      }

      if (hash.maskX < 192)
      {
        hash.mask = true;
        hash.maskX++;
        hash.maskY++;
        hash.maskWidth--;
        hash.maskHeight--;
      }
      else
      {
        hash.mask = false;
        hash.maskX = 0;
        hash.maskY = 0;
        hash.maskWidth = header.width;
        hash.maskHeight = header.height;
      }
      hash.width = header.width;
      hash.height = header.height;

      CalculateHash(rgb.data(), &hash, true);
      CalculateHash(rgb.data(), &hash, false);
      CalculateHashIndexed(indexed.data(), &hash);

      m_HashMap[triggerID] = hash;
      Log("Added PUP DMD %dx%d trigger ID: %03d, mask: %d, x: %03d, y: %03d, width: %03d, height: %03d, "
          "exactColorHash: %020" PRIu64 ", booleanHash: %020" PRIu64 ", indexedHash: %020" PRIu64,
          hash.width, hash.height, triggerID, hash.mask, hash.maskX, hash.maskY, hash.maskWidth, hash.maskHeight,
          hash.exactColorHash, hash.booleanHash, hash.indexedHash);
    }

    file.close();
  }

  return true;
}

void DMD::CalculateHash(const uint8_t* pFrame, Hash* pHash, bool exactColor)
{
  uint16_t pixels = pHash->width * pHash->height;
  if (exactColor)
  {
    if (pHash->mask)
    {
      uint16_t width = (uint16_t)pHash->maskWidth * 3;
      uint8_t height = pHash->maskY + pHash->maskHeight;
      uint16_t length = width * pHash->maskHeight;
      uint8_t* pBuffer = (uint8_t*)malloc(length);
      uint16_t idx = 0;
      for (uint8_t y = pHash->maskY; y < height; y++)
      {
        memcpy(&pBuffer[idx], &pFrame[((y * pHash->width) + pHash->maskX) * 3], width);
        idx += width;
      }
      pHash->exactColorHash = komihash(pBuffer, length, 0);
      free(pBuffer);
    }
    else
    {
      pHash->exactColorHash = komihash(pFrame, pixels * 3, 0);
    }
  }
  else
  {
    uint8_t* pBooleanFrame = (uint8_t*)malloc(pixels);
    for (uint16_t i = 0; i < pixels; i++)
    {
      pBooleanFrame[i] = !(pFrame[i * 3] == 0 && pFrame[(i * 3) + 1] == 0 && pFrame[(i * 3) + 2] == 0);
    }

    if (pHash->mask)
    {
      uint8_t height = pHash->maskY + pHash->maskHeight;
      uint16_t length = pHash->maskWidth * pHash->maskHeight;
      uint8_t* pBuffer = (uint8_t*)malloc(length);
      uint16_t idx = 0;
      for (uint8_t y = pHash->maskY; y < height; y++)
      {
        memcpy(&pBuffer[idx], &pBooleanFrame[(y * pHash->width) + pHash->maskX], pHash->maskWidth);
        idx += pHash->maskWidth;
      }
      pHash->booleanHash = komihash(pBuffer, length, 0);
      free(pBuffer);
    }
    else
    {
      pHash->booleanHash = komihash(pBooleanFrame, pixels, 0);
    }
    free(pBooleanFrame);
  }
}

void DMD::CalculateHashIndexed(const uint8_t* pFrame, Hash* pHash)
{
  if (pHash->mask)
  {
    uint8_t height = pHash->maskY + pHash->maskHeight;
    uint16_t length = pHash->maskWidth * pHash->maskHeight;
    uint8_t* pBuffer = (uint8_t*)malloc(length);
    uint16_t idx = 0;

    for (uint8_t y = pHash->maskY; y < height; y++)
    {
      memcpy(&pBuffer[idx], &pFrame[(y * pHash->width) + pHash->maskX], pHash->maskWidth);
      idx += pHash->maskWidth;
    }
    pHash->indexedHash = komihash(pBuffer, length, 0);
    free(pBuffer);
  }
  else
  {
    pHash->indexedHash = komihash(pFrame, 128 * 32, 0);
  }
}

uint16_t DMD::Match(const uint8_t* pFrame, uint8_t width, uint8_t height, bool exactColor)
{
  uint64_t fullHash = 0;
  Hash hash;
  hash.width = width;
  hash.height = height;

  for (const auto& pair : m_HashMap)
  {
    if (pair.second.width != width || pair.second.height != height) continue;

    hash.mask = pair.second.mask;
    if (hash.mask)
    {
      hash.maskX = pair.second.maskX;
      hash.maskY = pair.second.maskY;
      hash.maskWidth = pair.second.maskWidth;
      hash.maskHeight = pair.second.maskHeight;
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
      if (pair.first != m_lastTriggerID)
      {
        m_lastTriggerID = pair.first;
        Log("Matched PUP DMD trigger ID: %d", pair.first);
        return pair.first;
      }

      return 0;
    }
  }

  return 0;
}

uint16_t DMD::MatchIndexed(const uint8_t* pFrame, uint8_t width, uint8_t height)
{
  uint64_t fullHash = 0;
  Hash hash;
  hash.width = width;
  hash.height = height;

  for (const auto& pair : m_HashMap)
  {
    if (pair.second.width != width || pair.second.height != height) continue;

    hash.mask = pair.second.mask;
    if (hash.mask)
    {
      hash.maskX = pair.second.maskX;
      hash.maskY = pair.second.maskY;
      hash.maskWidth = pair.second.maskWidth;
      hash.maskHeight = pair.second.maskHeight;
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
      if (pair.first != m_lastTriggerID)
      {
        m_lastTriggerID = pair.first;
        Log("Matched PUP DMD trigger ID: %d", pair.first);
        return pair.first;
      }

      return 0;
    }
  }

  return 0;
}

}  // namespace PUPDMD

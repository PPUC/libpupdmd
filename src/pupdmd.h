#pragma once

#define PUPDMD_VERSION_MAJOR 0  // X Digits
#define PUPDMD_VERSION_MINOR 3  // Max 2 Digits
#define PUPDMD_VERSION_PATCH 0  // Max 2 Digits

#define _PUPDMD_STR(x) #x
#define PUPDMD_STR(x) _PUPDMD_STR(x)

#define PUPDMD_VERSION \
  PUPDMD_STR(PUPDMD_VERSION_MAJOR) "." PUPDMD_STR(PUPDMD_VERSION_MINOR) "." PUPDMD_STR(PUPDMD_VERSION_PATCH)
#define PUPDMD_MINOR_VERSION PUPDMD_STR(PUPDMD_VERSION_MAJOR) "." PUPDMD_STR(PUPDMD_VERSION_MINOR)

#ifdef _MSC_VER
#define PUPDMDAPI __declspec(dllexport)
#define PUPDMDCALLBACK __stdcall
#else
#define PUPDMDAPI __attribute__((visibility("default")))
#define PUPDMDCALLBACK
#endif

#define PUPDMD_MAX_NAME_SIZE 16
#define PUPDMD_MAX_PATH_SIZE 256

#define PUPDMD_MASK_R 253
#define PUPDMD_MASK_G 0
#define PUPDMD_MASK_B 253

#include <inttypes.h>
#include <stdarg.h>

#include <map>

typedef void(PUPDMDCALLBACK* PUPDMD_LogCallback)(const char* format, va_list args, const void* userData);

namespace PUPDMD
{

// BMP header structure
#pragma pack(push, 1)
struct BMPHeader
{
  char signature[2];         // Signature, should be 'BM'
  uint32_t fileSize;         // Size of the BMP file
  uint16_t reserved1;        // Reserved field 1
  uint16_t reserved2;        // Reserved field 2
  uint32_t dataOffset;       // Pixel data offset
  uint32_t headerSize;       // Size of the header
  int32_t width;             // Width of the image
  int32_t height;            // Height of the image
  uint16_t planes;           // Number of color planes, must be 1
  uint16_t bpp;              // Bits per pixel
  uint32_t compression;      // Compression method
  uint32_t imageSize;        // Size of the raw pixel data
  int32_t xPixelsPerMeter;   // Horizontal resolution
  int32_t yPixelsPerMeter;   // Vertical resolution
  uint32_t colorsUsed;       // Number of colors in the palette
  uint32_t colorsImportant;  // Number of important colors
};
#pragma pack(pop)

struct Hash
{
  bool mask = true;
  uint64_t exactColorHash = 0;
  uint64_t booleanHash = 0;
  uint64_t indexedHash = 0;
  uint8_t x = 255;
  uint8_t y = 255;
  uint8_t width = 0;
  uint8_t height = 0;
};

class PUPDMDAPI DMD
{
 public:
  DMD();
  ~DMD();

  void SetLogCallback(PUPDMD_LogCallback callback, const void* userData);
  bool Load(const char* const puppath, const char* const romname, uint8_t bitDepth = 2);
  uint16_t Match(const uint8_t* pFrame, bool exactColor = true);
  uint16_t MatchIndexed(const uint8_t* pFrame);
  const std::map<uint16_t, Hash> GetHashMap() { return m_HashMap; }

 private:
  void Log(const char* format, ...);
  void CalculateHash(const uint8_t* pFrame, Hash* pHash, bool exactColor);
  void CalculateHashIndexed(const uint8_t* pFrame, Hash* pHash);

  std::map<uint16_t, Hash> m_HashMap;

  PUPDMD_LogCallback m_logCallback = nullptr;
  const void* m_logUserData = nullptr;
};

}  // namespace PUPDMD

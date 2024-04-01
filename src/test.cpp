#include <inttypes.h>

#include <cstdio>

#include "pupdmd.h"

void PUPDMDCALLBACK LogCallback(const char* format, va_list args, const void* pUserData)
{
  char buffer[1024];
  vsnprintf(buffer, sizeof(buffer), format, args);

  printf("%s\n", buffer);
}

int main(int argc, const char* argv[])
{
  PUPDMD::DMD* pDmd = new PUPDMD::DMD();
  pDmd->SetLogCallback(LogCallback, nullptr);
  pDmd->Load(".", "test", 4);
  for (const auto& pair : pDmd->GetHashMap())
  {
    printf("triggerID: %03d, mask: %d, x: %03d, y: %03d, width: %03d, height: %03d, exactColorHash: %020" PRIu64
           ", booleanHash: %020" PRIu64 ", indexedHash: %020" PRIu64 "\n",
           pair.first, pair.second.mask, pair.second.maskX, pair.second.maskY, pair.second.maskWidth,
           pair.second.maskHeight, pair.second.exactColorHash, pair.second.booleanHash, pair.second.indexedHash);
  }

  return 0;
}

#include <inttypes.h>

#include <cstdio>

#include "pupdmd.h"

int main(int argc, const char* argv[])
{
  PUPDMD::DMD* pDmd = new PUPDMD::DMD();
  pDmd->Load(".", "test");
  for (const auto& pair : pDmd->GetHashMap())
  {
    printf("triggerID: %03d, mask: %d, x: %03d, y: %03d, width: %03d, height: %03d, exactColorHash: %020" PRIu64
           ", booleanHash: %020" PRIu64 "\n",
           pair.first, pair.second.mask, pair.second.x, pair.second.y, pair.second.width, pair.second.height,
           pair.second.exactColorHash, pair.second.booleanHash);
  }

  return 0;
}

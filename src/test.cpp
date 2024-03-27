#include <inttypes.h>

#include <cstdio>

#include "pupdmd.h"

int main(int argc, const char* argv[])
{
  PUPDMD::DMD* pDmd = new PUPDMD::DMD();
  pDmd->Load("/home/mkalkbrenner/PUP", "tf_180");
  for (const auto& pair : pDmd->GetExactColorMap())
  {
    printf("triggerID: %d, width: %d, height: %d, hash: %" PRIu64 "\n", pair.first, pair.second.width, pair.second.height, pair.second.hash);
  }

  return 0;
}
#include <inttypes.h>

#include <cstdio>

#include "pupdmd.h"

int main(int argc, const char* argv[])
{
  PUPDMD::DMD* pDmd = new PUPDMD::DMD();
  pDmd->Load("/home/mkalkbrenner/PUP", "tf_180");
  for (const auto& pair : pDmd->GetExactColorMap())
  {
    printf("triggerID: %03d, mask: %d, x: %03d, y: %03d, width: %03d, height: %03d, hash: %020" PRIu64 "\n", pair.first,
           pair.second.mask, pair.second.x, pair.second.y, pair.second.width, pair.second.height, pair.second.hash);
  }

  return 0;
}
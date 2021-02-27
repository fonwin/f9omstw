// \file f9omsrc/OmsRcClientApiVer.c
// \author fonwinz@gmail.com
#include "f9omsrc/OmsRc.h"

#define proj_NAME    "f9OmsRcAPI"
#include "proj_verinfo.h"

f9OmsRc_API_FN(const char*) f9OmsRc_ApiVersionInfo(void) {
   return proj_VERSION;
}

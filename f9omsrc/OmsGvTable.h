// \file f9omsrc/OmsGvTable.h
// \author fonwinz@gmail.com
#ifndef __f9omsrc_OmsGvTable_h__
#define __f9omsrc_OmsGvTable_h__
#include "fon9/CStrView.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   fon9_CStrView  Name_;
   int            Index_;
   char           Padding___[4];
} f9oms_GvItem;

typedef f9oms_GvItem    f9oms_GvField;

typedef struct {
   const f9oms_GvField*    FieldArray_;
   unsigned                FieldCount_;
   unsigned                RecordCount_;
   const fon9_CStrView**   RecordList_;
} f9oms_GvList;

typedef struct {
   f9oms_GvItem   Table_;
   f9oms_GvList   GvList_;
} f9oms_GvTable;

#ifdef __cplusplus
}//extern "C"
#endif
#endif//__f9omsrc_OmsGvTable_h__

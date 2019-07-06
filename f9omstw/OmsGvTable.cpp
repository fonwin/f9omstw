// \file f9omstw/OmsGvTable.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsGvTable.hpp"
#include "f9omstw/OmsRc.h"
#include "fon9/StrTools.hpp"
#include "fon9/Log.hpp"

namespace f9omstw {

OmsGvTable::~OmsGvTable() {
   for (auto* vlist : this->RecList_)
      delete[] vlist;
}
void OmsGvTable::InitializeGvList() {
   if (this->RecList_.empty())
      fon9::ZeroStruct(this->GvList_);
   else {
      this->GvList_.FieldArray_ = this->Fields_.GetVector();
      this->GvList_.FieldCount_ = static_cast<unsigned>(this->Fields_.size());
      this->GvList_.RecordCount_ = static_cast<unsigned>(this->RecList_.size());
      const fon9::StrView** recs = &this->RecList_[0];
      this->GvList_.RecordList_ = reinterpret_cast<const fon9_CStrView**>(recs);
   }
}

void OmsParseGvTables(OmsGvTables& dst, std::string& srcstr) {
   fon9::StrView src{&srcstr};
   OmsGvTable* curTab = nullptr;
   while (!fon9::StrTrimHead(&src).empty()) {
      fon9::StrView ln = fon9::StrFetchNoTrim(src, *fon9_kCSTR_ROWSPL);
      if (ln.Get1st() == *fon9_kCSTR_LEAD_TABLE) {
         ln.SetBegin(ln.begin() + 1);
         *const_cast<char*>(ln.end()) = '\0';
         if (!dst.Add(OmsGvTableSP{curTab = new OmsGvTable{ln}})) {
            curTab = nullptr;
            fon9_LOG_ERROR("OmsParseGvTables.AddTable|name=", ln);
            continue;
         }
         ln = fon9::StrFetchNoTrim(src, *fon9_kCSTR_ROWSPL);
         while (!fon9::StrTrimHead(&ln).empty()) {
            fon9::StrView fldName = fon9::StrFetchNoTrim(ln, *fon9_kCSTR_CELLSPL);
            *const_cast<char*>(fldName.end()) = '\0';
            curTab->Fields_.Add(OmsGvField{fldName});
         }
      }
      else if (curTab) {
         size_t         fldsz = curTab->Fields_.size();
         fon9::StrView* vlist;
         curTab->RecList_.emplace_back(vlist = new fon9::StrView[fldsz]);
         while (fldsz > 0) {
            *vlist = fon9::StrFetchNoTrim(ln, *fon9_kCSTR_CELLSPL);
            *const_cast<char*>(vlist->end()) = '\0';
            ++vlist;
            --fldsz;
         }
      }
   }

   unsigned L = 0;
   while (OmsGvTable* tab = dst.Get(L++)) {
      tab->InitializeGvList();
   }
}

} // namespaces

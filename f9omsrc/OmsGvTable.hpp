// \file f9omsrc/OmsGvTable.hpp
// \author fonwinz@gmail.com
#ifndef __f9omsrc_OmsGvTable_hpp__
#define __f9omsrc_OmsGvTable_hpp__
#include "f9omsrc/OmsGvTable.h"
#include "fon9/NamedIx.hpp"

namespace f9omstw {

struct OmsGvItem : public f9oms_GvItem {
   OmsGvItem(const fon9::StrView& name) {
      this->Name_ = name.ToCStrView();
      this->Index_ = -1;
   }
   OmsGvItem() {
      this->Name_.Begin_ = this->Name_.End_ = nullptr;
      this->Index_ = -1;
   }

   fon9::StrView GetNameStr() const { return this->Name_; }
   int           GetIndex()   const { return this->Index_; }
   bool SetIndex(size_t index) {
      if (this->Index_ >= 0)
         return false;
      this->Index_ = static_cast<int>(index);
      return true;
   }
};

struct OmsGvField : public OmsGvItem {
   using pointer = const OmsGvField*;
   using element_type = OmsGvField;
   using OmsGvItem::OmsGvItem;
   OmsGvField() = default;

   bool              operator!()  const { return false; }
   OmsGvField*       operator->() { return this; }
   const OmsGvField* operator->() const { return this; }
   const OmsGvField* get()        const { return this; }
};
using OmsGvFields = fon9::NamedIxMapNoRemove<OmsGvField>;

struct OmsGvTable : public OmsGvItem {
   fon9_NON_COPY_NON_MOVE(OmsGvTable);
   using RecList = std::vector<const fon9::StrView*>;
   f9oms_GvList   GvList_;
   OmsGvFields    Fields_;
   RecList        RecList_;

   using OmsGvItem::OmsGvItem;
   OmsGvTable() = default;
   ~OmsGvTable();

   void InitializeGvList();
};
using OmsGvTableSP = std::unique_ptr<OmsGvTable>;

using OmsGvTables = fon9::NamedIxMapNoRemove<OmsGvTableSP>;

/// srcstr 會被改變('\n' or '\x01' 變成 '\0'),
/// 在 dst 的有效期間, srcstr 必須有效.
void OmsParseGvTables(OmsGvTables& dst, std::string& srcstr);

} // namespaces
#endif//__f9omsrc_OmsGvTable_hpp__

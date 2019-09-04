// \file f9omstw/OmsTools.cpp
// \author fonwinz@gmail.com
#include "f9omstw/OmsTools.hpp"
#include "f9omstw/OmsBrk.hpp"
#include "fon9/StrTo.hpp"
#include "fon9/RevPrint.hpp"

namespace f9omstw {

void RevPrintFields(fon9::RevBuffer& rbuf, const fon9::seed::Tab& tab, const fon9::seed::RawRd& rd, char chSplitter) {
   for (size_t fidx = tab.Fields_.size(); fidx > 0;) {
      tab.Fields_.Get(--fidx)->CellRevPrint(rd, nullptr, rbuf);
      fon9::RevPrint(rbuf, chSplitter);
   }
   fon9::RevPrint(rbuf, tab.Name_);
}
void RevPrintTabFieldNames(fon9::RevBuffer& rbuf, const fon9::seed::Tab& tab, char chSplitter) {
   fon9::NumOutBuf nbuf;
   for (size_t fidx = tab.Fields_.size(); fidx > 0;) {
      auto fld = tab.Fields_.Get(--fidx);
      fon9::RevPrint(rbuf, chSplitter, fld->GetTypeId(nbuf), ' ', fld->Name_);
   }
   fon9::RevPrint(rbuf, tab.Name_);
}
void RevPrintLayout(fon9::RevBuffer& rbuf, const fon9::seed::Layout& layout) {
   for (size_t tidx = layout.GetTabCount(); tidx > 0;) {
      fon9::RevPrint(rbuf, '\n');
      RevPrintTabFieldNames(rbuf, *layout.GetTab(--tidx), *fon9_kCSTR_CELLSPL);
   }
}

void RevPrintIvKey(fon9::RevBuffer& rbuf, const OmsIvBase* ivSrc, const OmsIvBase* ivEnd) {
   for (;;) {
      switch (ivSrc->IvKind_) {
      case OmsIvKind::Unknown:
         break;
      case OmsIvKind::Subac:
         fon9::RevPrint(rbuf, static_cast<const OmsSubac*>(ivSrc)->SubacNo_);
         break;
      case OmsIvKind::Ivac:
         rbuf.SetPrefixUsed(fon9::Pic9ToStrRev<7>(rbuf.AllocPrefix(7), static_cast<const OmsIvac*>(ivSrc)->IvacNo_));
         break;
      case OmsIvKind::Brk:
         fon9::RevPrint(rbuf, static_cast<const OmsBrk*>(ivSrc)->BrkId_);
         break;
      }
      if ((ivSrc = ivSrc->Parent_.get()) == ivEnd)
         break;
      fon9::RevPrint(rbuf, '-');
   }
}

} // namespaces

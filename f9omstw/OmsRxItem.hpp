// \file f9omstw/OmsRxItem.hpp
// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsRxItem_hpp__
#define __f9omstw_OmsRxItem_hpp__
#include "f9omstw/OmsBase.hpp"
#include "fon9/seed/SeedBase.hpp"
#include "fon9/buffer/RevBuffer.hpp"

namespace f9omstw {

/// 每台主機自行依序編號.
/// 也就是OmsA.RxSNO=1, 與OmsB.RxSNO=1, 可能是不同的 OmsRxItem.
using OmsRxSNO = uint64_t;

/// OMS 回報項目基底類別.
/// - 回報項目包含: OmsRequest, OmsOrderRaw, OmsEvent...
class OmsRxItem {
   friend class OmsBackend;
   /// 此序號由 OmsBackend 填入, 不應列入 seed::Field;
   OmsRxSNO RxSNO_{0};
   /// 只會呼叫一次, 在 OmsBackend::Append();
   virtual void OnRxItem_AddRef() const = 0;
   /// 只會呼叫一次, 在 ~OmsBackend();
   virtual void OnRxItem_Release() const = 0;
public:
   virtual ~OmsRxItem();

   virtual const OmsRequestBase* ToRequest() const;
   virtual const OmsOrderRaw* ToOrderRaw() const;

   OmsRxSNO RxSNO() const {
      return this->RxSNO_;
   }

   /// 將內容依照 Fields 的順序輸出到 rbuf;
   /// - 前方須包含 Name, 尾端 **不用 '\n'**
   /// - 使用 fon9_kCSTR_CELLSPL 分隔欄位;
   /// - 不含 RxSNO();
   virtual void RevPrint(fon9::RevBuffer& rbuf) const = 0;

   /// 這裡只是提供欄位的型別及名稱, 不應使用此欄位存取 RxSNO_;
   /// fon9_MakeField_const(fon9::Named{"RxSNO"}, OmsRxItem, RxSNO_);
   static fon9::seed::FieldSP MakeField_RxSNO();
};

} // namespaces
#endif//__f9omstw_OmsRxItem_hpp__

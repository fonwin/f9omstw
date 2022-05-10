/// \file f9omstw/OmsPoIvList.hpp
/// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsPoIvList_hpp__
#define __f9omstw_OmsPoIvList_hpp__
#include "f9omstw/OmsPoIvList.h"
#include "f9omstw/OmsBase.hpp"
#include "fon9/CharVector.hpp"
#include "fon9/SortedVector.hpp"
#include "fon9/Utility.hpp"

namespace f9omstw {

enum class OmsIvRight : fon9::underlying_type_t<f9oms_IvRight> {
   DenyTradingNew    = f9oms_IvRight_DenyTradingNew,
   DenyTradingChgQty = f9oms_IvRight_DenyTradingChgQty,
   DenyTradingChgPri = f9oms_IvRight_DenyTradingChgPri,
   DenyTradingQuery  = f9oms_IvRight_DenyTradingQuery,
   DenyTradingAll    = f9oms_IvRight_DenyTradingAll,

   /// 只有在 DenyTradingAll 的情況下才需要判斷此旗標.
   /// 因為只要允許任一種交易, 就必定允許回報.
   AllowSubscribeReport = f9oms_IvRight_AllowSubscribeReport,

   /// 允許使用者建立「回報」補單.
   /// 通常用於「委託遺失(例如:新單尚未寫檔,但系統crash,重啟後委託遺失)」的補單操作。
   /// 僅允許 Admin 設定此欄, 檢查方式:
   /// IsEnumContains(pol.GetIvrAdminRights(), OmsIvRight::AllowAddReport);
   AllowAddReport = f9oms_IvRight_AllowAddReport,

   AllowAll = f9oms_IvRight_AllowAll,
   DenyAll  = f9oms_IvRight_DenyAll,
   IsAdmin  = f9oms_IvRight_IsAdmin,
};
fon9_ENABLE_ENUM_BITWISE_OP(OmsIvRight);

struct OmsIvConfig {
   OmsIvRight  Rights_{};
   LgOut       LgOut_{LgOut::Unknown};
   OmsIvConfig() = default;
   explicit OmsIvConfig(OmsIvRight ivRights) : Rights_{ivRights} {
   }
   void Clear() {
      fon9::ForceZeroNonTrivial(this);
   }
};

/// 可用帳號的 key, 使用字串儲存, 格式: "BrkId-IvacNo-SubacNo".
/// - IvacNo 部分, 若沒有 wildcard, 則會正規化為 7 碼數字.
class OmsIvKey : private fon9::CharVector {
   using base = fon9::CharVector;
   explicit OmsIvKey(base&& rhs) : base{rhs} {
   }
public:
   using BaseStrT = base;

   OmsIvKey() = default;
   OmsIvKey(const OmsIvKey& rhs) : base{rhs} {
   }
   OmsIvKey(const fon9::StrView& rhs) : base{Normalize(rhs)} {
   }
   bool operator<(const OmsIvKey& rhs) const {
      return base::operator<(rhs);
   }

   using base::clear;
   using base::empty;
   using base::size;
   using base::reserve;
   using base::append; // 提供給 Bitv IO 使用, 不做正規化!
   using base::ToString;

   int compare(const OmsIvKey& rhs) const {
      return base::compare(rhs);
   }
   void assign(const char* pbeg, const char* pend) {
      (*this) = Normalize(fon9::StrView(pbeg, pend));
   }
   void assign(const fon9::StrView& v) {
      (*this) = Normalize(v);
   }

   static OmsIvKey Normalize(fon9::StrView v);

   struct KeyItems {
      fon9::StrView  BrkId_;
      fon9::StrView  IvacNo_;
      fon9::StrView  SubacNo_;

      KeyItems(fon9::StrView key);
   };

   friend fon9::StrView ToStrView(const OmsIvKey& v) {
      return ToStrView(*static_cast<const base*>(&v));
   }

   /// 帳號移除前方的 '0'
   fon9::CharVector ToShortStr(char chSpl) const;
};

/// 使用者登入後, 可取得「可用帳號列表」.
using OmsIvList = fon9::SortedVector<OmsIvKey, OmsIvConfig>;

} // namespaces
#endif//__f9omstw_OmsPoIvList_hpp__

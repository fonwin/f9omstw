/// \file f9omstw/OmsPoUserRights.hpp
/// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsPoUserRights_hpp__
#define __f9omstw_OmsPoUserRights_hpp__
#include "f9omstw/OmsBase.hpp"
#include "f9omstw/OmsPoIvList.hpp"
#include "fon9/FlowCounter.hpp"

namespace f9omstw {

enum class OmsUserRightFlag : uint8_t {
   /// 是否允許 OmsUserRights::AllowOrdTeams_ 進行同步,
   /// 預設為不允許.
   AllowOrdTeamsSyn = 0x01,
};
fon9_ENABLE_ENUM_BITWISE_OP(OmsUserRightFlag);

/// Oms使用者權限:
/// - 是否可自訂櫃號 or 委託書號.
/// - 流量管制.
/// - 最多同時上線數量, 超過時的踢退方式(後踢前 or 拒絕最後)
class OmsUserRights {
public:
   OmsUserRights() {
      this->FcRequest_.Clear();
   }
   ~OmsUserRights();
   /// 允許的委託櫃號, 使用 "," 分隔, 或使用 "-" 設定範圍.
   /// 若為空白, 則不允許自訂櫃號 or 委託書號.
   fon9::CharVector        AllowOrdTeams_;
   /// 下單(新、刪、改、查...)要求的流量管制參數.
   fon9::FlowCounterArgs   FcRequest_;
   /// 預設下單群組.
   LgOut                   LgOut_{};
   OmsUserRightFlag        Flags_{};
   OmsIvRight              IvDenys_{};
   char                    Padding___[1];
};

} // namespaces
#endif//__f9omstw_OmsPoUserRights_hpp__

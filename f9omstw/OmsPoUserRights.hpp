/// \file f9omstw/OmsPoUserRights.hpp
/// \author fonwinz@gmail.com
#ifndef __f9omstw_OmsPoUserRights_hpp__
#define __f9omstw_OmsPoUserRights_hpp__
#include "fon9/CharVector.hpp"
#include "fon9/Utility.hpp"

namespace f9omstw {

struct FlowControlArgs {
   /// 流量管制: 單位時間內允許的數量.
   uint16_t Count_{0};
   /// 流量管制: 單位時間(ms).
   uint16_t IntervalMS_{0};
};

/// Oms使用者權限:
/// - 是否可自訂櫃號 or 委託書號.
/// - 流量管制.
/// - 最多同時上線數量, 超過時的踢退方式(後踢前 or 拒絕最後)
class OmsUserRights {
public:
   /// 允許的委託櫃號, 使用 "," 分隔.
   /// 若為空白, 則不允許自訂櫃號 or 委託書號.
   fon9::CharVector  AllowOrdTeams_;
   // 下單要求的流量管制參數.
   FlowControlArgs   FcRequest_;
   char              padding_[4];
};

} // namespaces
#endif//__f9omstw_OmsPoUserRights_hpp__

// \file f9omstw/OmsRequestPolicy_UT.cpp
// \author fonwinz@gmail.com
#define _CRT_SECURE_NO_WARNINGS
#include "f9omstw/OmsRequestPolicy.hpp"
#include "f9omstw/OmsCoreMgr.hpp"
#include "f9utw/UtwsBrk.hpp"
#include "f9utw/UtwsIvac.hpp"
#include "f9utw/UtwsSubac.hpp"
#include "fon9/TestTools.hpp"

fon9_WARN_DISABLE_PADDING;
struct IvacItem {
   fon9::StrView        BrkId_;
   f9omstw::IvacNo      IvacNo_;
   fon9::StrView        SubacNo_;
   f9omstw::OmsIvRight  Rights_;
};
struct PolicyItem {
   fon9::StrView        IvKey_;
   f9omstw::OmsIvRight  Rights_;
};
fon9_MSC_WARN_POP;
//--------------------------------------------------------------------------//
void CheckPolicy(const f9omstw::OmsRequestPolicy& pol, f9omstw::OmsIvBase* iv, f9omstw::OmsIvRight expected) {
   f9omstw::OmsIvRight ivRights = pol.GetIvRights(iv, nullptr);
   if (ivRights == expected)
      return;
   std::cout << "|iv=";
   fon9::RevBufferFixedSize<1024> rbuf;
   rbuf.RewindEOS();
   fon9::RevPrint(rbuf, "|rights=", ivRights, "|expected=", expected, "|err=(rights!=expected)");
   f9omstw::RevPrintIvKey(rbuf, iv, nullptr);
   fon9::RevPrint(rbuf, "|iv=");
   std::cout << rbuf.GetCurrent() << "\r[ERROR]" << std::endl;
   abort();
}

// 測試無效的帳號: 應該是 "*" 的權限, 若沒設定則應為 DenyAll;
void CheckPolicy_UnknowIv(const f9omstw::OmsRequestPolicy& pol, f9omstw::OmsIvRight expected) {
   f9omstw::OmsSubacSP iv{new f9omstw::UtwsSubac("UnknowIv", nullptr)};
   CheckPolicy(pol, static_cast<f9omstw::OmsIvBase*>(nullptr), expected);
   CheckPolicy(pol, iv.get(), expected);
}
//--------------------------------------------------------------------------//
struct OmsRequestPolicy_UT : public f9omstw::OmsCore {
   fon9_NON_COPY_NON_MOVE(OmsRequestPolicy_UT);
   using base = f9omstw::OmsCore;
   using base::Brks_;

   OmsRequestPolicy_UT() : f9omstw::OmsCore(fon9::LocalNow(), new f9omstw::OmsCoreMgr{nullptr}, "seed/path", "ut") {
      using namespace f9omstw;
      this->Brks_.reset(new OmsBrkTree(*this, UtwsBrk::MakeLayout(OmsBrkTree::DefaultTreeFlag()), &OmsBrkTree::TwsBrkIndex1));
      this->Brks_->Initialize(&UtwsBrk::BrkMaker, "8610", 5u, &f9omstw_IncStrAlpha);
   }
   ~OmsRequestPolicy_UT() {
      this->Brks_->InThr_OnParentSeedClear();
   }
   bool RunCoreTask(f9omstw::OmsCoreTask&&) override {
      assert(!"Not suppport RunCoreTask()");
      return false;
   }
   bool MoveToCoreImpl(f9omstw::OmsRequestRunner&&) override {
      assert(!"Not suppport MoveToCoreImpl()");
      return false;
   }

   void TestCase(const char* testName,
                 const PolicyItem* policies, size_t policyCount,
                 const IvacItem* ivacItems, size_t ivacItemCount,
                 f9omstw::OmsIvRight unknownIvRights) {
      std::cout << "[TEST ] " << testName;
      using namespace f9omstw;
      OmsRequestPolicySP pol{new OmsRequestPolicy{}};
      for (size_t L = 0; L < policyCount; ++L) {
         auto& item = policies[L];
         const char* err = "?";
         switch (OmsAddIvConfig(*const_cast<OmsRequestPolicy*>(pol.get()), item.IvKey_, f9omstw::OmsIvConfig{item.Rights_}, *this->Brks_)) {
         case OmsIvKind::Brk:       err = "Brk";   break;
         case OmsIvKind::Ivac:      err = "Ivac";  break;
         case OmsIvKind::Subac:     err = "Subac"; break;
         case OmsIvKind::Unknown:   continue;
         }
         std::cout << "|err=" << err << " not found.|key=" << item.IvKey_.ToString() << "\r[ERROR]" << std::endl;
         abort();
      }
      for (size_t L = 0; L < ivacItemCount; ++L) {
         auto& item = ivacItems[L];
         auto* ivac = static_cast<UtwsIvac*>(this->Brks_->GetBrkRec(item.BrkId_)->FetchIvac(item.IvacNo_));
         OmsIvBase*  ivBase;
         if (item.SubacNo_.empty())
            ivBase = ivac;
         else
            ivBase = ivac->FetchSubac(item.SubacNo_);
         CheckPolicy(*pol, ivBase, item.Rights_);
      }
      CheckPolicy_UnknowIv(*pol, unknownIvRights);         
      std::cout << "\r[OK   ]" << std::endl;
   }
   template <size_t kPolicyCount, size_t kIvacItemCount>
   void TestCase(const char* testName,
                 const PolicyItem (&policies)[kPolicyCount],
                 const IvacItem (&ivacItems)[kIvacItemCount],
                 f9omstw::OmsIvRight unknownIvRights) {
      this->TestCase(testName, policies, kPolicyCount, ivacItems, kIvacItemCount, unknownIvRights);
   }
   void TestCase1() {
      const f9omstw::OmsIvRight  kRightsAdmin = static_cast<f9omstw::OmsIvRight>(0x12);
      const PolicyItem kPolicyItems[] = {
         {fon9::StrView{"*"}, kRightsAdmin},
      };
      const f9omstw::OmsIvRight  kRightsOther = kRightsAdmin | f9omstw::OmsIvRight::IsAdmin;
      const IvacItem kIvacItems[] = {
         {fon9::StrView{"8610"}, 10, fon9::StrView{nullptr}, kRightsOther},
         {fon9::StrView{"8610"}, 21, fon9::StrView{"sa201"}, kRightsOther},
      };
      this->TestCase("admin", kPolicyItems, kIvacItems, kRightsOther);
   }
   void TestCase2() {
      const f9omstw::OmsIvRight  kRightsAdmin = static_cast<f9omstw::OmsIvRight>(0x12);
      const f9omstw::OmsIvRight  kRights32    = static_cast<f9omstw::OmsIvRight>(0x32);
      const f9omstw::OmsIvRight  kRights43    = static_cast<f9omstw::OmsIvRight>(0x43);
      const f9omstw::OmsIvRight  kRights4a    = static_cast<f9omstw::OmsIvRight>(0x4a);
      const PolicyItem kPolicyItems[] = {
         {fon9::StrView{"*"},         kRightsAdmin},
         {fon9::StrView{"8610-32"},   kRights32}, // 不含子帳, 若需要全部子帳, 則應增加 "8610-32-*"
         {fon9::StrView{"8610-43"},   kRights43},
         {fon9::StrView{"8610-43-*"}, kRights4a},
      };
      const f9omstw::OmsIvRight  kRightsOther = kRightsAdmin | f9omstw::OmsIvRight::IsAdmin;
      const IvacItem kIvacItems[] = {
         {fon9::StrView{"8610"}, 10, fon9::StrView{nullptr}, kRightsOther},
         {fon9::StrView{"8610"}, 21, fon9::StrView{"sa201"}, kRightsOther},
         {fon9::StrView{"8610"}, 32, fon9::StrView{nullptr}, kRights32},
         {fon9::StrView{"8610"}, 32, fon9::StrView{"sa201"}, kRightsOther},
         {fon9::StrView{"8610"}, 43, fon9::StrView{nullptr}, kRights43},
         {fon9::StrView{"8610"}, 43, fon9::StrView{"sa401"}, kRights4a},
      };
      this->TestCase("admin & some accounts", kPolicyItems, kIvacItems, kRightsOther);
   }
   void TestCase3() {
      const f9omstw::OmsIvRight  kRights32 = static_cast<f9omstw::OmsIvRight>(0x32);
      const f9omstw::OmsIvRight  kRights43 = static_cast<f9omstw::OmsIvRight>(0x43);
      const f9omstw::OmsIvRight  kRights401 = static_cast<f9omstw::OmsIvRight>(0x4a);
      const PolicyItem kPolicyItems[] = {
         {fon9::StrView{"8610-32"},       kRights32},
         {fon9::StrView{"8610-43"},       kRights43},
         {fon9::StrView{"8610-43-sa401"}, kRights401},
      };
      const f9omstw::OmsIvRight  kRightsOther = f9omstw::OmsIvRight::DenyAll;
      const IvacItem kIvacItems[] = {
         {fon9::StrView{"8610"}, 10, fon9::StrView{nullptr}, kRightsOther},
         {fon9::StrView{"8610"}, 21, fon9::StrView{"sa201"}, kRightsOther},
         {fon9::StrView{"8610"}, 32, fon9::StrView{nullptr}, kRights32},
         {fon9::StrView{"8610"}, 32, fon9::StrView{"sa201"}, kRightsOther},
         {fon9::StrView{"8610"}, 43, fon9::StrView{nullptr}, kRights43},
         {fon9::StrView{"8610"}, 43, fon9::StrView{"sa401"}, kRights401},
         {fon9::StrView{"8610"}, 43, fon9::StrView{"sa402"}, kRightsOther},
      };
      this->TestCase("some accounts", kPolicyItems, kIvacItems, kRightsOther);
   }
   void TestCase4() {
      const f9omstw::OmsIvRight  kRights8611 = static_cast<f9omstw::OmsIvRight>(0x11);
      const f9omstw::OmsIvRight  kRights8612 = static_cast<f9omstw::OmsIvRight>(0x12);
      const f9omstw::OmsIvRight  kRights32 = static_cast<f9omstw::OmsIvRight>(0x32);
      const f9omstw::OmsIvRight  kRights43 = static_cast<f9omstw::OmsIvRight>(0x43);
      const f9omstw::OmsIvRight  kRights4a = static_cast<f9omstw::OmsIvRight>(0x4a);
      const f9omstw::OmsIvRight  kRights97 = static_cast<f9omstw::OmsIvRight>(0x97);
      const f9omstw::OmsIvRight  kRights98 = static_cast<f9omstw::OmsIvRight>(0x98);
      const f9omstw::OmsIvRight  kRightsSA = static_cast<f9omstw::OmsIvRight>(0x0a);
      const f9omstw::OmsIvRight  kRights8B = static_cast<f9omstw::OmsIvRight>(0x0b);
      const PolicyItem kPolicyItems[] = {
         {fon9::StrView{"8611"},         kRights8611},
         {fon9::StrView{"8612-*"},       kRights8612},
         {fon9::StrView{"8610-32"},      kRights32},
         {fon9::StrView{"8610-43"},      kRights43},
         {fon9::StrView{"8610-43-sa??"}, kRights4a},
         {fon9::StrView{"8610-97?????"}, kRights97}, // = 8610-97????? 不含子帳.
         {fon9::StrView{"8610-98*"},     kRights98}, // = 8610-98????? 及其全部的子帳.
         {fon9::StrView{"8610-*-?A*"},   kRightsSA},
         {fon9::StrView{"8610-8*-*B*"},  kRights8B},
      };
      const f9omstw::OmsIvRight  kRightsOther = f9omstw::OmsIvRight::DenyAll;
      const IvacItem kIvacItems[] = {
         {fon9::StrView{"8611"}, 10, fon9::StrView{nullptr}, kRights8611},
         {fon9::StrView{"8612"}, 10, fon9::StrView{nullptr}, kRights8612},
         {fon9::StrView{"8612"}, 10, fon9::StrView{"sa001"}, kRights8612},

         {fon9::StrView{"8610"}, 10, fon9::StrView{nullptr}, kRightsOther},
         {fon9::StrView{"8610"}, 21, fon9::StrView{"sa201"}, kRightsOther},
         {fon9::StrView{"8610"}, 21, fon9::StrView{"SA201"}, kRightsSA},
         {fon9::StrView{"8610"}, 21, fon9::StrView{"SSA21"}, kRightsOther},
         {fon9::StrView{"8610"}, 21, fon9::StrView{"A21"},   kRightsOther},

         {fon9::StrView{"8610"}, 32, fon9::StrView{nullptr}, kRights32},
         {fon9::StrView{"8610"}, 32, fon9::StrView{"sa201"}, kRightsOther},

         {fon9::StrView{"8610"}, 43, fon9::StrView{nullptr}, kRights43},
         {fon9::StrView{"8610"}, 43, fon9::StrView{"sa401"}, kRightsOther},
         {fon9::StrView{"8610"}, 43, fon9::StrView{"sb401"}, kRightsOther},
         {fon9::StrView{"8610"}, 43, fon9::StrView{"sa41"},  kRights4a},
         {fon9::StrView{"8610"}, 43, fon9::StrView{"sb41"},  kRightsOther},

         {fon9::StrView{"8610"}, 97,      fon9::StrView{nullptr}, kRightsOther},
         {fon9::StrView{"8610"}, 970,     fon9::StrView{nullptr}, kRightsOther},
         {fon9::StrView{"8610"}, 9712345, fon9::StrView{nullptr}, kRights97},
         {fon9::StrView{"8610"}, 9765535, fon9::StrView{nullptr}, kRights97},
         {fon9::StrView{"8610"}, 9765535, fon9::StrView{"sa001"}, kRightsOther},

         {fon9::StrView{"8610"}, 98,      fon9::StrView{nullptr}, kRightsOther},
         {fon9::StrView{"8610"}, 980,     fon9::StrView{nullptr}, kRightsOther},
         {fon9::StrView{"8610"}, 9812345, fon9::StrView{nullptr}, kRights98},
         {fon9::StrView{"8610"}, 9865535, fon9::StrView{nullptr}, kRights98},
         {fon9::StrView{"8610"}, 9865535, fon9::StrView{"sa001"}, kRights98},

         {fon9::StrView{"8610"}, 88,      fon9::StrView{nullptr}, kRightsOther},
         {fon9::StrView{"8610"}, 880,     fon9::StrView{nullptr}, kRightsOther},
         {fon9::StrView{"8610"}, 8812345, fon9::StrView{nullptr}, kRightsOther},
         {fon9::StrView{"8610"}, 8865535, fon9::StrView{"sa001"}, kRightsOther},
         {fon9::StrView{"8610"}, 8865535, fon9::StrView{"B0001"}, kRights8B},
         {fon9::StrView{"8610"}, 8865535, fon9::StrView{"0B001"}, kRights8B},
         {fon9::StrView{"8610"}, 8865535, fon9::StrView{"00B01"}, kRights8B},
      };
      this->TestCase("wildcards", kPolicyItems, kIvacItems, kRightsOther);
   }
};
using UtCoreSP = fon9::intrusive_ptr<OmsRequestPolicy_UT>;
//--------------------------------------------------------------------------//
void TestTeamList(const fon9::StrView cfg, const fon9::StrView expected) {
   std::cout << "[TEST ] TeamConfig=" << cfg.begin() << "|expected=" << expected.begin();
   f9omstw::OmsOrdTeamList list;
   f9omstw::ConfigToTeamList(list, cfg);
   std::string res;
   while(!list.empty())  {
      if (!res.empty())
         res.push_back(',');
      const auto& team = list.back();
      res.append(team.begin(), team.size());
      list.pop_back();
   }
   if (expected == fon9::StrView(&res)) {
      std::cout << "\r[OK   ] " << std::endl;
      return;
   }
   std::cout << "|result=" << res << "\r[ERROR] " << std::endl;
   abort();
}
//--------------------------------------------------------------------------//
int main(int argc, char* argv[]) {
   (void)argc; (void)argv;
   #if defined(_MSC_VER) && defined(_DEBUG)
      _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
      //_CrtSetBreakAlloc(176);
   #endif
   fon9::AutoPrintTestInfo utinfo{"OmsRequestPolicy"};

   //----- 測試 ConfigToTeamList()
   TestTeamList("A",          "A");
   TestTeamList("A,B,C",      "A,B,C");
   TestTeamList("8-C,1,2,3",  "8,9,A,B,C,1,2,3");
   TestTeamList("0,1,2,8-C4", "0,1,2,8,9,A,B,C0,C1,C2,C3,C4");
   TestTeamList("8-,X",       "8,X");
   TestTeamList("-8,X",       "8,X");

   TestTeamList("C1-8",       "8,9,A,B,C0,C1");
   TestTeamList("8-8",        "8");
   TestTeamList("8-80",       "80");
   TestTeamList("8-81",       "80,81");
   TestTeamList("8-C0",       "8,9,A,B,C0");
   TestTeamList("8-C1",       "8,9,A,B,C0,C1");
   TestTeamList("8-C3",       "8,9,A,B,C0,C1,C2,C3");
   TestTeamList("8-C00",      "8,9,A,B,C00");
   TestTeamList("8-C01",      "8,9,A,B,C00,C01");
   TestTeamList("8-C03",      "8,9,A,B,C00,C01,C02,C03");

   TestTeamList("83-8",       "80,81,82,83"); // = "8-83"
   TestTeamList("8x-8x",      "8x");
   TestTeamList("8x-8y",      "8x,8y");
   TestTeamList("8x-8z",      "8x,8y,8z");
   TestTeamList("8x-8z3",     "8x,8y,8z0,8z1,8z2,8z3");
   TestTeamList("8x-C",       "8x,8y,8z,9,A,B,C");
   TestTeamList("8x-C3",      "8x,8y,8z,9,A,B,C0,C1,C2,C3");
   TestTeamList("8x-C03",     "8x,8y,8z,9,A,B,C00,C01,C02,C03");
   TestTeamList("8x-C23",     "8x,8y,8z,9,A,B,C0,C1,C20,C21,C22,C23");

   TestTeamList("8xy-C23",    "8xy,8xz,8y,8z,9,A,B,C0,C1,C20,C21,C22,C23");
   TestTeamList("8xy-C2",     "8xy,8xz,8y,8z,9,A,B,C0,C1,C2");
   TestTeamList("8xy-C",      "8xy,8xz,8y,8z,9,A,B,C");
   TestTeamList("8zz-C",      "8zz,9,A,B,C");
   TestTeamList("zyz-zz",     "zyz,zz");
   TestTeamList("yzz-zz",     "yzz,z");
   TestTeamList("Az-B",       "Az,B");
   TestTeamList("Azz-B",      "Azz,B");

   //----- 測試 OmsIvKey 的正規化.
   struct IvKeyTestCase {
      fon9::StrView  Str_;
      fon9::StrView  Res_;
   };
   const IvKeyTestCase ivKeyTestCases[] = {
      {fon9::StrView{},                 fon9::StrView{"*"}},
      {fon9::StrView{"*"},              fon9::StrView{"*"}},
      {fon9::StrView{"86*"},            fon9::StrView{"86*"}},
      {fon9::StrView{"1234567"},        fon9::StrView{"1234567"}},
      {fon9::StrView{"12345678"},       fon9::StrView{"1234567"}},
      {fon9::StrView{"8610"},           fon9::StrView{"8610"}},
      {fon9::StrView{"8610-"},          fon9::StrView{"8610"}},
      {fon9::StrView{"8610--"},         fon9::StrView{"8610"}},
      {fon9::StrView{"8610--sa123"},    fon9::StrView{"8610-*-sa123"}},
      {fon9::StrView{"--sa123"},        fon9::StrView{"*-*-sa123"}},
      {fon9::StrView{"8610-0"},         fon9::StrView{"8610-0000000"}},
      {fon9::StrView{"8610-12"},        fon9::StrView{"8610-0000012"}},
      {fon9::StrView{"8610-12-"},       fon9::StrView{"8610-0000012"}},
      {fon9::StrView{"8610-12-sa001"},  fon9::StrView{"8610-0000012-sa001"}},
      {fon9::StrView{"8610-12*"},       fon9::StrView{"8610-12*"}},
      {fon9::StrView{"8610-12*-"},      fon9::StrView{"8610-12*"}},
      {fon9::StrView{"8610-12*-sa001"}, fon9::StrView{"8610-12*-sa001"}},
   };
   std::cout << "[TEST ] OmsIvKey";
   for (const auto& tc : ivKeyTestCases) {
      f9omstw::OmsIvKey ivkey{tc.Str_};
      if (ToStrView(ivkey) != tc.Res_) {
         std::cout << "|str=" << tc.Str_.begin() << "|res=" << ivkey.ToString() << "|expected=" << tc.Res_.begin()
            << "\r[ERROR]" << std::endl;
         abort();
      }
   }
   std::cout << "\r[OK   ]" << std::endl;

   //----- 測試是否有取得正確的可用帳號.
   UtCoreSP utCore{new OmsRequestPolicy_UT};
   utCore->TestCase("Empty confg: DenyAll", nullptr, 0, nullptr, 0, f9omstw::OmsIvRight::DenyAll);
   utCore->TestCase1();
   utCore->TestCase2();
   utCore->TestCase3();
   utCore->TestCase4();
}
 
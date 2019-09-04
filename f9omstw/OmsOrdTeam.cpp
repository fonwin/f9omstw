/// \file f9omstw/OmsOrdTeam.cpp
/// \author fonwinz@gmail.com
#include "f9omstw/OmsOrdTeam.hpp"
#include "f9omstw/OmsTools.hpp"
#include <algorithm> // std::reverse()

namespace f9omstw {

static bool ValidateTeam(fon9::StrView team) {
   for (char ch : team)
      if (!fon9::isalnum(ch))
         return false;
   return true;
}
void ConfigToTeamList(OmsOrdTeamList& list, fon9::StrView cfg) {
   auto bfsz = list.size();
   while (!cfg.empty()) {
      fon9::StrView rangeTo = fon9::StrFetchTrim(cfg, ',');
      if (rangeTo.empty())
         continue;
      fon9::StrView rangeFrom = fon9::StrFetchTrim(rangeTo, '-');
      if (!ValidateTeam(rangeFrom) || !ValidateTeam(StrTrimHead(&rangeTo)))
         continue;
      if (rangeTo.empty() || rangeFrom == rangeTo) { // "X" 或 "X-" 或 "X-X"
         list.emplace_back(rangeFrom);
         continue;
      }
      if (rangeFrom.empty()) { // "-X"
         list.emplace_back(rangeTo);
         continue;
      }
      if (rangeFrom > rangeTo)
         std::swap(rangeFrom, rangeTo);
      OmsOrdTeam  team{rangeFrom, '\0'};
      OmsOrdTeam  last{rangeTo, '\0'};
      do {
         char& chz = last.back();
         if (chz != 'z')
            break;
         chz = '\0';
         last.pop_back();
      } while (!last.empty());

      const char* const pteam = team.begin();
      const char* const plast = last.begin();
      for (uint8_t idx = 0; idx < team.max_size(); ) {
         char ch = pteam[idx];
      __CONTINUE_CHECK_AT_IDX:
         if (ch == plast[idx]) {
            if (ch == 0) {
               list.push_back(team);
               break;
            }
            ++idx;
            continue;
         }
         if (ch == 0) { // "A-Axx" rangeFrom 的尾端補 '0' => "A00-Axx"
            assert(team.size() == idx);
            team.push_back(ch = '0');
            goto __CONTINUE_CHECK_AT_IDX;
         }
         ch = plast[idx];
         list.push_back(team);
         char* pteamBack = &team.back();
         if (++idx < team.size()) { // "Axy-X" => Axy,Axz,Ay,Az
            do { 
               while (f9omstw_IncStrAlpha(pteamBack, pteamBack + 1)) {
                  list.push_back(team);
               }
               *pteamBack-- = '\0';
               team.pop_back();
            } while (idx < team.size());
         }
         for(;;) {
            if (!f9omstw_IncStrAlpha(pteamBack, pteamBack + 1)) // "A1-Az"
               goto __RANGE_DONE;
            if (*pteamBack == ch) // "A1-A3" or "A1-A3x" 處理到 "A3" 時,
               break;             // 再回到 for(idx) 看看是否需要在尾端補 '0';
            list.push_back(team);
         }
      }
   __RANGE_DONE:;
   }
   std::reverse(list.begin() + static_cast<signed>(bfsz), list.end());
}

const OmsOrdTeamGroupCfg* OmsOrdTeamGroupMgr::SetTeamGroup(fon9::StrView name, fon9::StrView cfgstr) {
   if (name.empty())
      return nullptr;
   TeamNameMap::value_type* tname;
   if (!cfgstr.empty())
      tname = &this->TeamNameMap_.kfetch(fon9::CharVector{name});
   else {
      auto ifind = this->TeamNameMap_.find(fon9::CharVector::MakeRef(name));
      if (ifind == this->TeamNameMap_.end())
         return nullptr;
      tname = &*ifind;
   }
   OmsOrdTeamGroupCfg*  tgCfg;
   if (tname->second != 0)
      tgCfg = &this->TeamGroupCfgs_[tname->second - 1];
   else {
      this->TeamGroupCfgs_.resize(tname->second = static_cast<OmsOrdTeamGroupId>(this->TeamGroupCfgs_.size() + 1));
      tgCfg = &this->TeamGroupCfgs_.back();
      tgCfg->Name_.assign(name);
      tgCfg->TeamGroupId_ = tname->second;
   }
   if (ToStrView(tgCfg->Config_) != fon9::StrTrim(&cfgstr)) {
      tgCfg->Config_.assign(cfgstr);
      if ((tgCfg->IsAllowAnyOrdNo_ = (cfgstr.Get1st() == '*')) == true)
         cfgstr.SetBegin(cfgstr.begin() + 1);
      tgCfg->TeamList_.clear();
      ConfigToTeamList(tgCfg->TeamList_, cfgstr);
      ++tgCfg->UpdatedCount_;
   }
   return tgCfg;
}

} // namespaces

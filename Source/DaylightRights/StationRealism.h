#pragma once
#include "HAL/IConsoleManager.h"
namespace StationRealism
{
    inline bool Enabled()
    {
        const auto* Var=IConsoleManager::Get().FindConsoleVariable(TEXT("Daylight.Realism"));
        return !Var||Var->GetInt()!=0;
    }
}

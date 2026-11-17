// Interface colours. INK: dark text on the parchment panels. LT: light text on dark wood, for things drawn over the world.
#pragma once
#include "core/types.h"

namespace hc::pal {

inline constexpr Color text{58, 36, 20}, muted{107, 76, 46}, faint{156, 130, 98}, gold{138, 90, 12}, ok{46, 122, 60}, bad{176, 53, 44}, sel{63, 143, 94};
inline constexpr Color slot{220, 197, 150}, slotSel{207, 228, 178}, line{168, 134, 90}, well{236, 220, 174}, flash{214, 236, 192}, night{52, 80, 138}, btnInk{29, 58, 38};
inline constexpr Color ltText{246, 234, 208}, ltMuted{220, 202, 164}, ltGold{255, 210, 122}, ltBox{52, 34, 20, 224}, ltEdge{36, 22, 12}, ink{22, 18, 31};
inline constexpr Color xp{150, 110, 230}, heal{123, 224, 138}, sky{159, 210, 255};

}   // namespace hc::pal

// Interface colours. The panels are dark iron and leather, so text is light: bone, brass and faded gold.
// LT: light text on dark wood, for things drawn over the world.
#pragma once
#include "core/types.h"

namespace hc::pal {

inline constexpr Color text{226, 214, 190}, muted{164, 148, 122}, faint{112, 100, 84}, gold{216, 172, 86}, ok{132, 190, 112}, bad{222, 92, 74}, sel{200, 160, 82};
inline constexpr Color slot{34, 28, 24}, slotSel{62, 50, 32}, line{88, 72, 54}, well{24, 20, 17}, flash{78, 66, 38}, night{132, 160, 220}, btnInk{238, 224, 196};
inline constexpr Color ltText{246, 234, 208}, ltMuted{220, 202, 164}, ltGold{255, 210, 122}, ltBox{52, 34, 20, 224}, ltEdge{36, 22, 12}, ink{22, 18, 31};
inline constexpr Color xp{150, 110, 230}, heal{123, 224, 138}, sky{159, 210, 255};

}   // namespace hc::pal

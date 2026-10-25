#pragma once

#include "hardware/BoardProfile.h"
#include "hardware/displays/Ssd1683DisplayDriver.h"
#include "hardware/displays/Uc8151DisplayDriver.h"

namespace transitink::hardware {

template <DisplayDriverKind Kind>
struct DisplayDriverFor;

template <>
struct DisplayDriverFor<DisplayDriverKind::Ssd1683> {
    using Type = Ssd1683DisplayDriver;
};

template <>
struct DisplayDriverFor<DisplayDriverKind::Uc8151> {
    using Type = Uc8151DisplayDriver;
};

using SelectedDisplayDriver =
    typename DisplayDriverFor<kBoardProfile.display.driver>::Type;

}  // namespace transitink::hardware

#pragma once

#include <cstdint>

struct sector_t;

bool ChangeSector(sector_t& sector, const bool bCrunch) noexcept;

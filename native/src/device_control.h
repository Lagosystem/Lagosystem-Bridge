// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <nlohmann/json.hpp>
namespace lagosystem::hardware {
/// Generic device APIs without any Lagosystem account, server, tenant or authentication.
nlohmann::json DeviceDispatch(const nlohmann::json& request);
}

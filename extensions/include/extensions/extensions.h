#pragma once

#include "lego1_export.h"

#include <functional>
#include <optional>
#include <string>

namespace Extensions
{
constexpr const char* availableExtensions[] = {"extensions:texture loader"};

LEGO1_EXPORT void Enable(const char* p_key);

template <typename T>
struct Extension {
	template <typename Function, typename... Args>
	static auto Call(Function&& function, Args&&... args) -> std::optional<std::invoke_result_t<Function, Args...>>
	{
#ifdef EXTENSIONS
		if (T::enabled) {
			return std::invoke(std::forward<Function>(function), std::forward<Args>(args)...);
		}
#endif
		return std::nullopt;
	}
};
}; // namespace Extensions

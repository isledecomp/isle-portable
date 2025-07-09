#pragma once

#include "lego1_export.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace Extensions
{
extern std::vector<std::string> enabledExtensions;

constexpr const char* availableExtensions[] = {"extensions:texture loader"};

LEGO1_EXPORT void Enable(const char* p_key);

template <typename T>
struct Extension {
	template <typename Function, typename... Args>
	static auto Call(Function&& function, Args&&... args) -> std::optional<std::invoke_result_t<Function, Args...>>
	{
#ifdef EXTENSIONS
		if (std::find(enabledExtensions.begin(), enabledExtensions.end(), T::key) != enabledExtensions.end()) {
			return std::invoke(std::forward<Function>(function), std::forward<Args>(args)...);
		}
#endif
		return std::nullopt;
	}
};
}; // namespace Extensions

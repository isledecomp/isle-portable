#pragma once

#include "lego1_export.h"

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <type_traits>

namespace Extensions
{
constexpr const char* availableExtensions[] =
	{"extensions:texture loader", "extensions:si loader", "extensions:third person camera", "extensions:multiplayer"};

LEGO1_EXPORT void Enable(const char* p_key, std::map<std::string, std::string> p_options);

template <typename T>
struct Extension {
	template <typename Function, typename... Args>
	static auto Call(Function&& function, Args&&... args)
	{
		using result_t = std::invoke_result_t<Function, Args...>;
		if constexpr (std::is_void_v<result_t>) {
#ifdef EXTENSIONS
			if (T::enabled) {
				std::invoke(std::forward<Function>(function), std::forward<Args>(args)...);
			}
#endif
		}
		else {
#ifdef EXTENSIONS
			if (T::enabled) {
				return std::optional<result_t>(
					std::invoke(std::forward<Function>(function), std::forward<Args>(args)...)
				);
			}
#endif
			return std::optional<result_t>(std::nullopt);
		}
	}
};
}; // namespace Extensions

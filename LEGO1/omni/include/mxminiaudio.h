#ifndef MXMINIAUDIO_H
#define MXMINIAUDIO_H

#include "mxtypes.h"

#include <miniaudio.h>
#include <utility>

template <typename T>
class MxMiniaudio : public T {
public:
	MxMiniaudio() : m_initialized(false) {}

	template <typename Fn, typename... Args>
	ma_result Init(Fn p_init, Args&&... p_args)
	{
		ma_result result = p_init(std::forward<Args>(p_args)..., this);
		if (result == MA_SUCCESS) {
			m_initialized = true;
		}

		return result;
	}

	template <typename Fn>
	void Destroy(Fn p_uninit)
	{
		if (m_initialized) {
			p_uninit(this);
			m_initialized = false;
		}
	}

private:
	bool m_initialized;
};

#endif // MXMINIAUDIO_H

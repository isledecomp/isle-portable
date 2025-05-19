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
	ma_result Init(Fn ma_init, Args&&... args)
	{
		ma_result result = ma_init(std::forward<Args>(args)..., this);
		if (result == MA_SUCCESS) {
			m_initialized = true;
		}

		return result;
	}

	template <typename Fn>
	void Destroy(Fn ma_uninit)
	{
		if (m_initialized) {
			ma_uninit(this);
			m_initialized = false;
		}
	}

private:
	bool m_initialized;
};

#endif // MXMINIAUDIO_H

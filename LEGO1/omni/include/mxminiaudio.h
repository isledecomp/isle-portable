#ifndef MXMINIAUDIO_H
#define MXMINIAUDIO_H

#include "mxtypes.h"

#include <assert.h>
#include <miniaudio.h>
#include <utility>

template <typename T>
class MxMiniaudio {
public:
	MxMiniaudio() : m_initialized(false) {}

	template <typename Fn, typename... Args>
	ma_result Init(Fn p_init, Args&&... p_args)
	{
		assert(!m_initialized);

		ma_result result = p_init(std::forward<Args>(p_args)..., &m_object);
		if (result == MA_SUCCESS) {
			m_initialized = true;
		}

		return result;
	}

	template <typename Fn>
	void Destroy(Fn p_uninit)
	{
		if (m_initialized) {
			p_uninit(&m_object);
			m_initialized = false;
		}
	}

	T* operator->()
	{
		assert(m_initialized);
		if (m_initialized) {
			return &m_object;
		}

		return nullptr;
	}

	operator T*()
	{
		assert(m_initialized);
		if (m_initialized) {
			return &m_object;
		}

		return nullptr;
	}

	explicit operator bool() { return m_initialized; }

private:
	T m_object;
	bool m_initialized;
};

#endif // MXMINIAUDIO_H

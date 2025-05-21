#include "legostorage.h"

#include "decomp.h"

#include <SDL3/SDL_messagebox.h>
#include <memory.h>
#include <string.h>

DECOMP_SIZE_ASSERT(LegoStorage, 0x08);
DECOMP_SIZE_ASSERT(LegoMemory, 0x10);
DECOMP_SIZE_ASSERT(LegoFile, 0x0c);

// FUNCTION: LEGO1 0x10099080
LegoMemory::LegoMemory(void* p_buffer, LegoU32 p_size) : LegoStorage()
{
	m_buffer = (LegoU8*) p_buffer;
	m_position = 0;
	m_size = p_size;
}

// FUNCTION: LEGO1 0x10099160
LegoResult LegoMemory::Read(void* p_buffer, LegoU32 p_size)
{
	assert(m_position + p_size <= m_size);
	memcpy(p_buffer, m_buffer + m_position, p_size);
	m_position += p_size;
	return SUCCESS;
}

// FUNCTION: LEGO1 0x10099190
LegoResult LegoMemory::Write(const void* p_buffer, LegoU32 p_size)
{
	assert(m_position + p_size <= m_size);
	memcpy(m_buffer + m_position, p_buffer, p_size);
	m_position += p_size;
	return SUCCESS;
}

// FUNCTION: LEGO1 0x100991c0
LegoFile::LegoFile()
{
	m_file = NULL;
}

// FUNCTION: LEGO1 0x10099250
LegoFile::~LegoFile()
{
	if (m_file) {
		SDL_CloseIO(m_file);
	}
}

// FUNCTION: LEGO1 0x100992c0
LegoResult LegoFile::Read(void* p_buffer, LegoU32 p_size)
{
	if (!m_file) {
		return FAILURE;
	}
	if (SDL_ReadIO(m_file, p_buffer, p_size) != p_size) {
		return FAILURE;
	}
	return SUCCESS;
}

// FUNCTION: LEGO1 0x10099300
LegoResult LegoFile::Write(const void* p_buffer, LegoU32 p_size)
{
	if (!m_file) {
		return FAILURE;
	}
	if (SDL_WriteIO(m_file, p_buffer, p_size) != p_size) {
		return FAILURE;
	}
	return SUCCESS;
}

// FUNCTION: LEGO1 0x10099340
LegoResult LegoFile::GetPosition(LegoU32& p_position)
{
	if (!m_file) {
		return FAILURE;
	}
	Sint64 position = SDL_TellIO(m_file);
	if (position == -1) {
		return FAILURE;
	}
	p_position = position;
	return SUCCESS;
}

// FUNCTION: LEGO1 0x10099370
LegoResult LegoFile::SetPosition(LegoU32 p_position)
{
	if (!m_file) {
		return FAILURE;
	}
	if (SDL_SeekIO(m_file, p_position, SDL_IO_SEEK_SET) != p_position) {
		return FAILURE;
	}
	return SUCCESS;
}

// FUNCTION: LEGO1 0x100993a0
LegoResult LegoFile::Open(const char* p_name, LegoU32 p_mode)
{
	if (m_file) {
		SDL_CloseIO(m_file);
	}
	char mode[4];
	mode[0] = '\0';
	if (p_mode & c_read) {
		m_mode = c_read;
		strcat(mode, "r");
	}
	if (p_mode & c_write) {
		if (m_mode != c_read) {
			m_mode = c_write;
		}
		strcat(mode, "w");
	}
	if ((p_mode & c_text) != 0) {
	}
	else {
		strcat(mode, "b");
	}

	MxString path(p_name);
	path.MapPathToFilesystem();

	if (!(m_file = SDL_IOFromFile(path.GetData(), mode))) {
		char buffer[256];
		SDL_snprintf(
			buffer,
			sizeof(buffer),
			"\"LEGO® Island\" failed to load %s.\nPlease make sure this file is available on HD/CD.",
			path.GetData()
		);
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "LEGO® Island Error", buffer, NULL);
		return FAILURE;
	}
	return SUCCESS;
}

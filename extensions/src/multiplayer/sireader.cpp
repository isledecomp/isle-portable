#include "extensions/multiplayer/sireader.h"

#include "extensions/common/pathutils.h"
#include "mxautolock.h"

#include <SDL3/SDL_stdinc.h>
#include <file.h>
#include <interleaf.h>

using namespace Multiplayer;

SIReader::SIReader() : m_siFile(nullptr), m_interleaf(nullptr), m_siReady(false)
{
}

SIReader::~SIReader()
{
	delete m_interleaf;
	delete m_siFile;
}

bool SIReader::OpenHeaderOnly(const char* p_siPath, si::File*& p_file, si::Interleaf*& p_interleaf)
{
	p_file = new si::File();

	MxString path;
	if (!Extensions::Common::ResolveGamePath(p_siPath, path) || !p_file->Open(path.GetData(), si::File::Read)) {
		delete p_file;
		p_file = nullptr;
		return false;
	}

	p_interleaf = new si::Interleaf();
	if (p_interleaf->Read(p_file, si::Interleaf::HeaderOnly) != si::Interleaf::ERROR_SUCCESS) {
		delete p_interleaf;
		p_interleaf = nullptr;
		p_file->Close();
		delete p_file;
		p_file = nullptr;
		return false;
	}

	return true;
}

bool SIReader::Open()
{
	if (m_siReady) {
		return true;
	}

	if (!OpenHeaderOnly("\\lego\\scripts\\isle\\isle.si", m_siFile, m_interleaf)) {
		return false;
	}

	m_siReady = true;
	return true;
}

bool SIReader::ReadObject(uint32_t p_objectId)
{
	if (!m_siReady) {
		return false;
	}

	size_t childCount = m_interleaf->GetChildCount();
	if (p_objectId >= childCount) {
		return false;
	}

	si::Object* obj = static_cast<si::Object*>(m_interleaf->GetChildAt(p_objectId));
	if (obj->type() != si::MxOb::Null) {
		return true;
	}

	return m_interleaf->ReadObject(m_siFile, p_objectId) == si::Interleaf::ERROR_SUCCESS;
}

si::Object* SIReader::GetObject(uint32_t p_objectId)
{
	if (!m_siReady) {
		return nullptr;
	}

	size_t childCount = m_interleaf->GetChildCount();
	if (p_objectId >= childCount) {
		return nullptr;
	}

	return static_cast<si::Object*>(m_interleaf->GetChildAt(p_objectId));
}

bool SIReader::ExtractAudioTrack(si::Object* p_child, AudioTrack& p_out)
{
	auto& chunks = p_child->data_;
	if (chunks.size() < 2) {
		return false;
	}

	// data_[0] = WaveFormat header, data_[1..N] = raw PCM blocks
	const auto& header = chunks[0];
	if (header.size() < sizeof(MxWavePresenter::WaveFormat)) {
		return false;
	}

	SDL_memcpy(&p_out.format, header.data(), sizeof(MxWavePresenter::WaveFormat));
	p_out.pcmData = nullptr;
	p_out.pcmDataSize = 0;
	p_out.volume = (int32_t) p_child->volume_;
	p_out.timeOffset = p_child->time_offset_;
	p_out.mediaSrcPath = p_child->filename_;

	MxU32 totalPcm = 0;
	for (size_t i = 1; i < chunks.size(); i++) {
		totalPcm += (MxU32) chunks[i].size();
	}

	if (totalPcm == 0) {
		return false;
	}

	p_out.pcmData = new MxU8[totalPcm];
	p_out.pcmDataSize = totalPcm;
	p_out.format.m_dataSize = totalPcm;
	MxU32 offset = 0;
	for (size_t i = 1; i < chunks.size(); i++) {
		SDL_memcpy(p_out.pcmData + offset, chunks[i].data(), chunks[i].size());
		offset += (MxU32) chunks[i].size();
	}

	return true;
}

AudioTrack* SIReader::ExtractFirstAudio(uint32_t p_objectId)
{
	if (!Open()) {
		return nullptr;
	}

	if (!ReadObject(p_objectId)) {
		return nullptr;
	}

	si::Object* composite = GetObject(p_objectId);
	if (!composite) {
		return nullptr;
	}

	for (size_t i = 0; i < composite->GetChildCount(); i++) {
		si::Object* child = static_cast<si::Object*>(composite->GetChildAt(i));

		if (child->filetype() == si::MxOb::WAV) {
			AudioTrack* track = new AudioTrack();
			if (ExtractAudioTrack(child, *track)) {
				return track;
			}
			delete track;
			break;
		}
	}

	return nullptr;
}

#include "extensions/multiplayer/animation/loader.h"

#include "anim/legoanim.h"
#include "extensions/common/pathutils.h"
#include "flic.h"
#include "misc/legostorage.h"
#include "mxautolock.h"
#include "mxwavepresenter.h"

#include <SDL3/SDL_stdinc.h>
#include <file.h>
#include <interleaf.h>

using namespace Multiplayer::Animation;

static void ParseExtraDirectives(const si::bytearray& p_extra, SceneAnimData& p_data)
{
	if (p_extra.empty()) {
		return;
	}

	std::string extra(p_extra.data(), p_extra.size());
	while (!extra.empty() && extra.back() == '\0') {
		extra.pop_back();
	}

	if (extra.find("HIDE_ON_STOP") != std::string::npos) {
		p_data.hideOnStop = true;
	}

	size_t pos = extra.find("PTATCAM=");
	if (pos != std::string::npos) {
		pos += 8;
		size_t end = extra.find(' ', pos);
		std::string value = (end != std::string::npos) ? extra.substr(pos, end - pos) : extra.substr(pos);

		size_t start = 0;
		while (start < value.size()) {
			size_t delim = value.find_first_of(":;", start);
			std::string token = (delim != std::string::npos) ? value.substr(start, delim - start) : value.substr(start);

			if (!token.empty()) {
				p_data.ptAtCamNames.push_back(token);
			}

			start = (delim != std::string::npos) ? delim + 1 : value.size();
		}
	}
}

SceneAnimData::SceneAnimData() : anim(nullptr), duration(0.0f), actionTransform{}, hideOnStop(false)
{
}

SceneAnimData::~SceneAnimData()
{
	delete anim;
	ReleaseTracks();
}

void SceneAnimData::ReleaseTracks()
{
	for (auto& track : audioTracks) {
		delete[] track.pcmData;
	}

	for (auto& track : phonemeTracks) {
		delete[] reinterpret_cast<MxU8*>(track.flcHeader);
	}
}

SceneAnimData::SceneAnimData(SceneAnimData&& p_other) noexcept
	: anim(p_other.anim), duration(p_other.duration), audioTracks(std::move(p_other.audioTracks)),
	  phonemeTracks(std::move(p_other.phonemeTracks)), actionTransform(p_other.actionTransform),
	  ptAtCamNames(std::move(p_other.ptAtCamNames)), hideOnStop(p_other.hideOnStop)
{
	p_other.anim = nullptr;
}

SceneAnimData& SceneAnimData::operator=(SceneAnimData&& p_other) noexcept
{
	if (this != &p_other) {
		delete anim;
		ReleaseTracks();

		anim = p_other.anim;
		duration = p_other.duration;
		audioTracks = std::move(p_other.audioTracks);
		phonemeTracks = std::move(p_other.phonemeTracks);
		actionTransform = p_other.actionTransform;
		ptAtCamNames = std::move(p_other.ptAtCamNames);
		hideOnStop = p_other.hideOnStop;
		p_other.anim = nullptr;
	}
	return *this;
}

Loader::Loader()
	: m_siFile(nullptr), m_interleaf(nullptr), m_siReady(false), m_preloadThread(nullptr), m_preloadObjectId(0),
	  m_preloadDone(false)
{
}

Loader::~Loader()
{
	CleanupPreloadThread();
	delete m_interleaf;
	delete m_siFile;
}

bool Loader::OpenSIHeaderOnly(const char* p_siPath, si::File*& p_file, si::Interleaf*& p_interleaf)
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

bool Loader::OpenSI()
{
	if (m_siReady) {
		return true;
	}

	if (!OpenSIHeaderOnly("\\lego\\scripts\\isle\\isle.si", m_siFile, m_interleaf)) {
		return false;
	}

	m_siReady = true;
	return true;
}

bool Loader::ReadObject(uint32_t p_objectId)
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

bool Loader::ParseAnimationChild(si::Object* p_child, SceneAnimData& p_data)
{
	auto& chunks = p_child->data_;
	if (chunks.empty()) {
		return false;
	}

	auto& firstChunk = chunks[0];
	if (firstChunk.size() < 7 * sizeof(MxS32)) {
		return false;
	}

	LegoMemory storage(firstChunk.data(), (LegoU32) firstChunk.size());

	MxS32 magicSig;
	if (storage.Read(&magicSig, sizeof(MxS32)) != SUCCESS || magicSig != 0x11) {
		return false;
	}

	// Skip boundingRadius + centerPoint[3] (unused, but present in the binary format)
	LegoU32 pos;
	storage.GetPosition(pos);
	storage.SetPosition(pos + 4 * sizeof(float));

	LegoS32 parseScene = 0;
	MxS32 val3;
	if (storage.Read(&parseScene, sizeof(LegoS32)) != SUCCESS) {
		return false;
	}
	if (storage.Read(&val3, sizeof(MxS32)) != SUCCESS) {
		return false;
	}

	p_data.anim = new LegoAnim();
	if (p_data.anim->Read(&storage, parseScene) != SUCCESS) {
		delete p_data.anim;
		p_data.anim = nullptr;
		return false;
	}

	p_data.duration = (float) p_data.anim->GetDuration();
	return true;
}

bool Loader::ParseSoundChild(si::Object* p_child, SceneAnimData& p_data)
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

	SceneAnimData::AudioTrack track;
	SDL_memcpy(&track.format, header.data(), sizeof(MxWavePresenter::WaveFormat));
	track.pcmData = nullptr;
	track.pcmDataSize = 0;
	track.volume = (int32_t) p_child->volume_;
	track.timeOffset = p_child->time_offset_;
	track.mediaSrcPath = p_child->filename_;

	MxU32 totalPcm = 0;
	for (size_t i = 1; i < chunks.size(); i++) {
		totalPcm += (MxU32) chunks[i].size();
	}

	if (totalPcm == 0) {
		return false;
	}

	track.pcmData = new MxU8[totalPcm];
	track.pcmDataSize = totalPcm;
	track.format.m_dataSize = totalPcm;
	MxU32 offset = 0;
	for (size_t i = 1; i < chunks.size(); i++) {
		SDL_memcpy(track.pcmData + offset, chunks[i].data(), chunks[i].size());
		offset += (MxU32) chunks[i].size();
	}

	p_data.audioTracks.push_back(std::move(track));
	return true;
}

bool Loader::ParsePhonemeChild(si::Object* p_child, SceneAnimData& p_data)
{
	auto& chunks = p_child->data_;
	if (chunks.size() < 2) {
		return false;
	}

	SceneAnimData::PhonemeTrack track;

	const auto& headerChunk = chunks[0];
	if (headerChunk.size() < sizeof(FLIC_HEADER)) {
		return false;
	}

	MxU8* headerBuf = new MxU8[headerChunk.size()];
	SDL_memcpy(headerBuf, headerChunk.data(), headerChunk.size());
	track.flcHeader = reinterpret_cast<FLIC_HEADER*>(headerBuf);
	track.width = track.flcHeader->width;
	track.height = track.flcHeader->height;

	for (size_t i = 1; i < chunks.size(); i++) {
		track.frameData.push_back(chunks[i]);
	}

	if (!p_child->extra_.empty()) {
		track.roiName = std::string(p_child->extra_.data(), p_child->extra_.size());
		while (!track.roiName.empty() && track.roiName.back() == '\0') {
			track.roiName.pop_back();
		}
	}

	track.timeOffset = p_child->time_offset_;

	p_data.phonemeTracks.push_back(std::move(track));
	return true;
}

bool Loader::ParseComposite(si::Object* p_composite, SceneAnimData& p_data)
{
	bool hasAnim = false;

	for (size_t i = 0; i < p_composite->GetChildCount(); i++) {
		si::Object* child = static_cast<si::Object*>(p_composite->GetChildAt(i));

		if (child->presenter_.find("LegoPhonemePresenter") != std::string::npos) {
			ParsePhonemeChild(child, p_data);
		}
		else if (child->presenter_.find("LegoAnimPresenter") != std::string::npos || child->presenter_.find("LegoLoopingAnimPresenter") != std::string::npos) {
			if (!hasAnim) {
				if (ParseAnimationChild(child, p_data)) {
					hasAnim = true;
					ParseExtraDirectives(child->extra_, p_data);

					// Extract action transform. Try child first, fall back to composite if zero.
					si::Object* source = child;
					if (SDL_fabs(child->direction_.x) < 1e-7 && SDL_fabs(child->direction_.y) < 1e-7 &&
						SDL_fabs(child->direction_.z) < 1e-7) {
						source = p_composite;
					}

					p_data.actionTransform.location[0] = (float) source->location_.x;
					p_data.actionTransform.location[1] = (float) source->location_.y;
					p_data.actionTransform.location[2] = (float) source->location_.z;
					p_data.actionTransform.direction[0] = (float) source->direction_.x;
					p_data.actionTransform.direction[1] = (float) source->direction_.y;
					p_data.actionTransform.direction[2] = (float) source->direction_.z;
					p_data.actionTransform.up[0] = (float) source->up_.x;
					p_data.actionTransform.up[1] = (float) source->up_.y;
					p_data.actionTransform.up[2] = (float) source->up_.z;

					p_data.actionTransform.valid =
						(SDL_fabsf(p_data.actionTransform.direction[0]) >= 0.00000047683716f ||
						 SDL_fabsf(p_data.actionTransform.direction[1]) >= 0.00000047683716f ||
						 SDL_fabsf(p_data.actionTransform.direction[2]) >= 0.00000047683716f);
				}
			}
		}
		else if (child->filetype() == si::MxOb::WAV) {
			ParseSoundChild(child, p_data);
		}
	}

	return hasAnim;
}

SceneAnimData* Loader::EnsureCached(uint32_t p_objectId)
{
	{
		AUTOLOCK(m_cacheCS);
		auto it = m_cache.find(p_objectId);
		if (it != m_cache.end()) {
			return &it->second;
		}
	}

	// If a preload is in progress for this object, wait for it to finish
	if (m_preloadThread && m_preloadObjectId == p_objectId) {
		CleanupPreloadThread();

		AUTOLOCK(m_cacheCS);
		auto it = m_cache.find(p_objectId);
		if (it != m_cache.end()) {
			return &it->second;
		}
		// Preload failed — fall through to synchronous load
	}

	if (!OpenSI()) {
		return nullptr;
	}

	if (!ReadObject(p_objectId)) {
		return nullptr;
	}

	si::Object* composite = static_cast<si::Object*>(m_interleaf->GetChildAt(p_objectId));

	SceneAnimData data;
	if (!ParseComposite(composite, data)) {
		return nullptr;
	}

	AUTOLOCK(m_cacheCS);
	auto result = m_cache.emplace(p_objectId, std::move(data));
	return &result.first->second;
}

void Loader::CleanupPreloadThread()
{
	if (m_preloadThread) {
		delete m_preloadThread;
		m_preloadThread = nullptr;
	}
}

void Loader::PreloadAsync(uint32_t p_objectId)
{
	{
		AUTOLOCK(m_cacheCS);
		if (m_cache.find(p_objectId) != m_cache.end()) {
			return;
		}
	}

	if (m_preloadThread && m_preloadObjectId == p_objectId && !m_preloadDone) {
		return;
	}

	CleanupPreloadThread();

	m_preloadObjectId = p_objectId;
	m_preloadDone = false;
	m_preloadThread = new PreloadThread(this, p_objectId);
	m_preloadThread->Start(0x1000, 0);
}

Loader::PreloadThread::PreloadThread(Loader* p_loader, uint32_t p_objectId) : m_loader(p_loader), m_objectId(p_objectId)
{
}

MxResult Loader::PreloadThread::Run()
{
	si::File* siFile = nullptr;
	si::Interleaf* interleaf = nullptr;

	if (!OpenSIHeaderOnly("\\lego\\scripts\\isle\\isle.si", siFile, interleaf)) {
		m_loader->m_preloadDone = true;
		return MxThread::Run();
	}

	size_t childCount = interleaf->GetChildCount();
	if (m_objectId < childCount && interleaf->ReadObject(siFile, m_objectId) == si::Interleaf::ERROR_SUCCESS) {
		si::Object* composite = static_cast<si::Object*>(interleaf->GetChildAt(m_objectId));

		SceneAnimData data;
		if (ParseComposite(composite, data)) {
			AUTOLOCK(m_loader->m_cacheCS);
			m_loader->m_cache.emplace(m_objectId, std::move(data));
		}
	}

	m_loader->m_preloadDone = true;

	delete interleaf;
	delete siFile;

	return MxThread::Run();
}

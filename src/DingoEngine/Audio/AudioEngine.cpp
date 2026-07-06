#include "depch.h"
#include "DingoEngine/Audio/AudioEngine.h"

#include "DingoEngine/Audio/MiniAudio/MiniAudioEngine.h"

namespace Dingo
{

	AudioEngine* AudioEngine::Create()
	{
		// miniaudio is currently the only audio backend. Future backends would be
		// selected here, mirroring Physics3D::Create / GraphicsContext::Create.
		return new MiniAudioEngine();
	}

}

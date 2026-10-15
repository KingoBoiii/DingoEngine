#pragma once

namespace Dingo
{
	class Texture;
}

namespace Dingo::Internal
{

	// Called as a texture is destroyed, a font's atlas included. Forgets every root transition's kept
	// commands and the batcher's cached vertices that reference it, so an exiting root never reads a
	// freed texture or font. Does nothing while no DingoUI context exists.
	void DingoUIOnTextureDestroyed(const Texture* texture);

}

#pragma once

//defines a texture buffer

#include <BTDSTD/Types.hpp>

#include <vulkan/vulkan.h>
#include <vector>

namespace Smok::Texture
{
	//defines a buffer array for all the textures
	struct TextureBuffer
	{
		bool sizeHasChanged = false; //is the size changed

		//the texture data
		std::vector<VkImageView> textureViews;
		std::vector<VkSampler> textureSamplers;

		//adds a texture to the array, returns array index
		inline uint32 AddTexture(VkImageView& view, VkSampler& sampler)
		{
			//checks if it already exists
			for (uint32 i = 0; i < textureViews.size(); ++i)
			{
				//if it's there
				if (textureViews[i] == view)
				{
					//if the sampler is next to this one in the samplers, we don't have to change sizes
					if (sampler == textureSamplers[i])
						return i;
				}
			}

			textureViews.emplace_back(view); textureSamplers.emplace_back(sampler);
			sizeHasChanged = true;
			return textureViews.size() - 1;
		}

		//nukes the textures and adds a texture to the new array, returns array index
		inline uint32 NukeArray_AddTexture(VkImageView& view, VkSampler& sampler)
		{
			const size_t texCount = textureViews.size();
			textureViews.clear(); textureSamplers.clear();
			textureViews.reserve(texCount); textureSamplers.reserve(texCount);
			textureViews.emplace_back(view); textureSamplers.emplace_back(sampler);
			sizeHasChanged = true;
			return 0;
		}
	};
}
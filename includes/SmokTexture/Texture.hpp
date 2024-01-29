#pragma once

//defines a texture

#include <SmokGraphics/Utils/Image.hpp>
#include <SmokGraphics/Pools/CommandPool.h>

#include <SmokGraphics/Utils/Image.h>

#include <Stb_Image.h> 

namespace Smok::Texture
{
	//defines a texture
	struct Texture
	{
		uint64 assetID = 0; //asset ID for the asset manager

		VkImage image = VK_NULL_HANDLE;
		VmaAllocation imageMemoy = VK_NULL_HANDLE;
		VkImageView view = VK_NULL_HANDLE;
		uint8 channelCount = 0;

		std::string declPath;
	};

	//saves the texture settings to a decl file
	inline bool Texture_SaveDecl(const std::string& dir, const std::string& fileName,
		const std::string& assetName, const std::string& binaryPath)
	{
		//checks if the file exists/can be made
		BTD::IO::File file;
		if (!file.Open(std::string(dir + "/" + fileName + ".texdecl"), BTD::IO::FileOP::TextWrite_OpenCreateStart))
			return false;

		//starts the file
		YAML::Emitter emitter;
		emitter << YAML::BeginMap;
		
		//loads data
		emitter << YAML::Key << "assetName" << YAML::DoubleQuoted << assetName;
		emitter << YAML::Key << "binaryPath" << YAML::DoubleQuoted << binaryPath;

		emitter << YAML::EndMap;

		//write decl file
		file.Write(emitter.c_str());
		file.Close();

		return true;
	}

	//loads the texture settings from a decl file
	inline bool Texture_LoadDecl(const std::string& declFilePath,
		std::string& assetName, std::string& binaryPath)
	{
		//checks if the decl file exists
		BTD::IO::File file;
		if (!file.Open(declFilePath, BTD::IO::FileOP::TextRead_OpenExisting))
		{
			BTD::Logger::LogError("Smok Graphics", "Image", "Image_Decl_WriteFile",
				std::string("The given decl file path can't be opened at \"" + declFilePath + "\"").c_str());
			return false;
		}

		//loads the YAML data
		YAML::Node _data = YAML::Load(file.Read());
		file.Close();

		if (!_data)
		{
			BTD::Logger::LogError("Smok Graphics", "Image", "Image_Decl_WriteFile",
				"The decl file is invalid!");
			return false;
		}

		//loads data
		assetName = _data["assetName"].as<std::string>();
		binaryPath = _data["binaryPath"].as<std::string>();

		return true;
	}

	//creates a texture from a image file
	inline bool Texture_Create(Texture* texture, const std::string& binaryPath,
		VmaAllocator& allocator, SMGraphics_Core_GPU* GPU, SMGraphics_Pool_CommandPool* commandPool)
	{
		//loads the texture data
		int channelCount = 0; BTD::Math::I32Vec2 size = { 0, 0 };
		stbi_uc* pixels = stbi_load(binaryPath.c_str(),
			&size.x, &size.y, &channelCount, STBI_rgb_alpha);
		if (!pixels)
		{
			BTD::Logger::LogError("Smok Texture", "Texture", "Texture_Create",
				std::string(std::string("Failed to open image at \"") + binaryPath +
					"\" for a Smok Texture!").c_str());
			return false;
		}

		//reassigns to the texture
		texture->channelCount = (uint8)channelCount;
		//texture->image.size = Smok_Util_Typepun(size, BTD_Math_U32Vec2);

		//image specs for Vulkan
		VkDeviceSize imageSize = size.x * size.y * 4;

		//create a staging buffer and copy the data into it
		SMGraphics_Util_Buffer stagingBuffer;
		SMGraphics_Util_Buffer_CreateStagingBuffer(&stagingBuffer, imageSize, allocator);
		memcpy(stagingBuffer.allocationInfo.pMappedData, pixels, imageSize);

		//clean up the pixel data, since it's safly in the buffer
		stbi_image_free(pixels);

		//creates the image
		VkImageCreateInfo imageInfo = SMGraphics_Util_Image_CreateInfo_Default();
		SMGraphics_Util_Image_CreateInfo_SetSize(&imageInfo, size.x, size.y);
		
		VmaAllocationCreateInfo allocCreateInfo = { 0 };
		allocCreateInfo.flags = 0;
		allocCreateInfo.memoryTypeBits = 0;
		allocCreateInfo.pool = VK_NULL_HANDLE;
		allocCreateInfo.preferredFlags = 0;
		allocCreateInfo.pUserData = NULL;
		allocCreateInfo.requiredFlags = 0;

		allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
		allocCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
		allocCreateInfo.priority = 1.0f;

		//creates image
		if (vmaCreateImage(allocator, &imageInfo, &allocCreateInfo,
			&texture->image, &texture->imageMemoy, NULL) != VK_SUCCESS)
		{
			BTD_LogError("Smok Window", "Swapchain", "CreateSwapchain", "Failed to create swapchain image views!");
			return 0;
		}

		//starts a single use buffer so we can transfer to a useable image
		SMGraphics_Pool_CommandBuffer comBuffer;
		SMGraphics_Pool_CommandPool_StartSingleUseCommandBuffer(commandPool, &comBuffer, GPU->device);

		//transition image to a format that can have data written into it
		VkImageSubresourceRange range;
		range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		range.baseMipLevel = 0;
		range.levelCount = 1;
		range.baseArrayLayer = 0;
		range.layerCount = 1;

		VkImageMemoryBarrier imageBarrier_toTransfer = {};
		imageBarrier_toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;

		imageBarrier_toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageBarrier_toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageBarrier_toTransfer.image = texture->image;
		imageBarrier_toTransfer.subresourceRange = range;

		imageBarrier_toTransfer.srcAccessMask = 0;
		imageBarrier_toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		//barrier the image into the transfer-receive layout
		vkCmdPipelineBarrier(comBuffer.comBuffer,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			0, 0, nullptr, 0, nullptr, 1, &imageBarrier_toTransfer);

		//copies data into the image
		VkBufferImageCopy copyRegion = {};
		copyRegion.bufferOffset = 0;
		copyRegion.bufferRowLength = 0;
		copyRegion.bufferImageHeight = 0;

		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.mipLevel = 0;
		copyRegion.imageSubresource.baseArrayLayer = 0;
		copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageExtent = { Smok_Util_Typepun(size.x, uint32), Smok_Util_Typepun(size.y, uint32), 1 };

		//copy the buffer into the image
		vkCmdCopyBufferToImage(comBuffer.comBuffer,
			stagingBuffer.buffer, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		//transitions the image into one that can be read by the shader
		VkImageMemoryBarrier imageBarrier_toReadable = imageBarrier_toTransfer;

		imageBarrier_toReadable.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageBarrier_toReadable.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		imageBarrier_toReadable.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imageBarrier_toReadable.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		//barrier the image into the shader readable layout
		vkCmdPipelineBarrier(comBuffer.comBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0, 0, nullptr, 0, nullptr, 1, &imageBarrier_toReadable);

		SMGraphics_Pool_CommandPool_EndSingleUseCommandBuffer(commandPool, &comBuffer, GPU->device, GPU->graphicsQueue);

		//creates depth image view
		VkImageViewCreateInfo imageViewCreateInfo = SMGraphics_Util_ImageView_CreateInfo_Default();
		SMGraphics_Util_ImageView_CreateInfo_SetImage(&imageViewCreateInfo, texture->image);

		//create image view
		if (vkCreateImageView(GPU->device, &imageViewCreateInfo, NULL, &texture->view) != VK_SUCCESS)
		{
			BTD_LogError("Smok Window", "Swapchain", "CreateSwapchain", "Failed to create swapchain image views!");
			return 0;
		}
		
		//destroys the staging buffer
		SMGraphics_Util_Buffer_Destroy(&stagingBuffer, allocator);

		return true;
	}

	//destroys a texture
	inline void Texture_Destroy(Texture* texture, SMGraphics_Core_GPU* GPU, VmaAllocator& allocator)
	{
		vkDestroyImageView(GPU->device, texture->view, NULL);
		vmaDestroyImage(allocator, texture->image, texture->imageMemoy);
	}
}
#pragma once

#define SWAP_CHAIN_BUFFER_COUNT 3
#define BACK_BUFFER_FORMAT DXGI_FORMAT_R8G8B8A8_UNORM // DXGI_FORMAT_R8G8B8A8_UNORM = 28
#define MAX_LOADSTRING 100


typedef enum ComponentTypes
{
	TRANSFORM = 1 << 0,
	MESH_REFERENCE = 1 << 1,
	SHADER_REFERENCE = 1 << 2,
	COMPONENT_TYPES_COUNT	
} ComponentType;
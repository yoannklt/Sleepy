#pragma once

#include "MathHelper.h"


class D3DUtils
{
public:
	static ID3DBlob* CompileFromFile( const std::wstring& filename, const D3D_SHADER_MACRO* defines, const std::string& entrypoint, const std::string& target);
	static ID3D12Resource* CreateDefaultBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, const void* initData, UINT64 byteSize, ID3D12Resource& uploadBuffer);

};


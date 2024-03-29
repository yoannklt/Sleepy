#pragma once
#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <string>
#include "MathHelper.h"
#include "MathHelper.h"


class D3DUtils
{
public:
	static ID3DBlob* CompileFromFile( const std::wstring& filename, const D3D_SHADER_MACRO* defines, const std::string& entrypoint, const std::string& target);
	static ID3D12Resource* CreateDefaultBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, const void* initData, UINT64 byteSize, ID3D12Resource& uploadBuffer);
    static inline std::wstring AnsiToWString(const std::string& str)
    {
        WCHAR buffer[512];
        MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
        return std::wstring(buffer);
    }
    static inline std::string HrToString(HRESULT hr)
    {
        char errorMsg[256];
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, hr, 0, errorMsg, sizeof(errorMsg), nullptr);
        return errorMsg;
    }
};


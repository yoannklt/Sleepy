#include "pch.h"

ShaderReference::ShaderReference()
{
}

void ShaderReference::Init(ShaderReferenceDescriptor* desc)
{
	m_pShader = desc->meshRef;
}

void ShaderReference::Release()
{
	RELEASE(m_pShader);
}


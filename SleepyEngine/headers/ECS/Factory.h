#pragma once

class Factory
{
public:
	Factory();
	~Factory();

	void Initialize(ResourceAllocator* rRessourceAllocator, ShaderColor* pShaderColor);

protected:
	ResourceAllocator* m_pRessourceAllocator = nullptr;
	ShaderColor* m_pShaderColor = nullptr;
};
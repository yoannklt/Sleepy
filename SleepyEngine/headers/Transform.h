#pragma once

class Transform : public Component
{
public:
	Transform();
	~Transform();

// MEMBER VARIABLES
public:
	//scale
	XMFLOAT3 m_scaleVect;
	XMFLOAT4X4 m_scaleMatrix;

	//orientation
	XMFLOAT3 m_dir;
	XMFLOAT3 m_right;
	XMFLOAT3 m_up;
	XMFLOAT4 m_currentRotateQuat;
	XMFLOAT4X4 m_currentRotateMatrix;

	//position
	XMFLOAT3 m_positionVect;
	XMFLOAT4X4 m_positionMatrix;

	//"merged" matrix
	XMFLOAT4X4 m_transformMatrix;

// METHODES
public:
	// Init
	void Identity();

	// Methods
	void Rotate(float yaw, float pitch, float roll);
	void SetScale(float x, float y, float z); 
	void SetPosition(float x, float y, float z);
	void SetRotation(float x, float y, float z);
	void SetRotation(Transform* transform);  

	void LookAt(float x, float y, float z);
	void LookAt(Transform* transform);

	void Update();

};
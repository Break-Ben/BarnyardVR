#pragma once
#include <Math/TVector4.h>
#include <GUI/T2GUITextureSection.h>

class ASysMesh;
class ASysMaterial;

// A small world-anchored sprite batch. Positions/sizes are design units, not eye
// pixels. Each eye transforms a world-up-aligned tangent frame at the anchor;
// headset rotation does not orient the sprites. The UI quad layer is not used.
class AWorldSpaceUI
{
public:
	struct Sprite
	{
		TFLOAT x, y, width, height, angle;
		TUINT32 colour;
	};

	AWorldSpaceUI() = default;
	~AWorldSpaceUI();
	void Render( const Toshi::TVector4& a_rcAnchor, const Sprite* a_pSprites, TINT a_iCount, Toshi::T2GUITextureSection* a_pTexture, TFLOAT a_fWorldScale );

	AWorldSpaceUI( const AWorldSpaceUI& ) = delete;
	AWorldSpaceUI& operator=( const AWorldSpaceUI& ) = delete;

private:
	ASysMesh* m_pMesh = TNULL;
	ASysMaterial* m_pMaterial = TNULL;
};

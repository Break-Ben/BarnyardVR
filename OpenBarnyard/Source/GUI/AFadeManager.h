#pragma once
#include "AFade.h"

#include <Toshi/TTask.h>
#include <Math/TVector4.h>

class AFadeManager : public Toshi::TTask
    , public Toshi::TSingleton<AFadeManager>
{
public:
	TDECLARE_CLASS( AFadeManager, Toshi::TTask );

public:
	AFadeManager();
	~AFadeManager();

	virtual TBOOL OnUpdate( TFLOAT a_fDeltaTime ) override;

	virtual void StopAllFades();

	AFade* StartFade( const AFade::Color& a_rFadeFrom, const AFade::Color& a_rFadeTo, TFLOAT a_fFadeTime );
	TBOOL  HasAnyFadesInProgress( TBOOL a_bThrowFadeOver );

	// Premultiplied overlay colour, using the current GUI colours (no timer updates).
	Toshi::TVector4 GetCompositeColour();
	void SetOverlaysVisible( TBOOL a_bVisible );

private:
	Toshi::T2ObjectPool<AFade, 8> m_Fades;
	Toshi::TQList<AFade>          m_ActiveFades;
};

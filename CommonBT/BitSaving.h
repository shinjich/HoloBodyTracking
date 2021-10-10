// Original Author: Shinji Chiba (MSKK)
#pragma once

class BITSAVING
{
public:
	void Save( void* pOutput, void* pInput, int iInputElements, int iInputValidBits );
	void Restore( void* pOutput, void* pInput, int iOutputElements, int iInputValidBits );
};

// Original Author: Shinji Chiba (MSKK)
#include "BitSaving.h"

void BITSAVING::Save( void* pOutput, void* pInput, int iInputElements, int iInputValidBits )
{
	unsigned long* pdwOutput = (unsigned long*) pOutput;
	unsigned long dwOutputTemp = 0;
	int iOutputTempBitRemain = 32;
	int iInputBitRemain = iInputValidBits;
	unsigned short* puInput = (unsigned short*) pInput;
	unsigned short uBuf = *puInput;
	for( ; ; )
	{
		dwOutputTemp |= (((uBuf >> --iInputBitRemain) & 1) << --iOutputTempBitRemain);
		if ( iInputBitRemain <= 0 )
		{
			if ( --iInputElements <= 0 )
			{
				*pdwOutput = dwOutputTemp;
				break;
			}
			iInputBitRemain = iInputValidBits;
			uBuf = *++puInput;
		}
		if ( iOutputTempBitRemain <= 0 )
		{
			*pdwOutput++ = dwOutputTemp;
			iOutputTempBitRemain = 32;
			dwOutputTemp = 0;
		}
	}
}

void BITSAVING::Restore( void* pOutput, void* pInput, int iOutputElements, int iInputValidBits )
{
	unsigned short* puOutput = (unsigned short*) pOutput;
	unsigned short uOutputTemp = 0;
	int iInputTempBitRemain = 32;
	int iOutputBitRemain = iInputValidBits;
	unsigned long* pdwInput = (unsigned long*) pInput;
	unsigned long dwBuf = *pdwInput;
	for( ; ; )
	{
		uOutputTemp |= (((dwBuf >> --iInputTempBitRemain) & 1) << --iOutputBitRemain);
		if ( iOutputBitRemain <= 0 )
		{
			if ( --iOutputElements <= 0 )
			{
				*puOutput = uOutputTemp | (unsigned short) (dwBuf) & ((1 << iInputTempBitRemain) - 1);
				break;
			}
			*puOutput++ = uOutputTemp;
			uOutputTemp = 0;
			iOutputBitRemain = iInputValidBits;
		}
		if ( iInputTempBitRemain <= 0 )
		{
			dwBuf = *++pdwInput;
			iInputTempBitRemain = 32;
		}
	}
}

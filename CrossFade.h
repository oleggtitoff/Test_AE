/*
 * CrossFade.h
 *
 *  Created on: Aug 23, 2019
 *      Author: Intern_2
 */

#ifndef CROSS_FADE
#define CROSS_FADE

#include "ExternalAndInternalTypesConverters.h"
#include "InternalTypesArithmetic.h"
#include "PredicatesOperations.h"

typedef enum {
	isBypassID 	= 100,
	fadeTimeID 	= 101
} CrossFadeParamID;

typedef struct {
	int sampleRate;
	int8_t isBypass;
	double fadeTime;
} CrossFadeParams;

typedef struct {
	F32x2 targetGain;		// Q31
	F32x2 fadeAlpha;		// Q31
} CrossFadeCoeffs;

typedef struct {
	F32x2 prevGain;			// Q31
} CrossFadeStates;


ALWAYS_INLINE Status fadeParamsInit(CrossFadeParams *params, const int sampleRate)
{
	if (!params)
		return statusError;

	params->sampleRate = sampleRate;

	params->isBypass = 1;
	params->fadeTime = 0;

	return statusOK;
}

ALWAYS_INLINE Status fadeCoeffsInit(CrossFadeCoeffs *coeffs)
{
	if (!coeffs)
		return statusError;

	coeffs->targetGain = 1;
	coeffs->fadeAlpha = 0;

	return statusOK;
}

ALWAYS_INLINE Status fadeStatesInit(CrossFadeStates *states)
{
	if (!states)
		return statusError;

	states->prevGain = F32x2Set(0x7fffffff);

	return statusOK;
}

ALWAYS_INLINE Status CrossFadeInit(CrossFadeParams *params, CrossFadeCoeffs *coeffs,
								   CrossFadeStates *states, const int sampleRate)
{
	Status status = 0;

	status |= fadeParamsInit(params, sampleRate);
	status |= fadeCoeffsInit(coeffs);
	status |= fadeStatesInit(states);

	return status;
}


ALWAYS_INLINE double alphaCalc(const int sampleRate, const double time)
{
	// calculates alpha coefficient from sample rate and time in ms

	return (double)1 - exp((double)-1 / (sampleRate * time / 1000));
}

ALWAYS_INLINE Status CrossFadeSetParam(CrossFadeParams *params, CrossFadeCoeffs *coeffs,
									   CrossFadeStates *states, const uint16_t id,
									   const double value)
{
	if (!params || !coeffs)
		return statusError;

	switch (id)
	{
	case isBypassID:
		params->isBypass = (int)value;
		coeffs->targetGain = doubleToF32x2Set(value);
		break;

	case fadeTimeID:
		params->fadeTime = value;
		coeffs->fadeAlpha = doubleToF32x2Set(alphaCalc(params->sampleRate, value));
		break;

	default:
		return statusError;
	}

	return statusOK;
}


ALWAYS_INLINE F32x2 calcFadeGain(const CrossFadeCoeffs *coeffs, CrossFadeStates *states)
{
	F32x2 alpha2 	 = F32x2Sub(0x7fffffff, coeffs->fadeAlpha);
	F32x2 mulRes1 	 = F32x2Mul(coeffs->targetGain, coeffs->fadeAlpha);
	F32x2 mulRes2 	 = F32x2Mul(alpha2, states->prevGain);
	states->prevGain = F32x2Add(mulRes1, mulRes2);

	F32x2MovIfTrue(&states->prevGain, coeffs->targetGain,
				   F32x2LessEqual(F32x2Abs(F32x2Sub(coeffs->targetGain, states->prevGain)),
						   	   	  F32x2Set(0xf)));

	return states->prevGain;
}

ALWAYS_INLINE F32x2 CrossFade_Process(const CrossFadeCoeffs *coeffs, CrossFadeStates *states,
							  const F32x2 bypassSample, const F32x2 sample)
{
	F32x2 gain1   = calcFadeGain(coeffs, states);
	F32x2 gain2   = F32x2Sub(0x7fffffff, gain1);
	F32x2 mulRes1 = F32x2Mul(gain1, bypassSample);
	F32x2 mulRes2 = F32x2Mul(gain2, sample);

	return F32x2Add(mulRes1, mulRes2);
}

#endif /* CROSS_FADE */

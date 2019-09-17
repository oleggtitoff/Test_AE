/*
 * main.c
 *
 *  Created on: Aug 13, 2019
 *      Author: Intern_2
 */

#include "FilesOperations.h"
#include "GetOpt.h"

#include "ExternalAndInternalTypesConverters.h"
#include "GeneralArithmetic.h"
#include "InternalTypesArithmetic.h"

#include "CrossFade.h"
#include "AmplitudeProc.h"
#include "EQ.h"


#define SAMPLE_RATE 	 48000
#define BYTES_PER_SAMPLE 4

#define HEADROOM 4

#define OPTSTRING "c:i:o:"

typedef struct {
	char *configFileName;
	char *inputFileName;
	char *outputFileName;
} FileNames;

typedef struct {
	double inputGain;
	double outputGain;
	double balance;
	CrossFadeParams crossFadeParams;
	AmplitudeProcParams amplitudeProcParams;
	EQParams eqParams;
} Params;

typedef struct {
	F32x2 inputGain;				// Q31
	F32x2 outputGain;				// Q31
	F32x2 balance;					// Q31
	CrossFadeCoeffs crossFadeCoeffs;
	AmplitudeProcCoeffs amplitudeProcCoeffs;
	EQCoeffs eqCoeffs;
} Coeffs;

typedef struct {
	CrossFadeStates crossFadeStates;
	AmplitudeProcStates amplitudeProcStates;
	EQStates eqStates;
} States;


ALWAYS_INLINE Status initFileNames(FileNames *fileNames);
ALWAYS_INLINE Status runGetOpt(int argc, char *const argv[], const char *optstring, FileNames *fileNames);
ALWAYS_INLINE Status init(Params *params, Coeffs *coeffs, States *states, int sampleRate);
ALWAYS_INLINE Status readConfig(FILE *configFilePtr, Params *params, Coeffs *coeffs, States *states);
void run(FILE *inputFilePtr, FILE *outputFilePtr, Params *params, Coeffs *coeffs, States *states);


int main(int argc, char *argv[])
{
//	printf("log2Table = %d\n", sizeof(int32_t) * 130);
//	printf("pow2Table = %d\n", sizeof(int32_t) * 257);
//
//	printf("CrossFadeParams = %d\n", sizeof(CrossFadeParams));
//	printf("AmplitudeProcParams = %d\n", sizeof(AmplitudeProcParams));
//	printf("EQParams = %d\n", sizeof(EQParams));
//
//	printf("CrossFadeCoeffs = %d\n", sizeof(CrossFadeCoeffs));
//	printf("AmplitudeProcCoeffs = %d\n", sizeof(AmplitudeProcCoeffs));
//	printf("EQCoeffs = %d\n", sizeof(EQCoeffs));
//
//	printf("CrossFadeStates = %d\n", sizeof(CrossFadeStates));
//	printf("AmplitudeProcStates = %d\n", sizeof(AmplitudeProcStates));
//	printf("EQStates = %d\n", sizeof(EQStates));

	FileNames fileNames;
	initFileNames(&fileNames);
	runGetOpt(argc, argv, OPTSTRING, &fileNames);

	FILE *configFilePtr = openFile(fileNames.configFileName, read);
	FILE *inputFilePtr = openFile(fileNames.inputFileName, binaryRead);
	FILE *outputFilePtr = openFile(fileNames.outputFileName, binaryWrite);

	uint8_t headerBuff[FILE_HEADER_SIZE];
	Params params;
	Coeffs coeffs;
	States states;

	readHeader(headerBuff, inputFilePtr);
	writeHeader(headerBuff, outputFilePtr);

	init(&params, &coeffs, &states, SAMPLE_RATE);
	readConfig(configFilePtr, &params, &coeffs, &states);
	run(inputFilePtr, outputFilePtr, &params, &coeffs, &states);

	fclose(configFilePtr);
	fclose(inputFilePtr);
	fclose(outputFilePtr);


//	F32x2 x = doubleToF32x2Join(-0.5946 / 16, 0.793 / 16);
//	F32x2 y = doubleToF32x2Join(0.0004116, -0);
//	F32x2 res = F32x2Log2(x);
//	double res1 = F32x2ToDoubleExtract_h(res) * 16;
//	double res2 = F32x2ToDoubleExtract_l(res) * 16;

	return 0;
}


ALWAYS_INLINE Status initFileNames(FileNames *fileNames)
{
	if(!fileNames)
		return statusError;

	fileNames->configFileName = NULL;
	fileNames->inputFileName = NULL;
	fileNames->outputFileName = NULL;

	return statusOK;
}

ALWAYS_INLINE Status initAlgorithms(Params *params, Coeffs *coeffs, States *states, int sampleRate)
{
	Status status = CrossFadeInit(&params->crossFadeParams, &coeffs->crossFadeCoeffs,
						   &states->crossFadeStates, sampleRate);
	status |= AmplitudeProcInit(&params->amplitudeProcParams, &coeffs->amplitudeProcCoeffs,
								&states->amplitudeProcStates, sampleRate);
	status |= EQInit(&params->eqParams, &coeffs->eqCoeffs, &states->eqStates, sampleRate);

	return status;
}

ALWAYS_INLINE Status init(Params *params, Coeffs *coeffs, States *states, int sampleRate)
{
	params->inputGain = 0;
	params->outputGain = 0;
	params->balance = 0;

	coeffs->inputGain = F32x2Zero();
	coeffs->outputGain = F32x2Zero();
	coeffs->balance = F32x2Set(0x7fffffff);

	return initAlgorithms(params, coeffs, states, sampleRate);
}

ALWAYS_INLINE Status runGetOpt(int argc, char *const argv[], const char *optstring, FileNames *fileNames)
{
	int opt;
	int optind = 1;
	char* optarg = NULL;

	if(!fileNames)
		return statusError;

	while ((opt = getOpt(argc, argv, OPTSTRING, &optind, &optarg)) != -1)
	{
		switch (opt)
		{
		case 'c':
			fileNames->configFileName =  optarg;
			break;

		case 'i':
			fileNames->inputFileName = optarg;
			break;

		case 'o':
			fileNames->outputFileName = optarg;
			break;

		default:
			exit(0);
			break;
		}
	}

	return statusOK;
}

ALWAYS_INLINE void parseConfigString(char *str, uint16_t *paramId, double *paramValue)
{
	uint8_t inputIndex = 0;
	uint8_t idIndex = 0;
	uint8_t paramIndex = 0;
	uint8_t isIndex = 1;
	char idStr[6] = {0};
	char paramStr[13] = {0};

	while (str[inputIndex])
	{
		if (str[inputIndex] == '/' ||
		   ((str[inputIndex] == ' ' || str[inputIndex] == '	') && paramIndex > 0))
		{
			break;
		}
		else if (str[inputIndex] == ':')
		{
			isIndex = 0;
		}
		else if (str[inputIndex] != ' ' && str[inputIndex] != '	' && str[inputIndex] != '\n')
		{
			if (isIndex)
			{
				idStr[idIndex] = str[inputIndex];
				idIndex++;
			}
			else
			{
				paramStr[paramIndex] = str[inputIndex];
				paramIndex++;
			}
		}

		inputIndex++;
	}

	*paramId = atoi(idStr);
	*paramValue = atof(paramStr);
}

ALWAYS_INLINE Status readConfig(FILE *configFilePtr, Params *params, Coeffs *coeffs, States *states)
{
	if (!params || !coeffs || !states)
		return statusError;

	char str[100] = {0};
	uint16_t paramId;
	double paramValue;
	uint8_t i;

	while (fgets(str, 100, configFilePtr))
	{
		parseConfigString(str, &paramId, &paramValue);

		if (paramId < 150)
		{
			CrossFadeSetParam(&params->crossFadeParams, &coeffs->crossFadeCoeffs,
							  &states->crossFadeStates, paramId, paramValue);
		}
		else if (paramId == 151)
		{
			params->inputGain = paramValue;
			coeffs->inputGain = doubleToF32x2Set(dBtoGain(paramValue));
		}
		else if (paramId == 152)
		{
			params->outputGain = paramValue;
			coeffs->outputGain = doubleToF32x2Set(dBtoGain(paramValue));
		}
		else if (paramId == 153)
		{
			params->balance = paramValue;
			paramValue /= 10;

			if (paramValue < 0)
			{
				coeffs->balance = doubleToF32x2Join(1, 1 + paramValue);
			}
			else if (paramValue > 0)
			{
				coeffs->balance = doubleToF32x2Join(1 - paramValue, 1);
			}
		}
		else if (paramId < 500)
		{
			AmplitudeProcSetParam(&params->amplitudeProcParams, &coeffs->amplitudeProcCoeffs,
								  &states->amplitudeProcStates, paramId, paramValue);
		}
		else
		{
			EQSetParam(&params->eqParams, &coeffs->eqCoeffs, &states->eqStates,
					   paramId / 1000 - 1, paramId % 1000, paramValue);
		}
	}

}

void run(FILE *inputFilePtr, FILE *outputFilePtr, Params *params, Coeffs *coeffs, States *states)
{
	int32_t dataBuff[DATA_BUFF_SIZE * CHANNELS];
	size_t samplesRead;
	uint32_t i;
	F32x2 sample;					// Q27
	F32x2 bypassSample;				// Q31
	_Bool isFirstIteration = 1;
	uint32_t cyclesCounter = 0;

	while (1)
	{
		samplesRead = fread(dataBuff, BYTES_PER_SAMPLE, DATA_BUFF_SIZE * CHANNELS, inputFilePtr);

		if (!samplesRead)
		{
			break;
		}

// CROSSFADE TEST HERE
//		if (cyclesCounter == 60)
//		{
//			CrossFadeSetParam(&params->crossFadeParams, &coeffs->crossFadeCoeffs,
//							  &states->crossFadeStates, isBypassID, 1);
//		}

		for (i = 0; i < samplesRead / CHANNELS; i++)
		{
			bypassSample = F32x2Join(dataBuff[i * CHANNELS], dataBuff[i * CHANNELS + 1]);
			sample = F32x2RightShiftA(bypassSample, HEADROOM);
			sample = F32x2Mul(sample, coeffs->inputGain);
			sample = F32x2Mul(sample, coeffs->balance);

			sample = EQ_Process(&coeffs->eqCoeffs, &states->eqStates, sample);
			sample = AmplitudeProc_Process(&coeffs->amplitudeProcCoeffs,
										   &states->amplitudeProcStates, sample);

			sample = F32x2Mul(sample, coeffs->outputGain);
			sample = F32x2LeftShiftAS(sample, HEADROOM);

			sample = CrossFade_Process(&coeffs->crossFadeCoeffs, &states->crossFadeStates,
									   bypassSample, sample);

			dataBuff[i * CHANNELS] = F32x2ToI32Extract_h(sample);
			dataBuff[i * CHANNELS + 1] = F32x2ToI32Extract_l(sample);
		}

		fwrite(dataBuff, BYTES_PER_SAMPLE, samplesRead, outputFilePtr);

		cyclesCounter++;
	}
}

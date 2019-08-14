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

#include "AmplitudeProc.h"


#define BYTES_PER_SAMPLE 4

#define OPTSTRING "c:i:o:"

typedef struct {
	char *configFileName;
	char *inputFileName;
	char *outputFileName;
} FileNames;


ALWAYS_INLINE Status initFileNames(FileNames *fileNames);
ALWAYS_INLINE Status runGetOpt(int argc, char *const argv[], const char *optstring, FileNames *fileNames);
Status readConfig(FILE *configFilePtr, Params *params, Coeffs *coeffs, States *states);
void runAmplitudeProc(FILE *inputFilePtr, FILE *outputFilePtr, RingBuff *ringBuff,
		 	 	 	  const Coeffs *coeffs, States *states);


int main(int argc, char *argv[])
{
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
	RingBuff ringBuff;

	readHeader(headerBuff, inputFilePtr);
	writeHeader(headerBuff, outputFilePtr);

	AmplitudeProcInit(&params, &coeffs, &ringBuff, &states);
	readConfig(configFilePtr, &params, &coeffs, &states);
	runAmplitudeProc(inputFilePtr, outputFilePtr, &ringBuff, &coeffs, &states);

	fclose(configFilePtr);
	fclose(inputFilePtr);
	fclose(outputFilePtr);
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

void parseConfigString(char *str)
{
	uint8_t inputIndex = 0;
	uint8_t paramIndex = 0;
	char paramStr[13] = {0};

	while (str[inputIndex])
	{
		if (str[inputIndex] == '/' ||
		   ((str[inputIndex] == ' ' || str[inputIndex] == '	') && paramIndex > 0))
		{
			inputIndex = 0;
			paramIndex = 0;
			break;
		}
		else if (str[inputIndex] != ' ' && str[inputIndex] != '	' && str[inputIndex] != '\n')
		{
			paramStr[paramIndex] = str[inputIndex];
			paramIndex++;
		}

		inputIndex++;
	}

	strcpy(str, paramStr);
}

Status readConfig(FILE *configFilePtr, Params *params, Coeffs *coeffs, States *states)
{
	if (!params || !coeffs || !states)
		return statusError;

	char str[100] = {0};
	uint16_t paramId = 1;

	while (fgets(str, 100, configFilePtr))
	{
		parseConfigString(str);

		if (*str)
		{
			AmplitudeProcSetParam(params, coeffs, states, paramId, atof(str));
			paramId++;
		}
	}

}

void runAmplitudeProc(FILE *inputFilePtr, FILE *outputFilePtr, RingBuff *ringBuff,
		 const Coeffs *coeffs, States *states)
{
	int32_t dataBuff[DATA_BUFF_SIZE * CHANNELS];
	size_t samplesRead;
	uint32_t i;
	F32x2 res;					// Q31
	_Bool isFirstIteration = 1;

	while (1)
	{
		samplesRead = fread(dataBuff, BYTES_PER_SAMPLE, DATA_BUFF_SIZE * CHANNELS, inputFilePtr);

		if (!samplesRead)
		{
			break;
		}

		if (isFirstIteration)
		{
			ringBuffSet(ringBuff, dataBuff);
			i = RING_BUFF_SIZE;
			isFirstIteration = 0;
		}
		else
		{
			i = 0;
		}

		for (i; i < samplesRead / CHANNELS; i++)
		{
			AmplitudeProc_Process(coeffs, ringBuff, states);
			res = ringBuff->samples[ringBuff->currNum];

			ringBuff->samples[ringBuff->currNum] =
						F32x2Join(dataBuff[i * CHANNELS], dataBuff[i * CHANNELS + 1]);

			dataBuff[i * CHANNELS] = F32x2ToI32Extract_h(res);
			dataBuff[i * CHANNELS + 1] = F32x2ToI32Extract_l(res);

			ringBuff->currNum = (ringBuff->currNum + 1) & (RING_BUFF_SIZE - 1);
		}

		fwrite(dataBuff, BYTES_PER_SAMPLE, samplesRead, outputFilePtr);
	}
}

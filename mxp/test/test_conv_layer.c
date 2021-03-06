#include "unit_test.h"
#include "conv_layer.h"
#include "misc_utils.h"
#include <math.h>

FL_MAP_PIXEL *pConvRefFltOutput;

void compute_conv_ref(CONV_LYR_CTX_T *pCtx, FL_MAP_PIXEL *pFloatInput) {
	int map, row, col, oH, oW, iH, iW;
	iH = pCtx->convInfo.mapH + 2*pCtx->convInfo.pad;
	iW = pCtx->convInfo.mapW + 2*pCtx->convInfo.pad;
	oH = (pCtx->convInfo.mapH + 2 * pCtx->convInfo.pad - pCtx->convInfo.K + 1 + pCtx->convInfo.stride - 1) / pCtx->convInfo.stride;
	oW = (pCtx->convInfo.mapW + 2 * pCtx->convInfo.pad - pCtx->convInfo.K + 1 + pCtx->convInfo.stride - 1) / pCtx->convInfo.stride;

	for(int omap = 0; omap < pCtx->convInfo.nOutMaps; omap++) {
		for(int row = 0; row < oH; row++) {
			int hstart = row * pCtx->convInfo.stride;
			for(int col = 0; col < oW; col++) {
				int wstart = col * pCtx->convInfo.stride;
				float sum = 0.0f;
				for(int imap = 0; imap < pCtx->convInfo.nInMaps; imap++) {
					for(int kr = 0; kr < pCtx->convInfo.K; kr++) {
						for(int kc = 0; kc < pCtx->convInfo.K; kc++) {
							sum += 	pCtx->pFloatKer[((omap * pCtx->convInfo.nInMaps +imap) * pCtx->convInfo.K + kr)* pCtx->convInfo.K + kc] *
								pFloatInput[imap * iW * iH + (hstart + kr) * iW + wstart + kc];
						}
					}
				}
				sum += pCtx->pFloatBias[omap];
				pConvRefFltOutput[(omap * oH + row) * oW + col] = sum; 
			}	
		}
	}

}

CMP_STATUS_T compare_conv_out(CONV_LYR_CTX_T *pCtx, FL_MAP_PIXEL *pOutput) {
	int oW, oH;
	CMP_STATUS_T status;
	status.misMap = -1;
	status.misRow = -1;
	status.misCol = -1;
	status.flag = TEST_PASS;

	oH = (pCtx->convInfo.mapH + 2*pCtx->convInfo.pad - pCtx->convInfo.K + 1 + pCtx->convInfo.stride - 1)/ pCtx->convInfo.stride;
	oW = (pCtx->convInfo.mapW + 2*pCtx->convInfo.pad - pCtx->convInfo.K + 1 + pCtx->convInfo.stride - 1)/ pCtx->convInfo.stride;
	for(int	map = 0; map < pCtx->convInfo.nOutMaps; map++) {
		for(int row = 0; row < oH; row++) {
			for(int col = 0; col < oW; col++) {
				if(fabs(pOutput[(map*oH + row)*oW + col] - pConvRefFltOutput[(map*oH + row)*oW + col]) > ERR_THRESHOLD) {
					printf("Ref:%f\tAct:%f\n", pConvRefFltOutput[(map*oH + row)*oW + col], pOutput[(map*oH + row)*oW + col]);
					status.misMap = map;
					status.misRow = row;
					status.misCol = col;
					status.flag = TEST_FAIL;
					return status;
				}
			}
		}
	}
	return status;
}

TEST_STATUS_E test_conv_layer() {
	int noInputs, noOutputs, inputHeight, inputWidth, K, stride, pad, k, i;
	int outWidth, outHeight, nMapFracBits, nKerFracBits, map, coeff;
	FL_MAP_PIXEL *pFltInput;
	FP_MAP_PIXEL *pFixInput;
	
	CMP_STATUS_T status;

	CONV_LYR_CTX_T convCtx;

	printf("Testing CONV Layer\n");
	noInputs = 3;
	noOutputs = 10;
	inputHeight = 9;
	inputWidth = 9;
	K = 3;
	stride = 1;
	pad = 0;
	nMapFracBits = 12;
	nKerFracBits = 12;

	outHeight = (inputHeight + 2*pad - K + 1 + stride - 1)/ stride;
	outWidth = (inputWidth + 2*pad - K + 1 + stride - 1)/ stride;
	convCtx.convInfo = (CONV_INFO_T){inputHeight, inputWidth, K, noInputs, noOutputs, pad, stride, nMapFracBits, nKerFracBits};
	convCtx.mapLyt = MAP_ISOLATED;
	convCtx.lyrArithMode = FLOAT_POINT; 
	convCtx.optType = SCALAR;
	convCtx.blkInfo.blkH = convCtx.convInfo.mapH;
	convCtx.blkInfo.blkW = convCtx.convInfo.mapW;
	convCtx.blkInfo.optMaps = convCtx.convInfo.nOutMaps;

	// input and output buffer allocation	
	convCtx.pFloatOutput = malloc(outHeight * outWidth * noOutputs * sizeof(FL_MAP_PIXEL));
	convCtx.pFixOutput = malloc(outHeight * outWidth * noOutputs * sizeof(FP_MAP_PIXEL));
	convCtx.pFloatKer = malloc(K * K * noInputs * noOutputs * sizeof(FL_KERNEL));
	convCtx.pFixKer = malloc(K * K * noInputs * noOutputs * sizeof(FP_KERNEL));
	convCtx.pFloatBias = malloc(noOutputs *sizeof(FL_KERNEL));
	convCtx.pFixBias = malloc(noOutputs *sizeof(FP_KERNEL));
	// for kernel extension
	convCtx.ppExtKer = malloc(K * K * noInputs * sizeof(FP_KERNEL *));
	for( k = 0; k < convCtx.convInfo.K * convCtx.convInfo.K * convCtx.convInfo.nInMaps; k++) {
		convCtx.ppExtKer[k] = (FP_KERNEL *)malloc(convCtx.convInfo.nOutMaps * convCtx.blkInfo.blkW * sizeof(FP_KERNEL));
	}

	pConvRefFltOutput = malloc(outHeight * outWidth * noOutputs * sizeof(FL_MAP_PIXEL));
	pFltInput = malloc((inputHeight + 2*pad)*(inputWidth + 2*pad) * noInputs * sizeof(FL_MAP_PIXEL));
	pFixInput = malloc((inputHeight + 2*pad)*(inputWidth + 2*pad) * noInputs * sizeof(FP_MAP_PIXEL));

	// random input
	generate_random_data(pFltInput, (inputHeight + 2*pad)*(inputWidth + 2*pad) * noInputs, 123);
	generate_random_data(convCtx.pFloatKer, K * K * noInputs * noOutputs, 345);
	generate_random_data(convCtx.pFloatBias, noOutputs, 321);

	float_to_fix_data(pFltInput, (inputHeight + 2*pad)*(inputWidth + 2*pad) * noInputs, nMapFracBits, pFixInput);
	float_to_fix_data(convCtx.pFloatKer, K * K * noInputs * noOutputs, nKerFracBits, convCtx.pFixKer);
	float_to_fix_data(convCtx.pFloatBias, noOutputs, nMapFracBits, convCtx.pFixBias);
	// kernel extension for MXP mode
	for(i = 0; i < convCtx.convInfo.nInMaps; i++) {
		for(k = 0; k < convCtx.convInfo.K * convCtx.convInfo.K; k++) {
			for(map = 0; map < convCtx.convInfo.nOutMaps; map++) {
				// Repeate the coefficient image width no of times
				for(coeff = 0; coeff < convCtx.blkInfo.blkW; coeff++) {
					*(convCtx.ppExtKer[i * convCtx.convInfo.K * convCtx.convInfo.K + k] + convCtx.blkInfo.blkW * map + coeff) =
						//convCtx.pFixKer[(i * convCtx.convInfo.nOutMaps + map) * convCtx.convInfo.K * convCtx.convInfo.K + k];
						convCtx.pFixKer[(map * convCtx.convInfo.nInMaps + i) * convCtx.convInfo.K * convCtx.convInfo.K + k];
				}
			}
		}
	}
	//print_float_img(pFltInput, inputHeight, inputWidth);

	cnn_conv_layer(&convCtx, pFltInput, pFixInput, MAP_ISOLATED);
	convCtx.lyrArithMode = FIXED_POINT; 
	cnn_conv_layer(&convCtx, pFltInput, pFixInput, MAP_ISOLATED);

	//print_fix_img(convCtx.pFixOutput, outHeight, outWidth);
	print_float_img(convCtx.pFloatOutput, outHeight, outWidth);

	compute_conv_ref(&convCtx, pFltInput);
	//print_float_img(pConvRefFltOutput, outHeight, outWidth);

	status = compare_conv_out(&convCtx, convCtx.pFloatOutput);
	check_cmp_status(&status);

	fix16_to_float_data(convCtx.pFixOutput, outHeight * outWidth * noOutputs, nMapFracBits, convCtx.pFloatOutput);
	status = compare_conv_out(&convCtx, convCtx.pFloatOutput);
	print_float_img(convCtx.pFloatOutput, outHeight, outWidth);
	check_cmp_status(&status);

	convCtx.optType = VECTOR_MXP;
	cnn_conv_layer(&convCtx, pFltInput, pFixInput, MAP_ISOLATED);
	//print_fix_img(convCtx.pFixOutput, outHeight, outWidth);

	fix16_to_float_data(convCtx.pFixOutput, outHeight * outWidth * noOutputs, nMapFracBits, convCtx.pFloatOutput);
	print_float_img(convCtx.pFloatOutput, outHeight, outWidth);
	status = compare_conv_out(&convCtx, convCtx.pFloatOutput);
	check_cmp_status(&status);

	free(convCtx.pFloatOutput);
	free(convCtx.pFixOutput);
	free(convCtx.pFloatKer);
	free(convCtx.pFixKer);
	free(convCtx.pFloatBias);
	free(convCtx.pFixBias);

	for(k = 0; k < convCtx.convInfo.K * convCtx.convInfo.K * convCtx.convInfo.nInMaps; k++) {
		free(convCtx.ppExtKer[k]);
	}
	free(convCtx.ppExtKer);
	free(pConvRefFltOutput);
	free(pFixInput);
	free(pFltInput);
	return status.flag;
}

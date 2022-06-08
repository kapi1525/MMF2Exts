/* File generated by DarkEdifPostBuildTool, part of DarkEdif SDK. 
   DarkEdif license is available at its online repository location.
   Copyright of the Phi Object extension and all rights reserved by creator of Phi Object.
   
   A native iOS extension needs a Objective-C file to generate a Objective-C class.
   This file is required. The Phi Object creator may modify the copyright to suit their wishes,
   if they retain the preceding DarkEdif notice.
*/
//----------------------------------------------------------------------------------
// CRunPhi_Object
//----------------------------------------------------------------------------------
#import "CFile.h"
#import "CServices.h"
#import "CCreateObjectInfo.h"
#import "CCndExtension.h"
#import "CActExtension.h"
#import "CRun.h"
#import "CEventProgram.h"
#import "CExtension.h"
#import "CRunPhi_Object.hpp"

//static int CPPSideInited = 0;
@implementation CRunPhi_Object : CRunExtension

-(id)init
{
	if (++initedPhi_ObjectCPP == 1)
		Phi_Object_init();
	return [super init];
}
-(void)dealloc {
	if (cptr)
		NSLog(@"cptr was set when dealloc! Should be destroyRunPtr!");
	if (--initedPhi_ObjectCPP == 0)
		Phi_Object_dealloc();
	[super dealloc];
}
-(int)getNumberOfConditions
{
	if (initedPhi_ObjectCPP == 0)
		NSLog(@"Number of conditions was called before inited");
	return Phi_Object_getNumberOfConditions();
}
-(BOOL)createRunObject:(CFile*)file withCOB:(int)cob andVersion:(int)version
{
	if (initedPhi_ObjectCPP == 0)
		NSLog(@"Init for Phi Object was never run!");
	NSLog(@"CreateRunObject for Phi_Object");
	expRet = [[CValue alloc] initWithInt:0];
	
	// Passed CFile lacks the extHeader variable at the start of EDITDATA, so generate one.
	unsigned int eHeaderSize = 20;
	unsigned int dataSize = eHeaderSize + (file != nil ? (unsigned int)file->maxLength : 0);
	
	char * edPtr = (char *)malloc(dataSize);
	
	// create eHeader; ocPtr is not accessible here, which is sad cos it contains everything.
	((unsigned int *)edPtr)[0] = dataSize;        // extSize
	((unsigned int *)edPtr)[1] = dataSize;        // dummy - extMaxSize not read by CObjectCommon file reader
	((unsigned int *)edPtr)[2] = version;         // extVersion
	((unsigned int *)edPtr)[3] = 0;               // dummy - extID read by CObjectCommon file reader, but not passed
	((unsigned int *)edPtr)[4] = ho->privateData; // extPrivateData stored in CRunExtension,
	if (file != nil)
		memcpy(edPtr + eHeaderSize, file->pData, file->maxLength);
	
	cptr = Phi_Object_createRunObject(edPtr, cob, version, self);
	if (!cptr)
		NSLog(@"Failed to createRunObject for Phi Object!");
	else
	{
		UIDevice *device = [UIDevice currentDevice];
		proximityWasEnabled = [device isProximityMonitoringEnabled];
		[device setProximityMonitoringEnabled:YES]; // turns it on if it's off

		if (device.proximityMonitoringEnabled == YES) {
			[[NSNotificationCenter defaultCenter] addObserver:self 
													 selector:@selector(proximityChanged:) 
														 name:@"UIDeviceProximityStateDidChangeNotification"
													   object:device];
		}
	}
	free(edPtr);
	return cptr != NULL;
}
-(void)proximityChanged:(NSNotification *)notification {
    UIDevice *device = [notification object];
	
	// proximityState == true, then close to user, false: not
	// This is the opposite of Android, which returns proximity as "cm from sensor", but usually 1cm or 0cm
	proximityState = device.proximityState ? 0.0f : 1.0f;
}
-(int)handleRunObject
{
	return Phi_Object_handleRunObject(cptr);
}
-(void)destroyRunObject:(BOOL)bFast
{
	Phi_Object_destroyRunObject(cptr, bFast);
	cptr = NULL;
	[expRet dealloc];
}
-(BOOL)condition:(int)paramNum withCndExtension:(CCndExtension*)cnd
{
	// Currently, as of SDK v11, you will need to program comparison conditions manually,
	// by checking num and the return of Phi_Object_conditionJump, which is either a 32-bit integer,
	// 32-bit float, or a UTF-8 string pointer. For now, this is sufficient to check non-bool.
	// Use cnd.compareValues() or cnd.compareTime() as appropriate.
	return Phi_Object_conditionJump(cptr, paramNum, cnd) != 0;
}
-(void)action:(int)paramNum withActExtension:(CActExtension*)act
{
	Phi_Object_actionJump(cptr, paramNum, act);
}
-(CValue*)expression:(int)paramNum
{
	// Proximity sensor expression isn't passed to C++ side
	if (paramNum == 24)
		[expRet forceDouble:(double)proximityState];
	else
		Phi_Object_expressionJump(cptr, paramNum);
	return expRet;
}

@end

#if defined(__cplusplus) && !defined(DARKEDIF_WRAPPER_CODE)
#define DARKEDIF_WRAPPER_CODE

extern "C" {
	void DarkEdif_generateEvent(void * ext, int code, int param)
	{
		CRunPhi_Object * extRun = (CRunPhi_Object *)ext;
		[extRun->ho generateEvent:code withParam:param];
	}
	void DarkEdif_reHandle(void * ext)
	{
		CRunExtension * extRun = (CRunPhi_Object *)ext;
		[extRun->ho reHandle];
	}

	int DarkEdif_actGetParamExpression(void * ext, void * act, int paramNum)
	{
		CRunPhi_Object * extRun = (CRunPhi_Object *)ext;
		CActExtension * actExt = (CActExtension *)act;
		return [actExt getParamExpression:extRun->rh withNum:paramNum];
	}
	const char * DarkEdif_actGetParamExpString(void * ext, void * act, int paramNum)
	{
		CRunPhi_Object * extRun = (CRunPhi_Object *)ext;
		CActExtension * actExt = (CActExtension *)act;
		NSString* str = [actExt getParamExpString:extRun->rh withNum:paramNum];
		[str retain];
		return [str UTF8String];
	}
	double DarkEdif_actGetParamExpDouble(void * ext, void * act, int paramNum)
	{
		CRunPhi_Object * extRun = (CRunPhi_Object *)ext;
		CActExtension * actExt = (CActExtension *)act;
		return [actExt getParamExpDouble:extRun->rh withNum:paramNum];
	}

	int DarkEdif_cndGetParamExpression(void * ext, void * cnd, int paramNum)
	{
		CRunPhi_Object * extRun = (CRunPhi_Object *)ext;
		CCndExtension * cndExt = (CCndExtension *)cnd;
		return [cndExt getParamExpression:extRun->rh withNum:paramNum];
	}
	const char * DarkEdif_cndGetParamExpString(void * ext, void * cnd, int paramNum)
	{
		CRunPhi_Object * extRun = (CRunPhi_Object *)ext;
		CCndExtension * cndExt = (CCndExtension *)cnd;
		NSString* str = [cndExt getParamExpString:extRun->rh withNum:paramNum];
		return [str UTF8String];
	}
	double DarkEdif_cndGetParamExpDouble(void * ext, void * cnd, int paramNum)
	{
		CRunPhi_Object * extRun = (CRunPhi_Object *)ext;
		CCndExtension * cndExt = (CCndExtension *)cnd;
		return [cndExt getParamExpDouble:extRun->rh  withNum:paramNum];
	}


	int DarkEdif_expGetParamInt(void * ext)
	{
		CRunPhi_Object * extRun = (CRunPhi_Object *)ext;
		return [[extRun->ho getExpParam] getInt];
	}
	const char * DarkEdif_expGetParamString(void * ext)
	{
		CRunPhi_Object * extRun = (CRunPhi_Object *)ext;
		return [[[extRun->ho getExpParam] getString] UTF8String];
	}
	float DarkEdif_expGetParamFloat(void * ext)
	{
		CRunPhi_Object * extRun = (CRunPhi_Object *)ext;
		return (float)[[extRun->ho getExpParam] getDouble];
	}

	void DarkEdif_expSetReturnInt(void * ext, int i)
	{
		CRunPhi_Object * extRun = (CRunPhi_Object *)ext;
		[extRun->expRet forceInt:i];
	}
	void DarkEdif_expSetReturnString(void * ext, const char * str)
	{
		CRunPhi_Object * extRun = (CRunPhi_Object *)ext;
		[extRun->expRet forceString:[NSString stringWithUTF8String:str]];
	}
	void DarkEdif_expSetReturnFloat(void * ext, float f)
	{
		CRunPhi_Object * extRun = (CRunPhi_Object *)ext;
		[extRun->expRet forceDouble:(double)f];
	}

	void DarkEdif_freeString(void * ext, const char * cstr)
	{
		[(NSString*)cstr release];
	}

	int DarkEdif_getCurrentFusionEventNum(void * ext)
	{
		CRunPhi_Object * extRun = (CRunPhi_Object *)ext;
		return extRun->rh->rhEvtProg->rhEventGroup->evgFree;
	}
}
#endif

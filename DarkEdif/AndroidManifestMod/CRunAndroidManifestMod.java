/* File generated by DarkEdifPostBuildTool, part of DarkEdif SDK. 
   DarkEdif license is available at its online repository location.
   Copyright of the AndroidManifestMod extension and all rights reserved by the creator(s) of AndroidManifestMod.
   
   This file is required, but has been modified to do nothing in Android.
   The functionality of AndroidManifestMod is only in Windows; Fusion expects an Android Java file, though.
*/

package Extensions;
import android.util.Log;
import java.nio.ByteOrder;
import java.nio.ByteBuffer;
import java.io.InputStream;
import java.io.IOException;
import Services.CBinaryFile;
import Runtime.MMFRuntime;
import Objects.CExtension;
import RunLoop.CCreateObjectInfo;
import Conditions.CCndExtension;
import Expressions.CExpExtension;
import Actions.CActExtension;
import Expressions.CValue;
import Expressions.CNativeExpInstance;
import RunLoop.CRun;

public class CRunAndroidManifestMod extends CRunExtension
{
	public CRunAndroidManifestMod()
	{
		// Do nothing; dummy class
	}
	
	@Override
	public int getNumberOfConditions() {
		return 1;
	}
	
	// Methods accessed from C++ side of DarkEdif via JNI:

	public int darkedif_jni_getCurrentFusionEventNum()
	{
		return this.rh.rhEvtProg.rhEventGroup.evgLine;
	}
};
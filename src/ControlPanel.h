#pragma once

#include "ofMain.h"
#include "ofxGui.h"
#include "OSCParameterSync.h"
#include "ofxRPiCameraVideoGrabber.h"


class ControlPanel
{
public:
	ControlPanel();
	
	void setup(ofxRPiCameraVideoGrabber* rpiCameraVideoGrabber);
	void onUpdate(ofEventArgs &args);
	ofxRPiCameraVideoGrabber* rpiCameraVideoGrabber;
	
	void onParameterGroupChanged(ofAbstractParameter & param);
	void onVideoCodingNamesChanged(ofAbstractParameter & param);
	void onExposureControlNamesChanged(ofAbstractParameter & param);
	void increaseContrast();
	
	ofxPanel gui;
private:
	
	
	
	ofParameterGroup parameters;
	ofParameterGroup videoCodingNames;
	ofParameterGroup exposureControlNames;
	ofParameterGroup meteringNames;
	ofParameterGroup whiteBalanceNames;
	ofParameterGroup imageFilterNames;
	
	
	
	OSCParameterSync* sync;
	
	int localPort;
	int remotePort;
	
	
	void onSharpnessChanged(int &sharpness);
	void onContrastChanged(int &constrast);
	void onBrightnessChanged(int &brightness);
	void onSaturationChanged(int &saturation);
	
	void onFrameStabilizationChanged(bool &doFrameStabilization);
	void onColorEnhancementChanged(bool &doColorEnhancement);
	void onLEDEnabledChanged(bool &doEnableLED);
	
	
	
	ofXml* serializer;
	
	void createXMLFromParam(ofAbstractParameter&  parameter, ofXml& xml);
	void saveXML();
	
	ofParameterGroup* guiParamGroup;
	
	
};
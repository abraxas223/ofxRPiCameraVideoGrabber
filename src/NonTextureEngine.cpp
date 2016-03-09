/*
 Thanks to Tuomas Jormola for the recording demos!
 
 https://github.com/tjormola/rpi-openmax-demos/
 */

#include "NonTextureEngine.h"
NonTextureEngine::NonTextureEngine()
{
	frameCounter = 0;		
}

int NonTextureEngine::getFrameCounter()
{
	
	if (!isOpen) 
	{
		return 0;
	}
	OMX_CONFIG_BRCMPORTSTATSTYPE stats;
	
	OMX_INIT_STRUCTURE(stats);
	stats.nPortIndex = VIDEO_RENDER_INPUT_PORT;
	OMX_ERRORTYPE error =OMX_GetParameter(render, OMX_IndexConfigBrcmPortStats, &stats);
    OMX_TRACE(error);
	if (error == OMX_ErrorNone)
	{
		/*OMX_U32 nImageCount;
		 OMX_U32 nBufferCount;
		 OMX_U32 nFrameCount;
		 OMX_U32 nFrameSkips;
		 OMX_U32 nDiscards;
		 OMX_U32 nEOS;
		 OMX_U32 nMaxFrameSize;
		 
		 OMX_TICKS nByteCount;
		 OMX_TICKS nMaxTimeDelta;
		 OMX_U32 nCorruptMBs;*/
		//ofLogVerbose(__func__) << "nFrameCount: " << stats.nFrameCount;
		frameCounter = stats.nFrameCount;
	}else
	{		
		ofLog(OF_LOG_ERROR, "error OMX_CONFIG_BRCMPORTSTATSTYPE FAIL error: 0x%08x", error);
		//frameCounter = 0;
	}
	return frameCounter;
}


void NonTextureEngine::setup(OMXCameraSettings omxCameraSettings_)
{
	
	omxCameraSettings = omxCameraSettings_;
	
	
	OMX_ERRORTYPE error = OMX_ErrorNone;

	OMX_CALLBACKTYPE cameraCallbacks;
	cameraCallbacks.EventHandler    = &NonTextureEngine::cameraEventHandlerCallback;
	
	string cameraComponentName = "OMX.broadcom.camera";
	
	error = OMX_GetHandle(&camera, (OMX_STRING)cameraComponentName.c_str(), this , &cameraCallbacks);
    OMX_TRACE(error);

	
	configureCameraResolution();
	
	
}


OMX_ERRORTYPE NonTextureEngine::cameraEventHandlerCallback(OMX_HANDLETYPE hComponent, OMX_PTR pAppData, OMX_EVENTTYPE eEvent, OMX_U32 nData1, OMX_U32 nData2, OMX_PTR pEventData)
{
    /*
	ofLog(OF_LOG_VERBOSE, 
	 "NonTextureEngine::%s - eEvent(0x%x), nData1(0x%lx), nData2(0x%lx), pEventData(0x%p)\n",
	 __func__, eEvent, nData1, nData2, pEventData);
     */
	NonTextureEngine *grabber = static_cast<NonTextureEngine*>(pAppData);

    switch (eEvent) 
	{
		case OMX_EventParamOrConfigChanged:
		{
			
			return grabber->onCameraEventParamOrConfigChanged();
		}			
		default: 
		{
			break;
		}
	}
	return OMX_ErrorNone;
}


OMX_ERRORTYPE NonTextureEngine::onCameraEventParamOrConfigChanged()
{
    
    ofLogVerbose(__func__) << "START";
    
    OMX_ERRORTYPE error = OMX_SendCommand(camera, OMX_CommandStateSet, OMX_StateIdle, NULL);
    OMX_TRACE(error);
    
    
    //Enable Camera Output Port
    OMX_CONFIG_PORTBOOLEANTYPE cameraport;
    OMX_INIT_STRUCTURE(cameraport);
    cameraport.nPortIndex = CAMERA_OUTPUT_PORT;
    cameraport.bEnabled = OMX_TRUE;
    
    error =OMX_SetParameter(camera, OMX_IndexConfigPortCapturing, &cameraport);	
    OMX_TRACE(error);
    
    if(omxCameraSettings.doRecording)
    {
        //Set up video splitter
        OMX_CALLBACKTYPE splitterCallbacks;
        splitterCallbacks.EventHandler    = &BaseEngine::splitterEventHandlerCallback;
        
        string splitterComponentName = "OMX.broadcom.video_splitter";
        error = OMX_GetHandle(&splitter, (OMX_STRING)splitterComponentName.c_str(), this , &splitterCallbacks);
        OMX_TRACE(error);
        error =DisableAllPortsForComponent(&splitter);
        OMX_TRACE(error);
        
        //Set splitter to Idle
        error = OMX_SendCommand(splitter, OMX_CommandStateSet, OMX_StateIdle, NULL);
        OMX_TRACE(error);
        
    }
    
    OMX_CALLBACKTYPE renderCallbacks;
    renderCallbacks.EventHandler    = &BaseEngine::renderEventHandlerCallback;
    renderCallbacks.EmptyBufferDone	= &BaseEngine::renderEmptyBufferDone;
    renderCallbacks.FillBufferDone	= &BaseEngine::renderFillBufferDone;
    
    string renderComponentName = "OMX.broadcom.video_render";
    
    error = OMX_GetHandle(&render, (OMX_STRING)renderComponentName.c_str(), this , &renderCallbacks);
    OMX_TRACE(error);
    error = DisableAllPortsForComponent(&render);
    OMX_TRACE(error);
    
    //Set renderer to Idle
    error = OMX_SendCommand(render, OMX_CommandStateSet, OMX_StateIdle, NULL);
    OMX_TRACE(error);
    
    if(omxCameraSettings.doRecording)
    {
        //Create encoder
        
        OMX_CALLBACKTYPE encoderCallbacks;
        encoderCallbacks.EventHandler		= &BaseEngine::encoderEventHandlerCallback;
        encoderCallbacks.EmptyBufferDone	= &BaseEngine::encoderEmptyBufferDone;
        encoderCallbacks.FillBufferDone		= &NonTextureEngine::encoderFillBufferDone;
        
        
        string encoderComponentName = "OMX.broadcom.video_encode";
        
        error =OMX_GetHandle(&encoder, (OMX_STRING)encoderComponentName.c_str(), this , &encoderCallbacks);
        OMX_TRACE(error);
        
        configureEncoder();
        
        //Create camera->splitter Tunnel
        error = OMX_SetupTunnel(camera, CAMERA_OUTPUT_PORT,
                                splitter, VIDEO_SPLITTER_INPUT_PORT);
        OMX_TRACE(error);
        
        // Tunnel splitter2 output port and encoder input port
        error = OMX_SetupTunnel(splitter, VIDEO_SPLITTER_OUTPUT_PORT2,
                                encoder, VIDEO_ENCODE_INPUT_PORT);
        OMX_TRACE(error);
        
        
        //Create splitter->render Tunnel
        error = OMX_SetupTunnel(splitter, VIDEO_SPLITTER_OUTPUT_PORT1,
                                render, VIDEO_RENDER_INPUT_PORT);
        OMX_TRACE(error);
        
    }else 
    {
        //Create camera->render Tunnel
        error = OMX_SetupTunnel(camera, CAMERA_OUTPUT_PORT,
                                render, VIDEO_RENDER_INPUT_PORT);
        OMX_TRACE(error);
        
    }
    
    //Enable camera output port
    error = OMX_SendCommand(camera, OMX_CommandPortEnable, CAMERA_OUTPUT_PORT, NULL);
    OMX_TRACE(error);
    
    if(omxCameraSettings.doRecording)
    {
        //Enable splitter input port
        error = OMX_SendCommand(splitter, OMX_CommandPortEnable, VIDEO_SPLITTER_INPUT_PORT, NULL);
        OMX_TRACE(error);
        
        //Enable splitter output port
        error = OMX_SendCommand(splitter, OMX_CommandPortEnable, VIDEO_SPLITTER_OUTPUT_PORT1, NULL);
        OMX_TRACE(error);
        
        //Enable splitter output2 port
        error = OMX_SendCommand(splitter, OMX_CommandPortEnable, VIDEO_SPLITTER_OUTPUT_PORT2, NULL);
        OMX_TRACE(error);
        
    }

    //Enable render input port
    error = OMX_SendCommand(render, OMX_CommandPortEnable, VIDEO_RENDER_INPUT_PORT, NULL);
    OMX_TRACE(error);
    
    if(omxCameraSettings.doRecording)
    {
        //Enable encoder input port
        error = OMX_SendCommand(encoder, OMX_CommandPortEnable, VIDEO_ENCODE_INPUT_PORT, NULL);
        OMX_TRACE(error);
        
        //Set encoder to Idle
        error = OMX_SendCommand(encoder, OMX_CommandStateSet, OMX_StateIdle, NULL);
        OMX_TRACE(error);
        
        //Enable encoder output port
        error = OMX_SendCommand(encoder, OMX_CommandPortEnable, VIDEO_ENCODE_OUTPUT_PORT, NULL);
        OMX_TRACE(error);
        
        // Configure encoder output buffer
        OMX_PARAM_PORTDEFINITIONTYPE encoderOutputPortDefinition;
        OMX_INIT_STRUCTURE(encoderOutputPortDefinition);
        encoderOutputPortDefinition.nPortIndex = VIDEO_ENCODE_OUTPUT_PORT;
        error =OMX_GetParameter(encoder, OMX_IndexParamPortDefinition, &encoderOutputPortDefinition);
        OMX_TRACE(error);
        
        error =  OMX_AllocateBuffer(encoder, 
                                    &encoderOutputBuffer, 
                                    VIDEO_ENCODE_OUTPUT_PORT, 
                                    NULL, 
                                    encoderOutputPortDefinition.nBufferSize);
        
        OMX_TRACE(error);
        if(error != OMX_ErrorNone)
        {
            ofLogError(__func__) << "UNABLE TO RECORD - MAY REQUIRE MORE GPU MEMORY";
        }
        
        
    }
    //Start camera
    error = OMX_SendCommand(camera, OMX_CommandStateSet, OMX_StateExecuting, NULL);
    OMX_TRACE(error);
    
    if(omxCameraSettings.doRecording)
    {
        //Start encoder
        error = OMX_SendCommand(encoder, OMX_CommandStateSet, OMX_StateExecuting, NULL);
        OMX_TRACE(error);
        
        //Start splitter
        error = OMX_SendCommand(splitter, OMX_CommandStateSet, OMX_StateExecuting, NULL);
        OMX_TRACE(error);
    }
    
    //Start renderer
    error = OMX_SendCommand(render, OMX_CommandStateSet, OMX_StateExecuting, NULL);
    OMX_TRACE(error);
    
    error = setupDisplay(0, 0, omxCameraSettings.width, omxCameraSettings.height);
    OMX_TRACE(error);
    
    if(omxCameraSettings.doRecording)
    {
        error = OMX_FillThisBuffer(encoder, encoderOutputBuffer);
        OMX_TRACE(error);
        
        bool doThreadBlocking	= true;
        startThread(doThreadBlocking);
    }
    
    isOpen = true;
    return error;
}



OMX_ERRORTYPE NonTextureEngine::setupDisplay(int x, int y, int width, int height)
{

	OMX_CONFIG_DISPLAYREGIONTYPE region;
	
	OMX_INIT_STRUCTURE(region);
	region.nPortIndex = VIDEO_RENDER_INPUT_PORT; /* Video render input port */
	
	region.set = (OMX_DISPLAYSETTYPE)(OMX_DISPLAY_SET_DEST_RECT | OMX_DISPLAY_SET_FULLSCREEN | OMX_DISPLAY_SET_NOASPECT);

	region.dest_rect.x_offset = x;
	region.dest_rect.y_offset = y;
	region.dest_rect.width	= width;
	region.dest_rect.height = height;
    region.fullscreen = OMX_FALSE;
	region.noaspect = OMX_TRUE;
	OMX_ERRORTYPE error  = OMX_SetParameter(render, OMX_IndexConfigDisplayRegion, &region);
    OMX_TRACE(error);

	
	return error;	
}


NonTextureEngine::~NonTextureEngine()
{
    ofLogVerbose(__func__) << "START";
    OMX_ERRORTYPE error = OMX_ErrorNone;
    
    if(omxCameraSettings.doRecording && !didWriteFile)
    {
        writeFile();
        
    }
   
    if(omxCameraSettings.doRecording)
    {
        error = OMX_SendCommand(encoder, OMX_CommandFlush, VIDEO_ENCODE_INPUT_PORT, NULL);
        OMX_TRACE(error);
        error = OMX_SendCommand(encoder, OMX_CommandFlush, VIDEO_ENCODE_OUTPUT_PORT, NULL);
        OMX_TRACE(error);
        error = DisableAllPortsForComponent(&encoder);
        OMX_TRACE(error);
    }
    
    
    error = OMX_SendCommand(camera, OMX_CommandFlush, CAMERA_OUTPUT_PORT, NULL);
    OMX_TRACE(error);
    
    error = OMX_SendCommand(render, OMX_CommandFlush, VIDEO_RENDER_INPUT_PORT, NULL);
    OMX_TRACE(error);
    
    error = OMX_SendCommand(camera, OMX_CommandStateSet, OMX_StateIdle, NULL);
    OMX_TRACE(error);
    
    error = DisableAllPortsForComponent(&camera);
    OMX_TRACE(error);
    
    if(omxCameraSettings.doRecording)
    {
        error = OMX_FreeBuffer(encoder, VIDEO_ENCODE_OUTPUT_PORT, encoderOutputBuffer);
        OMX_TRACE(error);
        error = OMX_SendCommand(encoder, OMX_CommandStateSet, OMX_StateIdle, NULL);
        OMX_TRACE(error);
        
    }
    
    error = OMX_SendCommand(render, OMX_CommandStateSet, OMX_StateIdle, NULL);
    OMX_TRACE(error);
    
    error = DisableAllPortsForComponent(&render);
    OMX_TRACE(error);
   
    error = OMX_SetupTunnel(camera, CAMERA_OUTPUT_PORT,  NULL, 0);
    OMX_TRACE(error);
    error = OMX_SetupTunnel(render, VIDEO_RENDER_INPUT_PORT,  NULL, 0);
    OMX_TRACE(error);
    
    if(omxCameraSettings.doRecording)
    {
        //Destroy splitter->encoder Tunnel
        error = OMX_SetupTunnel(splitter, VIDEO_SPLITTER_OUTPUT_PORT2, NULL, 0);
        OMX_TRACE(error);
        error = OMX_SetupTunnel(encoder, VIDEO_ENCODE_INPUT_PORT, NULL, 0);
        OMX_TRACE(error);
    
    
        //Destroy splitter->render Tunnel
        error = OMX_SetupTunnel(splitter, VIDEO_SPLITTER_OUTPUT_PORT1, NULL, 0);
        OMX_TRACE(error);
        error = OMX_SetupTunnel(render, VIDEO_RENDER_INPUT_PORT, NULL, 0);
        OMX_TRACE(error);
    }
    
    
    OMX_CONFIG_PORTBOOLEANTYPE cameraport;
    OMX_INIT_STRUCTURE(cameraport);
    cameraport.nPortIndex = CAMERA_OUTPUT_PORT;
    cameraport.bEnabled = OMX_FALSE;
    
    error =OMX_SetParameter(camera, OMX_IndexConfigPortCapturing, &cameraport);	
    OMX_TRACE(error);
    
    error = OMX_FreeHandle(camera);
    OMX_TRACE(error);
    
    if(omxCameraSettings.doRecording)
    {
        error = OMX_FreeHandle(encoder);
        OMX_TRACE(error);
        
        error = OMX_FreeHandle(splitter);
        OMX_TRACE(error);
    }
    
    error = OMX_FreeHandle(render);
    OMX_TRACE(error);
    
    isOpen = false;
    ofLogVerbose(__func__) << "END";
    
}


OMX_ERRORTYPE NonTextureEngine::encoderFillBufferDone(OMX_IN OMX_HANDLETYPE hComponent, OMX_IN OMX_PTR pAppData, OMX_IN OMX_BUFFERHEADERTYPE* pBuffer)
{	
	NonTextureEngine *grabber = static_cast<NonTextureEngine*>(pAppData);
	grabber->lock();
		//ofLogVerbose(__func__) << "recordedFrameCounter: " << grabber->recordedFrameCounter;
		grabber->bufferAvailable = true;
		grabber->recordedFrameCounter++;
	grabber->unlock();
	return OMX_ErrorNone;
}




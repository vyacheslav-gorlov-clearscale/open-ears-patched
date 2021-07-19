//  OpenEars 
//  http://www.politepix.com/openears
//
//  OEContinuousAudioUnit.m
//  OpenEars
//
//  Copyright Politepix UG (haftungsbeschränkt) 2014. All rights reserved.
//  http://www.politepix.com
//  Contact at http://www.politepix.com/contact
//
//  this file is licensed under the Politepix Shared Source license found 
//  found in the root of the source distribution. Please see the file "Version.txt" in the root of 
//  the source distribution for the version number of this OpenEars package.

#import "OEContinuousAudioUnit.h"
#import "OEAudioUtilities.h"
#import "OELogging.h"
#import "OENotification.h"
// Let's get verbose about probably the most confusing thing about these kinds of simple Audio Units for a moment. The scopes (input and output i.e. mic and playback) also have elements (buses).
#define kInputElementOfInputScope 1 // This is the input element of the input scope. This is the stream into the input side of the unit, i.e. raw audio from the mic.
#define kOutputElementOfInputScope 0 // This is the output element of the input scope. This is the rendered stream of mic data coming out of the unit. We specify this.
#define kInputElementOfOutputScope 1 // This is the input element of the output scope.
#define kOutputElementOfOutputScope 0 // This is the output element of the output scope.

#define kOutputBufferSizeInBytes 1024 * 32
#define kSampleRateForRecognition 16000
#define kConverterDoneWithDataError -7001

static NSString * const kNotificationNameUnsuspended = @"AvailableUnsuspendedBuffer";
static NSString * const kNotificationNameSuspended = @"AvailableSuspendedBuffer";
static NSString * const kNotificationNameConvertable = @"AvailableConvertableBuffer";
static NSString * const kBufferKey = @"Buffer";
static NSString * const kDescriptionKey = @"Description";

extern int openears_logging;
NSUInteger times_counter = 0;
NSUInteger packets_read = 0;
//int lock = 0;
@interface OEContinuousAudioUnit() {
    AudioUnit _remoteIOAudioUnit;
}
@property (nonatomic, copy) NSString *notificationName;
@property (nonatomic, strong) NSMutableDictionary *dictionaryForNotification;
@property (nonatomic, assign) BOOL takeBuffersFromTestFile;
@property (nonatomic, assign) UInt32 bytesInTestFile;
@property (nonatomic, assign) UInt32 positionInTestFile;
@property (nonatomic, assign) SInt16 *testFileBuffer;

@end

struct AudioUnitCallbackData { // Things we'll need in the callback.
    AudioUnit remoteIOAudioUnit;
    AudioBufferList *bufferList;
    UInt32 bufferListCapacityFrames;
    AudioStreamBasicDescription requiredInputStreamDescription; 
    BOOL isSuspended;
    float previousSampleRate;
    int previousBitrate;
    int previousChannels;
    float inputDecibels;
    BOOL testFileEndingHasNotBeenAnnounced;
    NSInteger callbacks;
} audioUnitCallbackData;

#pragma mark -
#pragma mark Render callback
#pragma mark -

static OSStatus	renderRemoteIOAudio (void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData) {
    
    OEContinuousAudioUnit *audioUnit = (__bridge OEContinuousAudioUnit *)inRefCon;
    
    OSStatus error = noErr;
    error = AudioUnitRender(audioUnitCallbackData.remoteIOAudioUnit, ioActionFlags, inTimeStamp, 1, inNumberFrames, ioData);

    if (error == noErr) {
 
        if (audioUnit.takeBuffersFromTestFile && inNumberFrames > 0) { // If we're running recognition directly on a test file

            if (audioUnit.bytesInTestFile > (audioUnit.positionInTestFile + (inNumberFrames * 2))) { // If the file buffer still has some data in it
                memcpy(ioData->mBuffers[0].mData, audioUnit.testFileBuffer + (audioUnit.positionInTestFile / 2), inNumberFrames * 2); // Copy the data to this callback buffer inline
                audioUnit.positionInTestFile = audioUnit.positionInTestFile + (inNumberFrames * 2); // And advance the position of positionInTestFile.
                
            } else {
                memset(ioData->mBuffers[0].mData, 0, ioData->mBuffers[0].mDataByteSize); // If we don't have enough remaining audio we write out silence.
                audioUnit.positionInTestFile = audioUnit.bytesInTestFile;
                if (audioUnitCallbackData.testFileEndingHasNotBeenAnnounced) {
                    [OENotification performOpenEarsNotificationOnMainThread:@"TestRecognitionCompleted" withOptionalObjects:nil andKeys:nil];
                    audioUnitCallbackData.testFileEndingHasNotBeenAnnounced = FALSE;
                }
            }
        }
        NSInteger startUsingCallbacksFromRound = 3;
        audioUnitCallbackData.callbacks++;    
        if (audioUnitCallbackData.callbacks < startUsingCallbacksFromRound) return -1; // We never want the first two rendered buffers.
        audioUnitCallbackData.callbacks = startUsingCallbacksFromRound; // Don't overflow; some people run this for days.

        if (audioUnitCallbackData.isSuspended) {
            audioUnit.notificationName = kNotificationNameSuspended;
            audioUnitCallbackData.inputDecibels = 0.0;
        } else {
            audioUnit.notificationName = kNotificationNameUnsuspended;  
            SInt16 *samples = (SInt16 *)ioData->mBuffers[0].mData;
            audioUnitCallbackData.inputDecibels = getInputDecibels(samples, inNumberFrames);
        }
        
        @autoreleasepool {
            (audioUnit.dictionaryForNotification)[kBufferKey] = [NSData dataWithBytes:ioData->mBuffers[0].mData length:ioData->mBuffers[0].mDataByteSize];
            [[NSNotificationCenter defaultCenter] postNotificationName:audioUnit.notificationName object:nil userInfo:audioUnit.dictionaryForNotification]; // Send out the buffers.
        }
        
        return -1; // This is a more efficient way of zeroing out a playback buffer.        
    } else {
        reportAudioUnitError(error);
    }
    return error;
}

@implementation OEContinuousAudioUnit

- (instancetype) init {
    if (self = [super init]) {

        self.dictionaryForNotification = [[NSMutableDictionary alloc] initWithObjects:@[[NSData dataWithBytes:0 length:0]] forKeys:@[kBufferKey]];
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(setAllAudioSessionSettings) 
                                                     name:@"SetAllAudioSessionSettings"
                                                   object:nil];
        
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(handleInterruption:)
                                                     name:AVAudioSessionInterruptionNotification
                                                   object:[AVAudioSession sharedInstance]];
        
        
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(handleRouteChange:)
                                                     name:AVAudioSessionRouteChangeNotification
                                                   object:[AVAudioSession sharedInstance]];
        
        
        [[NSNotificationCenter defaultCenter]	addObserver:	self
                                                 selector:	@selector(handleMediaServerReset)
                                                     name:	AVAudioSessionMediaServicesWereResetNotification
                                                   object:	[AVAudioSession sharedInstance]];       

    }
    return self;
}

- (void) dealloc {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

#pragma mark -
#pragma mark Utility methods
#pragma mark -

- (NSError *) errorFromOSStatus:(OSStatus)status {
    return [NSError errorWithDomain:@"com.politepix.openears" code:status userInfo:nil];   
}

#pragma mark -
#pragma mark Audio file testing
#pragma mark -

- (NSError *) initiateTestIfRequested {
    
    OSStatus error = noErr;
    if (self.pathToTestFile) {
        self.takeBuffersFromTestFile = TRUE;
        audioUnitCallbackData.testFileEndingHasNotBeenAnnounced = TRUE;
        error = [self testFileLoad:self.pathToTestFile];
        if (error != noErr) {
            ReportError(error, @"couldn't load the testfile, continuing with no test file.");
            return [NSError errorWithDomain:@"com.politepix.openears" code:error userInfo:nil];
        }
    } else {
        self.takeBuffersFromTestFile = FALSE;
    }
    return nil;
}

- (OSStatus) testFileLoad:(NSString *)testFilePath {
    
    NSURL *audioURLToRead = [NSURL fileURLWithPath:testFilePath];  
    
    AudioFileID audioFileToReadID;  
    
    OSStatus statusError = noErr;
    
    UInt64 numberOfBytes;
    UInt32 propertySize = sizeof(numberOfBytes);
    
    statusError = AudioFileOpenURL((__bridge CFURLRef)audioURLToRead, kAudioFileReadPermission, 0, &audioFileToReadID); 
    
    if (statusError) {
        ReportError(statusError, @"AudioFileOpenURL Error:");
        return statusError;
    }
    
    statusError = AudioFileGetProperty(audioFileToReadID, kAudioFilePropertyAudioDataByteCount, &propertySize, &numberOfBytes);
    
    if (statusError) {
        ReportError(statusError, @"AudioFileGetProperty Error:");
        return statusError;
    }
    
    self.bytesInTestFile = (UInt32)numberOfBytes; 
    self.testFileBuffer = (SInt16 *)malloc(self.bytesInTestFile);
    self.positionInTestFile = 0;
    
    statusError = AudioFileReadBytes(audioFileToReadID, false, 0, &_bytesInTestFile, self.testFileBuffer);
    
    if (statusError) {
        ReportError(statusError, @"AudioFileReadBytes Error:");
        return statusError;
    }
    
    AudioFileClose(audioFileToReadID);
    
    return statusError;
}

- (void) testFileChange {

    NSError *initiateTestIfRequestedError = [self initiateTestIfRequested];
    if (initiateTestIfRequestedError && openears_logging == 1) NSLog(@"Error trying to load test file: %@", initiateTestIfRequestedError); // This is not a showstopper.
}

#pragma mark -
#pragma mark Recognition
#pragma mark -

- (float) getInputDecibels {
    return audioUnitCallbackData.inputDecibels;    
}

- (void) suspendRecognition {
    audioUnitCallbackData.isSuspended = TRUE;
}
- (void) resumeRecognition {
    audioUnitCallbackData.isSuspended = FALSE;    
}

#pragma mark -
#pragma mark Audio session
#pragma mark -

- (NSError *) setupAudioSession {
    
    NSError *error = [self setAllAudioSessionSettings];
    
    if (error) return error;
    
    
    if (error.code != noErr) {
        ReportError((OSStatus)error.code, @"Error: couldn't set session active.");
        return error;
    }
    return error;
}

- (NSError *) teardownAudioSession {
    // deactivate the audio session
    NSError *error = nil;
    [[AVAudioSession sharedInstance] setActive:NO error:&error];
    if (error.code != noErr) {
        ReportError((OSStatus)error.code, @"Error: couldn't set session inactive.");
        return error;
    }
    return nil;
}

- (NSError *) setAllAudioSessionSettings {
    // Configure the audio session
    AVAudioSession *sessionInstance = [AVAudioSession sharedInstance];
    [sessionInstance setActive:YES error:nil];
    // we are going to play and record so we pick that category
    NSError *error = nil;

    if (!self.disableBluetooth && !self.disableMixing) { // If disableBluetooth is FALSE and disableMixing is FALSE, do the default thing:
        [sessionInstance setCategory:AVAudioSessionCategoryPlayAndRecord withOptions:AVAudioSessionCategoryOptionMixWithOthers | AVAudioSessionCategoryOptionAllowBluetooth | AVAudioSessionCategoryOptionDefaultToSpeaker error:&error]; // We are just defaulting to mixing now.
        if (openears_logging == 1) NSLog(@"Creating audio session with default settings.");
    } else if (self.disableBluetooth && !self.disableMixing) { // If disableBluetooth is TRUE and disableMixing is FALSE, just disable bluetooth
        [sessionInstance setCategory:AVAudioSessionCategoryPlayAndRecord withOptions:AVAudioSessionCategoryOptionMixWithOthers | AVAudioSessionCategoryOptionDefaultToSpeaker error:&error];
        if (openears_logging == 1) NSLog(@"Creating audio session with bluetooth disabled.");
    } else if (!self.disableBluetooth && self.disableMixing) { // If disableBluetooth is FALSE and disableMixing is TRUE, just disable mixing
        [sessionInstance setCategory:AVAudioSessionCategoryPlayAndRecord withOptions:AVAudioSessionCategoryOptionAllowBluetooth | AVAudioSessionCategoryOptionDefaultToSpeaker error:&error];
        if (openears_logging == 1) NSLog(@"Creating audio session with mixing disabled.");
    } else { // If disableBluetooth is TRUE and disableMixing is TRUE, disable both
        [sessionInstance setCategory:AVAudioSessionCategoryPlayAndRecord withOptions:AVAudioSessionCategoryOptionDefaultToSpeaker error:&error];
        if (openears_logging == 1) NSLog(@"Creating audio session with bluetooth and mixing disabled.");        
    }
    
    if (error.code != noErr) {
        ReportError((OSStatus)error.code, @"Error: couldn't set session's audio category.");
        return error;
    }
    if (openears_logging == 1) NSLog(@"Done setting audio session category.");

    if (self.audioMode) {
        
        NSString *audioMode = nil;
        
        if ([self.audioMode isEqualToString:@"Default"]) {
            audioMode = AVAudioSessionModeDefault;
        } else if ([self.audioMode isEqualToString:@"VoiceChat"]) {
            audioMode = AVAudioSessionModeVoiceChat;
        } else if ([self.audioMode isEqualToString:@"VideoRecording"]) {
            audioMode = AVAudioSessionModeVideoRecording;
        } else if ([self.audioMode isEqualToString:@"Measurement"]) {
            audioMode = AVAudioSessionModeMeasurement;
        } else if ([self.audioMode isEqualToString:@"MoviePlayback"]) {
            audioMode = AVAudioSessionModeMoviePlayback;                
        } else {
            audioMode = AVAudioSessionModeDefault;
        }
        
        if ([[[AVAudioSession sharedInstance]mode] isEqualToString:audioMode]) {
            if (openears_logging == 1) NSLog(@"audioMode is correct, we will leave it as it is.");
        } else {
            if (openears_logging == 1) NSLog(@"audioMode is incorrect, we will change it.");
            NSError *error = nil;
            [[AVAudioSession sharedInstance] setMode:audioMode error:&error];
            
            if (error) {
                if (openears_logging == 1) NSLog(@"Error %@: Unable to set audio mode.", error);
            } else {
                if (openears_logging == 1) NSLog(@"audioMode is now on the correct setting.");
            }
        }
        
    }
    
    if (!_disablePreferredSampleRate) {
    
        float preferredSampleRate = 16000;
        
        if ([AVAudioSession sharedInstance].sampleRate != preferredSampleRate) {
        
            NSError *sampleRateError = nil;
            BOOL setSampleRateSuccess = [[AVAudioSession sharedInstance] setPreferredSampleRate:preferredSampleRate  error:&sampleRateError];
            if (!setSampleRateSuccess) {
                if (openears_logging == 1) NSLog(@"Error setting preferred sample rate: %@", sampleRateError);
            } else {
                if (openears_logging == 1) NSLog(@"Done setting preferred sample rate to %f – now the real sample rate is %f", [AVAudioSession sharedInstance].preferredSampleRate, [AVAudioSession sharedInstance].sampleRate);
            }
        } else {
            if (openears_logging == 1) NSLog(@"Sample rate is already the preferred rate of %f so not setting it.", preferredSampleRate); 
        }
    } else {
        if (openears_logging == 1) NSLog(@"Not setting a preferred sample rate at developer request, keeping the default rate for this hardware of %f.", [AVAudioSession sharedInstance].sampleRate);   
    }
    
    if (!_disablePreferredChannelNumber) {    
    
        NSInteger inputNumberOfChannels = 1;
        
        if ([AVAudioSession sharedInstance].inputNumberOfChannels != inputNumberOfChannels) {
            NSError *channelError = nil;
            BOOL setChannelNumberSuccess = [[AVAudioSession sharedInstance] setPreferredInputNumberOfChannels:inputNumberOfChannels  error:&channelError];
            if (!setChannelNumberSuccess) {
                if (openears_logging == 1) {
                    NSLog(@"Error setting preferred number of channels: %@", channelError);
                    NSLog(@"Exiting attempt to set preferred number of channels with channels of %@", @([AVAudioSession sharedInstance].inputNumberOfChannels));
                }
            } else {
                if (openears_logging == 1) NSLog(@"Done setting preferred number of channels to %@ – now the actual input number of channels is %@", @([AVAudioSession sharedInstance].preferredInputNumberOfChannels), @([AVAudioSession sharedInstance].inputNumberOfChannels));
            }
        } else {
            if (openears_logging == 1) NSLog(@"number of channels is already the preferred number of %@ so not setting it.", @(inputNumberOfChannels)); 
        }
    } else {
        if (openears_logging == 1) NSLog(@"Not setting a preferred channel count at developer request, keeping the default number for this hardware of %@.", @([AVAudioSession sharedInstance].inputNumberOfChannels));   
    }
    
    if (!_disablePreferredBufferSize) {       
        NSTimeInterval bufferDuration = kBufferLength;
        
        if ([AVAudioSession sharedInstance].IOBufferDuration != kBufferLength) {
        
            [sessionInstance setPreferredIOBufferDuration:bufferDuration error:&error];
            if (error.code != noErr) {
                ReportError((OSStatus)error.code, @"Error: couldn't set session's I/O buffer duration.");
                return error;
            } else {
                if (openears_logging == 1) NSLog(@"Done setting session's preferred I/O buffer duration to %f – now the actual buffer duration is %f", [AVAudioSession sharedInstance].preferredIOBufferDuration, [AVAudioSession sharedInstance].IOBufferDuration);
            }   
        } else {
            if (openears_logging == 1) NSLog(@"buffer duration is already the preferred size of %f so not setting it.", kBufferLength); 
        }
    } else {
        if (openears_logging == 1) NSLog(@"Not setting a preferred buffer duration at developer request, keeping the default duration for this hardware of %f.", [AVAudioSession sharedInstance].IOBufferDuration);  
    }
    return nil;
}

- (double) sessionSampleRate {
    return [[AVAudioSession sharedInstance] sampleRate];
}

- (void) handleInterruption:(NSNotification *)notification {
    
    NSInteger interruptionPhase = [[[notification userInfo] valueForKey:AVAudioSessionInterruptionTypeKey] intValue];
    
    if (interruptionPhase == AVAudioSessionInterruptionTypeBegan) {
        if (openears_logging == 1) NSLog(@"The Audio Session was interrupted.");
        
        NSDictionary *userInfoDictionary = @{@"OpenEarsNotificationType": @"AudioSessionInterruptionDidBegin"}; // Send notification to OEEventsObserver.
        NSNotification *notification = [NSNotification notificationWithName:@"OpenEarsNotification" object:nil userInfo:userInfoDictionary];
        [[NSNotificationCenter defaultCenter] performSelectorOnMainThread:@selector(postNotification:) withObject:notification waitUntilDone:YES];
        
    }
    
    if (interruptionPhase == AVAudioSessionInterruptionTypeEnded) {
        
        if (openears_logging == 1) NSLog(@"The Audio Session interruption is over.");
        
        NSDictionary *userInfoDictionary = @{@"OpenEarsNotificationType": @"AudioSessionInterruptionDidEnd"}; // Send notification to OEEventsObserver.
        NSNotification *notification = [NSNotification notificationWithName:@"OpenEarsNotification" object:nil userInfo:userInfoDictionary];
        [[NSNotificationCenter defaultCenter] performSelectorOnMainThread:@selector(postNotification:) withObject:notification waitUntilDone:YES];
        
        if (self.audioUnitState == kAudioUnitIsStopped && self.disableSessionResetsWhileStopped) return; // Don't call a reset on a stopped unit if this is unwanted behavior.
        
        NSError *error = nil;
        
        error = [self setAllAudioSessionSettings];
        
        if (error != nil) NSLog(@"AVAudioSession set active failed with error: %@", error);
        
    }
}

#pragma mark -
#pragma mark Route management
#pragma mark -

- (NSString *) getCurrentRoute {
    
    NSMutableString *routeString = [[NSMutableString alloc] init];
    AVAudioSessionRouteDescription* route = [[AVAudioSession sharedInstance] currentRoute];

    [routeString appendString:@"<Input route or routes: \""];
    
    NSInteger index = 0;
    for (AVAudioSessionPortDescription* routeDescription in [route inputs]) {
        [routeString appendString:[routeDescription portType]];
        if ([route inputs].count > 1 && index != [route inputs].count - 1) {
            [routeString appendString:@", "];
        }
        index++;
    }
    
    index = 0;
    [routeString appendString:@"\". Output route or routes: \""];
    
    for (AVAudioSessionPortDescription* routeDescription in [route outputs]) {
        [routeString appendString:[routeDescription portType]];
        if ([route outputs].count > 1 && index != [route outputs].count - 1) {
            [routeString appendString:@", "];
        }
        index++;
    }
    
    [routeString appendString:@"\">"];
    
    if ([routeString length]<2) {
        return @"Route description error: unknown route"; // If this is bizarre for whatever reason, cheese it.
    }
    return [NSString stringWithString:routeString];
}

- (void) handleRouteChange:(NSNotification *)notification {
    UInt8 reasonValue = [[notification.userInfo valueForKey:AVAudioSessionRouteChangeReasonKey] intValue];
    AVAudioSessionRouteDescription *routeDescription = [notification.userInfo valueForKey:AVAudioSessionRouteChangePreviousRouteKey];
    BOOL performChange = TRUE;
       
    if (openears_logging == 1) NSLog(@"Audio route has changed for the following reason:");
    
    NSDictionary *userInfoDictionary = nil;
    NSNotification *openEarsNotification = nil;
    switch (reasonValue) {
        case AVAudioSessionRouteChangeReasonNewDeviceAvailable:
            performChange = TRUE;
            if (openears_logging == 1) NSLog(@"A new device is available");
            userInfoDictionary = @{@"OpenEarsNotificationType": @"AudioInputDidBecomeAvailable"};
            openEarsNotification = [NSNotification notificationWithName:@"OpenEarsNotification" object:nil userInfo:userInfoDictionary];
            [[NSNotificationCenter defaultCenter] performSelectorOnMainThread:@selector(postNotification:) withObject:openEarsNotification waitUntilDone:YES]; // Forward the input availability change to OEEventsObserver.
            break;
        case AVAudioSessionRouteChangeReasonOldDeviceUnavailable:
            performChange = TRUE;
            if (openears_logging == 1) NSLog(@"An old device became unavailable");
            userInfoDictionary = @{@"OpenEarsNotificationType": @"AudioInputDidBecomeUnavailable"};
            openEarsNotification = [NSNotification notificationWithName:@"OpenEarsNotification" object:nil userInfo:userInfoDictionary];
            [[NSNotificationCenter defaultCenter] performSelectorOnMainThread:@selector(postNotification:) withObject:openEarsNotification waitUntilDone:YES]; // Forward the input availability change to OEEventsObserver.
            if (openears_logging == 1) NSLog(@"the audio input is now unavailable.");
            break;
        case AVAudioSessionRouteChangeReasonCategoryChange:
            performChange = FALSE;
            if (openears_logging == 1) NSLog(@"There was a category change. The new category is %@", [[AVAudioSession sharedInstance] category]);
            break;
        case AVAudioSessionRouteChangeReasonOverride:
            performChange = FALSE;
            if (openears_logging == 1) NSLog(@"There was a route override.");
            break;
        case AVAudioSessionRouteChangeReasonWakeFromSleep:
            if (openears_logging == 1) NSLog(@"There was a wake from sleep.");
            break;
        case AVAudioSessionRouteChangeReasonNoSuitableRouteForCategory:
            performChange = FALSE;
            if (openears_logging == 1) NSLog(@"There is no suitable route for this category.");
            break;
        default:
            performChange = FALSE;
            if (openears_logging == 1) NSLog(@"The route has changed for an unknown reason.");
    }
    
    if (performChange) {
        if (openears_logging == 1) NSLog(@"This is a case for performing a route change. At the close of this method, the new audio route will be %@. The previous route before changing to this route was \"%@\".", [self getCurrentRoute], routeDescription);        
        NSDictionary *userInfoDictionary = @{@"OpenEarsNotificationType": @"AudioRouteDidChangeRoute", @"AudioRoute": [self getCurrentRoute]};
        NSNotification *notification = [NSNotification notificationWithName:@"OpenEarsNotification" object:nil userInfo:userInfoDictionary];
        [[NSNotificationCenter defaultCenter] performSelectorOnMainThread:@selector(postNotification:) withObject:notification waitUntilDone:YES]; // Forward the audio route change to OEEventsObserver.
    } else {
        if (openears_logging == 1) NSLog(@"This is not a case in which OpenEars notifies of a route change. At the close of this method, the new audio route will be %@. The previous route before changing to this route was \"%@\".", [self getCurrentRoute], routeDescription);
    } 
}

- (void) handleMediaServerReset {
    
    if (openears_logging == 1) NSLog(@"Media server reset. You may wish to react to this by stopping an open listening session and restarting it. OpenEars reacts to it be re-initializing its audio session.");
    NSError *error = [self setAllAudioSessionSettings];
    if (error) NSLog(@"Error while resetting media server: %@", error);
}

#pragma mark -
#pragma mark Audio unit
#pragma mark -

- (NSError *) setupAudioUnit {
    
    // Where we are creating a remote IO audio unit.
    
    audioUnitCallbackData.inputDecibels = 0.0;
    audioUnitCallbackData.isSuspended = FALSE;
    _audioUnitState = kAudioUnitIsStopped;
    audioUnitCallbackData.callbacks = 0;
    
    AudioComponentDescription audioComponentDescription; // A description of the audio unit
    audioComponentDescription.componentType = kAudioUnitType_Output;
    audioComponentDescription.componentSubType = kAudioUnitSubType_RemoteIO;
    audioComponentDescription.componentManufacturer = kAudioUnitManufacturer_Apple;
    audioComponentDescription.componentFlags = 0;
    audioComponentDescription.componentFlagsMask = 0;
    
    AudioComponent component = AudioComponentFindNext(NULL, &audioComponentDescription); // Finding the component with this description
    
    OSStatus error = AudioComponentInstanceNew(component, &_remoteIOAudioUnit); // Create a new instance of this component.
    
    if (error != noErr) {
        ReportError(error, @"couldn't create a new instance of AURemoteIO"); 
        return [self errorFromOSStatus:error];
    }
    
    // Enable input on the input scope of the input element.
        
    UInt32 enableInput = 1;// 1 means enable for this property. This property starts out disabled for the input scope/input element, so it needs to be enabled for recording.                                              
    
    error = AudioUnitSetProperty(_remoteIOAudioUnit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, kInputElementOfInputScope, &enableInput, sizeof(enableInput)); // Enable it.
    if (error != noErr) {
        ReportError(error, @"Could not enable input on the input element of the input scope of the remote io audio unit."); 
        return [self errorFromOSStatus:error];
    }  
  
    // Create a description using our convenience method that will be the type of audio we need for recognition (16000 hz, mono, 16-bit).
    AudioStreamBasicDescription requiredInputFormat = setupPCMAudioStreamBasicDescription(kSampleRateForRecognition, 1, kOEPCMFormatInt16, FALSE);
    AudioStreamBasicDescription outputFormat = setupPCMAudioStreamBasicDescription(kSampleRateForRecognition, 1, kOEPCMFormatInt16, FALSE);
    
    error = AudioUnitSetProperty(_remoteIOAudioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, kInputElementOfOutputScope, &outputFormat, sizeof(outputFormat));
    if (error != noErr) {
        ReportError(error, @"couldn't set the output client format on AURemoteIO");
        return [self errorFromOSStatus:error];
    }
    
    error = AudioUnitSetProperty(_remoteIOAudioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, kOutputElementOfInputScope, &requiredInputFormat, sizeof(requiredInputFormat)); // When the stream is leaving the unit, this is the format we need it in.
    if (error != noErr) {
        ReportError(error, @"couldn't set the input client format on AURemoteIO");
        return [self errorFromOSStatus:error];
    }
    
    // We're going to keep a few things in a struct for reference from the render callback.
    
    audioUnitCallbackData.remoteIOAudioUnit = _remoteIOAudioUnit; 
    
    UInt32 maximumProbableBufferDuration = 5; // If we have buffers longer than 5 seconds I guess we have bigger problems.
    UInt32 maximumProbableSampleRate = 48000; // Not sure whether this is possible under any circumstances but why take chances.
    UInt32 localBufferListCapacityFrames = maximumProbableBufferDuration * maximumProbableSampleRate * outputFormat.mBytesPerFrame;
    UInt32 channelsPerFrame = outputFormat.mChannelsPerFrame;
    UInt32 bytesPerFrame = outputFormat.mBytesPerFrame; // Let's express capacity according to the converter's output format.
    
    AudioBufferList *localBufferList = AllocateABL(channelsPerFrame, bytesPerFrame, FALSE, localBufferListCapacityFrames);
    
    audioUnitCallbackData.bufferList = localBufferList;
    audioUnitCallbackData.bufferListCapacityFrames = localBufferListCapacityFrames;

    AURenderCallbackStruct renderCallback; // This is the callback where the remote io data will be rendered.
    renderCallback.inputProc = renderRemoteIOAudio; // The name of the callback
    renderCallback.inputProcRefCon = (__bridge void *)(self); // We don't access anything from this class in the callback, so we're not going to pass in context.
    
    error = AudioUnitSetProperty(_remoteIOAudioUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, kOutputElementOfInputScope, &renderCallback, sizeof(renderCallback));
    if (error != noErr) {
        ReportError(error, @"couldn't set render callback on AURemoteIO");
        return [self errorFromOSStatus:error];
    }

    
    // Initialize the AURemoteIO instance
    error = AudioUnitInitialize(_remoteIOAudioUnit);
    if (error != noErr) {
        ReportError(error, @"couldn't initialize AURemoteIO instance");
        return [self errorFromOSStatus:error];
    }
    return nil;
}

- (NSError *) teardownAudioUnit {
    
    audioUnitCallbackData.inputDecibels = 0.0;
    audioUnitCallbackData.isSuspended = FALSE;
    audioUnitCallbackData.callbacks = 0;
    
    if (_remoteIOAudioUnit != NULL && (self.audioUnitState == kAudioUnitIsStarted)) {
        NSError *error = [self stopAudioUnit];   
        if (error) {
            return [NSError errorWithDomain:@"com.politepix.openears" code:error.code userInfo:@{ NSLocalizedDescriptionKey : @"It was not possible to dispose of this audio unit because it was still started when teardownAudioUnit was called and it wasn't possible to stop it due to an error, so it isn't possible to attempt to dispose of it." }];   
        }
    }
    
    _audioUnitState = kAudioUnitIsStopped;
    
    OSStatus error = noErr;
    if (_remoteIOAudioUnit != NULL) {
        error = AudioComponentInstanceDispose(_remoteIOAudioUnit);
        if (error != noErr) {
            return [NSError errorWithDomain:@"com.politepix.openears" code:error userInfo:@{ NSLocalizedDescriptionKey : @"It was not possible to dispose of this audio unit." }];   
        } else {
            DeallocateABL(audioUnitCallbackData.bufferList);
            return nil;   
        }
    } else {
        return [NSError errorWithDomain:@"com.politepix.openears" code:error userInfo:@{ NSLocalizedDescriptionKey : @"No attempt was made to uninitialize this audio unit because it isn't currently initialized." }];
    }
    return nil;
}

- (NSError *) startAudioUnit {
    
    OSStatus error = noErr;
    
    error = AudioOutputUnitStart(_remoteIOAudioUnit);
    if (error != noErr) {
        ReportError(error, @"couldn't start AURemoteIO");
        self.audioUnitState = kAudioUnitIsStopped;
        return [self errorFromOSStatus:error];
    }
    audioUnitCallbackData.callbacks = 0;
    NSError *initiateTestIfRequestedError = [self initiateTestIfRequested];
    if (initiateTestIfRequestedError && openears_logging == 1) NSLog(@"Error trying to load test file: %@", initiateTestIfRequestedError); // This is not a showstopper.
    
    self.audioUnitState = kAudioUnitIsStarted;
    return nil;
}

- (NSError *) stopAudioUnit {
    if (self.audioUnitState != kAudioUnitIsStopped) {
        OSStatus error = AudioOutputUnitStop(_remoteIOAudioUnit);
        if (error != noErr) {
            
            ReportError(error, @"couldn't stop AURemoteIO");
            return [self errorFromOSStatus:error];
        }

        self.audioUnitState = kAudioUnitIsStopped;
    } else {
        return [NSError errorWithDomain:@"com.politepix.openears" code:-1 userInfo:@{NSLocalizedDescriptionKey : @"It wasn't possible to stop this audio unit because it wasn't started when stopAudioUnit was called."}];           
    }
    return nil;
}

@end

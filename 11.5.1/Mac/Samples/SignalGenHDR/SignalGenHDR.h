/* -LICENSE-START-
** Copyright (c) 2018 Blackmagic Design
**
** Permission is hereby granted, free of charge, to any person or organization
** obtaining a copy of the software and accompanying documentation covered by
** this license (the "Software") to use, reproduce, display, distribute,
** execute, and transmit the Software, and to prepare derivative works of the
** Software, and to permit third-parties to whom the Software is furnished to
** do so, all subject to the following:
** 
** The copyright notices in the Software and this entire statement, including
** the above license grant, this restriction and the following disclaimer,
** must be included in all copies of the Software, in whole or in part, and
** all derivative works of the Software, unless such copies or derivative
** works are solely in the form of machine-executable object code generated by
** a source language processor.
** 
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
** SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
** FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
** ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
** DEALINGS IN THE SOFTWARE.
** -LICENSE-END-
*/
//
//  SignalGenHDR.h
//  Signal Generator HDR
//

#include <Cocoa/Cocoa.h>
#include <map>
#include "com_ptr.h"
#include "DeckLinkAPI.h"
#include "HDRVideoFrame.h"


enum class EOTF { SDR = 0, HDR = 1, PQ = 2, HLG = 3 };

// Forward declarations
class DeckLinkDeviceDiscovery;

typedef std::map<BMDDisplayMode, com_ptr<IDeckLinkDisplayMode>> DisplayModeMap;

@interface SignalGenHDRAppDelegate : NSObject <NSApplicationDelegate> {
	NSWindow*					window;

	IBOutlet NSButton*			startButton;

	IBOutlet NSPopUpButton*		videoFormatPopup;
	IBOutlet NSPopUpButton*		pixelFormatPopup;
	IBOutlet NSPopUpButton*		deviceListPopup;

	IBOutlet NSPopUpButton*		eotfPopup;
	IBOutlet NSSlider*			displayPrimaryRedXSlider;
	IBOutlet NSTextField*		displayPrimaryRedXText;
	IBOutlet NSSlider*			displayPrimaryRedYSlider;
	IBOutlet NSTextField*		displayPrimaryRedYText;
	IBOutlet NSSlider*			displayPrimaryGreenXSlider;
	IBOutlet NSTextField*		displayPrimaryGreenXText;
	IBOutlet NSSlider*			displayPrimaryGreenYSlider;
	IBOutlet NSTextField*		displayPrimaryGreenYText;
	IBOutlet NSSlider*			displayPrimaryBlueXSlider;
	IBOutlet NSTextField*		displayPrimaryBlueXText;
	IBOutlet NSSlider*			displayPrimaryBlueYSlider;
	IBOutlet NSTextField*		displayPrimaryBlueYText;
	IBOutlet NSSlider*			whitePointXSlider;
	IBOutlet NSTextField*		whitePointXText;
	IBOutlet NSSlider*			whitePointYSlider;
	IBOutlet NSTextField*		whitePointYText;
	IBOutlet NSSlider*			maxDisplayMasteringLuminanceSlider;
	IBOutlet NSTextField*		maxDisplayMasteringLuminanceText;
	IBOutlet NSSlider*			minDisplayMasteringLuminanceSlider;
	IBOutlet NSTextField*		minDisplayMasteringLuminanceText;
	IBOutlet NSSlider*			maxCLLSlider;
	IBOutlet NSTextField*		maxCLLText;
	IBOutlet NSSlider*			maxFALLSlider;
	IBOutlet NSTextField*		maxFALLText;
	
	IBOutlet NSView*			previewView;
	
	BOOL						running;

	com_ptr<DeckLinkDeviceDiscovery>	deckLinkDiscovery;
	com_ptr<IDeckLinkOutput>			selectedDeckLinkOutput;
	com_ptr<IDeckLinkConfiguration>		selectedDeckLinkConfiguration;
	com_ptr<IDeckLinkDisplayMode>		selectedDisplayMode;
	com_ptr<HDRVideoFrame>				videoFrameBars;
	
	BMDPixelFormat						selectedPixelFormat;
	HDRMetadata							selectedHDRParameters;

	DisplayModeMap				supportedDisplayModeMap;
	
	NSNumberFormatter*			frac3DigitNumberFormatter;
	NSNumberFormatter*			frac4DigitNumberFormatter;
	NSNumberFormatter*			int5DigitNumberFormatter;
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification;
- (void)applicationWillTerminate:(NSNotification *)notification;

- (void)addDevice:(com_ptr<IDeckLink>)deckLink;
- (void)removeDevice:(com_ptr<IDeckLink>)deckLink;

- (void)refreshDisplayModeMenu;
- (void)refreshPixelFormatMenu;
- (void)refreshEOTFMenu;

- (IBAction)toggleStart:(id)sender;
- (IBAction)newDeviceSelected:(id)sender;
- (IBAction)newVideoFormatSelected:(id)sender;
- (IBAction)newPixelFormatSelected:(id)sender;
- (void)enableInterface:(BOOL)enable;
- (void)enableHDRInterface:(BOOL)enable;

- (void)startRunning;
- (void)stopRunning;

- (IBAction)newEOTFSelected:(id)sender;
- (IBAction)displayPrimaryRedXSliderChanged:(id)sender;
- (IBAction)displayPrimaryRedXTextChanged:(id)sender;
- (IBAction)displayPrimaryRedYSliderChanged:(id)sender;
- (IBAction)displayPrimaryRedYTextChanged:(id)sender;
- (IBAction)displayPrimaryGreenXSliderChanged:(id)sender;
- (IBAction)displayPrimaryGreenXTextChanged:(id)sender;
- (IBAction)displayPrimaryGreenYSliderChanged:(id)sender;
- (IBAction)displayPrimaryGreenYTextChanged:(id)sender;
- (IBAction)displayPrimaryBlueXSliderChanged:(id)sender;
- (IBAction)displayPrimaryBlueXTextChanged:(id)sender;
- (IBAction)displayPrimaryBlueYSliderChanged:(id)sender;
- (IBAction)displayPrimaryBlueYTextChanged:(id)sender;
- (IBAction)whitePointXSliderChanged:(id)sender;
- (IBAction)whitePointXTextChanged:(id)sender;
- (IBAction)whitePointYSliderChanged:(id)sender;
- (IBAction)whitePointYTextChanged:(id)sender;
- (IBAction)maxDisplayMasteringLuminanceSliderChanged:(id)sender;
- (IBAction)maxDisplayMasteringLuminanceTextChanged:(id)sender;
- (IBAction)minDisplayMasteringLuminanceSliderChanged:(id)sender;
- (IBAction)minDisplayMasteringLuminanceTextChanged:(id)sender;
- (IBAction)maxCLLSliderChanged:(id)sender;
- (IBAction)maxCLLTextChanged:(id)sender;
- (IBAction)maxFALLSliderChanged:(id)sender;
- (IBAction)maxFALLTextChanged:(id)sender;

@property (assign) IBOutlet NSWindow *window;

@end

int		GetBytesPerRow(BMDPixelFormat pixelFormat, uint32_t frameWidth);

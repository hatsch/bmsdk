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

#include <Accelerate/Accelerate.h>
#include <CoreServices/CoreServices.h>
#include <CoreGraphics/CGImage.h>
#include <Foundation/NSURL.h>
#include <Foundation/NSString.h>
#include <Foundation/NSFileManager.h>
#include <ImageIO/CGImageDestination.h>

#include "ImageWriter.h"

HRESULT ImageWriter::GetNextFilenameWithPrefix(const std::string& path, const std::string& filenamePrefix, std::string& nextFileName)
{
	HRESULT result = E_FAIL;
	static int idx = 0;
	
	while (idx < 10000)
	{
		NSString* filename = [NSString stringWithFormat:@"%s/%s%.4d.png", path.c_str(), filenamePrefix.c_str(), idx++];
		
		if (![[NSFileManager defaultManager] fileExistsAtPath:filename])
		{
			nextFileName = std::string([filename UTF8String]);
			result = S_OK;
			break;
		}
	}
	
	return result;
}

HRESULT ImageWriter::WriteBgra32VideoFrameToPNG(IDeckLinkVideoFrame* bgra32VideoFrame, const std::string& pngFilename)
{
	NSFileManager* localFileManager = [[NSFileManager alloc] init];
	NSString* pngFilenameString = [[NSString stringWithUTF8String:pngFilename.c_str()] stringByExpandingTildeInPath];
	
	// Ensure video frame has expected pixel format
	if (bgra32VideoFrame->GetPixelFormat() != bmdFormat8BitBGRA)
	{
		fprintf(stderr, "Video frame is not in 8-Bit BGRA pixel format\n");
		return E_FAIL;
	}
	
	// Check whether file already exists
	if ([localFileManager fileExistsAtPath:pngFilenameString])
		return E_FAIL;
	
	// Get a pointer to the video frame's buffer
	void* videoFrameBuffer = NULL;
	if (bgra32VideoFrame->GetBytes(&videoFrameBuffer) != S_OK)
	{
		fprintf(stderr, "Could not get DeckLinkVideoFrame buffer pointer\n");
		return E_FAIL;
	}
	
	// Add buffer into vImage
	vImage_Buffer imageBuffer = {
		.data		= videoFrameBuffer,
		.height		= static_cast<vImagePixelCount>(bgra32VideoFrame->GetHeight()),
		.width		= static_cast<vImagePixelCount>(bgra32VideoFrame->GetWidth()),
		.rowBytes	= static_cast<size_t>(bgra32VideoFrame->GetRowBytes()),
	};
	
	CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
			
	// Declare the pixel format for the vImage_Buffer
	vImage_CGImageFormat imageFormat = {
		.bitsPerComponent = 8,
		.bitsPerPixel = 32,
		.bitmapInfo = kCGBitmapByteOrder32Little | kCGImageAlphaFirst,
		.colorSpace = colorSpace,
	};
	
	// Convert vImage to CGImage
	vImage_Error imageError;
	CGImageRef image = vImageCreateCGImageFromBuffer( &imageBuffer, &imageFormat, NULL, NULL, kvImageNoFlags, &imageError);
	
	if (imageError != kvImageNoError)
		return E_FAIL;
	
	// Write CGImage to PNG file
	CGImageDestinationRef imageDestination = CGImageDestinationCreateWithURL((CFURLRef)[NSURL fileURLWithPath:pngFilenameString], kUTTypePNG, 1, NULL);
	CGImageDestinationAddImage(imageDestination, image, nil);
	CGImageDestinationFinalize(imageDestination);
																					
	CFRelease(imageDestination);
	CGImageRelease(image);
	
	return S_OK;
}

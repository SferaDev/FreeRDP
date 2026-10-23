/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL3 Client - macOS pasteboard helpers
 *
 * Copyright 2026 Alexis Rico <sferadev@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#import <AppKit/AppKit.h>
#import <ImageIO/ImageIO.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include <algorithm>
#include <cstring>
#include <limits>

#include "sdl_clip_macos.hpp"

namespace
{
	NSTimer* monitorTimer = nil;
	long monitorCount = 0;

	NSPasteboard* pasteboardNamed(const char* name)
	{
		if (!name)
			return [NSPasteboard generalPasteboard];
		return [NSPasteboard pasteboardWithName:[NSString stringWithUTF8String:name]];
	}

	void put16(std::vector<uint8_t>& out, size_t offset, uint16_t value)
	{
		out[offset] = static_cast<uint8_t>(value & 0xFF);
		out[offset + 1] = static_cast<uint8_t>(value >> 8);
	}

	void put32(std::vector<uint8_t>& out, size_t offset, uint32_t value)
	{
		for (size_t x = 0; x < 4; x++)
			out[offset + x] = static_cast<uint8_t>((value >> (8 * x)) & 0xFF);
	}

	constexpr size_t kBitmapInfoHeaderSize = 40;
	constexpr size_t kBitmapV5HeaderSize = 124;
	constexpr uint32_t kLcsSRGB = 0x73524742;
	constexpr uint32_t kLcsGmImages = 4;
} // namespace

namespace sdl::clip::macos
{
	long changeCount(const char* pasteboard)
	{
		@autoreleasepool
		{
			return static_cast<long>([pasteboardNamed(pasteboard) changeCount]);
		}
	}

	std::vector<std::string> mimeTypes(const char* pasteboard)
	{
		std::vector<std::string> list;
		@autoreleasepool
		{
			for (NSPasteboardItem* item in [pasteboardNamed(pasteboard) pasteboardItems])
			{
				for (NSString* type in [item types])
				{
					NSString* mime = [[UTType typeWithIdentifier:type] preferredMIMEType];
					if (mime)
						list.emplace_back([mime UTF8String]);
					list.emplace_back([type UTF8String]);
				}
			}
		}
		return list;
	}

	bool isConcealed(const std::vector<std::string>& mimeTypes)
	{
		return std::any_of(mimeTypes.begin(), mimeTypes.end(),
		                   [](const std::string& m)
		                   {
			                   return (m == "org.nspasteboard.ConcealedType") ||
			                          (m == "org.nspasteboard.TransientType");
		                   });
	}

	bool startMonitor(double intervalSeconds, void (*notify)(void*), void* userdata)
	{
		if (!notify || (intervalSeconds <= 0.0))
			return false;

		dispatch_async(dispatch_get_main_queue(), ^{
		  [monitorTimer invalidate];
		  monitorCount = changeCount();
		  monitorTimer = [NSTimer timerWithTimeInterval:intervalSeconds
		                                        repeats:YES
		                                          block:^(NSTimer*) {
			                                        const long count = changeCount();
			                                        if (count == monitorCount)
				                                        return;
			                                        monitorCount = count;
			                                        notify(userdata);
		                                          }];
		  monitorTimer.tolerance = intervalSeconds / 4.0;
		  /* common modes: keep watching while a menu is tracked or a window is resized */
		  [[NSRunLoop mainRunLoop] addTimer:monitorTimer forMode:NSRunLoopCommonModes];
		});
		return true;
	}

	void stopMonitor()
	{
		dispatch_async(dispatch_get_main_queue(), ^{
		  [monitorTimer invalidate];
		  monitorTimer = nil;
		});
	}

	std::vector<uint8_t> bgraToDib(const uint8_t* bgra, size_t width, size_t height, size_t stride,
	                               bool v5)
	{
		const size_t headerSize = v5 ? kBitmapV5HeaderSize : kBitmapInfoHeaderSize;
		constexpr auto maxDim = static_cast<size_t>(std::numeric_limits<int32_t>::max());
		if (!bgra || (width == 0) || (height == 0) || (width > maxDim) || (height > maxDim) ||
		    (stride < width * 4))
			return {};
		if (width > (std::numeric_limits<uint32_t>::max() - headerSize) / 4 / height)
			return {};

		const size_t rowSize = width * 4;
		const size_t imageSize = rowSize * height;
		std::vector<uint8_t> out(headerSize + imageSize, 0);

		put32(out, 0, static_cast<uint32_t>(headerSize));
		put32(out, 4, static_cast<uint32_t>(width));
		put32(out, 8, static_cast<uint32_t>(height)); /* positive: bottom-up */
		put16(out, 12, 1);                            /* planes */
		put16(out, 14, 32);                           /* bits per pixel */
		put32(out, 16, 0);                            /* BI_RGB */
		put32(out, 20, static_cast<uint32_t>(imageSize));
		if (v5)
		{
			/* What Windows writes for a 32 bpp DIBV5, masks included although BI_RGB */
			put32(out, 40, 0x00FF0000);
			put32(out, 44, 0x0000FF00);
			put32(out, 48, 0x000000FF);
			put32(out, 52, 0xFF000000);
			put32(out, 56, kLcsSRGB);
			put32(out, 108, kLcsGmImages);
		}

		uint8_t* dst = out.data() + headerSize;
		for (size_t y = 0; y < height; y++)
			std::memcpy(dst + (height - 1 - y) * rowSize, bgra + y * stride, rowSize);
		return out;
	}

	std::vector<uint8_t> imageToDib(const void* data, size_t size, bool v5)
	{
		if (!data || (size == 0))
			return {};

		@autoreleasepool
		{
			NSData* nsdata = [NSData dataWithBytesNoCopy:const_cast<void*>(data)
			                                      length:size
			                                freeWhenDone:NO];
			CGImageSourceRef source =
			    CGImageSourceCreateWithData((__bridge CFDataRef)nsdata, nullptr);
			if (!source)
				return {};
			CGImageRef image = CGImageSourceCreateImageAtIndex(source, 0, nullptr);
			CFRelease(source);
			if (!image)
				return {};

			const size_t width = CGImageGetWidth(image);
			const size_t height = CGImageGetHeight(image);
			std::vector<uint8_t> result;
			if ((width > 0) && (height > 0) &&
			    (width <= (std::numeric_limits<uint32_t>::max() / 4) / height))
			{
				const size_t stride = width * 4;
				std::vector<uint8_t> pixels(stride * height, 0);

				/* Keep the image's own RGB space so the pixel values reach the server unchanged */
				CGColorSpaceRef space = CGImageGetColorSpace(image);
				CGColorSpaceRef srgb = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
				const uint32_t info = kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little;
				CGContextRef ctx = nullptr;
				if (space && (CGColorSpaceGetModel(space) == kCGColorSpaceModelRGB))
					ctx = CGBitmapContextCreate(pixels.data(), width, height, 8, stride, space, info);
				if (!ctx)
					ctx = CGBitmapContextCreate(pixels.data(), width, height, 8, stride, srgb, info);
				CGColorSpaceRelease(srgb);

				if (ctx)
				{
					CGContextSetBlendMode(ctx, kCGBlendModeCopy);
					CGContextDrawImage(ctx,
					                   CGRectMake(0, 0, static_cast<CGFloat>(width),
					                              static_cast<CGFloat>(height)),
					                   image);
					CGContextRelease(ctx);

					/* CoreGraphics only draws premultiplied; DIBs carry straight alpha */
					for (size_t x = 0; x < pixels.size(); x += 4)
					{
						const uint32_t a = pixels[x + 3];
						if ((a == 0) || (a == 255))
							continue;
						for (size_t c = 0; c < 3; c++)
						{
							const uint32_t v = (pixels[x + c] * 255u + a / 2) / a;
							pixels[x + c] = static_cast<uint8_t>(std::min<uint32_t>(v, 255));
						}
					}
					result = bgraToDib(pixels.data(), width, height, stride, v5);
				}
			}
			CGImageRelease(image);
			return result;
		}
	}
} // namespace sdl::clip::macos

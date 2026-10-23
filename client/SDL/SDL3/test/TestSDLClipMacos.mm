#import <AppKit/AppKit.h>
#import <ImageIO/ImageIO.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>

#include "sdl_clip_macos.hpp"

namespace
{
	constexpr size_t kWidth = 64;
	constexpr size_t kHeight = 48;

	bool fail(const char* what)
	{
		(void)fprintf(stderr, "TestSDLClipMacos: %s\n", what);
		return false;
	}

	uint32_t get32(const std::vector<uint8_t>& v, size_t offset)
	{
		return static_cast<uint32_t>(v[offset]) | (static_cast<uint32_t>(v[offset + 1]) << 8) |
		       (static_cast<uint32_t>(v[offset + 2]) << 16) |
		       (static_cast<uint32_t>(v[offset + 3]) << 24);
	}

	/* The e2e test image: straight RGBA, alpha ramp along x */
	std::vector<uint8_t> rampRGBA()
	{
		std::vector<uint8_t> rgba(kWidth * kHeight * 4);
		for (size_t y = 0; y < kHeight; y++)
		{
			for (size_t x = 0; x < kWidth; x++)
			{
				uint8_t* p = &rgba[(y * kWidth + x) * 4];
				p[0] = static_cast<uint8_t>(x * 4);
				p[1] = static_cast<uint8_t>(y * 5);
				p[2] = 200;
				p[3] = static_cast<uint8_t>(255 - x * 3);
			}
		}
		return rgba;
	}

	std::vector<uint8_t> encode(const std::vector<uint8_t>& rgba, CFStringRef type)
	{
		CGColorSpaceRef srgb = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
		CFDataRef pixels = CFDataCreate(nullptr, rgba.data(), static_cast<CFIndex>(rgba.size()));
		CGDataProviderRef provider = CGDataProviderCreateWithCFData(pixels);
		CGImageRef image = CGImageCreate(kWidth, kHeight, 8, 32, kWidth * 4, srgb,
		                                 kCGImageAlphaLast | kCGBitmapByteOrderDefault, provider,
		                                 nullptr, false, kCGRenderingIntentDefault);
		CFMutableDataRef out = CFDataCreateMutable(nullptr, 0);
		CGImageDestinationRef dest = CGImageDestinationCreateWithData(out, type, 1, nullptr);
		CGImageDestinationAddImage(dest, image, nullptr);
		const bool ok = CGImageDestinationFinalize(dest);

		std::vector<uint8_t> result;
		if (ok)
		{
			const auto* bytes = CFDataGetBytePtr(out);
			result.assign(bytes, bytes + CFDataGetLength(out));
		}
		CFRelease(dest);
		CFRelease(out);
		CGImageRelease(image);
		CGDataProviderRelease(provider);
		CFRelease(pixels);
		CGColorSpaceRelease(srgb);
		return result;
	}

	bool testBgraToDib()
	{
		/* 2x2 top-down: row 0 red, green; row 1 blue, half transparent white */
		const uint8_t bgra[] = { 0, 0, 255, 255, 0, 255, 0, 255, 255, 0, 0, 255, 255, 255, 255, 128 };
		for (const bool v5 : { false, true })
		{
			const auto dib = sdl::clip::macos::bgraToDib(bgra, 2, 2, 8, v5);
			const size_t header = v5 ? 124 : 40;
			if (dib.size() != header + 16)
				return fail("bgraToDib size");
			if ((get32(dib, 0) != header) || (get32(dib, 4) != 2) || (get32(dib, 8) != 2) ||
			    (get32(dib, 12) != (1u | (32u << 16))) || (get32(dib, 16) != 0) ||
			    (get32(dib, 20) != 16))
				return fail("bgraToDib header");
			if (v5 && ((get32(dib, 40) != 0x00FF0000) || (get32(dib, 52) != 0xFF000000) ||
			           (get32(dib, 56) != 0x73524742)))
				return fail("bgraToDib v5 masks");
			/* bottom-up: the first stored row is the image's last */
			if ((memcmp(&dib[header], &bgra[8], 8) != 0) || (memcmp(&dib[header + 8], bgra, 8) != 0))
				return fail("bgraToDib row order");
		}
		if (!sdl::clip::macos::bgraToDib(bgra, 2, 2, 4, false).empty())
			return fail("bgraToDib accepted a short stride");
		if (!sdl::clip::macos::bgraToDib(bgra, 0x10000, 0x10000, 0x40000, false).empty())
			return fail("bgraToDib accepted an image over 4 GiB");
		return true;
	}

	bool checkDib(const std::vector<uint8_t>& dib, const std::vector<uint8_t>& rgba, bool v5)
	{
		const size_t header = v5 ? 124 : 40;
		if (dib.size() != header + kWidth * kHeight * 4)
			return fail("imageToDib size");
		if ((get32(dib, 4) != kWidth) || (get32(dib, 8) != kHeight))
			return fail("imageToDib dimensions");

		for (size_t y = 0; y < kHeight; y++)
		{
			for (size_t x = 0; x < kWidth; x++)
			{
				const uint8_t* want = &rgba[(y * kWidth + x) * 4];
				const uint8_t* got = &dib[header + ((kHeight - 1 - y) * kWidth + x) * 4];
				if (got[3] != want[3])
					return fail("imageToDib alpha");
				/* what the e2e test compares: premultiplied, within 3 */
				for (size_t c = 0; c < 3; c++)
				{
					const int w = want[c] * want[3] / 255;
					const int g = got[2 - c] * got[3] / 255;
					if (std::abs(w - g) > 3)
						return fail("imageToDib colour");
				}
			}
		}
		return true;
	}

	bool testImageToDib()
	{
		const auto rgba = rampRGBA();
		for (CFStringRef type : { (__bridge CFStringRef)UTTypePNG.identifier,
		                          (__bridge CFStringRef)UTTypeTIFF.identifier })
		{
			const auto encoded = encode(rgba, type);
			if (encoded.empty())
				return fail("encoding the test image");
			for (const bool v5 : { false, true })
			{
				if (!checkDib(sdl::clip::macos::imageToDib(encoded.data(), encoded.size(), v5), rgba,
				              v5))
					return false;
			}
		}

		const char junk[] = "not an image";
		if (!sdl::clip::macos::imageToDib(junk, sizeof(junk), false).empty())
			return fail("imageToDib accepted junk");
		return true;
	}

	bool has(const std::vector<std::string>& list, const char* value)
	{
		return std::find(list.begin(), list.end(), value) != list.end();
	}

	bool testPasteboard()
	{
		/* a private pasteboard, so the test leaves the user's clipboard alone */
		NSPasteboard* pb = [NSPasteboard pasteboardWithUniqueName];
		const std::string name = [[pb name] UTF8String];
		bool ok = true;

		const long before = sdl::clip::macos::changeCount(name.c_str());
		[pb clearContents];
		[pb setString:@"ñá€中" forType:NSPasteboardTypeString];
		if (sdl::clip::macos::changeCount(name.c_str()) == before)
			ok = fail("changeCount did not move");

		auto mimes = sdl::clip::macos::mimeTypes(name.c_str());
		if (ok && (!has(mimes, "text/plain;charset=utf-8") || !has(mimes, "public.utf8-plain-text")))
			ok = fail("text mime types");
		if (ok && sdl::clip::macos::isConcealed(mimes))
			ok = fail("plain text reported as concealed");

		if (ok)
		{
			const auto png = encode(rampRGBA(), (__bridge CFStringRef)UTTypePNG.identifier);
			NSPasteboardItem* item = [NSPasteboardItem new];
			[item setData:[NSData dataWithBytes:png.data() length:png.size()]
			      forType:NSPasteboardTypePNG];
			[item setData:[NSData data] forType:@"org.nspasteboard.ConcealedType"];
			[pb clearContents];
			[pb writeObjects:@[ item ]];
			mimes = sdl::clip::macos::mimeTypes(name.c_str());
			if (!has(mimes, "image/png") || has(mimes, "text/plain;charset=utf-8"))
				ok = fail("image mime types");
			else if (!sdl::clip::macos::isConcealed(mimes))
				ok = fail("concealed type missed");
		}

		[pb releaseGlobally];
		return ok;
	}
} // namespace

int TestSDLClipMacos(int argc, char* argv[])
{
	(void)argc;
	(void)argv;
	@autoreleasepool
	{
		if (!testBgraToDib() || !testImageToDib() || !testPasteboard())
			return -1;
	}
	return 0;
}

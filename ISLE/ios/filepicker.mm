#include "filepicker.h"

#include <SDL3/SDL.h>

#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

@interface IsleFolderPickerDelegate : NSObject <UIDocumentPickerDelegate>
@property(nonatomic, strong) NSURL* pickedURL;
@property(nonatomic) BOOL done;
@end

@implementation IsleFolderPickerDelegate

- (void)documentPicker:(UIDocumentPickerViewController*)controller didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls
{
	self.pickedURL = urls.firstObject;
	self.done = YES;
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController*)controller
{
	self.done = YES;
}

@end

static NSURL* ShowFolderPicker(SDL_Window* p_window) API_AVAILABLE(ios(14.0))
{
	UIWindow* uiwindow = (__bridge UIWindow*)
		SDL_GetPointerProperty(SDL_GetWindowProperties(p_window), SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, NULL);
	UIViewController* rootVC = uiwindow.rootViewController;
	if (!rootVC) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "No root view controller to present the folder picker");
		return nil;
	}

	IsleFolderPickerDelegate* delegate = [[IsleFolderPickerDelegate alloc] init];
	UIDocumentPickerViewController* picker =
		[[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[ UTTypeFolder ]];
	picker.delegate = delegate;
	picker.allowsMultipleSelection = NO;

	[rootVC presentViewController:picker animated:YES completion:nil];

	while (!delegate.done) {
		[[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
	}

	return delegate.pickedURL;
}

static bool CopyDirectoryContents(NSURL* p_sourceDir, NSURL* p_destDir)
{
	NSFileManager* fm = [NSFileManager defaultManager];
	NSError* error = nil;

	if (![fm createDirectoryAtURL:p_destDir withIntermediateDirectories:YES attributes:nil error:&error]) {
		SDL_LogError(
			SDL_LOG_CATEGORY_APPLICATION,
			"Failed to create '%s': %s",
			p_destDir.path.UTF8String,
			error.localizedDescription.UTF8String
		);
		return false;
	}

	NSArray<NSURL*>* items = [fm contentsOfDirectoryAtURL:p_sourceDir
							   includingPropertiesForKeys:@[ NSURLIsDirectoryKey ]
												  options:NSDirectoryEnumerationSkipsHiddenFiles
													error:&error];
	if (!items) {
		SDL_LogError(
			SDL_LOG_CATEGORY_APPLICATION,
			"Failed to enumerate '%s': %s",
			p_sourceDir.path.UTF8String,
			error.localizedDescription.UTF8String
		);
		return false;
	}

	for (NSURL* item in items) {
		NSNumber* isDirectory = nil;
		[item getResourceValue:&isDirectory forKey:NSURLIsDirectoryKey error:nil];

		NSURL* dest = [p_destDir URLByAppendingPathComponent:item.lastPathComponent];
		if (isDirectory.boolValue) {
			if (!CopyDirectoryContents(item, dest)) {
				return false;
			}
		}
		else {
			[fm removeItemAtURL:dest error:nil];
			if (![fm copyItemAtURL:item toURL:dest error:&error]) {
				SDL_LogError(
					SDL_LOG_CATEGORY_APPLICATION,
					"Failed to copy '%s': %s",
					item.path.UTF8String,
					error.localizedDescription.UTF8String
				);
				return false;
			}
		}
	}

	return true;
}

static bool ImportGameFiles(NSURL* p_sourceDir, const char* p_destRoot)
{
	NSURL* destRoot = [NSURL fileURLWithPath:@(p_destRoot)];

	// If the user picked the game data folder itself (e.g. "LEGO") rather than
	// the folder containing it, import it as <destRoot>/LEGO so the expected
	// LEGO/Scripts and LEGO/data layout is preserved.
	NSFileManager* fm = [NSFileManager defaultManager];
	bool hasLegoDir = false;
	bool hasScriptsDir = false;
	NSArray<NSURL*>* items = [fm contentsOfDirectoryAtURL:p_sourceDir
							   includingPropertiesForKeys:nil
												  options:NSDirectoryEnumerationSkipsHiddenFiles
													error:nil];
	for (NSURL* item in items) {
		NSString* name = item.lastPathComponent.lowercaseString;
		if ([name isEqualToString:@"lego"]) {
			hasLegoDir = true;
		}
		else if ([name isEqualToString:@"scripts"]) {
			hasScriptsDir = true;
		}
	}

	if (!hasLegoDir && hasScriptsDir) {
		destRoot = [destRoot URLByAppendingPathComponent:@"LEGO"];
	}

	return CopyDirectoryContents(p_sourceDir, destRoot);
}

bool IOS_TryImportGameFiles(SDL_Window* p_window, const char* p_destRoot)
{
	if (@available(iOS 14.0, *)) {
		const SDL_MessageBoxButtonData buttons[] = {
			{SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Select folder"},
			{SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Cancel"},
		};
		const SDL_MessageBoxData messageBox = {
			SDL_MESSAGEBOX_INFORMATION,
			p_window,
			"LEGO® Island",
			"The game files could not be found or read.\n\n"
			"If you have a copy of the LEGO® Island files on this device (for example in the Files app or iCloud "
			"Drive), you can select the folder containing them and they will be copied into this app's storage.",
			SDL_arraysize(buttons),
			buttons,
			NULL
		};

		int button = 0;
		if (!SDL_ShowMessageBox(&messageBox, &button) || button != 1) {
			return false;
		}

		NSURL* pickedURL = ShowFolderPicker(p_window);
		if (!pickedURL) {
			return false;
		}

		bool imported = false;
		bool secured = [pickedURL startAccessingSecurityScopedResource];
		imported = ImportGameFiles(pickedURL, p_destRoot);
		if (secured) {
			[pickedURL stopAccessingSecurityScopedResource];
		}

		if (!imported) {
			SDL_ShowSimpleMessageBox(
				SDL_MESSAGEBOX_ERROR,
				"LEGO® Island Error",
				"The game files could not be imported; see logs for details.",
				p_window
			);
		}

		return imported;
	}

	return false;
}

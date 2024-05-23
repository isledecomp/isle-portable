# Contributing

## General Guidelines

If you feel fit to contribute, feel free to create a pull request! Someone will review and merge it (or provide feedback) as soon as possible.

Please keep your pull requests small and understandable; you may be able to shoot ahead and make a lot of progress in a short amount of time, but this is a collaborative project, so you must allow others to catch up and follow along. Large pull requests become significantly more unwieldy to review, and as such make it exponentially more likely for a mistake or error to go undetected. They also make it harder to merge other pull requests because the more files you modify, the more likely it is for a merge conflict to occur. A general guideline is to keep submissions limited to one class at a time. Sometimes two or more classes may be too interlinked for this to be feasible, so this is not a hard rule, however if your PR is starting to modify more than 10 or so files, it's probably getting too big.

This repository has a single goal: achieving platform independence. Consequently, contributions that do not support this goal cannot be accepted. Examples of out-of-scope contributions include:

* Improving code efficiency or performance without addressing platform compatibility
* Replacing `MxString` with `std::string`
* Fixing gameplay bugs
* Enhancing audio or video quality

If in doubt, please contact us in the [Matrix chatroom](https://matrix.to/#/#isledecomp:matrix.org).

## Decompilation Project Foundation

The [decompilation project](https://github.com/isledecomp/isle) serves as the foundation for this initiative. Improvements and changes to the decompilation project will be continually merged into our codebase. Therefore, please limit modifications to the code's structure/layout to the absolute minimum to facilitate easier merging.

Please also consider contributing improvements to function and variable names directly to the decompilation project. The goal is to maintain the decompilation project as the "source of truth" for all knowledge about the game.

## Overview

* [`3rdparty`](/3rdparty): Contains code obtained from third parties, not including Mindscape. Generally, these are libraries that have been placed in the public domain or are freely available on the web. As these are unaltered files, our style guide (see below) does not apply.
* [`CONFIG`](/CONFIG): Source code based on the decompiled `CONFIG.EXE`. It depends on some code in `LEGO1`.
* [`ISLE`](/ISLE): Source code based on the decompiled `ISLE.EXE`. It depends on some code in `LEGO1`.
* [`LEGO1`](/LEGO1): Source code based on the decompiled `LEGO1.DLL`. This folder contains code from Mindscape's custom in-house engine called **Omni** (file pattern: `mx*`), the LEGO Island-specific extensions for Omni and the game's code (file pattern: `lego*`) as well as several utility libraries developed by Mindscape.
* [`tools`](/tools): A set of tools aiding in the modernization effort.
* [`util`](/util): Utility headers aiding in the modernization effort.

## Code Style

In general, we're not exhaustively strict about coding style, but there are some preferable guidelines to follow that have been adopted from what we know about the original codebase:

### Formatting

We are currently using [clang-format](https://clang.llvm.org/docs/ClangFormat.html) and [clang-tidy](https://clang.llvm.org/extra/clang-tidy/) with configuration files that aim to replicate the code formatting employed by the original developers. There are [integrations](https://clang.llvm.org/docs/ClangFormat.html#vim-integration) available for most editors and IDEs. The required `clang-format` version is `17.x`.

### Naming conventions

We are currently using a customized version of [ncc](https://github.com/nithinn/ncc) with a configuration file that aims to replicate the naming conventions employed by the original developers. `ncc` requires Clang `16.x`; please refer to the [tool](/tools/ncc) and the [GitHub action](/.github/workflows/naming.yml) for guidance.

## Questions?

For any further questions, feel free to ask in either the [Matrix chatroom](https://matrix.to/#/#isledecomp:matrix.org) or on the [forum](https://forum.mattkc.com/viewforum.php?f=1).

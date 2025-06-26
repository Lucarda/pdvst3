# pdvst3

Run Pure-Data inside a vst3 host.

Based on https://github.com/Lucarda/pdvst-0.52 but updated to vst3 
(Linux, macOS and Windows) and to load with Pd-0.55-2 or higher.

Get pre-built binaries on https://github.com/Lucarda/pdvst3/releases
or build. see [compiling.md](compiling.md)


## How does it work ?

When a pdvst3 plugin is opened by the host application, a setup file 
(config.txt) is read to determine information about the plugin, such as
the Pd patch file to use, the number of parameters, etc...
An instance of Pd (that optionally can be shipped inside the plug in)
is started and opens the Pd patch file whose name was found in the setup file.

## Installation

You can use your current pure-data installation (>= Pd-0.55-2) with your
favorite externals.

Copy your plugin `.vst3` bundle file (e.g. `pdvst3.vst3`) to the
default vst3 plugins directory for your OS.

default dirs:

**Windows**:
`C:\Program Files\Common Files\VST3`

**macOS**:
`/Library/Audio/Plug-Ins/VST3/` or `~/Library/Audio/Plug-Ins/VST3/`

**Linux**:
`$HOME/.vst3/`, `/usr/lib/vst3/`, and `/usr/local/lib/vst3/`.
The `~/.vst3/` directory is for user-specific plugins, while `/usr/lib/vst3/`
and `/usr/local/lib/vst3/` are for global (system-wide) plugins. Consult
your Linux distro docs if needed.

## Creating VST Plugins from Pd Patches

1) Make a copy of the `pdvst3.vst3` bundle and
rename it (must be lowercase) to for example "myplug.vst3".
If needed move your new plugin bundle to the vst3 plugins folder of your OS.

2) Edit `config.txt` setup file inside the bundle (see the Setup File section).
Make sure you update the NAME (= myplug) and ID (= <some random 4 letters>).

3)
**Linux**: inside the bundle locate the <plug>.so file ie
`<plug>/Contents/x86_64-linux/pdvst3.so` and rename the .so
file as `myplug.so`

**Windows**: inside the bundle locate the <plug>.dll file ie
`<plug>\Contents\x86_64-win\pdvst3.dll` and rename the .dll
file as `myplug.dll`

**macOS** (untested yet): inside the bundle locate the <plug> file ie
`<plug>/Contents/MacOS/pdvst3` and rename the file as `myplug`.
Then edit `<plug>/Contents/Info.plist` and change occurrence of "pdvst3"
to "myplug" (no quotes).

4) place your .pd patch(es) inside the bundle and update "config.txt"
with the new MAIN (= somepatch.pd)

## The `config.txt` setup file

This file contains all of the information about your plugin. The format is ASCII
text with keys and values separated by an '=' character and each key and value
pair separated by a carriage return. Comments are demarked with a '#' character.

  -Keys-

    PLUGNAME = <string>
    # Name of plugin must be lowercase and match with vst3 bundle name.

    MAIN = <string>
    # The .pd file for Pd to open when the plugin is opened.

    ID = <string[4]>
    # The 4-character unique ID for the VST plugin. This is required by VST and
    # just needs to be a unique string of four characters.

    CHANNELS = <integer>
	# Number of audio input and output channels.
	# must be a multiple of 2. (2, 4, 6, ...)
	# if you change this your host must rescan the plugins
	# or changes wont show up when you load the plugin.

    PDPATH_LINUX = /home/lucarda/Downloads/pure-data
    PDPATH_MAC = /Applications/Pd-0.55-2.app
    PDPATH_WIN = C:\Program Files\Pd
    # Where to find Pd:
    # you can supply the path of your Pd installation or specify what Pd to use.
    # Note: the following lines are appended to path:
    #   Windows: "\bin\pd.exe"
    #   macOS: "/Contents/Resources/bin/pd"
    #   Linux: "/bin/pd"
    # special wildcards:
    #   @plug_parent : find Pd in the parent dir of the plugin
    #   @resources   : find Pd inside the plugin's folder "Contents/Resources"
    #  example:
    #   PDPATH_LINUX = @resources
    # when using the wildcards the main Pd folder must be renamed per OS like:
    #   Windows: Pd-win
    #   macOS: Pd.app
    #   Linux: pd
    # Note: omit the final slash in path

    DEBUG = <TRUE/FALSE>
    # Boolean value stating whether to display the Pd GUI when the plugin is opened.

    PARAMETERS = <integer>
    # Number of parameters the plugin uses (up to 128).

    NAMEPARAMETER<integer> = <string>
    # Display name for parameters. Used when CUSTOMGUI is false or the VST host
    # doesn't support custom editors.

    VERSION = <string>
    AUTHOR = <string>
    URL = <string>
    MAIL = <string>
    # Optional info that shows in the vst host.

    PDMOREFLAGS = <string>
    # Flags to be passed when starting Pd.
    # flags we should not put here: -r, -outchannels, -inchannels
    # flag -nogui is set when we set DEBUG = FALSE


## Pd/VST audio/midi Communication

When the plugin is opened, the .pd patch file declared in the setup file's MAIN key
will be opened.

The Pd patch will receive its incoming audio stream from the adc~ object,
and incoming MIDI data from the Pd objects notein, etc.

Pd patches should output their audio stream to the dac~ object,
and their midi stream to the noteout, etc objects.

REMARKS :
- MIDI in out is rather limited in VST3 protocol.
- Inside puredata plugin, don't use anything on menu "media/audio
settings", you may crash pd & host.
- You can continue to use "media/midi settings" menu to select input
and output midi devices independently from host.

## Pd/VST3 - further communications

For purposes such as GUI interaction and VST automation, your patch may
need to communicate further with the VST host. Special Pd send/receive
symbols can be used in your Pd patch. For an example, see the example plugin.

- `rvstparameter<integer>` : Use this symbol to receive parameter values
from the VST host. Values will be floats between 0 and 1 inclusive.
- `svstparameter<integer>` : Use this symbol to send parameter values to
the VST host. Values should be floats between 0 and 1 inclusive.
- `svstdata` : Use this symbol to save a Pd list in a preset or in the
DAW project
- `rvstdata` : Use this symbol to receive a Pd list that was saved into
the preset or the DAW project. Triggered at load time or when the preset
gets loaded.
- `vstTimeInfo`: (play head information support) :

`vstTimeInfo.state`, `vstTimeInfo.tempo`, `vstTimeInfo.projectTimeMusic`,
`vstTimeInfo.barPositionMusic`, `vstTimeInfo.timeSigNumerator`,
`vstTimeInfo.timeSigDenominator` are receivers for getting time infos from host.
Names should change in the future.


## current features

- Multichannel audio in/out support
- Midi in out support
- Play head information support (see examples)

## additional notes on macOS

the plugin bundle is automatically build in github's CI and is not signed
with an apple-developer id. To use the plugin downloaded from here you must
remove its quarantine attributes:

- open Applications/Utilities/Terminal
- type "sudo xattr -r -d com.apple.quarantine" and add a space
- drag the pdvst3.vst3 bundle onto Terminal to add the path
- hit enter
- enter your password.

now you can place the bundle in your vst3 default dir and the host will
find it and make it available.


![vst logo](VST_Compatible_Logo_Steinberg_with_TM.png)

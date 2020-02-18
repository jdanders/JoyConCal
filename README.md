# JoyConCal

JoyConCal calibrates left and right Joy-Cons using a Windows console. This will allow good stick motion even when a Nintendo Switch is not available for calibration.

## Support this project
JoyConCal is a free product that I work on in my free time, so any contributions are greatly appreciated.

[![](https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=LZXRZRCXVQKX8&item_name=Create+useful+open+source+code&currency_code=USD&source=url)

## Installation

1. Download the latest zip from the [releases page](https://github.com/jdanders/JoyConCal/releases) and extract it somewhere
2. Install [Visual C++ 2015-2019 Redistributable x64](https://aka.ms/vs/16/release/vc_redist.x64.exe)
3. Run the executable

## Usage

1. Pair each of your Joy-Cons with Windows (hold down the button on the side to put into
   pairing mode, then go to add bluetooth device in Windows)
2. Ensure that both Joy-Cons show as "Connected" in your bluetooth devices page
3. Run JoyConCal.exe
4. Follow steps to calibrate left (-) and then right (+) Joy-Cons. You don't need to have both available, just one will work too.


## Steps

You will do the following:

* Wiggle the stick around a full couple of circles to get the Joy-Con reporting good numbers.
* Let go of the stick to center it, and then press a button to start calibration.
* Measure the full range of the sticks
  * Press the stick the full range of up, down, left, and right.
  * As you do so, watch the reported range, and keep going until you can't get the range numbers to change any more. You have to press pretty hard in this step to max out the range.
* Measure the dead zone.
  * The dead zone is the range in which the stick should be seen as centered. Because of the sensitivity of the sticks, it can report +/- a hundred or two even when untouched, depending on how loose your stick is.
  * Your goal is to measure the edges of the loose center of the stick. To get best results here, you're going to press the stick in the indicated direction, and then back off until you don't feel it resist your press any more.
  * Don't worry too much about which direction is "up", just make sure to do all four directions for the four steps of dead zone detection.

Note: The Joy-Con reports orientation as though attached to a Switch, but this calibration process can be done horizontally, just know that X and Y axes will be swapped.

## Tips

The Joy-Con reports a number which indicates the analog stick position. I've seen the number range from 500-4000 or so, max. The purpose of calibration is to record in the Joy-Con the actual range for any particular Joy-Con, as it varies.

The hardest part about calibrating the Joy-Con correctly is that if you push the stick really hard, you can push the range out even farther, but you are probably not going to push that hard during normal play. There are a couple of ways to account for this:

* Don't push that hard. I've found this to give really inconsistant calibration.
* Push as hard as you can, and record that range. This results in the real range never being used.
* Push as hard as you can, but record a scaled version of that range. This is the approach that seems to work best.

I found that pushing all the way until you can't change the range any more, and then scaling that range by 80% works pretty well (at least for the PC gaming I do with the Joy-Cons).


## Building

If you wish to build JoyConCal yourself, simply open the JoyConCal.sln file in Visual Studio 2019, and build. Everything should work out of the box but if it does not feel free to fix it and submit a pull request.

## Support

I only own two Joy-Cons (one left, one right), so that is all I have to test with. I don't own a Pro controller, so I can't add support for that. I also don't own a Switch so I have no idea how this process compares with the calibration the Switch does.

If you've got problems, I may be able to help you through an issue report, but only if I can reproduce the problem. If you have improvements, please feel free to do a pull request.

## Thanks

This project is based directly on [XJoy](https://github.com/sam0x17/XJoy) by [sam0x17](https://github.com/sam0x17), and then I replaced all the guts :)

This couldn't have been done without [dekuNukem](https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering) and his documentation, I also learned a lot from [mfosse](https://github.com/mfosse/JoyCon-Driver)'s JoyCon-Driver code.
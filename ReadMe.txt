==================== Info ===============================

  Capture RawInput keyboard messages.

  Sending MIDInotes to selected MIDI device.

  Many keyboards interpreted as one long piano-board.

==================== Download ============================

   Release\usbhid.exe
   Release\usbhid.cmd
   Release\midi_pc101.txt

   Run.
   Press any key on first keyboard.
   Press any key on second keyboard.

=================== Requirments ==========================

	0. Install Microsoft Visual C++ 2010 Redistributable Package (x86)
	 ( https://www.microsoft.com/en-US/download/details.aspx?id=5555 )

	1a. Install MIDI Yoke (It's a virtual cable between my program and other programs that have midi input's)
	   ( http://www.midiox.com/myoke.htm#Download )
	   ( MidiYokeSetup.msi for WinXP and later )
		Run it with capability for WinXP ("prev. windows")

	1b. Or Install loopbe1 ( http://www.nerds.de/en/download.html )

================== Test ==================================

	2. Install Pianoteq Trial version ( https://www.pianoteq.com/try )
	2a. Run 32bit version!

======================== Controls ========================

	NUMPAD + / -   -- octave shift
	F9 - random key
	F1 - show scan_codes (needed for midi_pc101.txt)
	Spacebar - Sustain Pedal

===================== Custom keymap ======================

	(image: http://computercraft.info/wiki/images/thumb/8/81/CC-Keyboard-Charcodes.png/1200px-CC-Keyboard-Charcodes.png )

	========= midi_pc101.txt =========
	SCAN_CODE MIDI_NOTE
	...
	SCAN_CODE MIDI_NOTE

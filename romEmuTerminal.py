#!/usr/bin/env python
#
# Rom Emulator Terminal 0.3.2
#
# Terminal/download application for use with the 
# Arduino Mega 2560 based ROM Emulator
# https://electrickery.nl/digaud/arduino/romEmu/
# fjkraan@electrickery.nl, 20240721
#
# This program is a GUI based version of the
# CLI romEmuFeed.py program, allowing downloading
# of HEX-intel files to the ROM Emulator and control 
# the settings.
#
# Note self.serial writes to the Arduino and self.text_ctrl_output writes
# to the terminal console.
#
# Adapted from:
#
# A simple terminal application with wxPython.
#
# (C) 2001-2020 Chris Liechti <cliechti@gmx.net>
#
# SPDX-License-Identifier:    BSD-3-Clause

import codecs
from serial.tools.miniterm import unichr
import serial
import serial.tools.list_ports
import threading
import time
import os
import sys
import wx
import wx.lib.newevent
import wxSerialConfigDialog

#try:
#    unichr
#except NameError:
#    unichr = chr
unichr = chr 

VERSION = '0.3.1'
RESETSW = True
  
print("Version:           " + str(VERSION)) 
print("Python version:    " + sys.version)
print("wxWidgets version: " + wx.version())


# A serial port configuration dialog for wxPython. A number of flags can
# be used to configure the fields that are displayed.
#
# (C) 2001-2015 Chris Liechti <cliechti@gmx.net>
#
# SPDX-License-Identifier:    BSD-3-Clause
#
# https://github.com/pyserial/pyserial/blob/master/examples/wxSerialConfigDialog.py


SHOW_BAUDRATE = 1 << 0
SHOW_FORMAT = 1 << 1
SHOW_FLOW = 1 << 2
SHOW_TIMEOUT = 1 << 3
SHOW_ALL = SHOW_BAUDRATE | SHOW_FORMAT | SHOW_FLOW | SHOW_TIMEOUT


class SerialConfigDialog(wx.Dialog):
    """\
    Serial Port configuration dialog, to be used with pySerial 2.0+
    When instantiating a class of this dialog, then the "serial" keyword
    argument is mandatory. It is a reference to a serial.Serial instance.
    the optional "show" keyword argument can be used to show/hide different
    settings. The default is SHOW_ALL which corresponds to
    SHOW_BAUDRATE|SHOW_FORMAT|SHOW_FLOW|SHOW_TIMEOUT. All constants can be
    found in this module (not the class).
    """

    def __init__(self, *args, **kwds):
        # grab the serial keyword and remove it from the dict
        self.serial = kwds['serial']
        del kwds['serial']
        self.show = SHOW_ALL
        if 'show' in kwds:
            self.show = kwds.pop('show')
        # begin wxGlade: SerialConfigDialog.__init__
        kwds["style"] = wx.DEFAULT_DIALOG_STYLE
        wx.Dialog.__init__(self, *args, **kwds)
        self.label_2 = wx.StaticText(self, -1, "Port")
        self.choice_port = wx.Choice(self, -1, choices=[])
        self.label_1 = wx.StaticText(self, -1, "Baudrate")
        self.combo_box_baudrate = wx.ComboBox(self, -1, choices=[], style=wx.CB_DROPDOWN)
        self.sizer_1_staticbox = wx.StaticBox(self, -1, "Basics")
        self.panel_format = wx.Panel(self, -1)
        self.label_3 = wx.StaticText(self.panel_format, -1, "Data Bits")
        self.choice_databits = wx.Choice(self.panel_format, -1, choices=["choice 1"])
        self.label_4 = wx.StaticText(self.panel_format, -1, "Stop Bits")
        self.choice_stopbits = wx.Choice(self.panel_format, -1, choices=["choice 1"])
        self.label_5 = wx.StaticText(self.panel_format, -1, "Parity")
        self.choice_parity = wx.Choice(self.panel_format, -1, choices=["choice 1"])
        self.sizer_format_staticbox = wx.StaticBox(self.panel_format, -1, "Data Format")
        self.panel_timeout = wx.Panel(self, -1)
        self.checkbox_timeout = wx.CheckBox(self.panel_timeout, -1, "Use Timeout")
        self.text_ctrl_timeout = wx.TextCtrl(self.panel_timeout, -1, "")
        self.label_6 = wx.StaticText(self.panel_timeout, -1, "seconds")
        self.sizer_timeout_staticbox = wx.StaticBox(self.panel_timeout, -1, "Timeout")
        self.panel_flow = wx.Panel(self, -1)
        self.checkbox_rtscts = wx.CheckBox(self.panel_flow, -1, "RTS/CTS")
        self.checkbox_xonxoff = wx.CheckBox(self.panel_flow, -1, "Xon/Xoff")
        self.sizer_flow_staticbox = wx.StaticBox(self.panel_flow, -1, "Flow Control")
        self.button_cancel = wx.Button(self, wx.ID_CANCEL, "")
        self.button_ok = wx.Button(self, wx.ID_OK, "")

        self.__set_properties()
        self.__do_layout()
        # end wxGlade
        # attach the event handlers
        self.__attach_events()



    def __set_properties(self):
        # begin wxGlade: SerialConfigDialog.__set_properties
        self.SetTitle("Serial Port Configuration")
        self.choice_databits.SetSelection(0)
        self.choice_stopbits.SetSelection(0)
        self.choice_parity.SetSelection(0)
        self.text_ctrl_timeout.Enable(False)
        self.button_ok.SetDefault()
        # end wxGlade
        self.SetTitle("Serial Port Configuration")
        if self.show & SHOW_TIMEOUT:
            self.text_ctrl_timeout.Enable(0)
        self.button_ok.SetDefault()

        if not self.show & SHOW_BAUDRATE:
            self.label_1.Hide()
            self.combo_box_baudrate.Hide()
        if not self.show & SHOW_FORMAT:
            self.panel_format.Hide()
        if not self.show & SHOW_TIMEOUT:
            self.panel_timeout.Hide()
        if not self.show & SHOW_FLOW:
            self.panel_flow.Hide()

        # fill in ports and select current setting
        preferred_index = 0
        self.choice_port.Clear()
        self.ports = []
        for n, (portname, desc, hwid) in enumerate(sorted(serial.tools.list_ports.comports())):
            self.choice_port.Append(u'{} - {}'.format(portname, desc))
            self.ports.append(portname)
            if self.serial.name == portname:
                preferred_index = n
        self.choice_port.SetSelection(preferred_index)
        if self.show & SHOW_BAUDRATE:
            preferred_index = None
            # fill in baud rates and select current setting
            self.combo_box_baudrate.Clear()
            for n, baudrate in enumerate(self.serial.BAUDRATES):
                self.combo_box_baudrate.Append(str(baudrate))
                if self.serial.baudrate == baudrate:
                    preferred_index = n
            if preferred_index is not None:
                self.combo_box_baudrate.SetSelection(preferred_index)
            else:
                self.combo_box_baudrate.SetValue(u'{}'.format(self.serial.baudrate))
        if self.show & SHOW_FORMAT:
            # fill in data bits and select current setting
            self.choice_databits.Clear()
            for n, bytesize in enumerate(self.serial.BYTESIZES):
                self.choice_databits.Append(str(bytesize))
                if self.serial.bytesize == bytesize:
                    index = n
            self.choice_databits.SetSelection(index)
            # fill in stop bits and select current setting
            self.choice_stopbits.Clear()
            for n, stopbits in enumerate(self.serial.STOPBITS):
                self.choice_stopbits.Append(str(stopbits))
                if self.serial.stopbits == stopbits:
                    index = n
            self.choice_stopbits.SetSelection(index)
            # fill in parities and select current setting
            self.choice_parity.Clear()
            for n, parity in enumerate(self.serial.PARITIES):
                self.choice_parity.Append(str(serial.PARITY_NAMES[parity]))
                if self.serial.parity == parity:
                    index = n
            self.choice_parity.SetSelection(index)
        if self.show & SHOW_TIMEOUT:
            # set the timeout mode and value
            if self.serial.timeout is None:
                self.checkbox_timeout.SetValue(False)
                self.text_ctrl_timeout.Enable(False)
            else:
                self.checkbox_timeout.SetValue(True)
                self.text_ctrl_timeout.Enable(True)
                self.text_ctrl_timeout.SetValue(str(self.serial.timeout))
        if self.show & SHOW_FLOW:
            # set the rtscts mode
            self.checkbox_rtscts.SetValue(self.serial.rtscts)
            # set the rtscts mode
            self.checkbox_xonxoff.SetValue(self.serial.xonxoff)

    def __do_layout(self):
        # begin wxGlade: SerialConfigDialog.__do_layout
        sizer_2 = wx.BoxSizer(wx.VERTICAL)      # Data Format
        sizer_3 = wx.BoxSizer(wx.HORIZONTAL)    # Cancel / OK
        self.sizer_flow_staticbox.Lower()
        sizer_flow = wx.StaticBoxSizer(self.sizer_flow_staticbox, wx.HORIZONTAL)
        self.sizer_timeout_staticbox.Lower()
        sizer_timeout = wx.StaticBoxSizer(self.sizer_timeout_staticbox, wx.HORIZONTAL)
        self.sizer_format_staticbox.Lower()
        sizer_format = wx.StaticBoxSizer(self.sizer_format_staticbox, wx.VERTICAL)
        grid_sizer_1 = wx.FlexGridSizer(3, 2, 0, 0) 
        self.sizer_1_staticbox.Lower()
        sizer_1 = wx.StaticBoxSizer(self.sizer_1_staticbox, wx.VERTICAL) # Basics; Port / Baudrate
        sizer_basics = wx.FlexGridSizer(3, 2, 0, 0)
        sizer_basics.Add(self.label_2, 0, wx.ALL | wx.ALIGN_CENTER_VERTICAL, 4)
        sizer_basics.Add(self.choice_port, 0, wx.EXPAND, 0)
        sizer_basics.Add(self.label_1, 0, wx.ALL | wx.ALIGN_CENTER_VERTICAL, 4)
        sizer_basics.Add(self.combo_box_baudrate, 0, wx.EXPAND, 0)
        sizer_basics.AddGrowableCol(1)
        sizer_1.Add(sizer_basics, 0, wx.EXPAND, 0)
        sizer_2.Add(sizer_1, 0, wx.EXPAND, 0)
        grid_sizer_1.Add(self.label_3, 1, wx.ALL | wx.ALIGN_CENTER_VERTICAL, 4)
        grid_sizer_1.Add(self.choice_databits, 1, wx.EXPAND | wx.ALIGN_RIGHT, 0)
        grid_sizer_1.Add(self.label_4, 1, wx.ALL | wx.ALIGN_CENTER_VERTICAL, 4)
        grid_sizer_1.Add(self.choice_stopbits, 1, wx.EXPAND | wx.ALIGN_RIGHT, 0)
        grid_sizer_1.Add(self.label_5, 1, wx.ALL | wx.ALIGN_CENTER_VERTICAL, 4)
        grid_sizer_1.Add(self.choice_parity, 1, wx.EXPAND | wx.ALIGN_RIGHT, 0)
        sizer_format.Add(grid_sizer_1, 1, wx.EXPAND, 0)
        self.panel_format.SetSizer(sizer_format)
        sizer_2.Add(self.panel_format, 0, wx.EXPAND, 0)
        sizer_timeout.Add(self.checkbox_timeout, 0, wx.ALL | wx.ALIGN_CENTER_VERTICAL, 4)
        sizer_timeout.Add(self.text_ctrl_timeout, 0, 0, 0)
        sizer_timeout.Add(self.label_6, 0, wx.ALL | wx.ALIGN_CENTER_VERTICAL, 4)
        self.panel_timeout.SetSizer(sizer_timeout)
        sizer_2.Add(self.panel_timeout, 0, wx.EXPAND, 0)
        sizer_flow.Add(self.checkbox_rtscts, 0, wx.ALL | wx.ALIGN_CENTER_VERTICAL, 4)
        sizer_flow.Add(self.checkbox_xonxoff, 0, wx.ALL | wx.ALIGN_CENTER_VERTICAL, 4)
        sizer_flow.Add((10, 10), 1, wx.EXPAND, 0)
        self.panel_flow.SetSizer(sizer_flow)
        sizer_2.Add(self.panel_flow, 0, wx.EXPAND, 0)
        sizer_3.Add(self.button_cancel, 0, 0, 0)
        sizer_3.Add(self.button_ok, 0, 0, 0)
        sizer_2.Add(sizer_3, 0, wx.ALL | wx.ALIGN_RIGHT, 4)
        self.SetSizer(sizer_2)
        sizer_2.Fit(self)
        self.Layout()
        # end wxGlade

    def __attach_events(self):
        self.button_ok.Bind(wx.EVT_BUTTON, self.OnOK)
        self.button_cancel.Bind(wx.EVT_BUTTON, self.OnCancel)
        if self.show & SHOW_TIMEOUT:
            self.checkbox_timeout.Bind(wx.EVT_CHECKBOX, self.OnTimeout)

    def OnOK(self, events):
        success = True
        self.serial.port = self.ports[self.choice_port.GetSelection()]
        if self.show & SHOW_BAUDRATE:
            try:
                b = int(self.combo_box_baudrate.GetValue())
            except ValueError:
                with wx.MessageDialog(
                        self,
                        'Baudrate must be a numeric value',
                        'Value Error',
                        wx.OK | wx.ICON_ERROR) as dlg:
                    dlg.ShowModal()
                success = False
            else:
                self.serial.baudrate = b
        if self.show & SHOW_FORMAT:
            self.serial.bytesize = self.serial.BYTESIZES[self.choice_databits.GetSelection()]
            self.serial.stopbits = self.serial.STOPBITS[self.choice_stopbits.GetSelection()]
            self.serial.parity = self.serial.PARITIES[self.choice_parity.GetSelection()]
        if self.show & SHOW_FLOW:
            self.serial.rtscts = self.checkbox_rtscts.GetValue()
            self.serial.xonxoff = self.checkbox_xonxoff.GetValue()
        if self.show & SHOW_TIMEOUT:
            if self.checkbox_timeout.GetValue():
                try:
                    self.serial.timeout = float(self.text_ctrl_timeout.GetValue())
                except ValueError:
                    with wx.MessageDialog(
                            self,
                            'Timeout must be a numeric value',
                            'Value Error',
                            wx.OK | wx.ICON_ERROR) as dlg:
                        dlg.ShowModal()
                    success = False
            else:
                self.serial.timeout = None
        if success:
            self.EndModal(wx.ID_OK)

    def OnCancel(self, events):
        self.EndModal(wx.ID_CANCEL)

    def OnTimeout(self, events):
        if self.checkbox_timeout.GetValue():
            self.text_ctrl_timeout.Enable(True)
        else:
            self.text_ctrl_timeout.Enable(False)
            

# end of class SerialConfigDialog


# ----------------------------------------------------------------------
# Create an own event type, so that GUI updates can be delegated
# this is required as on some platforms only the main thread can
# access the GUI without crashing. wxMutexGuiEnter/wxMutexGuiLeave
# could be used too, but an event is more elegant.

SerialRxEvent, EVT_SERIALRX = wx.lib.newevent.NewEvent()
SERIALRX = wx.NewEventType()

# ----------------------------------------------------------------------

ID_CLEAR    = wx.NewIdRef()
ID_DOWNLOAD = wx.NewIdRef()
ID_SAVEAS   = wx.NewIdRef()
ID_SETTINGS = wx.NewIdRef()
ID_TERM     = wx.NewIdRef()
ID_EXIT     = wx.NewIdRef()
ID_RTS      = wx.NewIdRef()
ID_DTR      = wx.NewIdRef()
ID_HELP     = wx.NewIdRef()
ID_ABOUT    = wx.NewIdRef()

NEWLINE_CR   = 0
NEWLINE_LF   = 1
NEWLINE_CRLF = 2

LF = "\n"
sendDelay = 0.05
resetDelay = 0.5

class TerminalSetup:
    """
    Placeholder for various terminal settings. Used to pass the
    options to the TerminalSettingsDialog.
    """
    def __init__(self):
        self.echo = False
        self.unprintable = False
        self.newline = NEWLINE_CRLF


class TerminalSettingsDialog(wx.Dialog):
    """Simple dialog with common terminal settings like echo, newline mode."""

    def __init__(self, *args, **kwds):
        self.settings = kwds['settings']
        del kwds['settings']
        # begin wxGlade: TerminalSettingsDialog.__init__
        kwds["style"] = wx.DEFAULT_DIALOG_STYLE
        wx.Dialog.__init__(self, *args, **kwds)
        self.checkbox_echo = wx.CheckBox(self, -1, "Local Echo")
        self.checkbox_unprintable = wx.CheckBox(self, -1, "Show unprintable characters")
        self.radio_box_newline = wx.RadioBox(self, -1, "Newline Handling", choices=["CR only", "LF only", "CR+LF"], majorDimension=0, style=wx.RA_SPECIFY_ROWS)
        self.sizer_4_staticbox = wx.StaticBox(self, -1, "Input/Output")
        self.button_cancel = wx.Button(self, wx.ID_CANCEL, "")
        self.button_ok = wx.Button(self, wx.ID_OK, "")

        self.__set_properties()
        self.__do_layout()
        # end wxGlade
        self.__attach_events()
        self.checkbox_echo.SetValue(True)
        self.checkbox_unprintable.SetValue(self.settings.unprintable)
        self.radio_box_newline.SetSelection(self.settings.newline)

    def __set_properties(self):
        # begin wxGlade: TerminalSettingsDialog.__set_properties
        self.SetTitle("Terminal Settings")
        self.radio_box_newline.SetSelection(0)
        self.checkbox_echo.SetValue(True)     # Added for ROM Emulator Terminal
        self.button_ok.SetDefault()
        # end wxGlade

    def __do_layout(self):
        # begin wxGlade: TerminalSettingsDialog.__do_layout
        sizer_2 = wx.BoxSizer(wx.VERTICAL)
        sizer_3 = wx.BoxSizer(wx.HORIZONTAL)
        self.sizer_4_staticbox.Lower()
        sizer_4 = wx.StaticBoxSizer(self.sizer_4_staticbox, wx.VERTICAL)
        sizer_4.Add(self.checkbox_echo, 0, wx.ALL, 4)
        sizer_4.Add(self.checkbox_unprintable, 0, wx.ALL, 4)
        sizer_4.Add(self.radio_box_newline, 0, 0, 0)
        sizer_2.Add(sizer_4, 0, wx.EXPAND, 0)
        sizer_3.Add(self.button_ok, 0, 0, 0)
        sizer_3.Add(self.button_cancel, 0, 0, 0)
        sizer_2.Add(sizer_3, 0, wx.ALL | wx.ALIGN_RIGHT, 4)
        self.SetSizer(sizer_2)
        sizer_2.Fit(self)
        self.Layout()
        # end wxGlade

    def __attach_events(self):
        self.Bind(wx.EVT_BUTTON, self.OnOK, id=self.button_ok.GetId())
        self.Bind(wx.EVT_BUTTON, self.OnCancel, id=self.button_cancel.GetId())

    def OnOK(self, events):
        """Update data with new values and close dialog."""
        self.settings.echo = self.checkbox_echo.GetValue()
        self.settings.unprintable = self.checkbox_unprintable.GetValue()
        self.settings.newline = self.radio_box_newline.GetSelection()
        self.EndModal(wx.ID_OK)

    def OnCancel(self, events):
        """Do not update data but close dialog."""
        self.EndModal(wx.ID_CANCEL)

# end of class TerminalSettingsDialog


class TerminalFrame(wx.Frame):
    """Simple terminal program for wxPython"""

    def __init__(self, *args, **kwds):
        self.serial = serial.Serial()
        self.serial.timeout = 0.5   # make sure that the alive event can be checked from time to time
        self.settings = TerminalSetup()  # placeholder for the settings
        self.settings.echo = True
        self.thread = None
        self.alive = threading.Event()
        # begin wxGlade: TerminalFrame.__init__
        kwds["style"] = wx.DEFAULT_FRAME_STYLE
        wx.Frame.__init__(self, *args, **kwds)

        # Menu Bar
        self.frame_terminal_menubar = wx.MenuBar()
        wxglade_tmp_menu = wx.Menu()
        wxglade_tmp_menu.Append(ID_CLEAR, "&Clear", "", wx.ITEM_NORMAL)
        wxglade_tmp_menu.Append(ID_DOWNLOAD, "&Download hex file...", "", wx.ITEM_NORMAL)
        wxglade_tmp_menu.Append(ID_SAVEAS, "&Save Text As...", "", wx.ITEM_NORMAL)
        wxglade_tmp_menu.AppendSeparator()
        wxglade_tmp_menu.Append(ID_TERM, "&Terminal Settings...", "", wx.ITEM_NORMAL)
        wxglade_tmp_menu.AppendSeparator()
        wxglade_tmp_menu.Append(ID_EXIT, "&Exit", "", wx.ITEM_NORMAL)
        self.frame_terminal_menubar.Append(wxglade_tmp_menu, "&File")
        
        wxglade_tmp_menu = wx.Menu()
        wxglade_tmp_menu.Append(ID_RTS, "RTS", "", wx.ITEM_CHECK)
        wxglade_tmp_menu.Append(ID_DTR, "&DTR", "", wx.ITEM_CHECK)
        wxglade_tmp_menu.Append(ID_SETTINGS, "&Port Settings...", "", wx.ITEM_NORMAL)
        self.frame_terminal_menubar.Append(wxglade_tmp_menu, "Serial Port")
        
        wxglade_tmp_menu = wx.Menu()
        wxglade_tmp_menu.Append(ID_HELP, "&Help", "", wx.ITEM_NORMAL)
        wxglade_tmp_menu.Append(ID_ABOUT, "&About...", "", wx.ITEM_NORMAL)
        self.frame_terminal_menubar.Append(wxglade_tmp_menu, "Help")
        
        self.SetMenuBar(self.frame_terminal_menubar)
        # Menu Bar end
        self.text_ctrl_output = wx.TextCtrl(self, -1, "", style=wx.TE_MULTILINE | wx.TE_READONLY)

        self.__set_properties()
        self.__do_layout()

        self.Bind(wx.EVT_MENU, self.OnClear, id=ID_CLEAR)
        self.Bind(wx.EVT_MENU, self.OnDownload, id=ID_DOWNLOAD)
        self.Bind(wx.EVT_MENU, self.OnSaveAs, id=ID_SAVEAS)
        self.Bind(wx.EVT_MENU, self.OnTermSettings, id=ID_TERM)
        self.Bind(wx.EVT_MENU, self.OnExit, id=ID_EXIT)
        self.Bind(wx.EVT_MENU, self.OnRTS, id=ID_RTS)
        self.Bind(wx.EVT_MENU, self.OnDTR, id=ID_DTR)
        self.Bind(wx.EVT_MENU, self.OnPortSettings, id=ID_SETTINGS)
        self.Bind(wx.EVT_MENU, self.OnHelp, id=ID_HELP)
        self.Bind(wx.EVT_MENU, self.OnAbout, id=ID_ABOUT)
        # end wxGlade
        self.__attach_events()          # register events
        self.OnPortSettings(None)       # call setup dialog on startup, opens port
        if not self.alive.is_set():
            self.Close()

    def StartThread(self):
        """Start the receiver thread"""
        self.thread = threading.Thread(target=self.ComPortThread)
        self.thread.daemon = True
        self.alive.set()
        self.thread.start()
        self.serial.rts = True
        self.serial.dtr = True
        self.frame_terminal_menubar.Check(ID_RTS, self.serial.rts)
        self.frame_terminal_menubar.Check(ID_DTR, self.serial.dtr)

    def StopThread(self):
        """Stop the receiver thread, wait until it's finished."""
        if self.thread is not None:
            self.alive.clear()          # clear alive event for thread
            self.thread.join()          # wait until thread has finished
            self.thread = None

    def __set_properties(self):
        # begin wxGlade: TerminalFrame.__set_properties
        self.SetTitle("Serial Terminal")
        self.SetSize((546, 383))
        self.text_ctrl_output.SetFont(wx.Font(9, wx.MODERN, wx.NORMAL, wx.NORMAL, 0, ""))
        # end wxGlade

    def __do_layout(self):
        # begin wxGlade: TerminalFrame.__do_layout
        sizer_1 = wx.BoxSizer(wx.VERTICAL)
        sizer_1.Add(self.text_ctrl_output, 1, wx.EXPAND, 0)
        self.SetSizer(sizer_1)
        self.Layout()
        # end wxGlade

    def __attach_events(self):
        # register events at the controls
        self.Bind(wx.EVT_MENU, self.OnClear, id=ID_CLEAR)
        self.Bind(wx.EVT_MENU, self.OnSaveAs, id=ID_SAVEAS)
        self.Bind(wx.EVT_MENU, self.OnExit, id=ID_EXIT)
        self.Bind(wx.EVT_MENU, self.OnPortSettings, id=ID_SETTINGS)
        self.Bind(wx.EVT_MENU, self.OnTermSettings, id=ID_TERM)
        self.text_ctrl_output.Bind(wx.EVT_CHAR, self.OnKey)
        self.Bind(wx.EVT_CHAR_HOOK, self.OnKey)
        self.Bind(EVT_SERIALRX, self.OnSerialRead)
        self.Bind(wx.EVT_CLOSE, self.OnClose)

    def OnExit(self, event):  # wxGlade: TerminalFrame.<event_handler>
        """Menu point Exit"""
        self.Close()

    def OnClose(self, event):
        """Called on application shutdown."""
        self.StopThread()               # stop reader thread
        self.serial.close()             # cleanup
        self.Destroy()                  # close windows, exit app

    def OnSaveAs(self, event):  # wxGlade: TerminalFrame.<event_handler>
        """Save contents of output window."""
        with wx.FileDialog(
                None,
                "Save Text As...",
                ".",
                "",
                "Text File|*.txt|All Files|*",
                wx.SAVE) as dlg:
            if dlg.ShowModal() == wx.ID_OK:
                filename = dlg.GetPath()
                with codecs.open(filename, 'w', encoding='utf-8') as f:
                    text = self.text_ctrl_output.GetValue().encode("utf-8")
                    f.write(text)

    def OnClear(self, event):  # wxGlade: TerminalFrame.<event_handler>
        """Clear contents of output window."""
        self.text_ctrl_output.Clear()
        
    def OnDownload(self, event):
        """ Open a file"""
        self.dirname = ''
        dlg = wx.FileDialog(self, "Choose a file", self.dirname, "", "*.*", wx.FD_OPEN)
        
        if dlg.ShowModal() == wx.ID_OK:
            if RESETSW:
                self.serial.write(("R1" + LF).encode())
                time.sleep(resetDelay)
                self.text_ctrl_output.AppendText(("Relay on" + LF).encode())
            self.filename = dlg.GetFilename()
            self.dirname = dlg.GetDirectory()
            f = open(os.path.join(self.dirname, self.filename), 'r')
            for line in f.readlines():
                print(line.strip())
                lineStrip = line.strip()
                self.serial.write((lineStrip + LF).encode())
                time.sleep(sendDelay)
            f.close()
            byteStr = ("File: '" + self.dirname + "/" + self.filename + "' downloaded.\r\n").encode()
            self.text_ctrl_output.AppendText(byteStr)
            if RESETSW:
                self.serial.write(("R0" + LF).encode())
                time.sleep(resetDelay)
                self.text_ctrl_output.AppendText(("Relay off" + LF).encode())
        dlg.Destroy()


    def OnPortSettings(self, event):  # wxGlade: TerminalFrame.<event_handler>
        """
        Show the port settings dialog. The reader thread is stopped for the
        settings change.
        """
        if event is not None:           # will be none when called on startup
            self.StopThread()
            self.serial.close()
        ok = False
        while not ok:
            with wxSerialConfigDialog.SerialConfigDialog(
                    self,
                    -1,
                    "",
                    show=wxSerialConfigDialog.SHOW_BAUDRATE | wxSerialConfigDialog.SHOW_FORMAT | wxSerialConfigDialog.SHOW_FLOW,
                    serial=self.serial) as dialog_serial_cfg:
                dialog_serial_cfg.CenterOnParent()
                result = dialog_serial_cfg.ShowModal()
            # open port if not called on startup, open it on startup and OK too
            if result == wx.ID_OK or event is not None:
                try:
                    self.serial.open()
                except serial.SerialException as e:
                    with wx.MessageDialog(self, str(e), "Serial Port Error", wx.OK | wx.ICON_ERROR)as dlg:
                        dlg.ShowModal()
                else:
                    self.StartThread()
                    self.SetTitle("ROM Emulator Terminal on {} [{},{},{},{}{}{}]".format(
                        self.serial.portstr,
                        self.serial.baudrate,
                        self.serial.bytesize,
                        self.serial.parity,
                        self.serial.stopbits,
                        ' RTS/CTS' if self.serial.rtscts else '',
                        ' Xon/Xoff' if self.serial.xonxoff else '',
                        ))
                    ok = True
            else:
                # on startup, dialog aborted
                self.alive.clear()
                ok = True

    def OnTermSettings(self, event):  # wxGlade: TerminalFrame.<event_handler>
        """\
        Menu point Terminal Settings. Show the settings dialog
        with the current terminal settings.
        """
        with TerminalSettingsDialog(self, -1, "", settings=self.settings) as dialog:
            dialog.CenterOnParent()
            dialog.ShowModal()

    def OnKey(self, event):
        """\
        Key event handler. If the key is in the ASCII range, write it to the
        serial port. Newline handling and local echo is also done here.
        """
        code = event.GetUnicodeKey()
        if code < 256:   # XXX bug in some versions of wx returning only capital letters
            code = event.GetKeyCode()
        if code == 13:                      # is it a newline? (check for CR which is the RETURN key)
            if self.settings.echo:          # do echo if needed 
                self.text_ctrl_output.AppendText('\n')
            if self.settings.newline == NEWLINE_CR:  # these go to the ROM Emu
                self.serial.write(b'\r')     # send CR
            elif self.settings.newline == NEWLINE_LF:
                self.serial.write(b'\n')     # send LF
            elif self.settings.newline == NEWLINE_CRLF:
                self.serial.write(b'\r\n')   # send CR+LF
        else:
            char = unichr(code)
            if self.settings.echo:          # do echo if needed
                self.WriteText(char)
            self.serial.write(char.encode('UTF-8', 'replace'))         # send the character
        event.StopPropagation()

    def WriteText(self, text):
        if self.settings.unprintable:
            text = ''.join([c if (c >= ' ' and c != '\x7f') else unichr(0x2400 + ord(c)) for c in text])
        self.text_ctrl_output.AppendText(text)

    def OnSerialRead(self, event):
        """Handle input from the serial port."""
        self.WriteText(event.data.decode('UTF-8', 'replace').replace('\r', '')) # remove \r from RomEmu output
        
    def OnHelp(self, event):
        """Sends a 'H' to the ROM Emulator."""
        helpString = 'H' + LF
        self.serial.write(helpString.encode('UTF-8', 'replace'))

    def OnAbout(self,e):
        # A message dialog box with an OK button. wx.OK is a standard ID in wxWidgets.
        aboutMessage = """    ROM Emulator terminal """ + str(VERSION) + """.

Derived from 
https://pyserial.readthedocs.io/en/latest/examples.html
for usage with the ROM Emulator
https://electrickery.nl/digaud/arduino/romEmu/

""" + sys.version + LF + wx.version()
        dlg = wx.MessageDialog( self, aboutMessage, "About ROM Emulator Terminal", wx.OK)
        dlg.ShowModal() # Show it
        dlg.Destroy() # finally destroy it when finished.

    def ComPortThread(self):
        """\
        Thread that handles the incoming traffic. Does the basic input
        transformation (newlines) and generates an SerialRxEvent
        """
        while self.alive.is_set():
            b = self.serial.read(self.serial.in_waiting or 1)
            if b:
                # newline transformation
                if self.settings.newline == NEWLINE_CR:
                    b = b.replace(b'\r', b'\n')
                elif self.settings.newline == NEWLINE_LF:
                    pass
                elif self.settings.newline == NEWLINE_CRLF:
                    b = b.replace(b'\r\n', b'\n')
                wx.PostEvent(self, SerialRxEvent(data=b))

    def OnRTS(self, event):  # wxGlade: TerminalFrame.<event_handler>
        self.serial.rts = event.IsChecked()

    def OnDTR(self, event):  # wxGlade: TerminalFrame.<event_handler>
        self.serial.dtr = event.IsChecked()

# end of class TerminalFrame


class MyApp(wx.App):
    def OnInit(self):
        frame_terminal = TerminalFrame(None, -1, "")
        self.SetTopWindow(frame_terminal)
        frame_terminal.Show(True)
        return 1

# end of class MyApp

if __name__ == "__main__":
    app = MyApp(0)
    app.MainLoop()

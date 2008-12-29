#!/usr/bin/env python

import sys
import wx
#import wx.py.editor as editor
import wx.lib.editor as editor

class UIEditor(wx.Dialog):
  def __init__(self, parent):
    pre=wx.PreDialog()
    pre.Create(parent,wx.ID_ANY,title='foo',pos=(100,100),size=(400,400),style=wx.DEFAULT_DIALOG_STYLE)
    self.PostCreate(pre)

    self.editor=editor.Editor(self,wx.ID_ANY)

